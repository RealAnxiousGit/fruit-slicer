#include "audio.h"

#include <stdio.h>
#include <string.h>

#include "audio_port.h"
#include "port_api.h"

namespace audio {
static Clip sSfx[SFX_COUNT];
static Clip sMusicClip;      // only the current track is ever in memory
static s16* sMusicRetired;   // the hardware may still read the old track for a moment
static int sMusicRetiredFrames;
static Clip sFuse;
static int sCurrent = MUSIC_NONE;
static bool sFuseOn;
static bool sReady;

static const char* const kSfxFiles[SFX_COUNT] = {
	"swish0", "swish1", "swish2", "shing",
	"crisp0", "crisp1", "juicy0", "juicy1", "soft0", "soft1", "melon0", "melon1",
	"knock", "shell",
	"throw", "boom",
	"combo3", "combo4", "combo5", "combo6", "combo8", "combo10",
	"strike", "click", "power", "freeze", "fever", "go", "over", "best", "gold",
	"finale_rise", "finale_hit", "finale_burst",
};
static const char* const kTrackFiles[kTrackCount] = {
	"music_menu", "music_play", "music_meadow", "music_lantern", "music_zen", "music_moss", "music_bamboo", "music_clouds",
};
static const char* const kTrackNames[kTrackCount] = {
	"Orchard Morning", "Picnic Stroll", "Sleepy Meadow", "Lantern Walk", "Still Water", "Moss Garden", "Bamboo Breeze", "Drifting Clouds",
};
// What each screen plays on Auto: menus, games, and Zen.
static constexpr int kAutoTrack[MUSIC_COUNT] = {0, 1, 4};
static constexpr u32 kReadPerFrame = 24 * 1024;  // whole tracks arrive in under a second

static int sContext = MUSIC_NONE;
static port::AssetFile* sLoadFile;
static u32 sLoadDone;

const char* trackName(int i) { return i >= 0 && i < kTrackCount ? kTrackNames[i] : "Auto"; }

static int resolveTrack(int context) {
	if (context == MUSIC_NONE) return -1;
	u8 choice = context == MUSIC_MENU ? gSave.menuTrack : gSave.gameTrack;
	return choice > 0 ? choice - 1 : kAutoTrack[context];
}

static float level(u8 slider) { return slider / 100.0f; }

static port::AssetFile* openClip(const char* name, u32 head[3]) {
	char path[64];
	snprintf(path, sizeof path, "audio/%s.pcm", name);
	port::AssetFile* f = port::assetOpen(path);
	if (!f) {
		fprintf(stderr, "[audio] missing %s\n", path);
		return nullptr;
	}
	if (port::assetRead(head, 12, f) != 12 || head[0] != 0x31415346) {  // "FSA1"
		port::assetClose(f);
		return nullptr;
	}
	return f;
}

static bool loadClip(Clip& c, const char* name) {
	u32 head[3];
	port::AssetFile* f = openClip(name, head);
	if (!f) return false;
	c.samples = head[1];
	c.rate = head[2];
	c.pcm = audioport::alloc(c.samples * 2);
	bool ok = c.pcm && port::assetRead(c.pcm, c.samples * 2, f) == c.samples * 2;
	if (ok) audioport::ready(c.pcm, c.samples * 2);
	else if (c.pcm) {
		audioport::release(c.pcm);
		c.pcm = nullptr;
	}
	port::assetClose(f);
	return ok;
}

bool init() {
	sReady = audioport::init();
	return sReady;
}

int loadStepCount() { return SFX_COUNT + 1; }

void loadStep(int i) {
	if (!sReady) return;
	// A clip that fails to load only silences that clip, never the whole game.
	if (i < SFX_COUNT) loadClip(sSfx[i], kSfxFiles[i]);
	else loadClip(sFuse, "fuse");
}

void shutdown() {
	if (sReady) audioport::shutdown();
	for (auto& c : sSfx)
		if (c.pcm) audioport::release(c.pcm);
	if (sLoadFile) port::assetClose(sLoadFile);
	sLoadFile = nullptr;
	if (sMusicClip.pcm) audioport::release(sMusicClip.pcm);
	if (sMusicRetired) audioport::release(sMusicRetired);
	if (sFuse.pcm) audioport::release(sFuse.pcm);
	sReady = false;
}

bool available() { return sReady; }

void click(float volume) {
	u8 saved = gSave.sfx;
	gSave.sfx = gSave.menuSfx;
	play(SFX_CLICK, volume);
	gSave.sfx = saved;
}

static void beginPlayback() {
	audioport::ready(sMusicClip.pcm, sMusicClip.samples * 2);
	refreshVolumes();
	audioport::musicStart(sMusicClip);
}

void update() {
	if (sLoadFile) {
		u32 left = sMusicClip.samples * 2 - sLoadDone;
		u32 want = left < kReadPerFrame ? left : kReadPerFrame;
		size_t got = port::assetRead((u8*)sMusicClip.pcm + sLoadDone, want, sLoadFile);
		sLoadDone += got;
		if (got != want || sLoadDone >= sMusicClip.samples * 2) {
			port::assetClose(sLoadFile);
			sLoadFile = nullptr;
			if (got == want) {
				beginPlayback();
			} else {
				audioport::release(sMusicClip.pcm);
				sMusicClip.pcm = nullptr;
			}
		}
	}
	if (sMusicRetired && --sMusicRetiredFrames <= 0) {
		audioport::release(sMusicRetired);
		sMusicRetired = nullptr;
	}
}

void play(Sfx s, float volume, float pan, float pitch) {
	if (!sReady || gSave.sfx == 0) return;
	const Clip& clip = sSfx[s];
	if (!clip.pcm) return;
	float v = volume * level(gSave.sfx) * 0.85f;  // headroom, so stacked combo sounds don't clip
	audioport::playSfx(clip, v * clampf(1.0f - pan, 0.0f, 1.0f), v * clampf(1.0f + pan, 0.0f, 1.0f), (float)clip.rate * pitch);
}

void refreshVolumes() {
	if (!sReady) return;
	audioport::musicVolume(0.6f * level(gSave.music));
}

static void startTrack(int track) {
	if (!sReady || track == sCurrent) return;
	sCurrent = track;
	audioport::musicStop();
	if (sLoadFile) {
		// Still arriving, so the hardware never saw this one.
		port::assetClose(sLoadFile);
		sLoadFile = nullptr;
		if (sMusicClip.pcm) audioport::release(sMusicClip.pcm);
		sMusicClip.pcm = nullptr;
	}
	if (sMusicClip.pcm) {
		if (sMusicRetired) {
			audioport::settle();  // two swaps in a row: let the hardware finish with the older one
			audioport::release(sMusicRetired);
		}
		sMusicRetired = sMusicClip.pcm;
		sMusicRetiredFrames = 6;
		sMusicClip.pcm = nullptr;
	}
	if (track < 0) return;
	u32 head[3];
	port::AssetFile* f = openClip(kTrackFiles[track], head);
	if (!f) return;
	sMusicClip.samples = head[1];
	sMusicClip.rate = head[2];
	sMusicClip.pcm = audioport::alloc(sMusicClip.samples * 2);
	if (!sMusicClip.pcm) {
		port::assetClose(f);
		return;
	}
	sLoadFile = f;
	sLoadDone = 0;
}

void music(Music m) {
	sContext = m;
	startTrack(resolveTrack(m));
}

void preview(Music m) { startTrack(resolveTrack(m)); }

void reapply() { startTrack(resolveTrack(sContext)); }

void fuse(bool on, float pan) {
	if (!sReady || !sFuse.pcm) return;
	if (on && gSave.sfx > 0) {
		float v = 0.2f * level(gSave.sfx);
		audioport::fuseMix(v * clampf(1.0f - pan, 0.0f, 1.0f), v * clampf(1.0f + pan, 0.0f, 1.0f));
		if (!sFuseOn) {
			audioport::fuseStart(sFuse);
			sFuseOn = true;
		}
	} else if (sFuseOn) {
		audioport::fuseStop();
		sFuseOn = false;
	}
}
}  // namespace audio
