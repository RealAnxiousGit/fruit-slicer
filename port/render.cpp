// 2D drawing for every SDL renderer: all of it becomes batched textured triangles.
#include "render.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "gfx_sdl.h"
#include "sheet_rects.h"

#if defined(_WIN32)
const char* const kRendererNames[] = {"DirectX", "OpenGL", "Vulkan", "Software"};
static const char* const kDrivers[] = {"direct3d11", "opengl", "vulkan", "software"};
#elif PORT_ANDROID
const char* const kRendererNames[] = {"OpenGL ES", "Vulkan", "Software"};
static const char* const kDrivers[] = {"opengles2", "vulkan", "software"};
#else
const char* const kRendererNames[] = {"OpenGL", "Vulkan", "Software"};
static const char* const kDrivers[] = {"opengl", "vulkan", "software"};
#endif
const int kRendererCount = sizeof kDrivers / sizeof kDrivers[0];

namespace gfx {
u32 drawCalls = 0;
u32 drawCallsLive = 0;

static SDL_Window* sWindow;
static SDL_Renderer* sRenderer;
static bool sSoftware;
static float sScale = 1, sOx, sOy;
static float sWinW = 1280, sWinH = 720;
static float sClipX0, sClipY0, sClipX1 = 1280, sClipY1 = 720;  // where drawing lands right now
static float sBaseX, sBaseY, sShakeX, sShakeY;
static bool sShaking;
static char sShotPath[300];

struct Sheet {
	SDL_Texture* tex;
	const SheetRect* rects;
	float w, h;
};
static Sheet sSheets[SHEET_COUNT];
static int sLoadedBoard = -1;

static constexpr int kBatchMax = 60000;
static SDL_Vertex* sBatch;
static int sBatchCount;
static SDL_Texture* sBatchTex;

SDL_Renderer* renderer() { return sRenderer; }
SDL_Window* window() { return sWindow; }
bool softwareRenderer() { return sSoftware; }
void requestShot(const char* path) { snprintf(sShotPath, sizeof sShotPath, "%s", path); }

static float sPresentMs;
static unsigned sTrisLive, sTris;
float presentMs() { return sPresentMs; }
unsigned trisDrawn() { return sTris; }

static void flush() {
	if (sBatchCount == 0) return;
	sTrisLive += (unsigned)sBatchCount / 3;
	SDL_RenderGeometry(sRenderer, sBatchTex, sBatch, sBatchCount, nullptr, 0);
	sBatchCount = 0;
	drawCallsLive++;
}

SDL_Vertex* reserveTris(SDL_Texture* tex, int count) {
	if (count > kBatchMax) return nullptr;
	if (tex != sBatchTex || sBatchCount + count > kBatchMax) {
		flush();
		sBatchTex = tex;
	}
	SDL_Vertex* at = sBatch + sBatchCount;
	sBatchCount += count;
	return at;
}

void pushTris(SDL_Texture* tex, const SDL_Vertex* v, int count) {
	if (SDL_Vertex* at = reserveTris(tex, count)) SDL_memcpy(at, v, (size_t)count * sizeof(SDL_Vertex));
}

SDL_Surface* loadPng(const char* path) {
	char full[512];
#if PORT_ANDROID
	snprintf(full, sizeof full, "romfs/%s", path);  // assets inside the APK
#else
	snprintf(full, sizeof full, "%sromfs/%s", SDL_GetBasePath() ? SDL_GetBasePath() : "", path);
#endif
	SDL_Surface* s = SDL_LoadPNG(full);
	if (!s) fprintf(stderr, "[gfx] failed to load %s: %s\n", full, SDL_GetError());
	return s;
}

bool lowMemory() {
#if PORT_ANDROID
	static const bool low = SDL_GetSystemRAM() < 3000;
	return low;
#else
	return false;
#endif
}

// Texture Quality: Low halves the 2D art and the 3D skins, Medium only the 3D skins. A phone short on
// memory always gets Low.
SDL_Surface* halveForQuality(SDL_Surface* s, int keepFrom) {
	int quality = lowMemory() ? 0 : gSave.texQuality;
	if (!s || quality >= keepFrom) return s;
	SDL_Surface* half = SDL_ScaleSurface(s, s->w / 2, s->h / 2, SDL_SCALEMODE_LINEAR);
	if (!half) return s;
	SDL_DestroySurface(s);
	return half;
}

static bool loadSheet(SheetId id, const char* path, const SheetRect* rects) {
	SDL_Surface* s = loadPng(path);
	if (!s) return false;
	// Coordinates stay in full-size pixels, since a half-size sheet covers the same 0 to 1. Text stays sharp.
	const float fullW = (float)s->w, fullH = (float)s->h;
	if (id != SHEET_FONT && id != SHEET_LOGO) s = halveForQuality(s, 1);
	SDL_Texture* t = SDL_CreateTextureFromSurface(sRenderer, s);
	Sheet& sh = sSheets[id];
	if (t) {
		flush();
		if (sBatchTex == sh.tex) sBatchTex = nullptr;
		if (sh.tex) SDL_DestroyTexture(sh.tex);
		sh.tex = t;
		sh.rects = rects;
		sh.w = fullW;
		sh.h = fullH;
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
	}
	SDL_DestroySurface(s);
	return t != nullptr;
}

bool init() {
	int choice = gSave.renderer < kRendererCount ? gSave.renderer : 0;
	const char* forced = SDL_getenv("FRUIT_SLICER_RENDERER");
	const char* driver = forced ? forced : kDrivers[choice];
	// Software draws every pixel on the CPU, so on a scaled screen it gets a window of plain pixels instead.
	Uint32 flags = SDL_WINDOW_RESIZABLE;
	if (strcmp(driver, "software") != 0) flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#if PORT_ANDROID
	flags |= SDL_WINDOW_FULLSCREEN;
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
	sWindow = SDL_CreateWindow("Fruit Slicer", 1280, 720, flags);
	if (!sWindow) {
		fprintf(stderr, "[gfx] no window: %s\n", SDL_GetError());
		return false;
	}
	SDL_SetWindowMinimumSize(sWindow, 640, 360);
	sRenderer = SDL_CreateRenderer(sWindow, driver);
	if (!sRenderer) {
		// Not every machine has every API, so a bad choice must never stop the game from starting.
		// The others are tried in the menu's order, and the one that works becomes the setting.
		fprintf(stderr, "[gfx] %s unavailable: %s\n", driver, SDL_GetError());
		for (int i = 0; i < kRendererCount && !sRenderer; i++) {
			if (!forced && i == choice) continue;
			sRenderer = SDL_CreateRenderer(sWindow, kDrivers[i]);
			if (sRenderer && !forced) {
				char note[96];
				snprintf(note, sizeof note, "%s would not start, so the game uses %s.", kRendererNames[choice], kRendererNames[i]);
				port::setNotice(note);
				gSave.renderer = (u8)i;
				saveWrite();
			}
		}
		if (!sRenderer) sRenderer = SDL_CreateRenderer(sWindow, nullptr);
	}
	if (!sRenderer) {
		fprintf(stderr, "[gfx] no renderer: %s\n", SDL_GetError());
		return false;
	}
	const char* name = SDL_GetRendererName(sRenderer);
	sSoftware = name && strcmp(name, "software") == 0;
	fprintf(stderr, "[gfx] renderer %s\n", name ? name : "?");
#if defined(AUTOTEST) || defined(TEST_SHOTS)
	// Windows programs have nowhere to print, so test runs leave the name in a file too.
	if (const char* dir = SDL_getenv("FRUIT_SLICER_SHOTS")) {
		char namePath[512];
		snprintf(namePath, sizeof namePath, "%s/renderer.txt", dir);
		if (FILE* f = fopen(namePath, "w")) {
			fputs(name ? name : "?", f);
			fclose(f);
		}
	}
#endif
	SDL_SetRenderDrawBlendMode(sRenderer, SDL_BLENDMODE_BLEND);
	sBatch = (SDL_Vertex*)SDL_malloc(kBatchMax * sizeof(SDL_Vertex));
	if (!sBatch) return false;
	bool ok = loadSheet(SHEET_FONT, "gfx/font.png", kRectsFont) && loadSheet(SHEET_SPRITES, "gfx/sprites.png", kRectsSprites) &&
	          loadSheet(SHEET_LOGO, "gfx/logo.png", kRectsLogo) && loadSheet(SHEET_FRUIT, "gfx/fruit.png", kRectsFruit);
	sync();
	return ok && sSheets[SHEET_BOARD].tex;
}

int loadStepCount() { return 1; }
void loadStep(int) { sync(); }

void reloadTextures() {
	loadSheet(SHEET_SPRITES, "gfx/sprites.png", kRectsSprites);
	loadSheet(SHEET_FRUIT, "gfx/fruit.png", kRectsFruit);
	sLoadedBoard = -1;
	sync();
	m3dport::reloadTextures();
}

void sync() {
	if (sLoadedBoard == gSave.board) return;
	char path[40];
	snprintf(path, sizeof path, "gfx/board%d.png", gSave.board);
	loadSheet(SHEET_BOARD, path, kRectsBoard);
	sLoadedBoard = gSave.board;
}

void shutdown() {
	for (auto& s : sSheets)
		if (s.tex) SDL_DestroyTexture(s.tex);
	SDL_free(sBatch);
	sBatch = nullptr;
	if (sRenderer) SDL_DestroyRenderer(sRenderer);
	if (sWindow) SDL_DestroyWindow(sWindow);
}

void forceStereo(bool) {}
void setDepth(float) {}
float stereoPerZ() { return 0; }
bool onTop() { return false; }
void setShake(float dx, float dy) {
	sShakeX = dx;
	sShakeY = dy;
}
void setBase(float dx, float dy) {
	sBaseX = dx;
	sBaseY = dy;
}
void getOffset(float* x, float* y) {
	*x = sBaseX + (sShaking ? sShakeX : 0);
	*y = sBaseY + (sShaking ? sShakeY : 0);
}

// Render Scale: anything but 100% draws into a texture of that size, which is then stretched to the window.
static SDL_Texture* sTarget;
static int sTargetW, sTargetH;
static float sRenderScale = 1;
float renderScaleNow() { return sRenderScale; }

void beginFrame() {
	int w = 1280, h = 720;
	SDL_GetCurrentRenderOutputSize(sRenderer, &w, &h);
	static const float kScales[] = {0.75f, 1.0f, 1.25f};
	float want = kScales[gSave.renderScale < 3 ? gSave.renderScale : 1];
	sRenderScale = 1;
	if (want != 1) {
		int tw = (int)(w * want), th = (int)(h * want);
		if (!sTarget || tw != sTargetW || th != sTargetH) {
			if (sTarget) SDL_DestroyTexture(sTarget);
			sTarget = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, tw, th);
			if (sTarget) SDL_SetTextureScaleMode(sTarget, SDL_SCALEMODE_LINEAR);
			sTargetW = tw;
			sTargetH = th;
		}
		if (sTarget && SDL_SetRenderTarget(sRenderer, sTarget)) {
			sRenderScale = want;
			w = tw;
			h = th;
		}
	} else if (sTarget) {
		SDL_DestroyTexture(sTarget);
		sTarget = nullptr;
	}
	sWinW = (float)w;
	sWinH = (float)h;
	sClipX0 = sClipY0 = 0;
	sClipX1 = sWinW;
	sClipY1 = sWinH;
	SDL_SetRenderClipRect(sRenderer, nullptr);
	SDL_SetRenderDrawColor(sRenderer, 0, 0, 0, 255);
	SDL_RenderClear(sRenderer);
	drawCallsLive = 0;
}

