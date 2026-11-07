#pragma once
#include "common.h"

#define WHITE 0xFFFFFFFFu
inline u32 rgba(u8 r, u8 g, u8 b, u8 a = 255) { return C2D_Color32(r, g, b, a); }
inline u32 withAlpha(u32 c, float a) {
	u8 v = (u8)(clampf(a, 0, 1) * ((c >> 24) & 0xFF));
	return (c & 0x00FFFFFFu) | ((u32)v << 24);
}

enum Align { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };
enum SheetId { SHEET_SPRITES, SHEET_FONT, SHEET_BGTOP, SHEET_BGBOT, SHEET_LOGO, SHEET_COUNT };

namespace gfx {
bool init();
void shutdown();
// Swaps the top-screen scenery. The old texture is freed a few frames later, once the GPU is done.
void setScene(int scene);
void beginFrame();
void endFrame();
// eye: 0 = left, 1 = right. Returns false if that eye does not need drawing.
bool beginTop(int eye);
void beginBottom();
// Stereo plane for following top-screen draws: >0 pops out, <0 sinks behind the screen.
void setDepth(float depth);
void setShake(float dx, float dy);

void sprite(SheetId sheet, int idx, float x, float y, float sx = 1, float sy = 1, float rot = 0, u32 color = WHITE);
void image(SheetId sheet, int idx, float x, float y, float w, float h, u32 color = WHITE);
void rect(float x, float y, float w, float h, u32 color);
void tri(float x0, float y0, u32 c0, float x1, float y1, u32 c1, float x2, float y2, u32 c2);
float textWidth(const char* s, float scale);
void text(const char* s, float x, float y, float scale, u32 color = WHITE, Align align = ALIGN_LEFT);

extern u32 drawCalls;      // draws issued last frame
extern u32 drawCallsLive;  // draws issued so far this frame
}  // namespace gfx
