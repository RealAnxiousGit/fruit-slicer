// Display Mode, Resolution and V-Sync for Windows, Linux and Android, plus the F3 frame-rate readout.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "gfx_sdl.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace port {
constexpr int kMaxRes = 24;
const char* gResNames[kMaxRes] = {"1280 x 720"};
u8 gResChoice;
bool gVsyncOn;
void applyGraphics();
static char sResText[kMaxRes][24];
static u16 sResW[kMaxRes], sResH[kMaxRes], sResHz[kMaxRes];
static int sResCount;

struct Shown {
	u8 mode;
	u16 winW, winH, fsW, fsH, fsHz;
};
static Shown sNow, sBefore;

static Shown fromSave() { return {gSave.windowMode, gSave.winW, gSave.winH, gSave.fsW, gSave.fsH, gSave.fsHz}; }
static void toSave(const Shown& s) {
	gSave.windowMode = s.mode;
	gSave.winW = s.winW;
	gSave.winH = s.winH;
	gSave.fsW = s.fsW;
	gSave.fsH = s.fsH;
	gSave.fsHz = s.fsHz;
}

static SDL_DisplayID display() {
	SDL_DisplayID d = gfx::window() ? SDL_GetDisplayForWindow(gfx::window()) : 0;
	return d ? d : SDL_GetPrimaryDisplay();
}

static void addRes(int w, int h, int hz, float density) {
	if (sResCount >= kMaxRes) return;
	for (int i = 0; i < sResCount; i++)
		if (sResW[i] == w && sResH[i] == h && sResHz[i] == hz) return;
	int i = sResCount++;
	sResW[i] = (u16)w;
	sResH[i] = (u16)h;
	sResHz[i] = (u16)hz;
	// Sizes are in screen points; the name shows real pixels, which differ on a scaled (high DPI) screen.
	int pw = (int)(w * density + 0.5f), ph = (int)(h * density + 0.5f);
	if (hz) snprintf(sResText[i], sizeof sResText[i], "%d x %d  %d Hz", pw, ph, hz);
	else snprintf(sResText[i], sizeof sResText[i], "%d x %d", pw, ph);
	gResNames[i] = sResText[i];
}

static const u16 kWindowSizes[][2] = {{640, 360}, {960, 540}, {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3200, 1800}, {3840, 2160}};

// The largest 16:9 window that fits, never above 1280 x 720 unless asked for.
static void defaultWindowSize(int* w, int* h) {
	SDL_Rect r{0, 0, 1920, 1040};
	SDL_GetDisplayUsableBounds(display(), &r);
	*w = 640;
	*h = 360;
	for (auto& s : kWindowSizes)
		if (s[0] <= 1280 && s[0] <= r.w && s[1] + 40 <= r.h) {
			*w = s[0];
			*h = s[1];
		}
}

int resolutionCount() {
	sResCount = 0;
	const SDL_DisplayMode* desk = SDL_GetDesktopDisplayMode(display());
	float density = desk && desk->pixel_density > 0 ? desk->pixel_density : 1;
	if (gSave.windowMode == 2) {
		int n = 0;
		SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display(), &n);
		for (int i = 0; modes && i < n; i++)
			if (modes[i]->w >= 640 && modes[i]->h >= 360) addRes(modes[i]->w, modes[i]->h, (int)(modes[i]->refresh_rate + 0.5f), modes[i]->pixel_density > 0 ? modes[i]->pixel_density : 1);
		SDL_free(modes);
	} else if (gSave.windowMode == 0) {
		SDL_Rect r{0, 0, 1920, 1040};
		SDL_GetDisplayUsableBounds(display(), &r);
		for (auto& s : kWindowSizes)
			if (s[0] <= r.w && s[1] + 40 <= r.h) addRes(s[0], s[1], 0, density);  // room for the title bar
	}
	if (!sResCount) addRes(desk ? desk->w : 1280, desk ? desk->h : 720, 0, density);

	// Show the current choice, or the closest thing to the default.
	int want = -1;
	int w = 0, h = 0, hz = 0;
	if (gSave.windowMode == 2) {
		w = gSave.fsW ? gSave.fsW : (desk ? desk->w : 0);
		h = gSave.fsW ? gSave.fsH : (desk ? desk->h : 0);
		hz = gSave.fsW ? gSave.fsHz : (desk ? (int)(desk->refresh_rate + 0.5f) : 0);
	} else if (gSave.windowMode == 0) {
		if (gSave.winW) {
			w = gSave.winW;
			h = gSave.winH;
		} else {
			defaultWindowSize(&w, &h);
		}
	}
	for (int i = 0; i < sResCount && want < 0; i++)
		if (sResW[i] == w && sResH[i] == h && (!hz || sResHz[i] == hz)) want = i;
	for (int i = 0; i < sResCount && want < 0; i++)
		if (sResW[i] == w && sResH[i] == h) want = i;
	gResChoice = (u8)(want >= 0 ? want : 0);
	return sResCount;
}