void endFrame() {
	flush();
	if (sRenderScale != 1) {
		SDL_SetRenderTarget(sRenderer, nullptr);
		SDL_SetRenderClipRect(sRenderer, nullptr);
		SDL_RenderTexture(sRenderer, sTarget, nullptr, nullptr);
	}
	port::drawOverlay();
	if (sShotPath[0]) {
		SDL_Surface* s = SDL_RenderReadPixels(sRenderer, nullptr);
		if (s) {
			SDL_SavePNG(s, sShotPath);
			SDL_DestroySurface(s);
		}
		sShotPath[0] = 0;
	}
	Uint64 from = SDL_GetTicksNS();
	SDL_RenderPresent(sRenderer);
	sPresentMs = (SDL_GetTicksNS() - from) / 1e6f;
	drawCalls = drawCallsLive;
	sTris = sTrisLive;
	sTrisLive = 0;
}

// The menus have a 400x240 picture side (the logo, and what's selected) and a 320x240 sign side. A wide
// window sets them side by side so the logo stays large beside the signs; a squarer one stacks them.
struct MenuLayout {
	float picScale, picX, picY;
	float signScale, signX, signY;
};

static MenuLayout menuLayout() {
	MenuLayout m;
	if (sWinW / sWinH >= 1.4f) {
		constexpr float kPic = 0.9f, kW = 400 * kPic + 320, kH = 240;
		float a = sWinW / kW, b = sWinH / kH, u = a < b ? a : b;
		float x = (sWinW - kW * u) * 0.5f, y = (sWinH - kH * u) * 0.5f;
		m = {u * kPic, x, y + (240 - 240 * kPic) * 0.5f * u, u, x + 400 * kPic * u, y};
	} else {
		constexpr float kPic = 0.6f, kMargin = 8, kW = 320, kH = 240 * kPic + 240 + kMargin * 2;
		float a = sWinW / kW, b = sWinH / kH, u = a < b ? a : b;
		float y = (sWinH - kH * u) * 0.5f + kMargin * u;
		m = {u * kPic, sWinW * 0.5f - 200 * u * kPic, y, u, sWinW * 0.5f - 160 * u, y + 240 * kPic * u};
	}
	return m;
}

