#pragma once
#include "common.h"

enum Sfx {
	SFX_SWISH0, SFX_SWISH1, SFX_SWISH2,
	SFX_SQUISH0, SFX_SQUISH1, SFX_SQUISH2,
	SFX_THROW, SFX_BOOM, SFX_COMBO3, SFX_COMBO4, SFX_COMBO5,
	SFX_STRIKE, SFX_CLICK, SFX_POWER, SFX_FREEZE, SFX_GO, SFX_OVER, SFX_BEST, SFX_GOLD,
	SFX_COUNT
};
enum Music { MUSIC_NONE = -1, MUSIC_MENU, MUSIC_PLAY, MUSIC_ZEN, MUSIC_COUNT };

namespace audio {
// Loads every clip into linear memory up front; returns false if ndsp is unavailable.
bool init();
void shutdown();
bool available();
// pan is -1 (left) to +1 (right).
void play(Sfx s, float volume = 1.0f, float pan = 0.0f);
void music(Music m);
void refreshMusicVolume();
}  // namespace audio
