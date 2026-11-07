// What every port provides to the shared game, besides render.h, audio_port.h and model3d_internal.h.
#pragma once
#include <stddef.h>

namespace port {
struct AssetFile;
// Read-only game files, by path inside the assets ("audio/click.pcm").
AssetFile* assetOpen(const char* path);
size_t assetRead(void* out, size_t bytes, AssetFile* f);
long assetSize(AssetFile* f);
void assetClose(AssetFile* f);

// Writable files: saves, and anything a player might want to send in.
void dataPath(char* out, size_t size, const char* name);
void ensureDataDir();

// False once the system or player wants the game closed.
bool running();
}  // namespace port
