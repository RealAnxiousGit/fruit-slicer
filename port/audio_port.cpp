// Sound on SDL: a small software mixer feeding one stereo stream.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio_port.h"

namespace audioport {
static constexpr int kVoices = 32;
static constexpr int kRate = 48000;
static constexpr int kChunk = 2048;

struct Voice {
	const s16* pcm;
	u32 samples;
	double pos, step;
	float left, right;
	bool on, loop;
};
static Voice sVoices[kVoices];
static Voice sMusic, sFuse;
static int sNext;
static float sMusicVolume = 0.45f;
static SDL_AudioStream* sStream;
static SDL_Mutex* sLock;
static float sMix[kChunk * 2];
static s16 sOut[kChunk * 2];

static void mixVoice(Voice& v, float l, float r, int frames) {
	if (!v.on) return;
	for (int i = 0; i < frames; i++) {
		u32 at = (u32)v.pos;
		if (at >= v.samples) {
			if (!v.loop) {
				v.on = false;
				return;
			}
			v.pos -= v.samples;
			at = (u32)v.pos;
			if (at >= v.samples) at = 0;
		}
		u32 next = at + 1 < v.samples ? at + 1 : (v.loop ? 0 : at);
		float t = (float)(v.pos - at);
		float s = (v.pcm[at] + (v.pcm[next] - v.pcm[at]) * t) / 32768.0f;
		sMix[i * 2] += s * l;
		sMix[i * 2 + 1] += s * r;
		v.pos += v.step;
	}
}

// A safety net for whatever the loudness budget in audio.cpp lets through. It does nothing below
// the ceiling, so normal play is untouched, and eases back over a fraction of a second after a peak.
static void limit(float* mix, int frames) {
	static constexpr float kCeiling = 0.95f;
	static constexpr float kRelease = 1.0f / (0.15f * kRate);
	static float gain = 1;
	for (int i = 0; i < frames; i++) {
		float l = mix[i * 2], r = mix[i * 2 + 1];
		float a = (l < 0 ? -l : l) > (r < 0 ? -r : r) ? (l < 0 ? -l : l) : (r < 0 ? -r : r);
		if (a * gain > kCeiling) gain = kCeiling / a;
		mix[i * 2] = l * gain;
		mix[i * 2 + 1] = r * gain;
		gain += (1.0f - gain) * kRelease;
	}
}

#ifdef AUTOTEST
// Test builds print the mix level each second, before anything limits it.
static void meter(const float* mix, int frames) {
	static float peak;
	static double sum;
	static u32 count, over;
	for (int i = 0; i < frames * 2; i++) {
		float a = mix[i] < 0 ? -mix[i] : mix[i];
		if (a > peak) peak = a;
		if (a > 1) over++;
		sum += a * a;
	}
	count += frames * 2;
	if (count >= (u32)kRate * 2) {
		int live = 0;
		for (const auto& v : sVoices) live += v.on;
		fprintf(stderr, "[mix] peak=%.3f rms=%.3f over=%lu voices=%d\n", peak, SDL_sqrt(sum / count), (unsigned long)over, live);
		peak = 0;
		sum = 0;
		count = over = 0;
	}
}
#endif

static void SDLCALL feed(void*, SDL_AudioStream* stream, int additional, int) {
	while (additional > 0) {
		int frames = additional / 4;
		if (frames > kChunk) frames = kChunk;
		if (frames <= 0) break;
		SDL_memset(sMix, 0, sizeof(float) * frames * 2);
		SDL_LockMutex(sLock);
		mixVoice(sMusic, sMusicVolume, sMusicVolume, frames);
		mixVoice(sFuse, sFuse.left, sFuse.right, frames);
		for (auto& v : sVoices) mixVoice(v, v.left, v.right, frames);
		SDL_UnlockMutex(sLock);
#ifdef AUTOTEST
		meter(sMix, frames);
#endif
		limit(sMix, frames);
		for (int i = 0; i < frames * 2; i++) sOut[i] = (s16)(clampf(sMix[i], -1, 1) * 32000);
		SDL_PutAudioStreamData(stream, sOut, frames * 4);
		additional -= frames * 4;
	}
}

#ifdef TEST_LATENCY
void playSfx(const Clip& c, float left, float right, float rate);
// Plays clicks into silence and prints a line as each is asked for, so a capture tool can time them.
static void latencyTest() {
	static s16 click[960];
	for (int i = 0; i < 960; i++) click[i] = (i / 12) % 2 ? 26000 : -26000;
	Clip c = {click, 960, 48000};
	SDL_AudioSpec spec;
	int frames = 0;
	SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(sStream), &spec, &frames);
	printf("device %d Hz, %d frames, driver %s\n", spec.freq, frames, SDL_GetCurrentAudioDriver());
	fflush(stdout);
	SDL_Delay(1500);
	for (int i = 0; i < 25; i++) {
		SDL_Delay(300 + (i * 37) % 101);
		printf("c\n");
		fflush(stdout);
		playSfx(c, 0.8f, 0.8f, 48000);
	}
	SDL_Delay(1500);
	exit(0);
}
#endif

