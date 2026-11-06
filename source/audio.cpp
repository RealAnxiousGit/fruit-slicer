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
static constexpr int kFirstSfx = 1;
static constexpr int kSfxChannels = 11;

static Clip sSfx[SFX_COUNT];
static Clip sMusic[MUSIC_COUNT];
static ndspWaveBuf sSfxBuf[kSfxChannels];
static ndspWaveBuf sMusicBuf;
static int sNext;
static int sCurrent = MUSIC_NONE;
static bool sReady;

static const char* const kSfxFiles[SFX_COUNT] = {
	"swish0", "swish1", "swish2", "squish0", "squish1", "squish2", "throw", "boom", "combo3", "combo4",
	"combo5", "strike", "click", "power", "freeze", "go", "over", "best", "gold",
};
static const char* const kMusicFiles[MUSIC_COUNT] = {"music_menu", "music_play", "music_zen"};

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
	bool ok = true;
	for (int i = 0; i < SFX_COUNT; i++) ok = loadClip(sSfx[i], kSfxFiles[i]) && ok;
	for (int i = 0; i < MUSIC_COUNT; i++) ok = loadClip(sMusic[i], kMusicFiles[i]) && ok;
	memset(sSfxBuf, 0, sizeof sSfxBuf);
	memset(&sMusicBuf, 0, sizeof sMusicBuf);
	sReady = ok;
	return ok;
}

void shutdown() {
	if (sReady || sSfx[0].pcm) {
		ndspChnWaveBufClear(kMusicChannel);
		for (int i = 0; i < kSfxChannels; i++) ndspChnWaveBufClear(kFirstSfx + i);
	}
	for (auto& c : sSfx)
		if (c.pcm) linearFree(c.pcm);
	for (auto& c : sMusic)
		if (c.pcm) linearFree(c.pcm);
	ndspExit();
	sReady = false;
}

bool available() { return sReady; }

void play(Sfx s, float volume, float pan) {
	if (!sReady || !gSave.sfx) return;
	const Clip& clip = sSfx[s];
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
	ndspChnSetRate(ch, (float)clip.rate);
	ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
	float l = volume * clampf(1.0f - pan, 0.0f, 1.0f);
	float r = volume * clampf(1.0f + pan, 0.0f, 1.0f);
	float mix[12] = {l, r};
	ndspChnSetMix(ch, mix);
	ndspWaveBuf& b = sSfxBuf[slot];
	memset(&b, 0, sizeof b);
	b.data_vaddr = clip.pcm;
	b.nsamples = clip.samples;
	ndspChnWaveBufAdd(ch, &b);
}

void refreshMusicVolume() {
	float v = gSave.music ? 0.55f : 0.0f;
	float mix[12] = {v, v};
	ndspChnSetMix(kMusicChannel, mix);
}

void music(Music m) {
	if (!sReady || m == sCurrent) return;
	sCurrent = m;
	ndspChnWaveBufClear(kMusicChannel);
	if (m == MUSIC_NONE) return;
	const Clip& clip = sMusic[m];
	ndspChnSetInterp(kMusicChannel, NDSP_INTERP_LINEAR);
	ndspChnSetRate(kMusicChannel, (float)clip.rate);
	ndspChnSetFormat(kMusicChannel, NDSP_FORMAT_MONO_PCM16);
	refreshMusicVolume();
	memset(&sMusicBuf, 0, sizeof sMusicBuf);
	sMusicBuf.data_vaddr = clip.pcm;
	sMusicBuf.nsamples = clip.samples;
	sMusicBuf.looping = true;
	ndspChnWaveBufAdd(kMusicChannel, &sMusicBuf);
}
}  // namespace audio
