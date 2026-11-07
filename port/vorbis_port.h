// Ogg Vorbis decoding for the Shop's recorded music (universal port only), on top of stb_vorbis.
#pragma once

namespace vorbis {
struct Decoder;
// Opens a whole file held in memory. Gives null for anything that isn't a playable Vorbis stream.
Decoder* open(const unsigned char* data, int bytes, int* channels, int* rate, unsigned* frames);
// Decodes up to `frames` frames of interleaved 16-bit samples; returns how many it wrote (0 at the end).
int read(Decoder* d, short* out, int channels, int frames);
void close(Decoder* d);
}  // namespace vorbis