// A side's slide-in and its long lists stay within that side.
static void clipTo(float x, float y, float w, float h) {
	flush();
	SDL_Rect r = {(int)x, (int)y, (int)w, (int)h};
	SDL_SetRenderClipRect(sRenderer, &r);
	sClipX0 = SDL_max((float)r.x, 0.0f);
	sClipY0 = SDL_max((float)r.y, 0.0f);
	sClipX1 = SDL_min((float)(r.x + r.w), sWinW);
	sClipY1 = SDL_min((float)(r.y + r.h), sWinH);
}

static void clipToWindow() {
	SDL_SetRenderClipRect(sRenderer, nullptr);
	sClipX0 = sClipY0 = 0;
	sClipX1 = sWinW;
	sClipY1 = sWinH;
}

bool beginTop(int eye) {
	if (eye != 0) return false;
	MenuLayout m = menuLayout();
	sScale = m.picScale;
	sOx = m.picX;
	sOy = m.picY;
	sShaking = false;
	clipTo(m.picX, m.picY, 400 * m.picScale, 240 * m.picScale);
	return true;
}

void beginBottom() {
	MenuLayout m = menuLayout();
	sScale = m.signScale;
	sOx = m.signX;
	sOy = m.signY;
	sShaking = false;
	clipTo(m.signX, m.signY, 320 * m.signScale, 240 * m.signScale);
}

