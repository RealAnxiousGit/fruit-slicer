// Orchard Blast's menus during a run: the level-up cards, picking a slot for a charm, and the Inventory.
// Included by main.cpp, so it can use its sign and plank helpers. Everything here is drawn and tapped in
// field coordinates, like the blade.

static bool sOrcInv;          // the Inventory is open (it holds the run, like the pause menu)
static int sOrcInvFocus;      // 0 Back to the run, 1 Swap Slots
static float sOrcOpenT;       // seconds the cards have been up: a short lock-out stops a swipe picking one
static bool sOrcSlotTouch;    // this touch began on a charm slot, so it never becomes a slice
static int sOrcCardDown = -1;
static int sOrcHowTo;         // a how-to panel is up: 0 none, 1 the run, 2 the first boss
static float sOrcHowToT;
static void drawOrchardHowTo();

static u32 rarityColor(int r) {
	return r == RARE_LEGEND ? rgba(255, 205, 80) : (r == RARE_RARE ? rgba(120, 190, 255) : rgba(220, 210, 190));
}
static const char* rarityName(int r) { return r == RARE_LEGEND ? "Legendary" : (r == RARE_RARE ? "Rare" : "Common"); }

// Text that shrinks to fit a width.
static void orcFit(const char* s, float x, float y, float scale, float maxW, u32 color, Align align = ALIGN_CENTER) {
	const float w = gfx::textWidth(s, scale);
	if (w > maxW && w > 0) scale *= maxW / w;
	gfx::text(s, x, y, scale, color, align);
}

static void orcCardRect(int i, int n, float* x, float* y, float* w, float* h) {
	const float gap = 8, top = kFieldTop + 36;
	float cw = (kFieldW - 24 - gap * (n - 1)) / n;
	if (cw > 130) cw = 130;
	float ch = kFieldH - top - 52;
	if (ch > 170) ch = 170;
	const float total = cw * n + gap * (n - 1);
	*x = (kFieldW - total) * 0.5f + i * (cw + gap);
	*y = top;
	*w = cw;
	*h = ch;
}
static Button orcRerollButton() { return {kFieldW * 0.5f - 60, kFieldH - 42.0f, 120, 32, "Reroll"}; }
static Button orcKeepButton() { return {kFieldW * 0.5f - 70, kFieldH - 42.0f, 140, 32, "Keep Mine"}; }

static void drawOrcCard(int card, float x, float y, float w, float h, bool focused) {
	const Orchard& o = sGame.orc;
	const CardInfo& info = kCards[card];
	const u32 rc = rarityColor(info.rarity);
	gfx::sign(sprites_plank_idx, x, y, w, h, focused ? COL_ACCENT : COL_PLANK);
	gfx::rect(x + 4, y + 4, w - 8, 4, rc);
	float ty = y + 12;
	orcFit(rarityName(info.rarity), x + w * 0.5f, ty, 0.24f, w - 10, rc);
	ty += 12;
	orcFit(cardKindName(info.kind), x + w * 0.5f, ty, 0.24f, w - 10, COL_DIM);
	ty += 16;
	orcFit(info.name, x + w * 0.5f, ty, 0.4f, w - 10, COL_CREAM);
	ty += 24;
	orcFit(info.line1, x + w * 0.5f, ty, 0.26f, w - 10, COL_CREAM);
	ty += 13;
	if (info.line2[0]) orcFit(info.line2, x + w * 0.5f, ty, 0.26f, w - 10, COL_CREAM);
	ty += 18;
	char buf[48];
	if (info.kind == KIND_UPGRADE) {
		cardNextLine(card, o.upgrade[card], buf, sizeof buf);
		orcFit(buf, x + w * 0.5f, ty, 0.24f, w - 10, rgba(170, 230, 255));
		ty += 13;
		if (o.upgrade[card]) snprintf(buf, sizeof buf, "You have level %d of %d", o.upgrade[card], info.maxLevel);
		else snprintf(buf, sizeof buf, "Up to level %d", info.maxLevel);
		orcFit(buf, x + w * 0.5f, ty, 0.22f, w - 10, COL_DIM);
	} else if (info.kind == KIND_REUSABLE) {
		if (info.lasts > 0) snprintf(buf, sizeof buf, "Lasts %d s, ready again %d s", (int)info.lasts, (int)info.cooldown);
		else snprintf(buf, sizeof buf, "Ready again after %d s", (int)info.cooldown);
		orcFit(buf, x + w * 0.5f, ty, 0.22f, w - 10, rgba(170, 230, 255));
	} else if (info.kind == KIND_SINGLE) {
		orcFit("Goes in a slot. Used once.", x + w * 0.5f, ty, 0.22f, w - 10, rgba(170, 230, 255));
	}
}

