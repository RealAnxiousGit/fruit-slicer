#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <math.h>

constexpr float kPi = 3.14159265f;
constexpr float kDt = 1.0f / 60.0f;
constexpr int kBottomW = 320;
constexpr int kBottomH = 240;
constexpr int kTopW = 400;
constexpr int kTopH = 240;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// xorshift32: gameplay randomness never touches the heap or libc state.
struct Rng {
	u32 s = 0x51CE51CE;
	u32 next() {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return s;
	}
	float unit() { return (next() & 0xFFFFFF) / 16777216.0f; }
	float range(float a, float b) { return a + (b - a) * unit(); }
	int below(int n) { return (int)(next() % (u32)n); }
};
extern Rng gRng;

enum Mode { MODE_CLASSIC, MODE_ARCADE, MODE_ZEN, MODE_BLITZ, MODE_SUDDEN, MODE_MINEFIELD, MODE_COUNT };

struct ModeInfo {
	const char* name;
	const char* line1;
	const char* line2;
};
extern const ModeInfo kModes[MODE_COUNT];

// Save data (save.cpp)
struct Settings {
	u32 best[MODE_COUNT];
	u8 sfx, music, blade, pad;
};
extern Settings gSave;
void saveLoad();
void saveWrite();

// One frame of player input, from the hardware or the AUTOTEST bot.
struct Input {
	u32 down, held;
	bool touching;
	int tx, ty;
};
