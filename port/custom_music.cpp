// Custom Music: the player's own songs for Beat Slice, and the Beat Tool's analysis that finds their beat.
//
// Finding the beat, as for the Shop's recorded tracks: the song is read as one channel at 11 kHz; a short
// spectrum every 23 ms gives how much new sound starts at each moment (spectral flux). The tempo is the
// beat length whose multiples line up best with itself (autocorrelation, 70 to 180 BPM). The first beat
// is where a comb of beats at that tempo lands on the most onsets, and the first beat of a bar is the
// strongest of the four. It is a good guess, and the Beat Tool lets the player nudge it.
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "custom_music.h"
#include "port.h"
#include "port_api.h"
#include "song_decoder.h"

namespace custom {
struct Song {
	char file[128];
	char name[48];
	char path[600];
	bool wav;
	bool hasBeat;
	float bpm, first;
};
static Song sSongs[kMax];
static int sCount;
static char sFolder[460];

static void makeFolder() {
	if (sFolder[0]) return;
#if PORT_ANDROID
	// Android keeps saves where nothing else can reach; songs go where a computer over USB can.
	if (const char* ext = SDL_GetAndroidExternalStoragePath()) snprintf(sFolder, sizeof sFolder, "%s/Custom Music/", ext);
#endif
	if (!sFolder[0]) {
		char base[400];
		port::dataPath(base, sizeof base, "");
		snprintf(sFolder, sizeof sFolder, "%sCustom Music/", base);
	}
	SDL_CreateDirectory(sFolder);
}

const char* folder() {
	makeFolder();
	return sFolder;
}

static bool endsWith(const char* s, const char* ext) {
	size_t n = strlen(s), e = strlen(ext);
	return n > e && SDL_strcasecmp(s + n - e, ext) == 0;
}

static void beatsPath(char* out, size_t size) { snprintf(out, size, "%sbeats.txt", sFolder); }

static void readBeats() {
	char p[520];
	beatsPath(p, sizeof p);
	size_t size = 0;
	char* text = (char*)SDL_LoadFile(p, &size);
	if (!text) return;
	for (char* line = text; line && *line;) {
		char* next = strchr(line, '\n');
		if (next) *next++ = 0;
		size_t len = strlen(line);
		if (len && line[len - 1] == '\r') line[len - 1] = 0;
		float bpm = 0, first = 0;
		int used = 0;
		if (line[0] != '#' && sscanf(line, "%f %f %n", &bpm, &first, &used) == 2 && bpm >= 30 && bpm <= 300) {
			for (int i = 0; i < sCount; i++) {
				if (strcmp(sSongs[i].file, line + used) != 0) continue;
				sSongs[i].hasBeat = true;
				sSongs[i].bpm = bpm;
				sSongs[i].first = first;
			}
		}
		line = next;
	}
	SDL_free(text);
}

static void writeBeats() {
	char p[520];
	beatsPath(p, sizeof p);
	SDL_IOStream* io = SDL_IOFromFile(p, "w");
	if (!io) return;
	SDL_IOprintf(io, "# Fruit Slicer: the beat of each song in this folder, for Beat Slice.\n");
	SDL_IOprintf(io, "# Beats per minute, then the first beat of a bar in seconds, then the file.\n");
	for (int i = 0; i < sCount; i++)
		if (sSongs[i].hasBeat) SDL_IOprintf(io, "%.2f %.3f %s\n", sSongs[i].bpm, sSongs[i].first, sSongs[i].file);
	SDL_CloseIO(io);
}

static int bySongName(const void* a, const void* b) { return SDL_strcasecmp(((const Song*)a)->file, ((const Song*)b)->file); }

void scan() {
	makeFolder();
	sCount = 0;
	int n = 0;
	char** list = SDL_GlobDirectory(sFolder, "*", SDL_GLOB_CASEINSENSITIVE, &n);
	for (int i = 0; list && i < n && sCount < kMax; i++) {
		const char* f = list[i];
		if (strchr(f, '/') || strchr(f, '\\') || strlen(f) >= sizeof sSongs[0].file) continue;
		const bool wav = endsWith(f, ".wav");
		if (!wav && !endsWith(f, ".ogg") && !endsWith(f, ".mp3") && !endsWith(f, ".flac")) continue;
		Song& s = sSongs[sCount++];
		memset(&s, 0, sizeof s);
		snprintf(s.file, sizeof s.file, "%s", f);
		snprintf(s.path, sizeof s.path, "%s%s", sFolder, f);
		s.wav = wav;
		// The game's lettering covers plain letters, numbers and punctuation; anything else becomes a dash.
		size_t len = (size_t)(strrchr(f, '.') - f), o = 0;  // up to the extension
		for (size_t k = 0; k < len && o < sizeof s.name - 1; k++) {
			unsigned char c = (unsigned char)f[k];
			if (c >= 0x80 && (c & 0xC0) == 0x80) continue;  // the rest of a UTF-8 letter
			s.name[o++] = c >= 32 && c < 127 ? (char)c : '-';
		}
		s.name[o] = 0;
	}
	SDL_free(list);
	qsort(sSongs, sCount, sizeof(Song), bySongName);
	readBeats();
}

int count() { return sCount; }
const char* name(int i) { return i >= 0 && i < sCount ? sSongs[i].name : ""; }
const char* path(int i) { return i >= 0 && i < sCount ? sSongs[i].path : ""; }
bool isWav(int i) { return i >= 0 && i < sCount && sSongs[i].wav; }

bool openFolder() {
#if PORT_ANDROID
	return false;
#else
	makeFolder();
	char url[620];
	snprintf(url, sizeof url, "file:///%s", sFolder[0] == '/' ? sFolder + 1 : sFolder);
	return SDL_OpenURL(url);
#endif
}

bool beat(int i, float* bpm, float* first) {
	if (i < 0 || i >= sCount || !sSongs[i].hasBeat) return false;
	*bpm = sSongs[i].bpm;
	*first = sSongs[i].first;
	return true;
}

void setBeat(int i, float bpm, float first) {
	if (i < 0 || i >= sCount) return;
	sSongs[i].hasBeat = true;
	sSongs[i].bpm = bpm;
	sSongs[i].first = first;
	writeBeats();
}

// ---------------------------------------------------------------- finding the beat
static constexpr int kRate = 11025, kWin = 1024, kHop = 256, kBins = kWin / 2 + 1;
static constexpr float kFps = (float)kRate / kHop;

// A 16-bit PCM WAV held in memory: where its samples are, and their shape. False for anything else.
static bool wavShape(const unsigned char* d, size_t size, int* ch, int* rate, size_t* at, size_t* bytes) {
	if (size < 12 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) return false;
	bool fmt = false;
	for (size_t p = 12; p + 8 <= size;) {
		const unsigned len = d[p + 4] | d[p + 5] << 8 | d[p + 6] << 16 | (unsigned)d[p + 7] << 24;
		if (memcmp(d + p, "fmt ", 4) == 0 && len >= 16 && p + 8 + 16 <= size) {
			const unsigned char* f = d + p + 8;
			const int format = f[0] | f[1] << 8, bits = f[14] | f[15] << 8;
			*ch = f[2] | f[3] << 8;
			*rate = f[4] | f[5] << 8 | f[6] << 16 | f[7] << 24;
			fmt = (format == 1 || format == 0xFFFE) && bits == 16 && *ch >= 1 && *ch <= 2 && *rate >= 8000 && *rate <= 192000;
		} else if (memcmp(d + p, "data", 4) == 0) {
			*at = p + 8;
			*bytes = len < size - *at ? len : size - *at;
			return fmt;
		}
		p += 8 + len + (len & 1);
	}
	return false;
}

struct Analysis {
	int song = -1;
	float result = 0;  // what analyzeStep reports once finished
	unsigned char* data = nullptr;
	size_t size = 0;
	songdec::Decoder* dec = nullptr;
	int ch = 1, rate = 44100;
	unsigned frames = 0, done = 0;
	size_t wavAt = 0;
	// Down to one channel at kRate, a box average at a time.
	double step = 1, pos = 0;
	float acc = 0;
	int accN = 0;
	float ring[kWin];
	unsigned heard = 0, sinceHop = 0;
	float prev[kBins];
	float* flux = nullptr;
	int fluxN = 0, fluxCap = 0;
};
static Analysis sA;
static float sCos[kWin / 2], sSin[kWin / 2], sHann[kWin];
static int sRev[kWin];

static void fftSetup() {
	static bool ready = false;
	if (ready) return;
	ready = true;
	int bits = 0;
	while ((1 << bits) < kWin) bits++;
	for (int i = 0; i < kWin; i++) {
		int r = 0;
		for (int b = 0; b < bits; b++) r |= ((i >> b) & 1) << (bits - 1 - b);
		sRev[i] = r;
		sHann[i] = 0.5f - 0.5f * cosf(6.2831853f * i / kWin);
	}
	for (int i = 0; i < kWin / 2; i++) {
		sCos[i] = cosf(6.2831853f * i / kWin);
		sSin[i] = -sinf(6.2831853f * i / kWin);
	}
}

static void fft(float* re, float* im) {
	for (int i = 0; i < kWin; i++)
		if (sRev[i] > i) {
			float t = re[i];
			re[i] = re[sRev[i]];
			re[sRev[i]] = t;
			t = im[i];
			im[i] = im[sRev[i]];
			im[sRev[i]] = t;
		}
	for (int len = 2; len <= kWin; len <<= 1) {
		const int half = len / 2, stride = kWin / len;
		for (int s = 0; s < kWin; s += len)
			for (int k = 0; k < half; k++) {
				const float wr = sCos[k * stride], wi = sSin[k * stride];
				float* ar = re + s + k;
				float* ai = im + s + k;
				const float xr = ar[half] * wr - ai[half] * wi, xi = ar[half] * wi + ai[half] * wr;
				ar[half] = *ar - xr;
				ai[half] = *ai - xi;
				*ar += xr;
				*ai += xi;
			}
	}
}

static void onsetFrame() {
	static float re[kWin], im[kWin];
	for (int k = 0; k < kWin; k++) {
		re[k] = sA.ring[(sA.heard + k) % kWin] * sHann[k];
		im[k] = 0;
	}
	fft(re, im);
	float f = 0;
	for (int k = 0; k < kBins; k++) {
		const float spec = log1pf(10 * sqrtf(re[k] * re[k] + im[k] * im[k]));
		if (sA.fluxN > 0 && spec > sA.prev[k]) f += spec - sA.prev[k];
		sA.prev[k] = spec;
	}
	if (sA.fluxN == sA.fluxCap) {
		int cap = sA.fluxCap ? sA.fluxCap * 2 : 8192;
		float* grown = (float*)realloc(sA.flux, cap * sizeof(float));
		if (!grown) return;
		sA.flux = grown;
		sA.fluxCap = cap;
	}
	sA.flux[sA.fluxN++] = f;
}

static void hear(float x) {
	sA.ring[sA.heard % kWin] = x;
	sA.heard++;
	if (sA.heard >= (unsigned)kWin && ++sA.sinceHop >= (unsigned)kHop) {
		sA.sinceHop = 0;
		onsetFrame();
	}
}

static void take(const short* s, int n) {
	for (int i = 0; i < n; i++) {
		float m = 0;
		for (int c = 0; c < sA.ch; c++) m += s[i * sA.ch + c];
		sA.acc += m / (32768.0f * sA.ch);
		sA.accN++;
		sA.pos += 1;
		if (sA.pos >= sA.step) {
			sA.pos -= sA.step;
			hear(sA.acc / sA.accN);
			sA.acc = 0;
			sA.accN = 0;
		}
	}
}

static float at(const float* v, int n, float i) {
	if (i < 0 || i >= n - 1) return 0;
	int k = (int)i;
	return v[k] + (v[k + 1] - v[k]) * (i - k);
}

// The whole song has been heard: work out the tempo and the first beat.
static bool finish(float* bpmOut, float* firstOut) {
	const int n = sA.fluxN;
	if (n < (int)(kFps * 8)) return false;  // under about eight seconds: too short to tell
	float* f = sA.flux;
	// Only rises above the local level count as onsets.
	float* level = (float*)malloc(n * sizeof(float));
	if (!level) return false;
	float run = 0;
	for (int i = 0; i < n; i++) {
		run += f[i] - (i >= 16 ? f[i - 16] : 0);
		level[i] = run / (i + 1 < 16 ? i + 1 : 16);
	}
	for (int i = 0; i < n; i++) f[i] = f[i] > level[i] ? f[i] - level[i] : 0;
	free(level);

	const int maxLag = (int)(60 * kFps / 70 * 4) + 2;
	static float ac[256];
	for (int l = 0; l < maxLag && l < 256; l++) {
		double s = 0;
		for (int i = 0; i + l < n; i++) s += (double)f[i] * f[i + l];
		ac[l] = (float)s;
	}
	float best = -1, bpm = 120;
	for (int b10 = 700; b10 <= 1800; b10++) {
		const float lag = 60 * kFps / (b10 / 10.0f);
		float s = at(ac, maxLag < 256 ? maxLag : 256, lag) + at(ac, maxLag < 256 ? maxLag : 256, lag * 2) + at(ac, maxLag < 256 ? maxLag : 256, lag * 4);
		if (s > best) {
			best = s;
			bpm = b10 / 10.0f;
		}
	}
	// That is only near: a beat is about 20 spectra long, so the lag can't tell 129 from 130. Over the whole
	// song a wrong tempo drifts off the beat, so the tempo whose comb of beats lines up best with the onsets,
	// at its best start, is far closer.
	auto comb = [&](float tryBpm, int steps, float* phase) {
		const float period = 60 * kFps / tryBpm;
		float top = -1;
		for (int p = 0; p < steps; p++) {
			const float ph = period * p / steps;
			float s = 0;
			int teeth = 0;
			for (float t = ph; t < n - 1; t += period, teeth++) s += at(f, n, t);
			if (teeth) s /= teeth;
			if (s > top) {
				top = s;
				*phase = ph;
			}
		}
		return top;
	};
	{
		const float coarse = bpm;
		float top = -1, ph;
		for (int k = -50; k <= 50; k++) {
			const float s = comb(coarse + k * 0.05f, 48, &ph);
			if (s > top) {
				top = s;
				bpm = coarse + k * 0.05f;
			}
		}
	}
	if (fabsf(bpm - roundf(bpm)) <= 0.15f) bpm = roundf(bpm);  // most songs keep a whole-number tempo

	// The first beat: the comb of beats that lands on the most onsets.
	const float period = 60 * kFps / bpm;
	float bestPhase = 0, bestScore = -1;
	for (int p = 0; p < 200; p++) {
		const float ph = period * p / 200;
		float s = 0;
		for (float t = ph; t < n - 1; t += period) s += at(f, n, t);
		if (s > bestScore) {
			bestScore = s;
			bestPhase = ph;
		}
	}
	// Spectrum i is taken over the samples from (i + 1) hops in, and covers 93 ms; its moment is the middle.
	const float beatLen = 60 / bpm, centre = (kHop + 0.5f * kWin) / kRate;
	float strength[4] = {};
	for (int k = 0; k < 4; k++) {
		int hits = 0;
		for (float t = bestPhase + k * period; t < n - 1; t += 4 * period, hits++) strength[k] += at(f, n, t);
		if (hits) strength[k] /= hits;
	}
	int down = 0;
	for (int k = 1; k < 4; k++)
		if (strength[k] > strength[down]) down = k;
	float first = bestPhase / kFps + centre + down * beatLen;
	first = fmodf(first, 4 * beatLen);
	*bpmOut = bpm;
	*firstOut = first;
	return true;
}

void analyzeCancel() {
	if (sA.dec) songdec::close(sA.dec);
	SDL_free(sA.data);
	free(sA.flux);
	sA = Analysis{};
}

bool analyzeStart(int i) {
	analyzeCancel();
	if (i < 0 || i >= sCount) return false;
	fftSetup();
	sA.song = i;
	sA.data = (unsigned char*)SDL_LoadFile(sSongs[i].path, &sA.size);
	if (!sA.data) {
		sA.result = -1;
		return false;
	}
	bool ok;
	if (sSongs[i].wav) {
		size_t bytes = 0;
		ok = wavShape(sA.data, sA.size, &sA.ch, &sA.rate, &sA.wavAt, &bytes);
		sA.frames = ok ? (unsigned)(bytes / (2 * sA.ch)) : 0;
	} else {
		sA.dec = songdec::open(sA.data, (int)sA.size, &sA.ch, &sA.rate, &sA.frames);
		ok = sA.dec && sA.ch >= 1 && sA.ch <= 2 && sA.rate >= 8000;
	}
	if (!ok || sA.frames == 0) {
		sA.result = -1;
		return false;
	}
	sA.step = (double)sA.rate / kRate;
	return true;
}

float analyzeStep() {
	if (sA.song < 0 || sA.result != 0) return sA.result;
	const Uint64 until = SDL_GetTicksNS() + 6000000;  // six milliseconds a frame
	static short chunk[4096 * 2];
	while (SDL_GetTicksNS() < until) {
		int got;
		if (sA.dec) {
			got = songdec::read(sA.dec, chunk, sA.ch, 4096);
		} else {
			const unsigned left = sA.frames - sA.done;
			got = (int)(left < 4096 ? left : 4096);
			memcpy(chunk, sA.data + sA.wavAt + (size_t)sA.done * 2 * sA.ch, (size_t)got * 2 * sA.ch);
		}
		if (got <= 0 || sA.done >= sA.frames) {
			float bpm, first;
			const int song = sA.song;
			const bool found = finish(&bpm, &first);
			analyzeCancel();
			sA.song = song;
			sA.result = found ? 1 : -1;
			if (found) setBeat(song, bpm, first);
			return sA.result;
		}
		take(chunk, got);
		sA.done += (unsigned)got;
	}
	return 0.99f * sA.done / (sA.frames ? sA.frames : 1);
}
}  // namespace custom
