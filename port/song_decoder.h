// Compressed music (universal port only): Ogg Vorbis for the Shop's recorded tracks, and for Custom Music
// also MP3 and FLAC. The format is told from the file's first bytes, not its name.
#pragma once

namespace songdec {
struct Decoder;
// Opens a whole file held in memory. Gives null for anything that isn't a playable stream.
Decoder* open(const unsigned char* data, int bytes, int* channels, int* rate, unsigned* frames);
// Decodes up to `frames` frames of interleaved 16-bit samples; returns how many it wrote (0 at the end).
int read(Decoder* d, short* out, int channels, int frames);
void close(Decoder* d);
}  // namespace songdec
