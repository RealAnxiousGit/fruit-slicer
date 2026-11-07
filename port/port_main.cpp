// Windows, Linux and Android entry point: the window, the keyboard, the mouse and touch.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "common.h"
#include "gfx_sdl.h"
#include "port_api.h"
#include "render.h"

namespace port {
struct AssetFile {
	SDL_IOStream* io;
};

AssetFile* fileOpen(const char* fullPath) {
	SDL_IOStream* io = SDL_IOFromFile(fullPath, "rb");
	return io ? new AssetFile{io} : nullptr;
}

AssetFile* assetOpen(const char* path) {
	char full[512];
#if PORT_ANDROID
	snprintf(full, sizeof full, "romfs/%s", path);
#else
	snprintf(full, sizeof full, "%sromfs/%s", SDL_GetBasePath() ? SDL_GetBasePath() : "", path);
#endif
	SDL_IOStream* io = SDL_IOFromFile(full, "rb");
	if (!io) return nullptr;
	return new AssetFile{io};
}
size_t assetRead(void* out, size_t bytes, AssetFile* a) { return SDL_ReadIO(a->io, out, bytes); }
long assetSize(AssetFile* a) { return (long)SDL_GetIOSize(a->io); }
void assetClose(AssetFile* a) {
	SDL_CloseIO(a->io);
	delete a;
}

static char sDataDir[400];
void dataPath(char* out, size_t size, const char* name) { snprintf(out, size, "%s%s", sDataDir, name); }
void ensureDataDir() {}  // SDL_GetPrefPath already made it

static bool sRunning = true;
bool running() {
	// The loading screen runs before the main loop, so a close request has to be seen here too.
	SDL_PumpEvents();
	if (SDL_HasEvent(SDL_EVENT_QUIT)) sRunning = false;
	return sRunning;
}
}  // namespace port

// ---------------------------------------------------------------- controllers
// Any pad SDL knows: Xbox, PlayStation, Switch and generic. Confirm and Back follow the pad's own labels,
// so A confirms on a Switch pad and Cross on a PlayStation one.
static SDL_Gamepad* sPad;
static u32 sPadHeld;      // menu buttons held on the pad
static u32 sStickDirs;    // directions the left stick is pushed past the menu threshold
static float sDirRepeat;  // seconds until a held direction repeats

static u32 padBits(SDL_Gamepad* pad, Uint8 button) {
	switch (button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP: return KEY_UP;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return KEY_DOWN;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return KEY_LEFT;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return KEY_RIGHT;
		case SDL_GAMEPAD_BUTTON_START: return KEY_START;
		case SDL_GAMEPAD_BUTTON_BACK: return KEY_SELECT;
		case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return KEY_L;
		case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return KEY_R;
		default: break;
	}
	switch (SDL_GetGamepadButtonLabel(pad, (SDL_GamepadButton)button)) {
		case SDL_GAMEPAD_BUTTON_LABEL_A: case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return KEY_A;
		case SDL_GAMEPAD_BUTTON_LABEL_B: case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return KEY_B;
		default: break;
	}
	if (button == SDL_GAMEPAD_BUTTON_SOUTH) return KEY_A;
	if (button == SDL_GAMEPAD_BUTTON_EAST) return KEY_B;
	return 0;
}

static float axis(SDL_GamepadAxis a) { return sPad ? SDL_GetGamepadAxis(sPad, a) / 32767.0f : 0; }

