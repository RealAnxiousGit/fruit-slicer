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
static constexpr int kSfxChannels = 16;

static Clip sSfx[SFX_COUNT];
static Clip sMusic[MUSIC_COUNT];
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
static const char* const kMusicFiles[MUSIC_COUNT] = {"music_menu", "music_play", "music_zen"};

static float level(u8 steps) { return steps / (float)(kVolumeLevels - 1); }

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
	// A clip that fails to load only silences that clip, never the whole game.
	int missing = 0;
	for (int i = 0; i < SFX_COUNT; i++) missing += !loadClip(sSfx[i], kSfxFiles[i]);
	for (int i = 0; i < MUSIC_COUNT; i++) missing += !loadClip(sMusic[i], kMusicFiles[i]);
	missing += !loadClip(sFuse, "fuse");
	if (missing) fprintf(stderr, "[audio] %d clips failed to load\n", missing);
	sReady = true;
	return missing == 0;
}

void shutdown() {
	if (sReady) {
		ndspChnWaveBufClear(kMusicChannel);
		ndspChnWaveBufClear(kFuseChannel);
		for (int i = 0; i < kSfxChannels; i++) ndspChnWaveBufClear(kFirstSfx + i);
	}
	for (auto& c : sSfx)
		if (c.pcm) linearFree(c.pcm);
	for (auto& c : sMusic)
		if (c.pcm) linearFree(c.pcm);
	if (sFuse.pcm) linearFree(sFuse.pcm);
	ndspExit();
	sReady = false;
}

bool available() { return sReady; }

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

void music(Music m) {
	if (!sReady || m == sCurrent) return;
	sCurrent = m;
	ndspChnWaveBufClear(kMusicChannel);
	if (m == MUSIC_NONE) return;
	const Clip& clip = sMusic[m];
	if (!clip.pcm) return;
	ndspChnSetInterp(kMusicChannel, NDSP_INTERP_LINEAR);
	ndspChnSetRate(kMusicChannel, (float)clip.rate);
	ndspChnSetFormat(kMusicChannel, NDSP_FORMAT_MONO_PCM16);
	refreshVolumes();
	memset(&sMusicBuf, 0, sizeof sMusicBuf);
	sMusicBuf.data_vaddr = clip.pcm;
	sMusicBuf.nsamples = clip.samples;
	sMusicBuf.looping = true;
	ndspChnWaveBufAdd(kMusicChannel, &sMusicBuf);
}

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