static inline float fieldUnit() {
	float a = sWinW / kFieldW, b = sWinH / kFieldH;
	return a < b ? a : b;
}

void beginField() {
	flush();
	clipToWindow();
	sScale = fieldUnit();
	sOx = (sWinW - kFieldW * sScale) * 0.5f;
	sOy = (sWinH - kFieldH * sScale) * 0.5f;
	sShaking = true;
}

void windowToGame(float wx, float wy, bool field, int* tx, int* ty) {
	float s, ox, oy;
	if (field) {
		s = fieldUnit();
		ox = (sWinW - kFieldW * s) * 0.5f;
		oy = (sWinH - kFieldH * s) * 0.5f;
	} else {
		MenuLayout m = menuLayout();
		s = m.signScale;
		ox = m.signX;
		oy = m.signY;
	}
	*tx = (int)((wx - ox) / s);
	*ty = (int)((wy - oy) / s);
}

static inline SDL_FColor fcolor(u32 c) {
	return {(c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, ((c >> 24) & 255) / 255.0f};
}

void toScreen(float x, float y, float* sx, float* sy) {
	float ox, oy;
	getOffset(&ox, &oy);
	*sx = (x + ox) * sScale + sOx;
	*sy = (y + oy) * sScale + sOy;
}

float viewScale() { return sScale; }

static void quad(SDL_Texture* tex, const SheetRect& r, float tw, float th, const float xs[4], const float ys[4], u32 color) {
	SDL_FColor c = fcolor(color);
	float u0 = r.x / tw, v0 = r.y / th, u1 = (r.x + r.w) / tw, v1 = (r.y + r.h) / th;
	// SDL's Software renderer turns an upright quad into a scaled copy, and a scaled copy that crosses
	// the edge of the drawing area loses its transparency: a glow becomes a solid box. So trim it first.
	if (sSoftware && xs[0] == xs[3] && xs[1] == xs[2] && ys[0] == ys[1] && ys[2] == ys[3] && xs[0] < xs[1] && ys[0] < ys[2]) {
		float x0 = xs[0], x1 = xs[1], y0 = ys[0], y1 = ys[2];
		float cx0 = SDL_max(x0, sClipX0), cx1 = SDL_min(x1, sClipX1), cy0 = SDL_max(y0, sClipY0), cy1 = SDL_min(y1, sClipY1);
		if (cx0 >= cx1 || cy0 >= cy1) return;
		const float du = (u1 - u0) / (x1 - x0), dv = (v1 - v0) / (y1 - y0);
		const float nu0 = u0 + (cx0 - x0) * du, nu1 = u0 + (cx1 - x0) * du, nv0 = v0 + (cy0 - y0) * dv, nv1 = v0 + (cy1 - y0) * dv;
		SDL_Vertex a = {{cx0, cy0}, c, {nu0, nv0}}, b = {{cx1, cy0}, c, {nu1, nv0}};
		SDL_Vertex d = {{cx1, cy1}, c, {nu1, nv1}}, e = {{cx0, cy1}, c, {nu0, nv1}};
		SDL_Vertex v[6] = {a, b, d, a, d, e};
		pushTris(tex, v, 6);
		return;
	}
	SDL_Vertex a = {{xs[0], ys[0]}, c, {u0, v0}}, b = {{xs[1], ys[1]}, c, {u1, v0}};
	SDL_Vertex d = {{xs[2], ys[2]}, c, {u1, v1}}, e = {{xs[3], ys[3]}, c, {u0, v1}};
	SDL_Vertex v[6] = {a, b, d, a, d, e};
	pushTris(tex, v, 6);
}

void sprite(SheetId sheet, int idx, float x, float y, float sx, float sy, float rot, u32 color) {
	const Sheet& s = sSheets[sheet];
	if (!s.tex) return;
	const SheetRect& r = s.rects[idx];
	float hw = r.lw * sx * 0.5f, hh = r.lh * sy * 0.5f;
	float c = 1, n = 0;
	if (rot != 0) {
		c = SDL_cosf(rot);
		n = SDL_sinf(rot);
	}
	float px, py;
	toScreen(x, y, &px, &py);
	float xs[4], ys[4];
	const float lx[4] = {-hw, hw, hw, -hw}, ly[4] = {-hh, -hh, hh, hh};
	for (int i = 0; i < 4; i++) {
		xs[i] = px + (lx[i] * c - ly[i] * n) * sScale;
		ys[i] = py + (lx[i] * n + ly[i] * c) * sScale;
	}
	quad(s.tex, r, s.w, s.h, xs, ys, color);
}

void image(SheetId sheet, int idx, float x, float y, float w, float h, u32 color) {
	const Sheet& s = sSheets[sheet];
	if (!s.tex) return;
	float x0, y0;
	toScreen(x, y, &x0, &y0);
	float x1 = x0 + w * sScale, y1 = y0 + h * sScale;
	const float xs[4] = {x0, x1, x1, x0}, ys[4] = {y0, y0, y1, y1};
	quad(s.tex, s.rects[idx], s.w, s.h, xs, ys, color);
}

void imagePart(SheetId sheet, int idx, float x, float y, float w, float h, float u0, float v0, float u1, float v1, u32 color) {
	const Sheet& s = sSheets[sheet];
	if (!s.tex) return;
	const SheetRect& r = s.rects[idx];
	const SheetRect part = {r.x + r.w * u0, r.y + r.h * v0, r.w * (u1 - u0), r.h * (v1 - v0), r.lw, r.lh};
	float x0, y0;
	toScreen(x, y, &x0, &y0);
	float x1 = x0 + w * sScale, y1 = y0 + h * sScale;
	const float xs[4] = {x0, x1, x1, x0}, ys[4] = {y0, y0, y1, y1};
	quad(s.tex, part, s.w, s.h, xs, ys, color);
}

void rect(float x, float y, float w, float h, u32 color) {
	float x0, y0;
	toScreen(x, y, &x0, &y0);
	float x1 = x0 + w * sScale, y1 = y0 + h * sScale;
	SDL_FColor c = fcolor(color);
	SDL_Vertex a = {{x0, y0}, c, {0, 0}}, b = {{x1, y0}, c, {0, 0}}, d = {{x1, y1}, c, {0, 0}}, e = {{x0, y1}, c, {0, 0}};
	SDL_Vertex v[6] = {a, b, d, a, d, e};
	pushTris(nullptr, v, 6);
}

void tri(float x0, float y0, u32 c0, float x1, float y1, u32 c1, float x2, float y2, u32 c2) {
	SDL_Vertex v[3];
	toScreen(x0, y0, &v[0].position.x, &v[0].position.y);
	toScreen(x1, y1, &v[1].position.x, &v[1].position.y);
	toScreen(x2, y2, &v[2].position.x, &v[2].position.y);
	v[0].color = fcolor(c0);
	v[1].color = fcolor(c1);
	v[2].color = fcolor(c2);
	v[0].tex_coord = v[1].tex_coord = v[2].tex_coord = {0, 0};
	pushTris(nullptr, v, 3);
}

// One board fills the whole window, whatever its shape, behind everything else.
void drawBackdrop(u32 tint) {
	const Sheet& s = sSheets[SHEET_BOARD];
	if (!s.tex) return;
	float a = sWinW / s.w, b = sWinH / s.h, k = a > b ? a : b;
	float w = s.w * k, h = s.h * k;
	flush();
	// A plain opaque copy: the biggest draw of the frame, and much the cheapest this way on the Software renderer.
	SDL_FRect to = {(sWinW - w) * 0.5f, (sWinH - h) * 0.5f, w, h};
	SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_NONE);
	SDL_SetTextureColorMod(s.tex, tint & 255, (tint >> 8) & 255, (tint >> 16) & 255);
	SDL_RenderTexture(sRenderer, s.tex, nullptr, &to);
	SDL_SetTextureColorMod(s.tex, 255, 255, 255);
	SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_BLEND);
	drawCallsLive++;
}

void fadeAll(float alpha) {
	flush();
	clipToWindow();
	SDL_FColor c = {0, 0, 0, alpha};
	SDL_Vertex a = {{0, 0}, c, {0, 0}}, b = {{sWinW, 0}, c, {0, 0}}, d = {{sWinW, sWinH}, c, {0, 0}}, e = {{0, sWinH}, c, {0, 0}};
	SDL_Vertex v[6] = {a, b, d, a, d, e};
	pushTris(nullptr, v, 6);
}
}  // namespace gfx