// Once a frame: the sticks and the trigger, and the left stick as menu directions with key-like repeat.
static void readPad(Input& in, float dt) {
	if (!sPad) return;
	float lx = axis(SDL_GAMEPAD_AXIS_LEFTX), ly = axis(SDL_GAMEPAD_AXIS_LEFTY);
	float sx = lx + axis(SDL_GAMEPAD_AXIS_RIGHTX), sy = ly + axis(SDL_GAMEPAD_AXIS_RIGHTY);
	in.stickX = sx < -1 ? -1 : (sx > 1 ? 1 : sx);
	in.stickY = sy < -1 ? -1 : (sy > 1 ? 1 : sy);
	in.stickSlice = axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.35f;
	u32 dirs = sPadHeld & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT);  // the D-pad repeats like the stick
	if (lx < -0.6f) dirs |= KEY_LEFT;
	if (lx > 0.6f) dirs |= KEY_RIGHT;
	if (ly < -0.6f) dirs |= KEY_UP;
	if (ly > 0.6f) dirs |= KEY_DOWN;
	u32 fresh = dirs & ~sStickDirs;
	if (fresh) {
		in.down |= fresh;
		in.repeat |= fresh;
		sDirRepeat = 0.4f;
	} else if (dirs) {
		sDirRepeat -= dt;
		if (sDirRepeat <= 0) {
			in.repeat |= dirs;
			sDirRepeat = 0.11f;
		}
	}
	sStickDirs = dirs;
	in.held |= sPadHeld | dirs;
}

static void openPad(SDL_JoystickID id) {
	if (sPad) return;
	sPad = SDL_OpenGamepad(id);
	if (sPad) fprintf(stderr, "[pad] %s\n", SDL_GetGamepadName(sPad) ? SDL_GetGamepadName(sPad) : "?");
}

static u32 keyBits(SDL_Keycode k) {
	switch (k) {
		case SDLK_UP: case SDLK_W: return KEY_UP;
		case SDLK_DOWN: case SDLK_S: return KEY_DOWN;
		case SDLK_LEFT: case SDLK_A: return KEY_LEFT;
		case SDLK_RIGHT: case SDLK_D: return KEY_RIGHT;
		case SDLK_RETURN: case SDLK_SPACE: return KEY_A;
		case SDLK_ESCAPE: case SDLK_AC_BACK: return KEY_B | KEY_START;
		default: return 0;
	}
}

int main(int, char**) {
#ifdef __linux__
	// WSL has a GPU, but left alone its OpenGL often falls back to drawing on the CPU.
	if (SDL_getenv("WSL_DISTRO_NAME") && !SDL_getenv("GALLIUM_DRIVER")) SDL_setenv_unsafe("GALLIUM_DRIVER", "d3d12", 1);
#endif
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");  // the click that focuses the window still counts
	SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
	SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "1");  // the title bar uses the icon inside the exe
	SDL_SetAppMetadata("Fruit Slicer", nullptr, "com.realanxiousgit.fruitslicer");
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		fprintf(stderr, "[boot] %s\n", SDL_GetError());
		return 1;
	}
	if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) fprintf(stderr, "[pad] no controller support: %s\n", SDL_GetError());
	// The full look by default, except on a phone with little memory: it starts on the light settings.
	gIsNew3ds = !gfx::lowMemory();
	gHas3dScreen = false;
	char* pref = SDL_GetPrefPath("RealAnxiousGit", "FruitSlicer");
	snprintf(port::sDataDir, sizeof port::sDataDir, "%s", pref ? pref : "");
	SDL_free(pref);
	if (!app::init()) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fruit Slicer", "The game's files could not be loaded. Keep the romfs folder beside the game.", nullptr);
		SDL_Quit();
		return 1;
	}
	gRng.s = (u32)SDL_GetTicks() | 0x51CE0001u;

	port::displayStart();

	app::showLoading();

	bool held = false, moved = false;
	float mx = 0, my = 0;
	u32 keysHeld = 0;
	const Uint64 kStep = 1000000000ull / 60;
	Uint64 last = SDL_GetTicksNS(), owed = kStep;
#ifdef TEST_SHOTS
	Uint64 workNs = 0, statFrom = last;
	u32 statDraws = 0, statSteps = 0, statTris = 0, statCalls = 0;
	float presentTotal = 0;
#endif
#if defined(AUTOTEST) || defined(TEST_SHOTS)
	const char* shotDir = SDL_getenv("FRUIT_SLICER_SHOTS");
	u32 frameNo = 0;
