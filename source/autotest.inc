// Test-only bot, compiled with `make AUTOTEST=1`. It presses what a player presses: all of its
// input goes through the same Input struct as the hardware. It plays every mode, then quits.
#ifdef BOT_ONLY
static int sBotMode = BOT_ONLY;
#else
static int sBotMode = 0;
#endif
static int sBotPhaseFrames = 0;
static int sBotStroke = 0;
static float sBotX, sBotY, sBotDX, sBotDY;
static int sBotTapFrames = 0;
static bool sBotDone = false;
static int sBotTapX, sBotTapY;
static u32 sBotModeFrames = 0;
#ifdef BOT_ONLY
static bool sBotOptionsDone = true;
#else
static bool sBotOptionsDone = false;
#endif
static int sBotOptionsStep = 0;

static bool autotestDone() { return sBotDone; }

static void botTap(Input& in, const Button& b) {
	sBotTapX = (int)(b.x + b.w / 2);
	sBotTapY = (int)(b.y + b.h / 2);
	sBotTapFrames = 8;
	(void)in;
}

static void autotestInput(Input& in) {
	in = Input{};
	sBotPhaseFrames++;
	static Screen lastScreen = SCREEN_MENU;
	if (sScreen != lastScreen) {
		lastScreen = sScreen;
		sBotPhaseFrames = 0;
	}

	if (sBotTapFrames > 0) {
		sBotTapFrames--;
		if (sBotTapFrames > 2) {
			in.touching = true;
			in.tx = sBotTapX;
			in.ty = sBotTapY;
		}
		return;
	}

	switch (sScreen) {
		case SCREEN_MENU:
			if (sBotPhaseFrames < 45) return;
			sBotPhaseFrames = 0;
			if (!sBotOptionsDone) {
				botTap(in, kMenuButtons[7]);
				return;
			}
#ifdef BOT_ONLY
			if (sBotMode != BOT_ONLY) {
#else
			if (sBotMode >= MODE_COUNT) {
#endif
				sBotDone = true;
				fprintf(stderr, "[bot] all modes finished\n");
				return;
			}
			fprintf(stderr, "[bot] starting %s\n", kModes[sBotMode].name);
			sBotModeFrames = 0;
			botTap(in, kMenuButtons[sBotMode]);
			return;

		case SCREEN_OPTIONS: {
			if (sBotPhaseFrames < 24) return;
			sBotPhaseFrames = 0;
			// Cycle each setting all the way round, so the save ends where it started.
			static const u8 kSeq[] = {
				OPT_BOARD, OPT_BOARD, OPT_BOARD, OPT_BOARD, OPT_BOARD, OPT_BOARD,
				OPT_SCENE, OPT_SCENE, OPT_SCENE,
				OPT_BLADE, OPT_BLADE, OPT_BLADE, OPT_BLADE, OPT_BLADE, OPT_BLADE,
				OPT_JUICE, OPT_JUICE, OPT_JUICE, OPT_DEPTH, OPT_DEPTH, OPT_DEPTH,
				OPT_SHAKE, OPT_SHAKE,
				OPT_SOUND, OPT_SOUND, OPT_SOUND, OPT_SOUND, OPT_SOUND,
				OPT_MUSIC, OPT_MUSIC, OPT_MUSIC, OPT_MUSIC, OPT_MUSIC,
			};
			constexpr int kSteps = sizeof kSeq / sizeof kSeq[0];
			if (sBotOptionsStep < kSteps) {
				botTap(in, kOptButtons[kSeq[sBotOptionsStep++]]);
				return;
			}
			fprintf(stderr, "[bot] options exercised blade=%d board=%d scene=%d sfx=%d music=%d\n", gSave.blade, gSave.board, gSave.scene, gSave.sfx, gSave.music);
			sBotOptionsDone = true;
			botTap(in, kOptButtons[OPT_BACK]);
			return;
		}

		case SCREEN_RESULT:
			if (sBotPhaseFrames < 80) return;
			sBotPhaseFrames = 0;
			fprintf(stderr, "[bot] result mode=%s score=%lu sliced=%d combo=%d best=%lu newBest=%d strikes=%d waves=%lu bombFrames=%lu overlapFrames=%lu finaleHits=%d\n", kModes[sGame.mode].name,
			        (unsigned long)sGame.score, sGame.sliced, sGame.bestCombo, (unsigned long)sGame.best, sGame.newBest, sGame.strikes,
			        (unsigned long)sGame.statWaves, (unsigned long)sGame.statBombFrames, (unsigned long)sGame.statOverlapFrames, sGame.finaleHits);
			{
				// Azahar buffers its log, so measurement runs read the result from the SD card.
				FILE* f = fopen("sdmc:/3ds/FruitSlicer/bot.txt", "a");
				if (f) {
					fprintf(f, "%s waves=%lu bombFrames=%lu overlapFrames=%lu\n", kModes[sGame.mode].name, (unsigned long)sGame.statWaves, (unsigned long)sGame.statBombFrames, (unsigned long)sGame.statOverlapFrames);
					fclose(f);
				}
			}
			sBotMode++;
			botTap(in, kResultButtons[1]);
			return;

		case SCREEN_PAUSE:
			if (sBotPhaseFrames < 40) return;
			sBotPhaseFrames = 0;
			fprintf(stderr, "[bot] resume from pause\n");
			botTap(in, kPauseButtons[0]);
			return;

		case SCREEN_GAME:
			break;
	}

	sBotModeFrames++;
	if (sBotModeFrames == 600) {
		in.down |= KEY_START;  // exercise the pause path once per mode
		sBotPhaseFrames = 0;
		return;
	}
	if (sBotModeFrames % 2400 == 0) fprintf(stderr, "[bot] t=%lus score=%lu\n", (unsigned long)(sBotModeFrames / 60), (unsigned long)sGame.score);

	if (sGame.phase == PHASE_FINALE) {
		// Scrub back and forth over the finale fruit, like a player would.
		in.touching = true;
		in.tx = (int)(160 + sinf(sBotModeFrames * 0.2f) * 75);
		in.ty = 112 + (int)(cosf(sBotModeFrames * 0.31f) * 18);
		return;
	}
#ifdef BOT_ONLY
	// Measurement run: keep hands off so every throw flies its whole path, then end the game.
	if (sBotModeFrames < 7200) return;
	bool suicide = true;
#else
	// After 25 seconds, go looking for bombs so the bomb and game-over paths get covered.
	// Timed modes stay alive instead, so that their finale always gets exercised.
	bool suicide = sBotModeFrames > 1500 && sGame.timeLeft <= 0;
#endif
	if (sBotStroke > 0) {
		sBotStroke--;
		sBotX += sBotDX;
		sBotY += sBotDY;
		in.touching = true;
		in.tx = (int)clampf(sBotX, 1, 318);
		in.ty = (int)clampf(sBotY, 1, 238);
		return;
	}
	if (sBotPhaseFrames % 4 != 0) return;
	const Obj* t = sGame.botTarget(suicide);
	if (!t && suicide && sGame.mode != MODE_ZEN) {
		// Stop slicing entirely so strikes can end strike-based modes too.
		return;
	}
	if (!t) t = sGame.botTarget(false);
	if (!t) return;
	// A short horizontal stroke through the target, staying clear of nearby bombs.
	int inRow = 0;
	bool bombInRow = false;
	if (!suicide) {
		for (const auto& o : sGame.objs) {
			if (!o.on) continue;
			float dx = o.x - t->x, dy = o.y - t->y;
			if (o.kind == OBJ_BOMB) {
				if (dx * dx + dy * dy < 60 * 60) return;
				if (fabsf(dy) < 50) bombInRow = true;
			} else if (fabsf(dy) < 22) {
				inRow++;
			}
		}
	}
	sBotY = t->y + t->vy * kDt * 3;
	sBotDY = 0;
	if (inRow >= 3 && !bombInRow) {
		// Several fruit level with each other: one long sweep to earn a combo.
		sBotX = 6;
		sBotDX = 26;
		sBotStroke = 12;
	} else {
		sBotX = t->x - 30;
		sBotDX = 15;
		sBotStroke = 4;
	}
}
