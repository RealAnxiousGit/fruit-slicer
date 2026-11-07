// stb_vorbis v1.22 by Sean Barrett (public domain / MIT, see the end of stb_vorbis.inc),
// built here with no file or push APIs: the game hands it whole files from its own assets.
#include "vorbis_port.h"

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
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
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace vorbis {
struct Decoder {
	stb_vorbis* v;
};

Decoder* open(const unsigned char* data, int bytes, int* channels, int* rate, unsigned* frames) {
	int err = 0;
	stb_vorbis* v = stb_vorbis_open_memory(data, bytes, &err, nullptr);
	if (!v) return nullptr;
	stb_vorbis_info info = stb_vorbis_get_info(v);
	*channels = info.channels;
	*rate = (int)info.sample_rate;
	*frames = stb_vorbis_stream_length_in_samples(v);
	Decoder* d = new Decoder{v};
	return d;
}

int read(Decoder* d, short* out, int channels, int frames) {
	return stb_vorbis_get_samples_short_interleaved(d->v, channels, out, frames * channels);
}

void close(Decoder* d) {
	if (!d) return;
	stb_vorbis_close(d->v);
	delete d;
}
}  // namespace vorbis
