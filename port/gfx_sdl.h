// What this port's 3D renderer and main loop need from its 2D renderer.
#pragma once
#include <SDL3/SDL.h>

namespace gfx {
SDL_Renderer* renderer();
SDL_Window* window();
bool softwareRenderer();
bool lowMemory();  // a phone with under 3 GB: pictures load at half size
SDL_Surface* halveForLowMemory(SDL_Surface* s);
void pushTris(SDL_Texture* tex, const SDL_Vertex* v, int count);
SDL_Vertex* reserveTris(SDL_Texture* tex, int count);  // room for this many vertices, to fill before the next draw
void toScreen(float x, float y, float* sx, float* sy);
float viewScale();
void windowToGame(float wx, float wy, bool field, int* tx, int* ty);
SDL_Surface* loadPng(const char* path);  // from the game's assets
void requestShot(const char* path);      // saves the next finished frame
float presentMs();                       // test builds: how long showing the last frame took
unsigned trisDrawn();                    // test builds: triangles in the last frame
}  // namespace gfx