#endif

	while (port::running()) {
		// The game always advances 60 steps a second of real time, however fast frames can be shown.
#ifdef TEST_BALANCE
		int steps = 20;
#else
		int steps = 1;
#endif
#if !defined(AUTOTEST) || defined(TEST_REALTIME)  // test builds run flat out unless asked to keep real time
		Uint64 now = SDL_GetTicksNS();
		owed += now - last;
		last = now;
		if (port::gVsyncOn && owed > kStep * 3 / 4 && owed < kStep * 5 / 4) owed = kStep;  // vsync's small jitter isn't a missed frame
		steps = (int)(owed / kStep);
		if (steps == 0) {
			SDL_DelayPrecise(kStep - owed);
			continue;
		}
		owed -= steps * kStep;
		if (steps > 6) steps = 6;  // after a long stall the game just carries on, rather than leaping ahead
#endif
#ifdef TEST_SHOTS
		Uint64 workFrom = SDL_GetTicksNS();
		statSteps += steps;
#endif
		Input in{};
		SDL_Event e;
		bool pressedNow = false, releaseAfter = false;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
				case SDL_EVENT_QUIT:
					port::sRunning = false;
					break;
				case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
				case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
					port::applyVsync();  // moved to a screen with another refresh rate
					break;
				case SDL_EVENT_KEY_DOWN: {
					if (e.key.key == SDLK_F11 || (e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT))) {
						if (!e.key.repeat) port::toggleFullscreen();
						break;
					}
					if (e.key.key == SDLK_F3) {
						if (!e.key.repeat) port::toggleOverlay();
						break;
					}
					u32 b = keyBits(e.key.key);
					in.repeat |= b;
					if (!e.key.repeat) {
						in.down |= b;
						keysHeld |= b;
					}
					break;
				}
				case SDL_EVENT_KEY_UP:
					keysHeld &= ~keyBits(e.key.key);
					break;
				case SDL_EVENT_MOUSE_MOTION:
					mx = e.motion.x;
					my = e.motion.y;
					moved = true;
					break;
				case SDL_EVENT_MOUSE_WHEEL:
					in.wheel += e.wheel.y > 0 ? 1 : (e.wheel.y < 0 ? -1 : 0);
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					if (e.button.button == SDL_BUTTON_LEFT) {
						mx = e.button.x;
						my = e.button.y;
						held = true;
						pressedNow = true;
					}
					break;
				case SDL_EVENT_MOUSE_BUTTON_UP:
					if (e.button.button == SDL_BUTTON_LEFT) {
						// A quick tap on a slow device can start and end between two frames. The game still sees it.
						if (pressedNow) releaseAfter = true;
						else held = false;
					}
					break;
				case SDL_EVENT_GAMEPAD_ADDED:
					openPad(e.gdevice.which);
					break;
				case SDL_EVENT_GAMEPAD_REMOVED:
					if (sPad && SDL_GetGamepadID(sPad) == e.gdevice.which) {
						// Unplugged mid-run: the game pauses, and any other pad still plugged in takes over.
						SDL_CloseGamepad(sPad);
						sPad = nullptr;
						sPadHeld = sStickDirs = 0;
						app::notifySuspended();
						int n = 0;
						SDL_JoystickID* ids = SDL_GetGamepads(&n);
						if (ids && n > 0) openPad(ids[0]);
						SDL_free(ids);
					}
					break;
				case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
					if (sPad && e.gbutton.which == SDL_GetGamepadID(sPad)) {
						u32 b = padBits(sPad, e.gbutton.button);
						in.down |= b;
						in.repeat |= b;
						sPadHeld |= b;
					}
					break;
				case SDL_EVENT_GAMEPAD_BUTTON_UP:
					if (sPad && e.gbutton.which == SDL_GetGamepadID(sPad)) sPadHeld &= ~padBits(sPad, e.gbutton.button);
					break;
				case SDL_EVENT_WINDOW_FOCUS_LOST:
				case SDL_EVENT_DID_ENTER_BACKGROUND:
					held = false;
					keysHeld = 0;
					app::notifySuspended();
					break;
				default:
					break;
			}
		}
		in.held = keysHeld;
		readPad(in, steps / 60.0f);
		float rx = mx, ry = my;
		SDL_RenderCoordinatesFromWindow(gfx::renderer(), mx, my, &rx, &ry);
		rx *= gfx::renderScaleNow();  // Render Scale draws at another size than the window
		ry *= gfx::renderScaleNow();
		int tx = 0, ty = 0;
		gfx::windowToGame(rx, ry, app::inFieldView(), &tx, &ty);
		in.tx = tx;
		in.ty = ty;
		in.touching = held;