static void drawOrcSlotBox(int s, float x, float y, float w, float h, bool focused) {
	const int card = sGame.orc.slot[s];
	gfx::sign(sprites_plank_idx, x, y, w, h, focused ? COL_ACCENT : COL_PLANK);
	char buf[24];
	snprintf(buf, sizeof buf, "Slot %d", s + 1);
	gfx::text(buf, x + w * 0.5f, y + 6, 0.26f, COL_DIM, ALIGN_CENTER);
	orcFit(card == CARD_NONE ? "Empty" : kCards[card].name, x + w * 0.5f, y + h * 0.5f - 6, 0.36f, w - 10, COL_CREAM);
}

static void orcPlaceRect(int s, float* x, float* y, float* w, float* h) {
	*w = 130;
	*h = 70;
	*x = kFieldW * 0.5f + (s == 0 ? -*w - 8 : 8);
	*y = kFieldTop + 80;
}

static void drawOrchardMenus() {
	if (sGame.mode != MODE_ORCHARD) return;
	if (sOrcHowTo) {
		drawOrchardHowTo();
		return;
	}
	const Orchard& o = sGame.orc;
	char buf[64];
	if (o.choosing) {
		gfx::rect(kFillX, kFillX, kFillW, kFillW, rgba(20, 12, 8, 170));
		if (o.placing) {
			snprintf(buf, sizeof buf, "Pick a slot for %s", kCards[o.placingCard].name);
			orcFit(buf, kFieldW * 0.5f, kFieldTop + 30, 0.5f, kFieldW - 20, COL_CREAM);
			orcFit("It replaces the charm in that slot.", kFieldW * 0.5f, kFieldTop + 56, 0.3f, kFieldW - 20, COL_DIM);
			for (int s = 0; s < kSlots; s++) {
				float x, y, w, h;
				orcPlaceRect(s, &x, &y, &w, &h);
				drawOrcSlotBox(s, x, y, w, h, o.focus == s);
			}
			const Button k = orcKeepButton();
			drawSign(k, k.label, o.focus == 2, false, 0.5f);
			return;
		}
		snprintf(buf, sizeof buf, "LEVEL %d! PICK A CARD", o.level - o.levelsWaiting + 1);
		orcFit(buf, kFieldW * 0.5f, kFieldTop + 8, 0.6f, kFieldW - 20, rgba(255, 230, 140));
		for (int i = 0; i < o.offerCount; i++) {
			float x, y, w, h;
			orcCardRect(i, o.offerCount, &x, &y, &w, &h);
			drawOrcCard(o.offer[i], x, y, w, h, o.focus == i);
		}
		if (o.rerolls > 0) {
			const Button r = orcRerollButton();
			snprintf(buf, sizeof buf, "Reroll (%d)", o.rerolls);
			drawSign(r, buf, o.focus == o.offerCount, false, 0.46f);
		}
		return;
	}
	if (!sOrcInv) return;
	// The Inventory: what the run has, and the stats it adds up to.
	gfx::rect(kFillX, kFillX, kFillW, kFillW, rgba(20, 12, 8, 190));
	gfx::text("INVENTORY", kFieldW * 0.5f, kFieldTop + 4, 0.6f, rgba(255, 230, 140), ALIGN_CENTER);
	const float colW = (kFieldW - 30) * 0.5f;
	const float lx = 12, rx = 18 + colW;
	float y = kFieldTop + 34;
	snprintf(buf, sizeof buf, "Room %d of %d   Level %d", o.room > kOrchardRooms ? kOrchardRooms : o.room, kOrchardRooms, o.level);
	orcFit(buf, lx, y, 0.32f, colW, COL_CREAM, ALIGN_LEFT);
	y += 16;
	snprintf(buf, sizeof buf, "HP %d of %d   XP %d of %d", (int)ceilf(o.hp), (int)o.maxHp, (int)o.xp, sGame.orcXpNeed());
	orcFit(buf, lx, y, 0.3f, colW, COL_CREAM, ALIGN_LEFT);
	y += 20;
	gfx::text("Upgrades", lx, y, 0.34f, rgba(255, 210, 150));
	y += 16;
	bool any = false;
	for (int c = 0; c < CARD_COUNT; c++) {
		if (kCards[c].kind != KIND_UPGRADE || !o.upgrade[c]) continue;
		any = true;
		snprintf(buf, sizeof buf, "%s %d", kCards[c].name, o.upgrade[c]);
		orcFit(buf, lx + 6, y, 0.28f, colW - 6, COL_CREAM, ALIGN_LEFT);
		y += 13;
		if (y > kFieldH - 60) break;
	}
	if (!any) {
		gfx::text("None yet", lx + 6, y, 0.28f, COL_DIM);
		y += 13;
	}
	// Right column: passive charms, then the slots.
	float ry = kFieldTop + 34;
	gfx::text("Passive charms", rx, ry, 0.34f, rgba(255, 210, 150));
	ry += 16;
	if (o.passiveCount == 0) {
		gfx::text("None yet", rx + 6, ry, 0.28f, COL_DIM);
		ry += 13;
	}
	for (int i = 0; i < o.passiveCount && ry < kFieldH - 110; i++) {
		orcFit(kCards[o.passiveOrder[i]].name, rx + 6, ry, 0.28f, colW - 6, COL_CREAM, ALIGN_LEFT);
		ry += 13;
	}
	ry += 8;
	gfx::text("Charm slots", rx, ry, 0.34f, rgba(255, 210, 150));
	ry += 16;
	for (int s = 0; s < kSlots; s++) {
		const int card = o.slot[s];
		if (card == CARD_NONE) snprintf(buf, sizeof buf, "%d: Empty", s + 1);
		else if (o.slotCool[s] > 0) snprintf(buf, sizeof buf, "%d: %s (%ds)", s + 1, kCards[card].name, (int)ceilf(o.slotCool[s]));
		else snprintf(buf, sizeof buf, "%d: %s", s + 1, kCards[card].name);
		orcFit(buf, rx + 6, ry, 0.28f, colW - 6, card == CARD_NONE ? COL_DIM : COL_CREAM, ALIGN_LEFT);
		ry += 13;
		if (card != CARD_NONE) {
			orcFit(kCards[card].line1, rx + 14, ry, 0.24f, colW - 14, COL_DIM, ALIGN_LEFT);
			ry += 12;
		}
	}
	const Button back = {kFieldW * 0.5f - 130, kFieldH - 42.0f, 120, 32, "Back to Run"};
	const Button swap = {kFieldW * 0.5f + 10, kFieldH - 42.0f, 120, 32, "Swap Slots"};
	drawSign(back, back.label, sOrcInvFocus == 0, false, 0.44f);
	drawSign(swap, swap.label, sOrcInvFocus == 1, false, 0.44f);
}

