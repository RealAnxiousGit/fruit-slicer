#include "render.h"

#include <stdio.h>
#include <string.h>

#include "font_metrics.h"

namespace gfx {
u32 drawCalls = 0;
u32 drawCallsLive = 0;

static C3D_RenderTarget* sTopL;
static C3D_RenderTarget* sTopR;
static C3D_RenderTarget* sBottom;
static C2D_SpriteSheet sSheets[SHEET_COUNT];
static bool sOnTop;
static int sEye;
static float sIod;
static float sOffX, sOffY;
static float sShakeX, sShakeY;

// Pixels of total disparity at full slider per unit of depth.
static constexpr float kMaxDisparity = 5.0f;
static constexpr float kZ = 0.5f;
static constexpr u32 kMaxObjects = 3072;

static C2D_SpriteSheet load(const char* p) {
	C2D_SpriteSheet s = C2D_SpriteSheetLoad(p);
	if (!s) {
		fprintf(stderr, "[gfx] failed to load %s\n", p);
		return nullptr;
	}
	C2D_Image first = C2D_SpriteSheetGetImage(s, 0);
	C3D_TexSetFilter(first.tex, GPU_LINEAR, GPU_LINEAR);
	C3D_TexSetWrap(first.tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
	return s;
}

bool init() {
	gfxInitDefault();
	gfxSet3D(true);
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(kMaxObjects);
	C2D_Prepare();
	sTopL = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	sTopR = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
	sBottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	static const char* const paths[SHEET_COUNT] = {
		"romfs:/gfx/sprites.t3x", "romfs:/gfx/font.t3x", "romfs:/gfx/bgtop.t3x",
		"romfs:/gfx/bgbot.t3x",   "romfs:/gfx/logo.t3x",
	};
	bool ok = true;
	for (int i = 0; i < SHEET_COUNT; i++) {
		sSheets[i] = load(paths[i]);
		ok = ok && sSheets[i];
	}
	return ok;
}

void shutdown() {
	for (auto& s : sSheets)
		if (s) C2D_SpriteSheetFree(s);
	C2D_Fini();
	C3D_Fini();
	gfxExit();
}

void beginFrame() {
	sIod = osGet3DSliderState();
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_SetTintMode(C2D_TintMult);
	drawCallsLive = 0;
}

void endFrame() {
	C3D_FrameEnd(0);
	drawCalls = drawCallsLive;
}

bool beginTop(int eye) {
	if (eye == 1 && sIod <= 0.0f) return false;
	C3D_RenderTarget* t = eye == 0 ? sTopL : sTopR;
	C2D_TargetClear(t, C2D_Color32(0, 0, 0, 255));
	C2D_SceneBegin(t);
	sOnTop = true;
	sEye = eye;
	sOffX = sOffY = 0;
	return true;
}

void beginBottom() {
	C2D_TargetClear(sBottom, C2D_Color32(0, 0, 0, 255));
	C2D_SceneBegin(sBottom);
	sOnTop = false;
	sOffX = sShakeX;
	sOffY = sShakeY;
}

void setDepth(float depth) {
	if (!sOnTop) return;
	// A plane in front of the screen is shifted right for the left eye, left for the right eye.
	float half = sIod * kMaxDisparity * depth * 0.5f;
	sOffX = (sEye == 0 ? half : -half);
}

void setShake(float dx, float dy) {
	sShakeX = dx;
	sShakeY = dy;
}

void sprite(SheetId sheet, int idx, float x, float y, float sx, float sy, float rot, u32 color) {
	C2D_Image img = C2D_SpriteSheetGetImage(sSheets[sheet], idx);
	float w = img.subtex->width * sx;
	float h = img.subtex->height * sy;
	C2D_DrawParams p;
	p.pos = {x + sOffX, y + sOffY, w, h};
	p.center = {w * 0.5f, h * 0.5f};
	p.depth = kZ;
	p.angle = rot;
	C2D_ImageTint tint;
	C2D_PlainImageTint(&tint, color, 1.0f);
	C2D_DrawImage(img, &p, &tint);
	drawCallsLive++;
}

void image(SheetId sheet, int idx, float x, float y, float w, float h, u32 color) {
	C2D_Image img = C2D_SpriteSheetGetImage(sSheets[sheet], idx);
	C2D_DrawParams p;
	p.pos = {x + sOffX, y + sOffY, w, h};
	p.center = {0, 0};
	p.depth = kZ;
	p.angle = 0;
	C2D_ImageTint tint;
	C2D_PlainImageTint(&tint, color, 1.0f);
	C2D_DrawImage(img, &p, &tint);
	drawCallsLive++;
}

void rect(float x, float y, float w, float h, u32 color) {
	C2D_DrawRectSolid(x + sOffX, y + sOffY, kZ, w, h, color);
	drawCallsLive++;
}

void tri(float x0, float y0, u32 c0, float x1, float y1, u32 c1, float x2, float y2, u32 c2) {
	C2D_DrawTriangle(x0 + sOffX, y0 + sOffY, c0, x1 + sOffX, y1 + sOffY, c1, x2 + sOffX, y2 + sOffY, c2, kZ);
	drawCallsLive++;
}

static inline int glyphSlot(char c) {
	if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
	if (c < 32 || c > 127) return 0;
	return c - 32;
}

float textWidth(const char* s, float scale) {
	float w = 0;
	for (; *s; s++) w += kFontAdvance[glyphSlot(*s)];
	return w * scale;
}

void text(const char* s, float x, float y, float scale, u32 color, Align align) {
	if (align != ALIGN_LEFT) {
		float w = textWidth(s, scale);
		x -= align == ALIGN_CENTER ? w * 0.5f : w;
	}
	const float cell = FONT_CELL * scale;
	for (; *s; s++) {
		int slot = glyphSlot(*s);
		float adv = kFontAdvance[slot] * scale;
		int g = kFontIndex[slot];
		if (g >= 0) image(SHEET_FONT, g, x + (adv - cell) * 0.5f, y, cell, cell, color);
		x += adv;
	}
}
}  // namespace gfx
