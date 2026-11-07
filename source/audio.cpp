#include "audio.h"

#include <stdio.h>
#include <string.h>

namespace audio {
struct Clip {
	s16* pcm;
	u32 samples;
	u32 rate;
};

static constexpr int kMusicChannel = 0;
static constexpr int kFuseChannel = 1;
static constexpr int kFirstSfx = 2;
static constexpr int kSfxChannels = 12;

static Clip sSfx[SFX_COUNT];
static Clip sMusicClip;      // only the current track is ever in memory
static s16* sMusicRetired;   // the DSP may still read the old track for a moment
static int sMusicRetiredFrames;
static Clip sFuse;
static ndspWaveBuf sSfxBuf[kSfxChannels];
static ndspWaveBuf sMusicBuf;
static ndspWaveBuf sFuseBuf;
static int sNext;
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
static constexpr int kAutoTrack[MUSIC_COUNT] = {0, 1, 4};
static constexpr u32 kReadPerFrame = 24 * 1024;  // whole tracks arrive in under a second

static int sContext = MUSIC_NONE;
static FILE* sLoadFile;
static u32 sLoadDone;

const char* trackName(int i) { return i >= 0 && i < kTrackCount ? kTrackNames[i] : "Auto"; }

static int resolveTrack(int context) {
	if (context == MUSIC_NONE) return -1;
	u8 choice = context == MUSIC_MENU ? gSave.menuTrack : gSave.gameTrack;
	return choice > 0 ? choice - 1 : kAutoTrack[context];
}

static float level(u8 slider) { return slider / 100.0f; }

static bool loadClip(Clip& c, const char* name) {
	char path[64];
	snprintf(path, sizeof path, "romfs:/audio/%s.pcm", name);
	FILE* f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "[audio] missing %s\n", path);
		return false;
	}
	u32 head[3];
	bool ok = fread(head, sizeof head, 1, f) == 1 && head[0] == 0x31415346;  // "FSA1"
	if (ok) {
		c.samples = head[1];
		c.rate = head[2];
		c.pcm = (s16*)linearAlloc(c.samples * 2);
		ok = c.pcm && fread(c.pcm, 2, c.samples, f) == c.samples;
		if (ok) DSP_FlushDataCache(c.pcm, c.samples * 2);
		else if (c.pcm) {
			linearFree(c.pcm);
			c.pcm = nullptr;
		}
	}
	fclose(f);
	return ok;
}

bool init() {
	Result rc = ndspInit();
	if (R_FAILED(rc)) {
		// Real hardware needs sdmc:/3ds/dspfirm.cdc; the game stays playable without sound.
		fprintf(stderr, "[audio] ndspInit failed: %08lX\n", (unsigned long)rc);
		return false;
	}
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	sReady = true;
	return true;
}

int loadStepCount() { return SFX_COUNT + 1; }

void loadStep(int i) {
	if (!sReady) return;
	// A clip that fails to load only silences that clip, never the whole game.
	if (i < SFX_COUNT) loadClip(sSfx[i], kSfxFiles[i]);
	else loadClip(sFuse, "fuse");
}

void shutdown() {
	if (sReady) {
		ndspChnWaveBufClear(kMusicChannel);
		ndspChnWaveBufClear(kFuseChannel);
		for (int i = 0; i < kSfxChannels; i++) ndspChnWaveBufClear(kFirstSfx + i);
	}
	for (auto& c : sSfx)
		if (c.pcm) linearFree(c.pcm);
	if (sLoadFile) fclose(sLoadFile);
	sLoadFile = nullptr;
	if (sMusicClip.pcm) linearFree(sMusicClip.pcm);
	if (sMusicRetired) linearFree(sMusicRetired);
	if (sFuse.pcm) linearFree(sFuse.pcm);
	ndspExit();
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
	DSP_FlushDataCache(sMusicClip.pcm, sMusicClip.samples * 2);
	ndspChnSetInterp(kMusicChannel, NDSP_INTERP_LINEAR);
	ndspChnSetRate(kMusicChannel, (float)sMusicClip.rate);
	ndspChnSetFormat(kMusicChannel, NDSP_FORMAT_MONO_PCM16);
	refreshVolumes();
	memset(&sMusicBuf, 0, sizeof sMusicBuf);
	sMusicBuf.data_vaddr = sMusicClip.pcm;
	sMusicBuf.nsamples = sMusicClip.samples;
	sMusicBuf.looping = true;
	ndspChnWaveBufAdd(kMusicChannel, &sMusicBuf);
}