// How-to panels, once each: the first run, and the first boss. They hold the run until tapped away.
static void drawOrchardHowTo() {
	if (!sOrcHowTo) return;
	const char* slots = PORT_3DS ? "L and R" : (gStickInUse ? "LB and RB" : "Q and E");
	const char* inv = PORT_3DS ? "Select" : (gStickInUse ? "Back" : "Tab");
	char a[64], b[64];
	const char* lines[6];
	const char* title;
	int n = 0;
	if (sOrcHowTo == 1) {
		title = "ORCHARD BLAST";
		lines[n++] = "Ten rooms. A boss waits in rooms 5 and 10.";
		lines[n++] = "Bombs and bosses cost HP. Dropped fruit never do.";
		lines[n++] = "Slice fruit for XP. Each level up, pick a card.";
		snprintf(a, sizeof a, "Charms sit in two slots: tap them, or use %s.", slots);
		lines[n++] = a;
		snprintf(b, sizeof b, "%s opens your Inventory.", inv);
		lines[n++] = b;
	} else {
		title = "BOSS FIGHT";
		lines[n++] = "A blue ring means its guard is up.";
		lines[n++] = "Slicing it then costs you HP: Deflected!";
		lines[n++] = "A gold ring means the guard is down. Slice!";
		lines[n++] = "After a slice it's confused for a moment.";
		lines[n++] = "Slice its shots before they land.";
	}
	gfx::rect(kFillX, kFillX, kFillW, kFillW, rgba(20, 12, 8, 190));
	const float w = kFieldW - 24 > 380 ? 380 : kFieldW - 24, x = (kFieldW - w) * 0.5f;
	const float h = 40 + n * 20 + 34, y = kFieldTop + (kFieldH - kFieldTop - h) * 0.5f;
	gfx::sign(sprites_plank_idx, x, y, w, h, COL_PLANK);
	orcFit(title, kFieldW * 0.5f, y + 8, 0.56f, w - 16, rgba(255, 230, 140));
	for (int i = 0; i < n; i++) orcFit(lines[i], kFieldW * 0.5f, y + 40 + i * 20, 0.3f, w - 16, COL_CREAM);
	if (sOrcHowToT > 0.6f) orcFit(PORT_3DS ? "Tap or press A" : "Tap, click or press A", kFieldW * 0.5f, y + h - 24, 0.28f, w - 16, COL_DIM);
}

