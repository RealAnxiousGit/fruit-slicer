// stb_vorbis v1.22 by Sean Barrett (public domain / MIT, see the end of stb_vorbis.inc), and dr_mp3 v0.7.4
// and dr_flac v0.13.4 by David Reid (public domain / MIT-0, see the end of dr_mp3.inc and dr_flac.inc),
// built here with no file or push APIs: the game hands them whole files it has already read.
#include "song_decoder.h"

#include <string.h>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_OGG
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#include "stb_vorbis.inc"
#include "dr_mp3.inc"
#include "dr_flac.inc"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace songdec {
enum Kind { OGG, MP3, FLAC };
struct Decoder {
	Kind kind;
	stb_vorbis* ogg;
	drmp3 mp3;
	drflac* flac;
};

Decoder* open(const unsigned char* data, int bytes, int* channels, int* rate, unsigned* frames) {
	if (!data || bytes < 4) return nullptr;
	if (memcmp(data, "OggS", 4) == 0) {
		int err = 0;
		stb_vorbis* v = stb_vorbis_open_memory(data, bytes, &err, nullptr);
		if (!v) return nullptr;
		stb_vorbis_info info = stb_vorbis_get_info(v);
		*channels = info.channels;
		*rate = (int)info.sample_rate;
		*frames = stb_vorbis_stream_length_in_samples(v);
		Decoder* d = new Decoder{};
		d->kind = OGG;
		d->ogg = v;
		return d;
	}
	if (memcmp(data, "fLaC", 4) == 0) {
		drflac* f = drflac_open_memory(data, (size_t)bytes, nullptr);
		if (!f) return nullptr;
		*channels = f->channels;
		*rate = (int)f->sampleRate;
		*frames = f->totalPCMFrameCount > 0xFFFFFFFFu ? 0xFFFFFFFFu : (unsigned)f->totalPCMFrameCount;
		Decoder* d = new Decoder{};
		d->kind = FLAC;
		d->flac = f;
		return d;
	}
	// Anything else may be MP3: dr_mp3 finds the first frame itself, past any ID3 tag.
	Decoder* d = new Decoder{};
	d->kind = MP3;
	if (!drmp3_init_memory(&d->mp3, data, (size_t)bytes, nullptr)) {
		delete d;
		return nullptr;
	}
	*channels = (int)d->mp3.channels;
	*rate = (int)d->mp3.sampleRate;
	const drmp3_uint64 n = drmp3_get_pcm_frame_count(&d->mp3);  // walks the file once, then starts again
	*frames = n > 0xFFFFFFFFu ? 0xFFFFFFFFu : (unsigned)n;
	return d;
}

int read(Decoder* d, short* out, int channels, int frames) {
	switch (d->kind) {
		case OGG: return stb_vorbis_get_samples_short_interleaved(d->ogg, channels, out, frames * channels);
		case FLAC: return (int)drflac_read_pcm_frames_s16(d->flac, (drflac_uint64)frames, out);
		case MP3: return (int)drmp3_read_pcm_frames_s16(&d->mp3, (drmp3_uint64)frames, out);
	}
	return 0;
}

void close(Decoder* d) {
	if (!d) return;
	if (d->kind == OGG) stb_vorbis_close(d->ogg);
	else if (d->kind == FLAC) drflac_close(d->flac);
	else drmp3_uninit(&d->mp3);
	delete d;
}
}  // namespace songdec