void update() {
	if (sLoadFile) {
		u32 left = sMusicClip.samples * 2 - sLoadDone;
		u32 want = left < kReadPerFrame ? left : kReadPerFrame;
		size_t got = fread((u8*)sMusicClip.pcm + sLoadDone, 1, want, sLoadFile);
		sLoadDone += got;
		if (got != want || sLoadDone >= sMusicClip.samples * 2) {
			fclose(sLoadFile);
			sLoadFile = nullptr;
			if (got == want) {
				beginPlayback();
			} else {
				linearFree(sMusicClip.pcm);
				sMusicClip.pcm = nullptr;
			}
		}
	}
	if (sMusicRetired && --sMusicRetiredFrames <= 0) {
		linearFree(sMusicRetired);
		sMusicRetired = nullptr;
	}
}

void play(Sfx s, float volume, float pan, float pitch) {
	if (!sReady || gSave.sfx == 0) return;
	const Clip& clip = sSfx[s];
	if (!clip.pcm) return;
	int slot = -1;
	for (int i = 0; i < kSfxChannels; i++) {
		int cand = (sNext + i) % kSfxChannels;
		if (sSfxBuf[cand].status != NDSP_WBUF_QUEUED && sSfxBuf[cand].status != NDSP_WBUF_PLAYING) {
			slot = cand;
			break;
		}
	}
	if (slot < 0) slot = sNext;  // every voice busy: steal the oldest
	sNext = (slot + 1) % kSfxChannels;

	int ch = kFirstSfx + slot;
	ndspChnWaveBufClear(ch);
	ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
	ndspChnSetRate(ch, (float)clip.rate * pitch);
	ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
	float v = volume * level(gSave.sfx) * 0.85f;  // headroom, so stacked combo sounds don't clip
	float mix[12] = {v * clampf(1.0f - pan, 0.0f, 1.0f), v * clampf(1.0f + pan, 0.0f, 1.0f)};
	ndspChnSetMix(ch, mix);
	ndspWaveBuf& b = sSfxBuf[slot];
	memset(&b, 0, sizeof b);
	b.data_vaddr = clip.pcm;
	b.nsamples = clip.samples;
	ndspChnWaveBufAdd(ch, &b);
}

void refreshVolumes() {
	if (!sReady) return;
	float v = 0.6f * level(gSave.music);
	float mix[12] = {v, v};
	ndspChnSetMix(kMusicChannel, mix);
}

static void startTrack(int track) {
	if (!sReady || track == sCurrent) return;
	sCurrent = track;
	ndspChnWaveBufClear(kMusicChannel);
	if (sLoadFile) {
		// Still arriving, so the DSP never saw this one.
		fclose(sLoadFile);
		sLoadFile = nullptr;
		if (sMusicClip.pcm) linearFree(sMusicClip.pcm);
		sMusicClip.pcm = nullptr;
	}
	if (sMusicClip.pcm) {
		if (sMusicRetired) {
			svcSleepThread(10000000);  // two swaps in a row: let the DSP finish with the older one
			linearFree(sMusicRetired);
		}
		sMusicRetired = sMusicClip.pcm;
		sMusicRetiredFrames = 6;
		sMusicClip.pcm = nullptr;
	}
	if (track < 0) return;
	char path[64];
	snprintf(path, sizeof path, "romfs:/audio/%s.pcm", kTrackFiles[track]);
	FILE* f = fopen(path, "rb");
	if (!f) return;
	u32 head[3];
	if (fread(head, sizeof head, 1, f) != 1 || head[0] != 0x31415346) {
		fclose(f);
		return;
	}
	sMusicClip.samples = head[1];
	sMusicClip.rate = head[2];
	sMusicClip.pcm = (s16*)linearAlloc(sMusicClip.samples * 2);
	if (!sMusicClip.pcm) {
		fclose(f);
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
		float mix[12] = {v * clampf(1.0f - pan, 0.0f, 1.0f), v * clampf(1.0f + pan, 0.0f, 1.0f)};
		ndspChnSetMix(kFuseChannel, mix);
		if (!sFuseOn) {
			ndspChnSetInterp(kFuseChannel, NDSP_INTERP_LINEAR);
			ndspChnSetRate(kFuseChannel, (float)sFuse.rate);
			ndspChnSetFormat(kFuseChannel, NDSP_FORMAT_MONO_PCM16);
			memset(&sFuseBuf, 0, sizeof sFuseBuf);
			sFuseBuf.data_vaddr = sFuse.pcm;
			sFuseBuf.nsamples = sFuse.samples;
			sFuseBuf.looping = true;
			ndspChnWaveBufAdd(kFuseChannel, &sFuseBuf);
			sFuseOn = true;
		}
	} else if (sFuseOn) {
		ndspChnWaveBufClear(kFuseChannel);
		sFuseOn = false;
	}
}
}  // namespace audio
