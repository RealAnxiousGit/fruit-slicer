#pragma once
#include "common.h"

#define WHITE 0xFFFFFFFFu
inline u32 rgba(u8 r, u8 g, u8 b, u8 a = 255) { return C2D_Color32(r, g, b, a); }
inline u32 withAlpha(u32 c, float a) {
	u8 v = (u8)(clampf(a, 0, 1) * ((c >> 24) & 0xFF));
	return (c & 0x00FFFFFFu) | ((u32)v << 24);
}

// Text stays cream everywhere. Sign colour and the one accent follow the board chosen in Visuals.
#define COL_CREAM 0xFFD2F0FFu   // 255, 240, 210
#define COL_DIM 0xFFB4CADCu     // 220, 202, 180
#define COL_ACCENT gfx::themeAccent()
#define COL_PLANK gfx::themePlank()

enum Align { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };
enum SheetId { SHEET_SPRITES, SHEET_FONT, SHEET_LOGO, SHEET_BGTOP, SHEET_BOARD, SHEET_FRUIT, SHEET_COUNT };

namespace gfx {
// Loads the little that the loading screen itself needs. The rest arrives through loadStep().
bool init();
int loadStepCount();
void loadStep(int i);
void shutdown();
u32 themePlank();   // sign colour for the chosen board
u32 themeAccent();  // highlight colour for the chosen board
u32 themeBar();     // dark strip colour for the chosen board

// Keeps the chosen scenery and board loaded, and the 2D fruit sheet only while the 2D art style
// needs it. Replaced textures are freed a few frames later, once the GPU is done with them.
void sync();

void beginFrame();
void endFrame();
// eye: 0 = left, 1 = right. Returns false if that eye does not need drawing.
bool beginTop(int eye);
void beginBottom();
bool onTop();
// Stereo plane for following top-screen draws: >0 pops out, <0 sinks behind the screen.
void setDepth(float depth);
// Sideways shift per unit of model z for the eye being drawn, used by the 3D fruit.
float stereoPerZ();
void setShake(float dx, float dy);
// Moves everything drawn from now on; screen transitions slide with this.
void setBase(float dx, float dy);
void getOffset(float* x, float* y);  // base + shake, without the stereo shift

#if !PORT_TWO_SCREENS
// One screen: the board behind everything, the field in its own view, and a fade over all of it.
void drawBackdrop(u32 tint);
void beginField();
void fadeAll(float alpha);
#endif

// Test hook: draws both eyes as if the 3D slider were all the way up, even on a 2DS.
void forceStereo(bool on);

void sprite(SheetId sheet, int idx, float x, float y, float sx = 1, float sy = 1, float rot = 0, u32 color = WHITE);
void image(SheetId sheet, int idx, float x, float y, float w, float h, u32 color = WHITE);
void rect(float x, float y, float w, float h, u32 color);
void tri(float x0, float y0, u32 c0, float x1, float y1, u32 c1, float x2, float y2, u32 c2);
float textWidth(const char* s, float scale);
void text(const char* s, float x, float y, float scale, u32 color = WHITE, Align align = ALIGN_LEFT);

extern u32 drawCalls;      // draws issued last frame
extern u32 drawCallsLive;  // draws issued so far this frame
}  // namespace gfx
