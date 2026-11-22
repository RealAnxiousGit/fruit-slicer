#include "vector_logo.h"

#include <math.h>
#include <vector>

#include "render.h"

namespace gfx {
namespace {

struct Vec2 {
	float x, y;
};

struct LogoTri {
	Vec2 p[3];
	float yFrac[3];
	u8 layer;       // 0 cream, 1 ink, 2 fill, 3 slash glint
	s8 letterIdx;   // 0..10 for FRUIT SLICER
};

static std::vector<LogoTri> sTris;
static bool sBuilt = false;

struct Poly {
	const Vec2* pts;
	int n;
};

struct GlyphDef {
	char ch;
	const Poly* polys;
	int polyCount;
};

static const Vec2 kF0[] = {{4, 0}, {0, 0}, {0, 6}};
static const Vec2 kF1[] = {{0, 3}, {3, 3}};
static const Poly kPF[] = {{kF0, 3}, {kF1, 2}};

static const Vec2 kR0[] = {{0, 6}, {0, 0}, {3, 0}, {4, 1}, {4, 2.2f}, {3, 3.2f}, {0, 3.2f}};
static const Vec2 kR1[] = {{2, 3.2f}, {4, 6}};
static const Poly kPR[] = {{kR0, 7}, {kR1, 2}};

static const Vec2 kU0[] = {{0, 0}, {0, 5}, {1, 6}, {3, 6}, {4, 5}, {4, 0}};
static const Poly kPU[] = {{kU0, 6}};

static const Vec2 kI0[] = {{1, 0}, {3, 0}};
static const Vec2 kI1[] = {{2, 0}, {2, 6}};
static const Vec2 kI2[] = {{1, 6}, {3, 6}};
static const Poly kPI[] = {{kI0, 2}, {kI1, 2}, {kI2, 2}};

static const Vec2 kT0[] = {{0, 0}, {4, 0}};
static const Vec2 kT1[] = {{2, 0}, {2, 6}};
static const Poly kPT[] = {{kT0, 2}, {kT1, 2}};

static const Vec2 kS0[] = {{4, 1}, {3, 0}, {1, 0}, {0, 1}, {0, 2}, {1, 3}, {3, 3}, {4, 4}, {4, 5}, {3, 6}, {1, 6}, {0, 5}};
static const Poly kPS[] = {{kS0, 12}};

static const Vec2 kL0[] = {{0, 0}, {0, 6}, {4, 6}};
static const Poly kPL[] = {{kL0, 3}};

static const Vec2 kC0[] = {{4, 1}, {3, 0}, {1, 0}, {0, 1}, {0, 5}, {1, 6}, {3, 6}, {4, 5}};
static const Poly kPC[] = {{kC0, 8}};

static const Vec2 kE0[] = {{4, 0}, {0, 0}, {0, 6}, {4, 6}};
static const Vec2 kE1[] = {{0, 3}, {3, 3}};
static const Poly kPE[] = {{kE0, 4}, {kE1, 2}};

static const GlyphDef kGlyphs[] = {
    {'F', kPF, 2}, {'R', kPR, 2}, {'U', kPU, 1}, {'I', kPI, 3}, {'T', kPT, 2},
    {'S', kPS, 1}, {'L', kPL, 1}, {'C', kPC, 1}, {'E', kPE, 2},
};

static const GlyphDef* findGlyph(char c) {
	for (const auto& g : kGlyphs)
		if (g.ch == c) return &g;
	return nullptr;
}

static void glyphExtent(const GlyphDef* g, float* lo, float* hi) {
	float mn = 1e9f, mx = -1e9f;
	for (int p = 0; p < g->polyCount; p++) {
		for (int i = 0; i < g->polys[p].n; i++) {
			const float x = g->polys[p].pts[i].x;
			if (x < mn) mn = x;
			if (x > mx) mx = x;
		}
	}
	*lo = mn;
	*hi = mx;
}

static inline float yFracAt(float y) { return clampf((y + 54.0f) / 108.0f, 0.0f, 1.0f); }

// Diagonal blade cut across the 340x118 logo space, centered at (0, 0).
static inline float slashTopY(float x) { return -1.0f - (32.0f / 340.0f) * x; }
static inline float slashBotY(float x) { return 2.75f - (32.5f / 340.0f) * x; }
static inline float glintTopY(float x) { return 0.0f - (32.0f / 340.0f) * x; }
static inline float glintBotY(float x) { return 1.2f - (32.0f / 340.0f) * x; }

// Clips a polygon against ax + by + c >= 0 using Sutherland-Hodgman.
static int clipPolyHalfPlane(const Vec2* inPts, int inN, Vec2* outPts, float a, float b, float c) {
	if (inN < 3) return 0;
	int outN = 0;
	for (int i = 0; i < inN; i++) {
		const Vec2 p = inPts[i];
		const Vec2 q = inPts[(i + 1) % inN];
		const float dp = a * p.x + b * p.y + c;
		const float dq = a * q.x + b * q.y + c;
		const bool ip = dp >= -1e-5f;
		const bool iq = dq >= -1e-5f;
		if (ip) {
			if (outN < 12) outPts[outN++] = p;
		}
		if (ip != iq && fabsf(dp - dq) > 1e-6f) {
			const float t = dp / (dp - dq);
			if (outN < 12) outPts[outN++] = {p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t};
		}
	}
	return outN;
}

static void emitClippedTri(Vec2 a, Vec2 b, Vec2 c, u8 layer, s8 letterIdx) {
	const Vec2 src[3] = {a, b, c};
	if (layer == 3) {
		Vec2 buf1[12], buf2[12];
		// Inside glint band: y >= glintTopY(x) and y <= glintBotY(x).
		const float k = 32.0f / 340.0f;
		const int n1 = clipPolyHalfPlane(src, 3, buf1, k, 1.0f, 0.0f);
		const int n2 = clipPolyHalfPlane(buf1, n1, buf2, -k, -1.0f, 1.2f);
		for (int i = 1; i + 1 < n2; i++) {
			sTris.push_back({{buf2[0], buf2[i], buf2[i + 1]},
			                 {yFracAt(buf2[0].y), yFracAt(buf2[i].y), yFracAt(buf2[i + 1].y)},
			                 layer, letterIdx});
		}
		return;
	}
	const float kt = 32.0f / 340.0f, kb = 32.5f / 340.0f;
	Vec2 top[12], bot[12];
	// Above slash: y <= -1.0 - kt*x  ->  -kt*x - y - 1.0 >= 0.
	const int nt = clipPolyHalfPlane(src, 3, top, -kt, -1.0f, -1.0f);
	for (int i = 1; i + 1 < nt; i++) {
		sTris.push_back({{top[0], top[i], top[i + 1]},
		                 {yFracAt(top[0].y), yFracAt(top[i].y), yFracAt(top[i + 1].y)},
		                 layer, letterIdx});
	}
	// Below slash: y >= 2.75 - kb*x  ->  kb*x + y - 2.75 >= 0.
	const int nb = clipPolyHalfPlane(src, 3, bot, kb, 1.0f, -2.75f);
	for (int i = 1; i + 1 < nb; i++) {
		sTris.push_back({{bot[0], bot[i], bot[i + 1]},
		                 {yFracAt(bot[0].y), yFracAt(bot[i].y), yFracAt(bot[i + 1].y)},
		                 layer, letterIdx});
	}
}

static void emitDisc(Vec2 center, float radius, u8 layer, s8 letterIdx) {
	static constexpr int kSegs = 8;
	for (int i = 0; i < kSegs; i++) {
		const float a0 = i * (6.2831853f / kSegs);
		const float a1 = (i + 1) * (6.2831853f / kSegs);
		const Vec2 p1 = {center.x + cosf(a0) * radius, center.y + sinf(a0) * radius};
		const Vec2 p2 = {center.x + cosf(a1) * radius, center.y + sinf(a1) * radius};
		emitClippedTri(center, p1, p2, layer, letterIdx);
	}
}

static void emitThickStroke(const Vec2* pts, int n, float hw, u8 layer, s8 letterIdx) {
	for (int i = 0; i < n; i++) {
		emitDisc(pts[i], hw, layer, letterIdx);
		if (i + 1 < n) {
			const float dx = pts[i + 1].x - pts[i].x;
			const float dy = pts[i + 1].y - pts[i].y;
			const float len = sqrtf(dx * dx + dy * dy);
			if (len > 1e-4f) {
				const float nx = -dy / len * hw, ny = dx / len * hw;
				const Vec2 a = {pts[i].x + nx, pts[i].y + ny};
				const Vec2 b = {pts[i].x - nx, pts[i].y - ny};
				const Vec2 c = {pts[i + 1].x - nx, pts[i + 1].y - ny};
				const Vec2 d = {pts[i + 1].x + nx, pts[i + 1].y + ny};
				emitClippedTri(a, b, c, layer, letterIdx);
				emitClippedTri(a, c, d, layer, letterIdx);
			}
		}
	}
}

static void buildLine(const char* word, float cy, int letterBase, float hw, u8 layer) {
	constexpr float kSize = 42.0f, kUnit = kSize / 6.0f, kGap = kUnit * 1.5f;
	float total = -kGap;
	for (const char* s = word; *s; s++) {
		const GlyphDef* g = findGlyph(*s);
		float lo = 0, hi = 0;
		if (g) glyphExtent(g, &lo, &hi);
		total += (hi - lo) * kUnit + kGap;
	}
	float pen = -total * 0.5f;
	int idx = letterBase;
	for (const char* s = word; *s; s++, idx++) {
		const GlyphDef* g = findGlyph(*s);
		if (!g) continue;
		float lo = 0, hi = 0;
		glyphExtent(g, &lo, &hi);
		for (int p = 0; p < g->polyCount; p++) {
			Vec2 pts[16];
			const int n = g->polys[p].n;
			for (int k = 0; k < n; k++) {
				pts[k].x = pen + (g->polys[p].pts[k].x - lo) * kUnit;
				pts[k].y = cy - kSize * 0.5f + g->polys[p].pts[k].y * kUnit;
			}
			emitThickStroke(pts, n, hw, layer, (s8)idx);
		}
		pen += (hi - lo) * kUnit + kGap;
	}
}

static void buildLogoGeometry() {
	if (sBuilt) return;
	sBuilt = true;
	sTris.reserve(1600);
	buildLine("FRUIT", -26.0f, 0, 9.95f, 0);
	buildLine("SLICER", 27.0f, 5, 9.95f, 0);
	buildLine("FRUIT", -26.0f, 0, 7.75f, 1);
	buildLine("SLICER", 27.0f, 5, 7.75f, 1);
	buildLine("FRUIT", -26.0f, 0, 4.75f, 2);
	buildLine("SLICER", 27.0f, 5, 4.75f, 2);
	buildLine("FRUIT", -26.0f, 0, 7.75f, 3);
	buildLine("SLICER", 27.0f, 5, 7.75f, 3);
}

static u32 hsvRgb(float h, float s, float v) {
	h = h - floorf(h);
	const float hh = h * 6.0f;
	const int sector = (int)hh;
	const float f = hh - sector;
	const float p = v * (1.0f - s);
	const float q = v * (1.0f - s * f);
	const float t = v * (1.0f - s * (1.0f - f));
	float r = v, g = t, b = p;
	switch (sector % 6) {
		case 0: r = v; g = t; b = p; break;
		case 1: r = q; g = v; b = p; break;
		case 2: r = p; g = v; b = t; break;
		case 3: r = p; g = q; b = v; break;
		case 4: r = t; g = p; b = v; break;
		case 5: r = v; g = p; b = q; break;
	}
	return rgba((u8)(r * 255.0f + 0.5f), (u8)(g * 255.0f + 0.5f), (u8)(b * 255.0f + 0.5f));
}

}  // namespace

u32 logoLetterColor(int letterIdx, float yFrac, bool rainbow, float timeSec) {
	yFrac = clampf(yFrac, 0.0f, 1.0f);
	if (!rainbow) {
		const u8 r = (u8)(255.0f + (240.0f - 255.0f) * yFrac + 0.5f);
		const u8 g = (u8)(208.0f + (66.0f - 208.0f) * yFrac + 0.5f);
		const u8 b = (u8)(66.0f + (112.0f - 66.0f) * yFrac + 0.5f);
		return rgba(r, g, b);
	}
	const float hue = timeSec * 0.55f + letterIdx * 0.095f + yFrac * 0.08f;
	const float sat = lerpf(0.84f, 0.92f, yFrac);
	const float val = lerpf(1.0f, 0.86f, yFrac);
	return hsvRgb(hue, sat, val);
}

void drawVectorLogo(float cx, float cy, float scale, bool rainbow, float timeSec) {
	buildLogoGeometry();
	const u32 cream = rgba(255, 246, 226);
	const u32 ink = rgba(38, 28, 54);
	const u32 glint = rgba(255, 252, 245);
	for (const LogoTri& t : sTris) {
		u32 c0 = cream, c1 = cream, c2 = cream;
		if (t.layer == 1) {
			c0 = c1 = c2 = ink;
		} else if (t.layer == 2) {
			c0 = logoLetterColor(t.letterIdx, t.yFrac[0], rainbow, timeSec);
			c1 = logoLetterColor(t.letterIdx, t.yFrac[1], rainbow, timeSec);
			c2 = logoLetterColor(t.letterIdx, t.yFrac[2], rainbow, timeSec);
		} else if (t.layer == 3) {
			c0 = c1 = c2 = glint;
		}
		gfx::tri(cx + t.p[0].x * scale, cy + t.p[0].y * scale, c0,
		         cx + t.p[1].x * scale, cy + t.p[1].y * scale, c1,
		         cx + t.p[2].x * scale, cy + t.p[2].y * scale, c2);
	}
}

bool logoHitTest(float lx, float ly, float cx, float cy, float scale) {
	return fabsf(lx - cx) <= 116.0f * scale && fabsf(ly - cy) <= 58.0f * scale;
}

int vectorLogoTriCount() {
	buildLogoGeometry();
	return (int)sTris.size();
}

}  // namespace gfx
