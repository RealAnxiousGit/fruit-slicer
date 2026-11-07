// Fruit Slicer 3DS: application loop, menus and the top-screen stereo scene.
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "blade.h"
#include "common.h"
#include "game.h"
#include "render.h"
#include "sprites.h"

#include "bgbot.h"
#include "bgtop0.h"
#include "logo.h"

#define VERSION_STRING "0.2.0-alpha"

enum Screen { SCREEN_MENU, SCREEN_OPTIONS, SCREEN_GAME, SCREEN_PAUSE, SCREEN_RESULT };

static Screen sScreen = SCREEN_MENU;
static Game sGame;  // static storage: all gameplay pools are allocated once at load time
static Blade sBlade;
static int sMenuSel = 0;
static int sOptSel = 0;
static int sPressed = -1;
static int sResultSel = 0;
static int sPauseSel = 0;
static bool sResetArmed = false;
static bool sShowDebug = false;
static bool sSuspended = false;
static float sTime = 0;
static float sFps = 60;
static u64 sLastTick = 0;
static aptHookCookie sAptCookie;

struct Button {
	float x, y, w, h;
	const char* label;
};

static const Button kMenuButtons[] = {
	{6, 6, 150, 52, "Classic"},        {164, 6, 150, 52, "Arcade"},
	{6, 64, 150, 52, "Zen"},           {164, 64, 150, 52, "Blitz"},
	{6, 122, 150, 52, "Sudden Death"}, {164, 122, 150, 52, "Minefield"},
	{6, 180, 150, 52, "Combo Rush"},   {164, 180, 150, 52, "Options"},
};
static constexpr int kMenuCount = 8;
static const u32 kModeTints[] = {0xFF4A7AF2u, 0xFF3AA4F5u, 0xFFB0A84Au, 0xFF8C5AE8u, 0xFF4C50D8u, 0xFF6A6A6Au, 0xFFC0609Au, 0xFF9A7A5Au};

static const Button kResultButtons[] = {{20, 170, 134, 52, "Again"}, {166, 170, 134, 52, "Menu"}};
static const Button kPauseButtons[] = {{50, 66, 220, 52, "Resume"}, {50, 130, 220, 52, "Quit"}};

enum OptionId { OPT_SOUND, OPT_MUSIC, OPT_BOARD, OPT_SCENE, OPT_BLADE, OPT_JUICE, OPT_DEPTH, OPT_SHAKE, OPT_RESET, OPT_BACK, OPT_COUNT };
static constexpr int kOptCount = OPT_COUNT;
static const Button kOptButtons[kOptCount] = {
	{6, 4, 150, 44, nullptr},   {164, 4, 150, 44, nullptr},
	{6, 51, 150, 44, nullptr},  {164, 51, 150, 44, nullptr},
	{6, 98, 150, 44, nullptr},  {164, 98, 150, 44, nullptr},
	{6, 145, 150, 44, nullptr}, {164, 145, 150, 44, nullptr},
	{6, 192, 150, 44, nullptr}, {164, 192, 150, 44, nullptr},
};
static const char* const kBladeNames[kBladeCount] = {"Ember", "Azure", "Rose", "Mint", "Gold", "Rainbow"};
static const char* const kBoardNames[kBoardCount] = {"Picnic Blanket", "Cutting Board", "Plate", "Marble", "Beach", "Starlight"};
static const char* const kSceneNames[kSceneCount] = {"Dusk Orchard", "Morning Meadow", "Moonlit Bay"};
static const char* const kVolumeNames[kVolumeLevels] = {"Off", "Low", "Medium", "High", "Full"};
static const char* const kJuiceNames[] = {"Low", "Normal", "Extra"};
static const char* const kDepthNames[] = {"Off", "Normal", "Strong"};