// True while a how-to panel is up.
static bool orchardHowTo(const Input& in, bool wasTouching) {
	const Orchard& o = sGame.orc;
	if (!sOrcHowTo && !o.choosing && !sOrcInv) {
		if (!(gSave.howToSeen & 1)) sOrcHowTo = 1;
		else if (o.boss.on && !o.boss.beaten && o.boss.enterT <= 0 && !(gSave.howToSeen & 2)) sOrcHowTo = 2;
		if (sOrcHowTo) {
			sOrcHowToT = 0;
			sBlade.reset();
			audio::fuse(false);
		}
	}
	if (!sOrcHowTo) return false;
	sOrcHowToT += kDt;
	if (sOrcHowToT > 0.6f && ((in.touching && !wasTouching) || (in.down & (KEY_A | KEY_B | KEY_START | KEY_SELECT)))) {
		audio::click();
		gSave.howToSeen |= sOrcHowTo == 1 ? 1 : 2;
		sOrcHowTo = 0;
		sGame.orcResume();
	}
	return true;
}

static bool tapped(const Input& in, bool wasTouching, float x, float y, float w, float h) {
	return in.touching && !wasTouching && in.tx >= x && in.tx < x + w && in.ty >= y && in.ty < y + h;
}

// Runs the Orchard menus for the game screen. True when they took the frame, so the blade and game input wait.
static bool updateOrchardMenus(const Input& in, bool wasTouching) {
	if (sGame.mode != MODE_ORCHARD || sGame.phase != PHASE_PLAY) {
		sOrcInv = false;
		return false;
	}
	if (orchardHowTo(in, wasTouching)) return true;
	Orchard& o = sGame.orc;
	if (o.choosing) {
		sOrcInv = false;
		sOrcOpenT += kDt;
		if (sOrcOpenT < 0.45f) return true;
		if (o.placing) {
			int pick = -2;
			for (int s = 0; s < kSlots; s++) {
				float x, y, w, h;
				orcPlaceRect(s, &x, &y, &w, &h);
				if (tapped(in, wasTouching, x, y, w, h)) pick = s;
			}
			const Button k = orcKeepButton();
			if (in.touching && !wasTouching && hit(k, in.tx, in.ty)) pick = -1;
			if (in.down & KEY_LEFT) o.focus = o.focus == 0 ? 2 : o.focus - 1;
			if (in.down & KEY_RIGHT) o.focus = (o.focus + 1) % 3;
			if (in.down & KEY_A) pick = o.focus == 2 ? -1 : o.focus;
			if (in.down & KEY_B) pick = -1;
			if (pick != -2) {
				sGame.orcPlace(pick);
				sOrcOpenT = 0.2f;
			}
			return true;
		}
		const int n = o.offerCount;
		const int last = o.rerolls > 0 ? n : n - 1;  // the Reroll sign is one more stop for the keys
		if (in.down & KEY_LEFT) o.focus = o.focus <= 0 ? last : o.focus - 1;
		if (in.down & KEY_RIGHT) o.focus = o.focus >= last ? 0 : o.focus + 1;
		if (in.down & KEY_DOWN) o.focus = last;
		if (in.down & KEY_UP && o.focus >= n) o.focus = 0;
		int pick = -1;
		bool reroll = false;
		// A tap picks on release over the same card, so a press that slides off cancels.
		if (in.touching && !wasTouching) {
			sOrcCardDown = -1;
			for (int i = 0; i < n; i++) {
				float x, y, w, h;
				orcCardRect(i, n, &x, &y, &w, &h);
				if (in.tx >= x && in.tx < x + w && in.ty >= y && in.ty < y + h) sOrcCardDown = o.focus = i;
			}
			if (o.rerolls > 0 && hit(orcRerollButton(), in.tx, in.ty)) reroll = true;
		} else if (!in.touching && wasTouching && sOrcCardDown >= 0) {
			pick = sOrcCardDown;
			sOrcCardDown = -1;
		}
#if PORT_MOUSE
		if (in.hovering)
			for (int i = 0; i < n; i++) {
				float x, y, w, h;
				orcCardRect(i, n, &x, &y, &w, &h);
				if (in.tx >= x && in.tx < x + w && in.ty >= y && in.ty < y + h) o.focus = i;
			}
#endif
		if (in.down & KEY_A) {
			if (o.focus >= n) reroll = true;
			else pick = o.focus;
		}
		if (in.down & (KEY_X | KEY_Y)) reroll = true;
		if (reroll && sGame.orcCanReroll()) {
			audio::click();
			sGame.orcReroll();
			sOrcOpenT = 0.25f;
		} else if (pick >= 0 && pick < n) {
			sGame.orcPick(pick);
			sOrcOpenT = 0.2f;
		}
		return true;
	}
	sOrcOpenT = 0;
	// Select (Tab or I on a keyboard) opens and closes the Inventory.
	if (in.down & KEY_SELECT) {
		audio::click();
		if (sOrcInv) {
			sOrcInv = false;
			sGame.orcResume();
		} else {
			sOrcInv = true;
			sOrcInvFocus = 0;
			sBlade.reset();
			audio::fuse(false);
		}
		return true;
	}
	if (sOrcInv) {
		const Button back = {kFieldW * 0.5f - 130, kFieldH - 42.0f, 120, 32, "Back to Run"};
		const Button swap = {kFieldW * 0.5f + 10, kFieldH - 42.0f, 120, 32, "Swap Slots"};
		if (in.down & (KEY_LEFT | KEY_RIGHT)) sOrcInvFocus ^= 1;
		bool close = (in.down & KEY_B) || (in.touching && !wasTouching && hit(back, in.tx, in.ty)) || ((in.down & KEY_A) && sOrcInvFocus == 0);
		if ((in.touching && !wasTouching && hit(swap, in.tx, in.ty)) || ((in.down & KEY_A) && sOrcInvFocus == 1)) {
			audio::click();
			sGame.orcSwapSlots();
		}
		if (close) {
			audio::click();
			sOrcInv = false;
			sGame.orcResume();
		}
		return true;
	}
	// Charm slots: L and R (Q and E on a keyboard), or a tap on the slot. A touch that starts on a slot
	// waits: let go quickly without moving and the charm fires; move away and it becomes a slice after all.
	static int slotDown = -1;
	static float slotT, slotX, slotY;
	if (sGame.orchardHeld()) {
		sOrcSlotTouch = false;
		return false;
	}
	if (in.down & KEY_L) sGame.orcUse(0);
	if (in.down & KEY_R) sGame.orcUse(1);
	if (in.touching && !wasTouching) {
		for (int s = 0; s < kSlots; s++) {
			float x, y, w, h;
			Game::orcSlotRect(s, &x, &y, &w, &h);
			if (in.tx >= x && in.tx < x + w && in.ty >= y && in.ty < y + h && o.slot[s] != CARD_NONE) {
				slotDown = s;
				slotT = 0;
				slotX = (float)in.tx;
				slotY = (float)in.ty;
				sOrcSlotTouch = true;
			}
		}
	}
	if (sOrcSlotTouch) {
		slotT += kDt;
		if (in.touching) {
			if (fabsf(in.tx - slotX) + fabsf(in.ty - slotY) > 14) sOrcSlotTouch = false;  // a swipe after all
		} else {
			if (slotT < 0.4f && slotDown >= 0) sGame.orcUse(slotDown);
			sOrcSlotTouch = false;
		}
		if (!sOrcSlotTouch) slotDown = -1;
	}
	return false;
}