bool init() {
#ifdef TEST_LATENCY
	const char* tryFrames = SDL_getenv("FS_LAT_FRAMES");
	SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, tryFrames ? tryFrames : "256");
#else
	SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");  // about 5 ms: keeps a press and its sound close together
#endif
	if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		fprintf(stderr, "[audio] %s\n", SDL_GetError());
		return false;
	}
	SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, kRate};
	sLock = SDL_CreateMutex();
	sStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, nullptr);
	if (!sStream) {
		fprintf(stderr, "[audio] %s\n", SDL_GetError());
		return false;
	}
	SDL_ResumeAudioStreamDevice(sStream);
#ifdef TEST_LATENCY
	latencyTest();
#endif
	return true;
}

void shutdown() {
	if (sStream) SDL_DestroyAudioStream(sStream);
	sStream = nullptr;
	if (sLock) SDL_DestroyMutex(sLock);
	sLock = nullptr;
}

s16* alloc(u32 bytes) { return (s16*)malloc(bytes); }
void release(s16* p) {
	// Music is replaced while the mixer runs: nothing may still be reading what gets freed.
	if (sLock) SDL_LockMutex(sLock);
	if (sMusic.pcm == p) sMusic.on = false;
	if (sFuse.pcm == p) sFuse.on = false;
	for (auto& v : sVoices)
		if (v.pcm == p) v.on = false;
	if (sLock) SDL_UnlockMutex(sLock);
	free(p);
}
void ready(const s16*, u32) {}
void settle() {}

void playSfx(const Clip& c, float left, float right, float rate) {
	SDL_LockMutex(sLock);
	int slot = -1;
	for (int i = 0; i < kVoices; i++) {
		int cand = (sNext + i) % kVoices;
		if (!sVoices[cand].on) {
			slot = cand;
			break;
		}
	}
	if (slot < 0) slot = sNext;
	sNext = (slot + 1) % kVoices;
	sVoices[slot] = {c.pcm, c.samples, 0.0, (double)rate / kRate, left, right, true, false};
	SDL_UnlockMutex(sLock);
}

void musicStart(const Clip& c) {
	SDL_LockMutex(sLock);
	sMusic = {c.pcm, c.samples, 0.0, (double)c.rate / kRate, 1, 1, true, true};
	SDL_UnlockMutex(sLock);
}

void musicStop() {
	SDL_LockMutex(sLock);
	sMusic.on = false;
	SDL_UnlockMutex(sLock);
}

void musicVolume(float v) {
	SDL_LockMutex(sLock);
	sMusicVolume = v;
	SDL_UnlockMutex(sLock);
}

void fuseStart(const Clip& c) {
	SDL_LockMutex(sLock);
	float l = sFuse.left, r = sFuse.right;
	sFuse = {c.pcm, c.samples, 0.0, (double)c.rate / kRate, l, r, true, true};
	SDL_UnlockMutex(sLock);
}

void fuseMix(float left, float right) {
	SDL_LockMutex(sLock);
	sFuse.left = left;
	sFuse.right = right;
	SDL_UnlockMutex(sLock);
}

void fuseStop() {
	SDL_LockMutex(sLock);
	sFuse.on = false;
	SDL_UnlockMutex(sLock);
}
}  // namespace audioport
