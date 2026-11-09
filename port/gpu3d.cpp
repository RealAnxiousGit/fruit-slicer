// 3D fruit for every SDL renderer: lit and sorted on the CPU, then drawn as textured triangles.
// That keeps one code path for DirectX, OpenGL and Vulkan. The HD detail maps multiply over the
// colours the meshes already carry.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include <vector>

#include "gfx_sdl.h"
#include "model3d_internal.h"
#include "render.h"

namespace m3dport {
using m3d::PartData;
using m3d::Pose;
using m3d::Vertex;

static const char* const kTexNames[m3d::MESH_COUNT] = {
	"apple", "orange", "lemon", "pear", "plum", "melon", "kiwi", "berry", "peach", "pineapple", "coconut", "gold", "bomb", "sunburst",
	"banana", "mango", "cherry", "dragon", "spiky", "grapes", "grape", "honey", "jelly", "chili", "coin", "bunch", "pomegranate", "pinata",
};
static SDL_Texture* sTex[m3d::MESH_COUNT];

struct Uv {
	float u, v;
};
// Texture coordinates by the vertices they belong to. A fruit's are worked out once, and a cut half's
// once per cut, because the angles behind them are the dearest part of drawing it.
static std::unordered_map<const Vertex*, std::vector<Uv>> sUvCache;

// Meshes are read by the CPU here, so nothing needs handing over. The call does mean a cut half changed.
void flush(const void* p, u32) {
	auto it = sUvCache.find((const Vertex*)p);
	if (it != sUvCache.end()) it->second.clear();
}

// A detail map holds its picture twice side by side, so a triangle across the seam can run past the
// first copy with no wrap mode. Its last rows are white, for the parts of a fruit that have no skin.
static constexpr float kVScale = 0.97f;
static constexpr float kWhiteV = 0.995f;

static SDL_Texture* loadDetail(const char* name) {
	char path[64];
	snprintf(path, sizeof path, "tex/%s.png", name);
	SDL_Surface* src = gfx::halveForQuality(gfx::loadPng(path), 2);
	if (!src) return nullptr;
	SDL_Surface* rgb = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
	SDL_DestroySurface(src);
	if (!rgb) return nullptr;
	const int w = rgb->w, h = rgb->h;
	std::vector<u8> px((size_t)w * 2 * h * 4);
	for (int y = 0; y < h; y++) {
		const u8* row = (const u8*)rgb->pixels + y * rgb->pitch;
		u8* out = &px[(size_t)y * w * 2 * 4];
		SDL_memcpy(out, row, w * 4);
		SDL_memcpy(out + w * 4, row, w * 4);
		if (y >= h - h / 40) SDL_memset(out, 255, (size_t)w * 2 * 4);
	}
	SDL_DestroySurface(rgb);
	SDL_Texture* t = SDL_CreateTexture(gfx::renderer(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w * 2, h);
	if (!t) return nullptr;
	SDL_UpdateTexture(t, nullptr, px.data(), w * 2 * 4);
	SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
	SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
	return t;
}

bool init() {
	if (gfx::softwareRenderer()) return false;  // thousands of triangles are for GPUs: this renderer uses 2D art
	for (int i = 0; i < m3d::MESH_COUNT; i++) sTex[i] = loadDetail(kTexNames[i]);
	return true;
}

void shutdown() {
	for (auto& t : sTex) {
		if (t) SDL_DestroyTexture(t);
		t = nullptr;
	}
}

// Texture Quality changed. Called between frames, so no batch still points at the old skins.
void reloadTextures() {
	if (gfx::softwareRenderer()) return;
	for (int i = 0; i < m3d::MESH_COUNT; i++) {
		SDL_Texture* t = loadDetail(kTexNames[i]);
		if (!t) continue;  // keep the old skin rather than none
		if (sTex[i]) SDL_DestroyTexture(sTex[i]);
		sTex[i] = t;
	}
}

void* alloc(u32 bytes) { return malloc(bytes); }
void release(void* p) {
	sUvCache.clear();  // meshes are going away, and the next ones may land at the same addresses
	free(p);
}
void begin(float, float, bool) {}
void end() {}

struct Lit {
	float x, y, z, facing;
	SDL_FColor c;
};
struct Order {
	float z;
	u32 at;
};
static std::vector<Lit> sLit;
static std::vector<Order> sOrder;
static std::vector<u32> sSorted;
static float sM[3][3];  // the pose being drawn

static inline void uvOf(const Vertex& p, Uv& o) {
	float r = SDL_sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
	if (r < 0.001f) r = 0.001f;
	o.u = SDL_atan2f(p.z, p.x) / (2 * 3.14159265f) + 0.5f;
	o.v = SDL_acosf(clampf(p.y / r, -1, 1)) / 3.14159265f;
}

// Tilt, then Rz, Rx, Ry: the same pose as the 3DS. Then its light: soft and directional, with a small highlight.
static void lightVerts(const Vertex* v, u32 n, const Pose& p) {
	float cz = SDL_cosf(p.rz), sz = SDL_sinf(p.rz), cx = SDL_cosf(p.rx), sx = SDL_sinf(p.rx), cy = SDL_cosf(p.ry), sy = SDL_sinf(p.ry);
	const float ry[3][3] = {{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}};
	float xy[3][3], m[3][3];
	for (int c = 0; c < 3; c++) {
		xy[0][c] = ry[0][c];
		xy[1][c] = cx * ry[1][c] - sx * ry[2][c];
		xy[2][c] = sx * ry[1][c] + cx * ry[2][c];
	}
	for (int c = 0; c < 3; c++) {
		m[0][c] = cz * xy[0][c] - sz * xy[1][c];
		m[1][c] = sz * xy[0][c] + cz * xy[1][c];
		m[2][c] = xy[2][c];
	}
	if (p.tilt != 0) {
		float ax = p.axisX, ay = p.axisY, len = SDL_sqrtf(ax * ax + ay * ay);
		if (len > 0.001f) {
			ax /= len;
			ay /= len;
			float c = SDL_cosf(p.tilt), s = SDL_sinf(p.tilt), t = 1 - c;
			const float r[3][3] = {{c + ax * ax * t, ax * ay * t, ay * s}, {ax * ay * t, c + ay * ay * t, -ax * s}, {-ay * s, ax * s, c}};
			float o[3][3];
			for (int i = 0; i < 3; i++)
				for (int j = 0; j < 3; j++) o[i][j] = r[i][0] * m[0][j] + r[i][1] * m[1][j] + r[i][2] * m[2][j];
			SDL_memcpy(m, o, sizeof m);
		}
	}
	float unit = gfx::viewScale();
	float ox, oy;
	gfx::toScreen(p.x, p.y, &ox, &oy);
	if (sLit.size() < n) sLit.resize(n);  // never shrunk: growing back would clear it for no reason
	const float s = p.scale;
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++) sM[i][j] = m[i][j];
	for (u32 i = 0; i < n; i++) {
		const Vertex& q = v[i];
		Lit& l = sLit[i];
		l.x = ox + (m[0][0] * q.x + m[0][1] * q.y + m[0][2] * q.z) * s * unit;
		l.y = oy + (m[1][0] * q.x + m[1][1] * q.y + m[1][2] * q.z) * s * unit;
		l.z = (m[2][0] * q.x + m[2][1] * q.y + m[2][2] * q.z) * s;
		l.facing = (m[2][0] * q.nx + m[2][1] * q.ny + m[2][2] * q.nz) / 127.0f;
		l.c.a = 0;  // not lit yet: about half of a fruit faces away and never needs it
	}
}

static inline const Lit& lit(const Vertex* v, u32 i) {
	Lit& l = sLit[i];
	if (l.c.a == 1) return l;
	const Vertex& q = v[i];
	float nx = q.nx / 127.0f, ny = q.ny / 127.0f, nz = q.nz / 127.0f;
	float wx = sM[0][0] * nx + sM[0][1] * ny + sM[0][2] * nz;
	float wy = sM[1][0] * nx + sM[1][1] * ny + sM[1][2] * nz;
	float wz = l.facing;
	float diff = -0.46f * wx - 0.62f * wy + 0.64f * wz;
	if (diff < 0) diff = 0;
	float h = -0.25f * wx - 0.34f * wy + 0.9f * wz;
	if (h < 0) h = 0;
	h = h * h;
	h = h * h;
	h = h * h;
	float ambient = 0.62f;
	// Ambient occlusion: less of the room's light reaches the parts of a fruit that face the board.
	if (gSave.ambientOcc) ambient *= 0.8f + 0.2f * (0.5f - 0.5f * wy);
	float shade = (ambient + 0.5f * diff) / 255.0f, high = 0.32f * h;
	l.c.r = clampf(q.r * shade + high, 0, 1);
	l.c.g = clampf(q.g * shade + high, 0, 1);
	l.c.b = clampf(q.b * shade + high, 0, 1);
	l.c.a = 1;
	return l;
}

static inline void emit(SDL_Vertex* o, const Lit& l, float u, float v) {
	o->position = {l.x, l.y};
	o->color = l.c;
	o->tex_coord = {u, v};
}

// Without a depth buffer, far triangles go first so that the near side ends up on top. Depth is sorted
// into fine buckets instead of compared: a mesh is thousands of triangles, drawn again every frame.
// `body` is where the skin ends in the triangle list: past it are stems, flesh and cut faces.
static constexpr int kDepthBuckets = 512;
static void sortAndDraw(int mesh, const Vertex* v, u32 tris, const u16* idx, const Uv* uv, u32 body) {
	sOrder.clear();
	sOrder.reserve(tris);
	float nearest = -1e9f, farthest = 1e9f;
	for (u32 t = 0; t < tris; t++) {
		u32 a = idx ? idx[t * 3] : t * 3, b = idx ? idx[t * 3 + 1] : t * 3 + 1, c = idx ? idx[t * 3 + 2] : t * 3 + 2;
		if (sLit[a].facing < 0 && sLit[b].facing < 0 && sLit[c].facing < 0) continue;  // the fruit's far side
		float z = sLit[a].z + sLit[b].z + sLit[c].z;
		sOrder.push_back({z, t});
		if (z > nearest) nearest = z;
		if (z < farthest) farthest = z;
	}
	u32 count = (u32)sOrder.size();
	if (count == 0) return;
	static u32 starts[kDepthBuckets + 1];
	SDL_memset(starts, 0, sizeof starts);
	float toBucket = nearest > farthest ? (kDepthBuckets - 1) / (nearest - farthest) : 0;
	for (const Order& e : sOrder) starts[(int)((e.z - farthest) * toBucket) + 1]++;
	for (int i = 0; i < kDepthBuckets; i++) starts[i + 1] += starts[i];
	if (sSorted.size() < count) sSorted.resize(count);
	for (const Order& e : sOrder) sSorted[starts[(int)((e.z - farthest) * toBucket)]++] = e.at;

	SDL_Vertex* o = gfx::reserveTris(sTex[mesh], (int)count * 3);
	if (!o) return;
	for (u32 i = 0; i < count; i++) {
		u32 t = sSorted[i];
		u32 a = idx ? idx[t * 3] : t * 3, b = idx ? idx[t * 3 + 1] : t * 3 + 1, c = idx ? idx[t * 3 + 2] : t * 3 + 2;
		const Lit& la = lit(v, a);
		const Lit& lb = lit(v, b);
		const Lit& lc = lit(v, c);
		if (t * 3 < body && sTex[mesh]) {
			float u0 = uv[a].u, u1 = uv[b].u, u2 = uv[c].u;
			float lo = SDL_min(u0, SDL_min(u1, u2)), hi = SDL_max(u0, SDL_max(u1, u2));
			if (hi - lo > 0.5f) {
				if (u0 < 0.5f) u0 += 1;
				if (u1 < 0.5f) u1 += 1;
				if (u2 < 0.5f) u2 += 1;
			}
			emit(o++, la, u0 * 0.5f, uv[a].v * kVScale);
			emit(o++, lb, u1 * 0.5f, uv[b].v * kVScale);
			emit(o++, lc, u2 * 0.5f, uv[c].v * kVScale);
		} else {
			emit(o++, la, 0.25f, kWhiteV);
			emit(o++, lb, 0.25f, kWhiteV);
			emit(o++, lc, 0.25f, kWhiteV);
		}
	}
}

static const Uv* uvsFor(const Vertex* v, u32 count) {
	std::vector<Uv>& uv = sUvCache[v];
	if (uv.size() != count) {
		uv.resize(count);
		for (u32 i = 0; i < count; i++) uvOf(v[i], uv[i]);
	}
	return uv.data();
}

void drawIndexed(int mesh, const PartData& d, const Pose& pose) {
	const Uv* uv = uvsFor(d.v, d.nv);
	lightVerts(d.v, d.nv, pose);
	sortAndDraw(mesh, d.v, d.ni / 3, d.idx, uv, d.body);
}

void drawCut(int mesh, const Vertex* v, u32 count, u32 surface, const Pose& pose) {
	const Uv* uv = uvsFor(v, count);
	lightVerts(v, count, pose);
	sortAndDraw(mesh, v, count / 3, nullptr, uv, surface);
}
}  // namespace m3dport