static void showNow() {
	SDL_Window* win = gfx::window();
	if (!win) return;
#if !PORT_ANDROID
	if (gSave.windowMode == 0) {
		int w = gSave.winW, h = gSave.winH;
		if (!w || !h) defaultWindowSize(&w, &h);
		SDL_SetWindowFullscreen(win, false);
		SDL_SyncWindow(win);
		SDL_SetWindowSize(win, w, h);
		SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED_DISPLAY(display()), SDL_WINDOWPOS_CENTERED_DISPLAY(display()));
	} else if (gSave.windowMode == 1) {
		SDL_SetWindowFullscreenMode(win, nullptr);
		SDL_SetWindowFullscreen(win, true);
	} else {
		SDL_DisplayMode m;
		bool found = gSave.fsW && SDL_GetClosestFullscreenDisplayMode(display(), gSave.fsW, gSave.fsH, gSave.fsHz, true, &m);
		if (!found) {
			const SDL_DisplayMode* d = SDL_GetDesktopDisplayMode(display());
			if (d) {
				m = *d;
				found = true;
			}
		}
		SDL_SetWindowFullscreenMode(win, found ? &m : nullptr);
		SDL_SetWindowFullscreen(win, true);
	}
	SDL_SyncWindow(win);
#endif
	sNow = fromSave();
	applyVsync();  // the refresh rate may have changed
}

bool displayApply() {
	if (gResChoice < sResCount) {
		if (gSave.windowMode == 0) {
			gSave.winW = sResW[gResChoice];
			gSave.winH = sResH[gResChoice];
		} else if (gSave.windowMode == 2) {
			gSave.fsW = sResW[gResChoice];
			gSave.fsH = sResH[gResChoice];
			gSave.fsHz = sResHz[gResChoice];
		}
	}
	Shown want = fromSave();
	bool changed = memcmp(&want, &sNow, sizeof want) != 0;
	sBefore = sNow;
	if (changed) showNow();
	return changed;
}

void displayRevert() {
	toSave(sBefore);
	showNow();
	resolutionCount();
}

void displayDiscard() {
	toSave(sNow);
	resolutionCount();
}

// The game runs 60 steps a second. On a 60 Hz screen V-Sync shows each one; at 120 Hz, each one twice.
// Any other rate (or V-Sync off) is paced by a timer.
void applyVsync() {
	SDL_Renderer* r = gfx::renderer();
	if (!r) return;
	const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display());
	float hz = mode && mode->refresh_rate > 0 ? mode->refresh_rate : 60;
	int interval = 0;
	if (gSave.vsync && !SDL_getenv("FRUIT_SLICER_NO_VSYNC")) {
		if (hz > 58 && hz < 62) interval = 1;
		else if (hz > 118 && hz < 122) interval = 2;
	}
#ifdef AUTOTEST
	interval = 0;
#endif
	if (interval && !SDL_SetRenderVSync(r, interval)) interval = 0;
	if (!interval) SDL_SetRenderVSync(r, 0);
	gVsyncOn = interval != 0;
}

// At start, after the renderer: the window takes the saved mode.
void displayStart() {
	applyGraphics();  // remembers the Texture Quality the art was loaded at
	resolutionCount();
	if (gSave.windowMode != 0 || gSave.winW) showNow();
	else {
		int w, h;
		defaultWindowSize(&w, &h);
		if (w != 1280) SDL_SetWindowSize(gfx::window(), w, h);
		sNow = fromSave();
		applyVsync();
	}
	sBefore = sNow;
}

// F11 and Alt+Enter: between a window and borderless fullscreen.
void toggleFullscreen() {
	sBefore = sNow;
	gSave.windowMode = gSave.windowMode == 0 ? 1 : 0;
	showNow();
	resolutionCount();
	saveWrite();
}

// ---------------------------------------------------------------- F3 readout
static bool sOverlay;
static int sFrames;
static Uint64 sFrom;
static char sOverlayText[96];

void toggleOverlay() { sOverlay = !sOverlay; }

