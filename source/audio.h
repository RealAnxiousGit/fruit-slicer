#pragma once
#include "common.h"

enum Sfx {
	SFX_SWISH0, SFX_SWISH1, SFX_SWISH2, SFX_SHING,
	SFX_CRISP0, SFX_CRISP1, SFX_JUICY0, SFX_JUICY1, SFX_SOFT0, SFX_SOFT1, SFX_MELON0, SFX_MELON1,
	SFX_KNOCK, SFX_SHELL,
	SFX_THROW, SFX_BOOM,
	SFX_COMBO3, SFX_COMBO4, SFX_COMBO5, SFX_COMBO6, SFX_COMBO8, SFX_COMBO10,
	SFX_STRIKE, SFX_CLICK, SFX_POWER, SFX_FREEZE, SFX_FEVER, SFX_GO, SFX_OVER, SFX_BEST, SFX_GOLD,
	SFX_FINALE_RISE, SFX_FINALE_HIT, SFX_FINALE_BURST,
	SFX_COUNT
};
enum Music { MUSIC_NONE = -1, MUSIC_MENU, MUSIC_PLAY, MUSIC_ZEN, MUSIC_COUNT };

namespace audio {
// Loads every clip into linear memory up front; returns false if ndsp is unavailable.
bool init();
void shutdown();
bool available();
// pan is -1 (left) to +1 (right). pitch multiplies the playback rate, so one clip can vary.
void play(Sfx s, float volume = 1.0f, float pan = 0.0f, float pitch = 1.0f);
void music(Music m);
void refreshVolumes();
// The sizzle that plays while a bomb is in the air.
void fuse(bool on, float pan = 0.0f);
}  // namespace audio