#if PORT_MOUSE
		// Slice Without Holding: during a game, a moving mouse counts as a held button. A short grace
		// keeps one swipe in one piece when a frame passes with no motion event.
		static int movingFrames = 0;
		if (gSave.sliceNoHold && app::inFieldView()) {
			movingFrames = moved ? 4 : (movingFrames > 0 ? movingFrames - 1 : 0);
			if (movingFrames > 0) in.touching = true;
		} else {
			movingFrames = 0;
		}
#endif
		in.hovering = moved && !held;
		moved = false;
		if (releaseAfter) held = false;

#if defined(AUTOTEST) || defined(TEST_SHOTS)
		if (shotDir) {
			// Test runs ask for a picture by creating a file; bots get one every few seconds as well.
			char path[300];
			snprintf(path, sizeof path, "%s/shot", shotDir);
			SDL_PathInfo info;
			if (SDL_GetPathInfo(path, &info)) {
				char name[300];
				SDL_IOStream* f = SDL_IOFromFile(path, "r");
				size_t n = f ? SDL_ReadIO(f, name, sizeof name - 1) : 0;
				if (f) SDL_CloseIO(f);
				name[n] = 0;
				while (n > 0 && (name[n - 1] == '\n' || name[n - 1] == '\r')) name[--n] = 0;
				SDL_RemovePath(path);
				snprintf(path, sizeof path, "%s/%s.png", shotDir, name);
				gfx::requestShot(path);
			}
#ifdef AUTOTEST
			if (++frameNo % 150 == 0) {
				snprintf(path, sizeof path, "%s/f%06u.png", shotDir, frameNo);
				gfx::requestShot(path);
			}
#endif
		}
#endif
		Uint64 frameFrom = SDL_GetTicksNS();
		if (!app::frame(in, steps)) break;
		port::hardwareCheck((SDL_GetTicksNS() - frameFrom) / 1e6f - gfx::presentMs());
#ifdef TEST_SHOTS
		workNs += SDL_GetTicksNS() - workFrom;
		presentTotal += gfx::presentMs();
		statTris += gfx::trisDrawn();
		statCalls += gfx::drawCalls;
		if (++statDraws == 120) {
			double secs = (SDL_GetTicksNS() - statFrom) / 1e9;
			fprintf(stderr, "[perf] %.1f draws/s, %.1f steps/s, draw+update %.2f ms (present %.2f ms), tris %u calls %u\n", statDraws / secs, statSteps / secs, workNs / 1e6 / statDraws, presentTotal / statDraws, statTris / statDraws, statCalls / statDraws);
			statDraws = statSteps = statTris = statCalls = 0;
			workNs = 0;
			presentTotal = 0;
			statFrom = SDL_GetTicksNS();
		}
#endif
	}
	app::shutdown();
	SDL_Quit();
	return 0;
}

// Vibration: the phone's own motor. Controllers rumble through SDL's gamepad API instead.
namespace port {
void vibrate(float strength, int ms) {
	if (!gSave.vibration) return;
#if PORT_ANDROID
	static SDL_Haptic* motor = nullptr;
	static bool tried = false;
	if (!tried) {
		tried = true;
		if (SDL_InitSubSystem(SDL_INIT_HAPTIC)) {
			int n = 0;
			SDL_HapticID* ids = SDL_GetHaptics(&n);
			if (ids && n > 0) motor = SDL_OpenHaptic(ids[0]);
			SDL_free(ids);
			if (motor && !SDL_InitHapticRumble(motor)) {
				SDL_CloseHaptic(motor);
				motor = nullptr;
			}
		}
	}
	if (motor) SDL_PlayHapticRumble(motor, strength, (Uint32)ms);
#endif
	if (sPad) SDL_RumbleGamepad(sPad, (Uint16)(strength * 65535), (Uint16)(strength * 0.6f * 65535), (Uint32)ms);
}
}  // namespace port