static unsigned memoryMb() {
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS pmc;
	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) return (unsigned)(pmc.WorkingSetSize >> 20);
	return 0;
#else
	unsigned long pages = 0, resident = 0;
	if (FILE* f = fopen("/proc/self/statm", "r")) {
		if (fscanf(f, "%lu %lu", &pages, &resident) != 2) resident = 0;
		fclose(f);
	}
	return (unsigned)(resident * 4096 >> 20);
#endif
}

void drawOverlay() {
	Uint64 now = SDL_GetTicksNS();
	sFrames++;
	if (now - sFrom >= 500000000ull) {
		float fps = sFrames * 1e9f / (float)(now - sFrom);
		int w = 0, h = 0;
		SDL_GetCurrentRenderOutputSize(gfx::renderer(), &w, &h);
		const char* name = SDL_GetRendererName(gfx::renderer());
		snprintf(sOverlayText, sizeof sOverlayText, "%.0f FPS  %u MB  %s  %dx%d%s", fps, memoryMb(), name ? name : "?", w, h, gVsyncOn ? "  vsync" : "");
		sFrames = 0;
		sFrom = now;
	}
	if (!sOverlay) return;
	SDL_Renderer* r = gfx::renderer();
	SDL_SetRenderClipRect(r, nullptr);
	SDL_SetRenderScale(r, 2, 2);
	SDL_FRect back{2, 2, 8.0f * (float)strlen(sOverlayText) + 4, 12};
	SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
	SDL_RenderFillRect(r, &back);
	SDL_SetRenderDrawColor(r, 160, 255, 160, 255);
	SDL_RenderDebugText(r, 4, 4, sOverlayText);
	SDL_SetRenderScale(r, 1, 1);
}
}  // namespace port

// ---------------------------------------------------------------- graphics presets
namespace port {
struct Preset {
	u8 tex, shadow, ao, bloom, effects, detail, scale;
};
// Performance draws fewer pixels and skips the extras; Beautiful draws more pixels than the screen has.
static const Preset kPresets[3] = {
	{1, 0, 0, 0, FX_LOW, 0, 0},
	{2, 1, 1, 1, FX_FULL, 1, 1},
	{2, 2, 1, 2, FX_FULL, 1, 2},
};
static u8 sLoadedTex = 255;
static char sNotice[96];

void setNotice(const char* text) { snprintf(sNotice, sizeof sNotice, "%s", text); }
const char* notice() { return sNotice[0] ? sNotice : nullptr; }

void applyGraphics() {
	if (sLoadedTex != gSave.texQuality) {
		if (sLoadedTex != 255) gfx::reloadTextures();
		sLoadedTex = gSave.texQuality;
	}
}

void matchPreset() {
	gSave.preset = 3;
	for (int i = 0; i < 3; i++) {
		const Preset& p = kPresets[i];
		if (gSave.texQuality == p.tex && gSave.shadowQuality == p.shadow && gSave.ambientOcc == p.ao && gSave.bloom == p.bloom &&
		    gSave.effects == p.effects && gSave.detail == p.detail && gSave.renderScale == p.scale)
			gSave.preset = (u8)i;
	}
}

void applyPreset(int i) {
	if (i < 0 || i > 2) return;
	const Preset& p = kPresets[i];
	gSave.texQuality = p.tex;
	gSave.shadowQuality = p.shadow;
	gSave.ambientOcc = p.ao;
	gSave.bloom = p.bloom;
	gSave.effects = p.effects;
	gSave.detail = p.detail;
	gSave.renderScale = p.scale;
	gSave.preset = (u8)i;
	applyGraphics();
}

// The first start picks Quality, or Performance where the computer can't hold 60 frames a second with
// room to spare: the Software renderer, a phone short on memory, or a slow title screen.
void hardwareCheck(float workMs) {
#ifdef AUTOTEST
	(void)workMs;
	return;
#else
	if (gSave.hwChecked) return;
	static int frames = 0;
	static float total = 0;
	int pick = -1;
	if (gfx::softwareRenderer() || gfx::lowMemory()) {
		pick = 0;
	} else if (++frames > 60) {  // the first second still has loading in it
		total += workMs;
		if (frames >= 60 + 180) pick = total / 180 > 8 ? 0 : 1;
	}
	if (pick < 0) return;
	applyPreset(pick);
	gSave.hwChecked = 1;
	saveWrite();
	fprintf(stderr, "[gfx] first start: %s graphics\n", pick ? "Quality" : "Performance");
	if (pick == 0) setNotice("Graphics set to Performance for this device.");
#endif
}
}  // namespace port
