// What this port's 3D renderer and main loop need from its 2D renderer.
#pragma once
#include <SDL3/SDL.h>

namespace gfx {
SDL_Renderer* renderer();
SDL_Window* window();
bool softwareRenderer();
bool lowMemory();  // a phone with under 3 GB: pictures load at half size
SDL_Surface* halveForQuality(SDL_Surface* s, int keepFrom);  // half size below this Texture Quality (or on a phone short on memory)
void pushTris(SDL_Texture* tex, const SDL_Vertex* v, int count);
SDL_Vertex* reserveTris(SDL_Texture* tex, int count);  // room for this many vertices, to fill before the next draw
void toScreen(float x, float y, float* sx, float* sy);
float viewScale();
void windowToGame(float wx, float wy, bool field, int* tx, int* ty);
SDL_Surface* loadPng(const char* path);  // from the game's assets
void requestShot(const char* path);      // saves the next finished frame
void requestShotArea(const char* path, float x, float y, float w, float h);  // only this part of the top screen (top-screen units)
int shotResult();                        // after a shot: 1 saved, -1 failed, 0 none since last asked
float presentMs();                       // test builds: how long showing the last frame took
unsigned trisDrawn();                    // test builds: triangles in the last frame
}  // namespace gfx

// port/display.cpp, for this port's own code.
namespace port {
extern bool gVsyncOn;       // V-Sync is keeping time, so the main loop can trust the frame rate
void displayStart();        // the saved display mode, once the renderer exists
void toggleFullscreen();    // F11 and Alt+Enter
void toggleOverlay();       // F3
void drawOverlay();         // the F3 readout, last thing before a frame is shown
}  // namespace port
namespace port {
void setNotice(const char* text);
void hardwareCheck(float workMs);  // each frame until the first-start check has picked a preset
}  // namespace port
namespace gfx {
void reloadTextures();     // after Texture Quality changes
float renderScaleNow();    // what Render Scale is drawing at this frame
}  // namespace gfx
namespace m3dport {
void reloadTextures();
}