static bool hit(const Button& b, int x, int y) {
	return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

static void drawButton(const Button& b, const char* label, u32 tint, bool selected, bool pressed, float textScale = 0.56f) {
	float dy = pressed ? 2 : 0;
	if (selected) gfx::image(SHEET_SPRITES, sprites_button_idx, b.x - 3, b.y - 3 + dy, b.w + 6, b.h + 6, rgba(255, 236, 150));
	gfx::image(SHEET_SPRITES, sprites_button_idx, b.x, b.y + dy, b.w, b.h, tint);
	gfx::text(label, b.x + b.w * 0.5f, b.y + b.h * 0.5f - 20 * textScale - 1 + dy, textScale, WHITE, ALIGN_CENTER);
}

// Shared tap handling: press highlights, release inside activates. Returns the activated index or -1.
static int handleButtons(const Button* btns, int n, const Input& in, bool wasTouching) {
	int result = -1;
	if (in.touching) {
		// A press must begin on a button and stay on it; a swipe that ends on one does nothing.
		if (!wasTouching) {
			sPressed = -1;
			for (int i = 0; i < n; i++)
				if (hit(btns[i], in.tx, in.ty)) sPressed = i;
		} else if (sPressed >= 0 && !hit(btns[sPressed], in.tx, in.ty)) {
			sPressed = -1;
		}
	} else if (wasTouching) {
		if (sPressed >= 0) result = sPressed;
		sPressed = -1;
	}
	return result;
}

static void dpadGrid(int& sel, int count, u32 down, int cols) {
	if (down & KEY_DOWN) sel = (sel + cols) < count ? sel + cols : count - 1;
	if ((down & KEY_UP) && sel - cols >= 0) sel -= cols;
	if ((down & KEY_RIGHT) && (sel % cols) + 1 < cols && sel + 1 < count) sel++;
	if ((down & KEY_LEFT) && (sel % cols) > 0) sel--;
}

// ---------------------------------------------------------------- top-screen background
static void drawTopBackground() {
	const float d = sTime;
	// Layers sit behind the screen plane and slowly drift for life.
	gfx::setDepth(-2.0f);
	gfx::image(SHEET_BGTOP, bgtop0_sky_idx, -12, 0, 424, 240);
	gfx::setDepth(-1.3f);
	gfx::image(SHEET_BGTOP, bgtop0_far_idx, -12 + sinf(d * 0.11f) * 2, 120, 424, 120);
	gfx::setDepth(-0.7f);
	gfx::image(SHEET_BGTOP, bgtop0_mid_idx, -12 + sinf(d * 0.15f) * 3, 130, 424, 110);
	gfx::setDepth(-0.15f);
	gfx::image(SHEET_BGTOP, bgtop0_near_idx, -12, 170, 424, 70);
}

static void drawBestTable() {
	char buf[40];
	gfx::setDepth(0.4f);
	gfx::image(SHEET_SPRITES, sprites_panel_idx, 40, 124, 320, 106, rgba(24, 16, 44, 175));
	gfx::setDepth(0.8f);
	if (sMenuSel < MODE_COUNT) {
		gfx::text(kModes[sMenuSel].name, 200, 128, 0.56f, rgba(255, 226, 120), ALIGN_CENTER);
		gfx::text(kModes[sMenuSel].line1, 200, 154, 0.38f, rgba(255, 246, 226), ALIGN_CENTER);
		gfx::text(kModes[sMenuSel].line2, 200, 170, 0.38f, rgba(255, 246, 226), ALIGN_CENTER);
		snprintf(buf, sizeof buf, "BEST %lu", (unsigned long)gSave.best[sMenuSel]);
		gfx::text(buf, 200, 192, 0.56f, rgba(170, 240, 255), ALIGN_CENTER);
	} else {
		gfx::text("Options", 200, 128, 0.56f, rgba(255, 226, 120), ALIGN_CENTER);
		gfx::text("Boards, scenery, sound and more", 200, 160, 0.38f, rgba(255, 246, 226), ALIGN_CENTER);
	}
}

static void drawDebug() {
	char buf[64];
	gfx::setDepth(0);
	gfx::rect(4, 186, 300, 50, rgba(0, 0, 0, 170));
	snprintf(buf, sizeof buf, "FPS %.1f CPU %.0f%% GPU %.0f%%", sFps, C3D_GetProcessingTime() * 6.0f, C3D_GetDrawingTime() * 6.0f);
	gfx::text(buf, 8, 186, 0.36f, rgba(160, 255, 160));
	snprintf(buf, sizeof buf, "DRAWS %lu OBJ %d PART %d", (unsigned long)gfx::drawCalls, sGame.activeObjects(), sGame.activeParticles());
	gfx::text(buf, 8, 202, 0.36f, rgba(160, 255, 160));
	snprintf(buf, sizeof buf, "TOUCH RAW %.0f,%.0f BLADE %.0f,%.0f", sBlade.rawX, sBlade.rawY, sBlade.x, sBlade.y);
	gfx::text(buf, 8, 218, 0.36f, rgba(160, 255, 160));
}

static void drawTop() {
	char buf[40];
	drawTopBackground();
	switch (sScreen) {
		case SCREEN_MENU:
			gfx::setDepth(1.2f);
			gfx::sprite(SHEET_LOGO, logo_logo_idx, 200, 64 + sinf(sTime * 1.6f) * 2);
			drawBestTable();
			break;
		case SCREEN_OPTIONS:
			gfx::setDepth(1.0f);
			gfx::sprite(SHEET_LOGO, logo_logo_idx, 200, 54, 0.72f, 0.72f);
			gfx::setDepth(0.4f);
			gfx::image(SHEET_SPRITES, sprites_panel_idx, 40, 106, 320, 124, rgba(24, 16, 44, 175));
			gfx::setDepth(0.7f);
			gfx::text("Fan-made game. Free, non-commercial.", 200, 112, 0.36f, rgba(255, 246, 226), ALIGN_CENTER);
			gfx::text("Art, sound and music generated", 200, 130, 0.36f, rgba(255, 246, 226), ALIGN_CENTER);
			gfx::text("from code in the source repo.", 200, 146, 0.36f, rgba(255, 246, 226), ALIGN_CENTER);
			gfx::text("Not affiliated with any publisher.", 200, 164, 0.36f, rgba(255, 246, 226), ALIGN_CENTER);
			if (!audio::available()) gfx::text("No sound: needs 3ds/dspfirm.cdc", 200, 178, 0.32f, rgba(255, 130, 110), ALIGN_CENTER);
			snprintf(buf, sizeof buf, "FRUIT SLICED %lu  BEST COMBO %lu", (unsigned long)gSave.totalFruit, (unsigned long)gSave.bestCombo);
			gfx::text(buf, 200, 194, 0.34f, rgba(255, 226, 120), ALIGN_CENTER);
			gfx::text("V" VERSION_STRING, 200, 211, 0.34f, rgba(170, 240, 255), ALIGN_CENTER);
			break;
		case SCREEN_GAME:
			sGame.drawHud();
			break;
		case SCREEN_PAUSE:
			sGame.drawHud();
			gfx::setDepth(1.8f);
			gfx::text("PAUSED", 200, 110, 1.3f, rgba(255, 230, 140), ALIGN_CENTER);
			break;
		case SCREEN_RESULT: {
			float in = clampf(sGame.phaseT * 4, 0, 1);
			gfx::setDepth(0.5f);
			gfx::image(SHEET_SPRITES, sprites_panel_idx, 40, 22, 320, 196, rgba(24, 16, 44, (u8)(200 * in)));
			gfx::setDepth(1.0f);
			gfx::text(kModes[sGame.mode].name, 200, 28, 0.56f, rgba(255, 246, 226), ALIGN_CENTER);
			gfx::setDepth(1.7f);
			snprintf(buf, sizeof buf, "%lu", (unsigned long)sGame.score);
			gfx::text(buf, 200, 54, 1.7f, rgba(255, 226, 120), ALIGN_CENTER);
			gfx::setDepth(1.0f);
			if (sGame.newBest) {
				gfx::text("NEW BEST!", 200, 118, 0.7f + sinf(sTime * 6) * 0.04f, rgba(140, 240, 150), ALIGN_CENTER);
			} else {
				snprintf(buf, sizeof buf, "BEST %lu", (unsigned long)sGame.best);
				gfx::text(buf, 200, 118, 0.6f, rgba(170, 240, 255), ALIGN_CENTER);
			}
			snprintf(buf, sizeof buf, "FRUIT SLICED %d", sGame.sliced);
			gfx::text(buf, 200, 156, 0.44f, rgba(255, 246, 226), ALIGN_CENTER);
			snprintf(buf, sizeof buf, "BEST COMBO %d", sGame.bestCombo);
			gfx::text(buf, 200, 178, 0.44f, rgba(255, 246, 226), ALIGN_CENTER);
			break;
		}
	}
	if (sShowDebug) drawDebug();
}

static void drawBottom() {
	switch (sScreen) {
		case SCREEN_MENU:
			gfx::image(SHEET_BGBOT, bgbot_board0_idx + gSave.board, 0, 0, 320, 240);
			for (int i = 0; i < kMenuCount; i++)
				drawButton(kMenuButtons[i], kMenuButtons[i].label, kModeTints[i], sMenuSel == i, sPressed == i, (i == 4 || i == 6) ? 0.48f : 0.56f);
			break;
		case SCREEN_OPTIONS: {
			gfx::image(SHEET_BGBOT, bgbot_board0_idx + gSave.board, 0, 0, 320, 240);
			const u32 tints[kOptCount] = {
				rgba(66, 150, 200), rgba(90, 170, 130), rgba(176, 116, 66), rgba(120, 100, 190), rgba(230, 150, 70),
				rgba(226, 92, 110), rgba(70, 130, 170), rgba(150, 120, 90), rgba(200, 80, 90), rgba(110, 100, 150),
			};
			const char* titles[OPT_RESET] = {"SOUND", "MUSIC", "BOARD", "SCENERY", "BLADE", "JUICE", "3D DEPTH", "SCREEN SHAKE"};
			const char* values[OPT_RESET] = {
				kVolumeNames[gSave.sfx], kVolumeNames[gSave.music], kBoardNames[gSave.board], kSceneNames[gSave.scene],
				kBladeNames[gSave.blade], kJuiceNames[gSave.juice % 3], kDepthNames[gSave.depth % 3], gSave.shake ? "On" : "Off",
			};
			for (int i = 0; i < OPT_RESET; i++) {
				const Button& b = kOptButtons[i];
				float dy = sPressed == i ? 2 : 0;
				if (sOptSel == i) gfx::image(SHEET_SPRITES, sprites_button_idx, b.x - 2, b.y - 2 + dy, b.w + 4, b.h + 4, rgba(255, 236, 150));
				gfx::image(SHEET_SPRITES, sprites_button_idx, b.x, b.y + dy, b.w, b.h, tints[i]);
				gfx::text(titles[i], b.x + b.w * 0.5f, b.y + 2 + dy, 0.3f, rgba(255, 240, 200), ALIGN_CENTER);
				float vs = 0.46f;
				float w = gfx::textWidth(values[i], vs);
				if (w > b.w - 12) vs *= (b.w - 12) / w;
				gfx::text(values[i], b.x + b.w * 0.5f, b.y + 16 + dy + (0.46f - vs) * 18, vs, WHITE, ALIGN_CENTER);
			}
			drawButton(kOptButtons[OPT_RESET], sResetArmed ? "Tap to erase" : "Reset scores", tints[OPT_RESET], sOptSel == OPT_RESET, sPressed == OPT_RESET, 0.4f);
			drawButton(kOptButtons[OPT_BACK], "Back", tints[OPT_BACK], sOptSel == OPT_BACK, sPressed == OPT_BACK, 0.5f);
			break;
		}
		case SCREEN_GAME:
			sGame.drawField(sBlade);
			break;
		case SCREEN_PAUSE:
			sGame.drawField(sBlade);
			gfx::rect(0, 0, 320, 240, rgba(10, 6, 24, 150));
			for (int i = 0; i < 2; i++)
				drawButton(kPauseButtons[i], kPauseButtons[i].label, i == 0 ? rgba(90, 170, 130) : rgba(200, 80, 90), sPauseSel == i, sPressed == i);
			break;
		case SCREEN_RESULT:
			sGame.drawField(sBlade);
			for (int i = 0; i < 2; i++)
				drawButton(kResultButtons[i], kResultButtons[i].label, i == 0 ? rgba(90, 170, 130) : rgba(110, 100, 150), sResultSel == i, sPressed == i);
			break;
	}
}

// ---------------------------------------------------------------- flow
static void startGame(Mode m) {
	sGame.start(m);
	sBlade.reset();
	gfx::setShake(0, 0);
	sScreen = SCREEN_GAME;
}

static void toMenu() {
	sScreen = SCREEN_MENU;
	sPressed = -1;
	gfx::setShake(0, 0);
	audio::music(MUSIC_MENU);
}

static void activateMenu(int i) {
	audio::play(SFX_CLICK);
	sMenuSel = i;
	if (i < MODE_COUNT) startGame((Mode)i);
	else {
		sScreen = SCREEN_OPTIONS;
		sOptSel = 0;
		sResetArmed = false;
	}
}

static void activateOption(int i) {
	sOptSel = i;
	if (i != OPT_RESET) sResetArmed = false;
	switch (i) {
		case OPT_SOUND: gSave.sfx = (gSave.sfx + 1) % kVolumeLevels; break;
		case OPT_MUSIC: gSave.music = (gSave.music + 1) % kVolumeLevels; audio::refreshVolumes(); break;
		case OPT_BOARD: gSave.board = (gSave.board + 1) % kBoardCount; break;
		case OPT_SCENE:
			gSave.scene = (gSave.scene + 1) % kSceneCount;
			gfx::setScene(gSave.scene);
			break;
		case OPT_BLADE: gSave.blade = (gSave.blade + 1) % kBladeCount; break;
		case OPT_JUICE: gSave.juice = (gSave.juice + 1) % 3; break;
		case OPT_DEPTH: gSave.depth = (gSave.depth + 1) % 3; break;
		case OPT_SHAKE: gSave.shake = !gSave.shake; break;
		case OPT_RESET:
			if (!sResetArmed) {
				sResetArmed = true;
				audio::play(SFX_CLICK);
				return;
			}
			memset(gSave.best, 0, sizeof gSave.best);
			gSave.totalFruit = 0;
			gSave.bestCombo = 0;
			sResetArmed = false;
			break;
		case OPT_BACK:
			audio::play(SFX_CLICK);
			saveWrite();
			sScreen = SCREEN_MENU;
			return;
	}
	audio::play(SFX_CLICK);
	saveWrite();
}

static void pauseGame() {
	if (sScreen != SCREEN_GAME || sGame.phase == PHASE_OVER) return;
	sScreen = SCREEN_PAUSE;
	audio::fuse(false);
	sPauseSel = 0;
	sPressed = -1;
	sBlade.reset();
}

static void aptHook(APT_HookType hook, void*) {
	if (hook == APTHOOK_ONSUSPEND || hook == APTHOOK_ONSLEEP) sSuspended = true;
}

#ifdef AUTOTEST
#include "autotest.inc"
#endif

static void updateFrame(const Input& in, bool wasTouching) {
	sTime += kDt;
	if (in.down & KEY_SELECT) sShowDebug = !sShowDebug;
	if (sSuspended) {
		sSuspended = false;
		pauseGame();
	}

	switch (sScreen) {
		case SCREEN_MENU: {
			audio::music(MUSIC_MENU);
			int prev = sMenuSel;
			dpadGrid(sMenuSel, kMenuCount, in.down, 2);
			if (prev != sMenuSel) audio::play(SFX_CLICK, 0.5f);
			if (in.touching) {
				for (int i = 0; i < kMenuCount; i++)
					if (hit(kMenuButtons[i], in.tx, in.ty)) sMenuSel = i;
			}
			int tapped = handleButtons(kMenuButtons, kMenuCount, in, wasTouching);
			if (tapped >= 0) activateMenu(tapped);
			else if (in.down & KEY_A) activateMenu(sMenuSel);
			break;
		}
		case SCREEN_OPTIONS: {
			dpadGrid(sOptSel, kOptCount, in.down, 2);
			int tapped = handleButtons(kOptButtons, kOptCount, in, wasTouching);
			if (tapped >= 0) activateOption(tapped);
			else if (in.down & KEY_A) activateOption(sOptSel);
			else if (in.down & KEY_B) activateOption(OPT_BACK);
			break;
		}
		case SCREEN_GAME:
			sBlade.update(in);
			sGame.update(sBlade);
			if (in.down & KEY_START) {
				pauseGame();
				break;
			}
			if (sGame.phase == PHASE_OVER) {
				sScreen = SCREEN_RESULT;
				sResultSel = 0;
				sPressed = -1;
				sBlade.reset();
			}
			break;
		case SCREEN_PAUSE: {
			if (in.down & (KEY_UP | KEY_DOWN)) sPauseSel = !sPauseSel;
			int tapped = handleButtons(kPauseButtons, 2, in, wasTouching);
			if (tapped < 0 && (in.down & KEY_A)) tapped = sPauseSel;
			if (tapped < 0 && (in.down & KEY_START)) tapped = 0;
			if (tapped == 0) {
				audio::play(SFX_CLICK);
				sScreen = SCREEN_GAME;
				sBlade.reset();
			} else if (tapped == 1) {
				audio::play(SFX_CLICK);
				toMenu();
			}
			break;
		}
		case SCREEN_RESULT: {
			sGame.update(sBlade);  // keeps debris falling behind the buttons
			if (in.down & (KEY_LEFT | KEY_RIGHT)) sResultSel = !sResultSel;
			// A short lock-out stops a frantic swipe from tapping a button by accident.
			int tapped = sGame.phaseT > 0.7f ? handleButtons(kResultButtons, 2, in, wasTouching) : -1;
			if (tapped < 0 && sGame.phaseT > 0.7f && (in.down & KEY_A)) tapped = sResultSel;
			if (tapped == 0) {
				audio::play(SFX_CLICK);
				startGame(sGame.mode);
			} else if (tapped == 1) {
				audio::play(SFX_CLICK);
				toMenu();
			}
			break;
		}
	}
}

static void debugLog(const Input& in) {
	static u32 frame = 0;
	frame++;
	if (frame % 60) return;
	fprintf(stderr, "[dbg] f=%lu fps=%.1f cpu=%.1f%% gpu=%.1f%% draws=%lu screen=%d mode=%d phase=%d score=%lu obj=%d part=%d touch=%d,%d blade=%.0f,%.0f\n",
	        (unsigned long)frame, sFps, C3D_GetProcessingTime() * 6.0f, C3D_GetDrawingTime() * 6.0f, (unsigned long)gfx::drawCalls,
	        (int)sScreen, (int)sGame.mode, (int)sGame.phase, (unsigned long)sGame.score, sGame.activeObjects(), sGame.activeParticles(),
	        in.tx, in.ty, sBlade.x, sBlade.y);
}

int main() {
	romfsInit();
	consoleDebugInit(debugDevice_SVC);  // stderr goes to svcOutputDebugString, visible in Azahar's log
	osSetSpeedupEnable(true);           // New 3DS: 804 MHz + L2 cache; no effect on Old 3DS

	bool assetsOk = gfx::init();
	saveLoad();
	bool audioOk = audio::init();
	aptHookCookie* cookie = &sAptCookie;
	aptHook(cookie, aptHook, nullptr);
	gRng.s = (u32)osGetTime() | 1;
	bool isNew3ds = false;
	APT_CheckNew3DS(&isNew3ds);
	fprintf(stderr, "[boot] Fruit Slicer %s assets=%d audio=%d new3ds=%d\n", VERSION_STRING, assetsOk, audioOk, isNew3ds);
	if (!assetsOk) {
		// Without romfs there is nothing to draw; bail out instead of crashing.
		gfx::shutdown();
		romfsExit();
		return 1;
	}
	if (gSave.scene != 0) gfx::setScene(gSave.scene);
	audio::music(MUSIC_MENU);
	sLastTick = osGetTime();

	bool wasTouching = false;
	while (aptMainLoop()) {
		hidScanInput();
		Input in{};
		in.down = hidKeysDown();
		in.held = hidKeysHeld();
		if (in.held & KEY_TOUCH) {
			touchPosition tp;
			hidTouchRead(&tp);
			in.touching = true;
			in.tx = tp.px;
			in.ty = tp.py;
		}
#ifdef AUTOTEST
		autotestInput(in);
		if (autotestDone()) break;
#else
		// START+SELECT from the menu quits, for builds launched from the Homebrew Launcher.
		if (sScreen == SCREEN_MENU && (in.held & KEY_START) && (in.held & KEY_SELECT)) break;
#endif

		updateFrame(in, wasTouching);
		wasTouching = in.touching;
		debugLog(in);

		gfx::beginFrame();
		for (int eye = 0; eye < 2; eye++) {
			if (gfx::beginTop(eye)) drawTop();
		}
		gfx::beginBottom();
		drawBottom();
		gfx::endFrame();

		u64 now = osGetTime();
		if (now > sLastTick) sFps = lerpf(sFps, 1000.0f / (float)(now - sLastTick), 0.1f);
		sLastTick = now;
	}

	aptUnhook(cookie);
	audio::shutdown();
	gfx::shutdown();
	romfsExit();
	return 0;
}
