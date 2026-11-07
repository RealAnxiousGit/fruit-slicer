// The parts of drawing that look the same everywhere: board colours and the sign-painted font.
#include "font_metrics.h"
#include "render.h"

namespace gfx {
struct BoardTheme {
	u8 plank[3], accent[3], bar[3];
};
// Picked from each board: its check, its wood, its plate rim, its veins, its sea, its night.
static const BoardTheme kThemes[kBoardCount] = {
	{{46, 84, 116}, {255, 204, 96}, {22, 42, 62}},
	{{112, 68, 36}, {255, 170, 60}, {62, 36, 18}},
	{{44, 84, 148}, {255, 214, 120}, {70, 50, 40}},
	{{78, 82, 100}, {130, 214, 240}, {52, 54, 68}},
	{{24, 100, 108}, {255, 214, 110}, {16, 70, 76}},
	{{66, 46, 118}, {255, 200, 120}, {24, 18, 54}},
};
u32 themePlank() {
	const u8* c = kThemes[gSave.board % kBoardCount].plank;
	// The plank art is about 84% white, so this lands on the listed colour.
	auto up = [](u8 v) { int n = v * 6 / 5; return (u8)(n > 255 ? 255 : n); };
	return C2D_Color32(up(c[0]), up(c[1]), up(c[2]), 255);
}
u32 themeAccent() {
	const u8* c = kThemes[gSave.board % kBoardCount].accent;
	return C2D_Color32(c[0], c[1], c[2], 255);
}
u32 themeBar() {
	const u8* c = kThemes[gSave.board % kBoardCount].bar;
	return C2D_Color32(c[0], c[1], c[2], 255);
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
