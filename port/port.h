// Everything the shared game needs to know about this port: Windows, Linux and Android, one screen.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __ANDROID__
#define PORT_ANDROID 1
#else
#define PORT_ANDROID 0
#endif

#define PORT_3DS 0
#define PORT_TWO_SCREENS 0
#define PORT_HAS_SENSITIVITY PORT_ANDROID
#define PORT_PAUSE_BUTTON PORT_ANDROID
#define PORT_MOUSE (!PORT_ANDROID)
#define PORT_LIGHT_FRENZY PORT_ANDROID
#define PORT_SENSITIVITY_LABEL "Touch Sensitivity"
#define PORT_SENSITIVITY_HINT "Higher follows quick finger flicks."
#define PORT_ART_HINT "2D suits the Software renderer."
#define PORT_DETAIL_HINT "Low suits older devices."
#define PORT_RESET_HINT "Back to the default look."
#define PORT_VISUALS_HINT "Art style, board, blade,"
#define PORT_DATA_HINT "Resetting scores and settings."
#define PORT_LOADING_TEXT "Getting the fruit ready"
#define PORT_NO_SOUND_TEXT "No sound device found"
#if PORT_ANDROID
#define PORT_HOWTO_PAUSE "The pause sign stops the game."
#define PORT_CONFIRM_TEXT "Tap again to confirm"
#define PORT_GAMEPLAY_HINT "Touch, difficulty, and how the"
#define PORT_DEFAULT_SENSITIVITY 70
#else
#define PORT_HOWTO_PAUSE "Escape pauses and goes back."
#define PORT_CONFIRM_TEXT "Press again to confirm"
#define PORT_GAMEPLAY_HINT "Difficulty, and how the"
#define PORT_DEFAULT_SENSITIVITY 75
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;

// The buttons the shared menus read. The keyboard fills these in.
enum {
	KEY_A = 1u << 0, KEY_B = 1u << 1, KEY_SELECT = 1u << 2, KEY_START = 1u << 3,
	KEY_RIGHT = 1u << 4, KEY_LEFT = 1u << 5, KEY_UP = 1u << 6, KEY_DOWN = 1u << 7,
	KEY_L = 1u << 9, KEY_R = 1u << 8, KEY_X = 1u << 10, KEY_Y = 1u << 11, KEY_TOUCH = 1u << 20,
};

constexpr u32 C2D_Color32(u8 r, u8 g, u8 b, u8 a) { return (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24); }

// The renderer choices this platform offers, in the order the Visuals row shows them.
extern const char* const kRendererNames[];
extern const int kRendererCount;
// Display and graphics settings. They live in the save's fixed port block (32 bytes).
#define PORT_SETTINGS_FIELDS 	u8 renderer;      /* index into kRendererNames */ 	u8 windowMode;    /* 0 windowed, 1 borderless fullscreen, 2 exclusive fullscreen */ 	u8 vsync;         /* 0 off (frame cap), 1 on */ 	u8 preset;        /* 0 Performance, 1 Quality, 2 Beautiful, 3 Custom */ 	u8 texQuality;    /* 0 low, 1 medium, 2 high */ 	u8 shadowQuality; /* 0 off, 1 simple, 2 soft */ 	u8 ambientOcc;    /* 0 off, 1 on */ 	u8 bloom;         /* 0 off, 1 low, 2 high */ 	u8 renderScale;   /* 0 75%, 1 100%, 2 125% */ 	u8 hwChecked;     /* the first-start hardware check has picked a preset */ 	u16 winW, winH;   /* windowed size, 0 = default */ 	u16 fsW, fsH, fsHz; /* exclusive fullscreen mode, 0 = desktop mode */
#define PORT_CREDITS_TOOLS_1 "SDL 3, CMake, Ninja, GCC, MinGW-w64"
#define PORT_CREDITS_TOOLS_2 "Android SDK and NDK, Gradle, appimagetool"
#define PORT_RESET_VISUALS
#define PORT_RESET_DISPLAY 	gSave.renderer = 0; gSave.windowMode = 0; gSave.vsync = 1; gSave.preset = 1; 	gSave.texQuality = 2; gSave.shadowQuality = 2; gSave.ambientOcc = 1; gSave.bloom = 1; gSave.renderScale = 1; gSave.hwChecked = 0; 	gSave.winW = gSave.winH = 0; gSave.fsW = gSave.fsH = gSave.fsHz = 0;
// 0.4 universal saves had the renderer right after the 60 shared bytes.
#define PORT_UPGRADE_V4(data, size) if ((size) > 60) gSave.renderer = (data)[60]; gSave.hwChecked = 1; gSave.preset = 3;
#define PORT_CHECK_SETTINGS 	if (gSave.renderer >= kRendererCount) gSave.renderer = 0; 	if (gSave.windowMode > 2) gSave.windowMode = 0; 	gSave.vsync &= 1; gSave.ambientOcc &= 1; gSave.hwChecked &= 1; 	if (gSave.preset > 3) gSave.preset = 1; 	if (gSave.texQuality > 2) gSave.texQuality = 2; 	if (gSave.shadowQuality > 2) gSave.shadowQuality = 2; 	if (gSave.bloom > 2) gSave.bloom = 1; 	if (gSave.renderScale > 2) gSave.renderScale = 1;

// The field fruit flies over, in game units: 16:9, the same height as the 3DS field.
constexpr int kFieldW = 427;
constexpr int kFieldH = 240;
constexpr float kFieldTop = 34;      // the score sign covers this much
constexpr float kFillX = -500;       // full-field washes also cover any bars beside the field
constexpr float kFillW = 1427;
constexpr float kJumpReject = 1e6f;  // mice and glass don't spike, so a real flick must never restart a stroke
#if PORT_ANDROID
constexpr float kCutReach = 1.2f;    // a fingertip is wider than the point it reports
#else
constexpr float kCutReach = 1.0f;
#endif

// Display settings, done by port/display.cpp. The shared Options menu shows them in a Display section.
#define PORT_HAS_DISPLAY 1
#if PORT_ANDROID
#define PORT_DISPLAY_HINT "Renderer"
#else
#define PORT_DISPLAY_HINT "Window, resolution, V-Sync"
#endif
namespace port {
extern const char* gResNames[];  // the Resolution row's choices
extern u8 gResChoice;
int resolutionCount();    // rebuilds the choices for the chosen Display Mode
bool displayApply();      // makes the window match the settings; true if anything changed
void displayRevert();     // back to how it was before the last apply
void displayDiscard();    // forgets a mode or resolution that was picked but never applied
void applyVsync();
void applyPreset(int preset);  // Performance, Quality or Beautiful
void matchPreset();       // names the preset the graphics settings match, or Custom
void applyGraphics();     // after a graphics row changes: textures reload if their quality did
const char* notice();     // something the player should know since starting, or null
}  // namespace port
#define PORT_DISPLAY_HINT2 "and graphics quality."
// How the shared game draws a few things, from the Display section's graphics settings.
#define PORT_SHADOWS (gSave.shadowQuality)  // 0 off, 1 simple, 2 soft
#define PORT_BLOOM (gSave.bloom)            // 0 off, 1 low, 2 high
#define PORT_AMBIENT_OCC (gSave.ambientOcc)
