#include "game.h"

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "bgbot.h"
#include "render.h"
#include "sprites.h"

Rng gRng;

const ModeInfo kModes[MODE_COUNT] = {
	{"Classic", "Three misses and you are out.", "A bomb ends the run."},
	{"Arcade", "60 seconds. Bombs cost 10 points.", "Grab power-ups. Big finale fruit!"},
	{"Zen", "90 calm seconds. No bombs.", "No misses. Just slice."},
	{"Blitz", "30 seconds. Bombs take 5 seconds.", "Finish on the big finale fruit!"},
	{"Sudden Death", "One miss or one bomb.", "Fruit is worth double."},
	{"Minefield", "Bombs everywhere. Misses are free.", "How long can you last?"},
	{"Combo Rush", "45 seconds. Doubled combo bonus.", "Finish on the big finale fruit!"},
};

enum CutClass { CUT_CRISP, CUT_JUICY, CUT_SOFT, CUT_MELON, CUT_SHELL };

struct FruitDef {
	int whole, left, right;
	u32 juice;
	CutClass cut;
};
static const FruitDef kFruit[] = {
	{sprites_apple_idx, sprites_apple_l_idx, sprites_apple_r_idx, 0xFF6AD8F5u, CUT_CRISP},
	{sprites_orange_idx, sprites_orange_l_idx, sprites_orange_r_idx, 0xFF2E9CFFu, CUT_JUICY},
	{sprites_lemon_idx, sprites_lemon_l_idx, sprites_lemon_r_idx, 0xFF52E6FFu, CUT_JUICY},
	{sprites_pear_idx, sprites_pear_l_idx, sprites_pear_r_idx, 0xFF8AEBDDu, CUT_CRISP},
	{sprites_plum_idx, sprites_plum_l_idx, sprites_plum_r_idx, 0xFF9A4AC2u, CUT_SOFT},
	{sprites_melon_idx, sprites_melon_l_idx, sprites_melon_r_idx, 0xFF4A3CF0u, CUT_MELON},
	{sprites_kiwi_idx, sprites_kiwi_l_idx, sprites_kiwi_r_idx, 0xFF52D69Au, CUT_SOFT},
	{sprites_berry_idx, sprites_berry_l_idx, sprites_berry_r_idx, 0xFF5A32F0u, CUT_SOFT},
	{sprites_peach_idx, sprites_peach_l_idx, sprites_peach_r_idx, 0xFF80B0FFu, CUT_SOFT},
	{sprites_pineapple_idx, sprites_pineapple_l_idx, sprites_pineapple_r_idx, 0xFF50E0FFu, CUT_JUICY},
	{sprites_coconut_idx, sprites_coconut_l_idx, sprites_coconut_r_idx, 0xFFF0F5FFu, CUT_SHELL},
};
static constexpr int kFruitKinds = sizeof(kFruit) / sizeof(kFruit[0]);
static constexpr int kCoconut = kFruitKinds - 1;
static const FruitDef kGold = {sprites_gold_idx, sprites_gold_l_idx, sprites_gold_r_idx, 0xFF40D8FFu, CUT_JUICY};

static constexpr float kSpawnY = 262.0f;
static constexpr float kKillY = 276.0f;
// Bomb and fruit radii are 19 and 23, so paths this far apart never visually touch.
static constexpr float kWantClearance = 62.0f;
static constexpr float kFeverGrace = 1.0f;    // seconds without a slice before the meter drains
static constexpr float kFeverDrain = 1.0f / 15.0f;  // full to empty in 15 seconds

u32 hueColor(float t) {
	t = t - floorf(t);
	float h = t * 6.0f;
	float f = h - floorf(h);
	u8 up = (u8)(f * 255), down = (u8)((1 - f) * 255);
	switch ((int)h % 6) {
		case 0: return rgba(255, up, 60);
		case 1: return rgba(down, 255, 60);
		case 2: return rgba(60, 255, up);
		case 3: return rgba(60, down, 255);
		case 4: return rgba(up, 60, 255);
		default: return rgba(255, 60, down);
	}
}

static int juiceCount(int base) {
	static const float kScale[] = {0.45f, 1.0f, 1.7f};
	return (int)(base * kScale[gSave.juice % 3]);
}

// ---------------------------------------------------------------- lifecycle
void Game::start(Mode m) {
	memset((void*)this, 0, sizeof(Game));
	mode = m;
	phase = PHASE_READY;
	best = gSave.best[m];
	gravity = 300;
	waveTimer = 0.2f;
	switch (m) {
		case MODE_CLASSIC: maxStrikes = 3; break;
		case MODE_ARCADE: timeLeft = 60; break;
		case MODE_ZEN: timeLeft = 90; break;
		case MODE_BLITZ: timeLeft = 30; break;
		case MODE_SUDDEN: maxStrikes = 1; break;
		case MODE_COMBO: timeLeft = 45; break;
		default: break;
	}
	audio::music(m == MODE_ZEN ? MUSIC_ZEN : MUSIC_PLAY);
}

float Game::difficulty() const {
	// Skill and time both push difficulty; each mode ramps at its own pace.
	float rampTime = 150, rampScore = 400;
	switch (mode) {
		case MODE_ARCADE: rampTime = 60; rampScore = 400; break;
		case MODE_ZEN: rampTime = 240; rampScore = 1200; break;
		case MODE_BLITZ: return clampf(0.6f + elapsed / 30.0f * 0.4f, 0, 1);
		case MODE_SUDDEN: rampTime = 120; rampScore = 300; break;
		case MODE_MINEFIELD: rampTime = 90; rampScore = 200; break;
		case MODE_COMBO: rampTime = 45; rampScore = 600; break;
		default: break;
	}
	return clampf(elapsed / rampTime + score / rampScore, 0, 1);
}

// ---------------------------------------------------------------- spawning
void Game::queue(float delay, float x, float vx, float vy, ObjKind kind, int fruit) {
	for (auto& p : pending) {
		if (!p.on) {
			p = {delay, x, vx, vy, kind, (u8)fruit, true};
			return;
		}
	}
}

void Game::launch(const Pending& p) {
	for (auto& o : objs) {
		if (o.on) continue;
		o = {};
		o.on = true;
		o.kind = p.kind;
		o.fruit = p.fruit;
		o.x = p.x;
		o.y = kSpawnY;
		o.vx = p.vx;
		o.vy = p.vy;
		o.rot = gRng.range(0, 6.28f);
		// Spin follows the direction of travel, like a tossed object.
		o.rotv = (p.vx >= 0 ? 1.0f : -1.0f) * gRng.range(1.2f, 4.2f);
		o.radius = p.kind == OBJ_BOMB ? 19 : 23;
		o.hits = (p.kind == OBJ_FRUIT && p.fruit == kCoconut) ? 2 : 1;
		audio::play(SFX_THROW, 0.3f, (p.x - 160) / 200.0f, gRng.range(0.9f, 1.15f));
		return;
	}
}

// Closest a throw comes to anything of the opposite type (bomb vs fruit), over its whole flight.
float Game::clearance(float x, float vx, float vy, float delay, bool bomb) const {
	float closest = 1e6f;
	for (float t = delay; t < delay + 2.8f; t += 0.14f) {
		float tt = t - delay;
		float px = x + vx * tt, py = kSpawnY + vy * tt + 0.5f * gravity * tt * tt;
		if (tt > 0.2f && py > kSpawnY) break;
		for (const auto& o : objs) {
			if (!o.on || (o.kind == OBJ_BOMB) == bomb) continue;
			float ox = o.x + o.vx * t, oy = o.y + o.vy * t + 0.5f * gravity * t * t;
			if (oy > kKillY) continue;
			float dx = ox - px, dy = oy - py, dist = dx * dx + dy * dy;
			if (dist < closest) closest = dist;
		}
		for (const auto& q : pending) {
			if (!q.on || (q.kind == OBJ_BOMB) == bomb || t < q.delay) continue;
			float qt = t - q.delay;
			float ox = q.x + q.vx * qt, oy = kSpawnY + q.vy * qt + 0.5f * gravity * qt * qt;
			if (oy > kKillY) continue;
			float dx = ox - px, dy = oy - py, dist = dx * dx + dy * dy;
			if (dist < closest) closest = dist;
		}
	}
	return closest >= 1e6f ? 1e3f : sqrtf(closest);
}

// Tries a few throws and keeps the one that stays furthest from the other type.
void Game::pickThrow(bool bomb, float delay, float& x, float& vx, float& vy) {
	float bestClear = -1;
#ifdef NO_BOMB_SPACING
	static constexpr int kTries = 1;
#else
	static constexpr int kTries = 7;
#endif
	for (int i = 0; i < kTries; i++) {
		float apexY = gRng.range(20, 90);
		float cvy = -sqrtf(2.0f * gravity * (kSpawnY - apexY));
		float cx = gRng.range(36, 284);
		float air = -2.0f * cvy / gravity;
		float cvx = clampf((gRng.range(50, 270) - cx) / air, -75, 75);
		float c = kTries > 1 ? clearance(cx, cvx, cvy, delay, bomb) : 0;
		if (c > bestClear) {
			bestClear = c;
			x = cx;
			vx = cvx;
			vy = cvy;
		}
		if (c >= kWantClearance) break;
	}
}

bool Game::bombAround() const {
	for (const auto& o : objs)
		if (o.on && o.kind == OBJ_BOMB) return true;
	for (const auto& q : pending)
		if (q.on && q.kind == OBJ_BOMB) return true;
	return false;
}

// Choreographed waves built for big combos. They always stay bomb-free.
bool Game::spawnPattern(float d) {
	if (bombAround()) return false;
	const float rise = 200;
	const float vy = -sqrtf(2.0f * gravity * rise);
	switch (gRng.below(4)) {
		case 0: {  // Row: rises level, so one straight swipe can take them all.
			int n = 4 + (int)(d * 3) + gRng.below(2);
			float gap = n > 1 ? 260.0f / (n - 1) : 0;
			float wobble = gRng.range(-14, 14);
			for (int i = 0; i < n; i++) queue(0, 30 + gap * i, wobble, vy, OBJ_FRUIT, gRng.below(kCoconut));
			break;
		}
		case 1: {  // Fountain: a fan from the middle.
			int n = 5 + (int)(d * 3);
			for (int i = 0; i < n; i++) {
				float t = n > 1 ? (float)i / (n - 1) : 0.5f;
				queue(0.07f * i, 160 + (t - 0.5f) * 40, lerpf(-85, 85, t), vy * gRng.range(0.92f, 1.0f), OBJ_FRUIT, gRng.below(kCoconut));
			}
			break;
		}
		case 2: {  // Crossfire: alternating from both edges.
			int n = 6 + (int)(d * 2);
			for (int i = 0; i < n; i++) {
				bool left = i % 2 == 0;
				queue(0.13f * i, left ? -14.0f : 334.0f, (left ? 1 : -1) * gRng.range(95, 130), vy * gRng.range(0.86f, 0.98f), OBJ_FRUIT, gRng.below(kCoconut));
			}
			break;
		}
		default: {  // Staircase: rising row, easy to catch with a diagonal stroke.
			int n = 5 + (int)(d * 2);
			for (int i = 0; i < n; i++) queue(0.1f * i, 34 + 252.0f * i / (n - 1), 0, vy, OBJ_FRUIT, gRng.below(kCoconut));
			break;
		}
	}
	return true;
}

void Game::spawnWave() {
	float d = difficulty();
#ifdef AUTOTEST
	statWaves++;
#endif
	// Low gravity gives long, floaty arcs with plenty of hang time near the top.
	gravity = lerpf(300, 430, d) * (mode == MODE_ZEN ? 0.9f : 1.0f);

	float patternChance = lerpf(0.10f, 0.28f, d);
	if (mode == MODE_COMBO) patternChance = 0.7f;
	if (mode == MODE_MINEFIELD || mode == MODE_SUDDEN) patternChance = 0.0f;
	if (elapsed > 6 && gRng.unit() < patternChance && spawnPattern(d)) {
		waveTimer = lerpf(3.1f, 2.1f, d);
		return;
	}

	float bombChance = 0;
	int maxExtra = (int)lerpf(1, 5, d);
	switch (mode) {
		case MODE_CLASSIC: bombChance = lerpf(0.05f, 0.24f, d); break;
		case MODE_ARCADE: bombChance = lerpf(0.08f, 0.20f, d); maxExtra += 1; break;
		case MODE_BLITZ: bombChance = 0.18f; maxExtra += 2; break;
		case MODE_SUDDEN: bombChance = lerpf(0.05f, 0.18f, d); maxExtra = (int)lerpf(0, 3, d); break;
		case MODE_MINEFIELD: bombChance = lerpf(0.38f, 0.62f, d); maxExtra += 1; break;
		case MODE_COMBO: bombChance = 0.10f; maxExtra += 2; break;
		default: break;
	}
	int count = 1 + gRng.below(maxExtra + 1);
	float stagger = gRng.below(3) == 0 ? lerpf(0.22f, 0.11f, d) : 0.0f;
	bool bombUsed = false;

	// Decide the types first, so fruit can route around bombs from the same wave.
	ObjKind kinds[8];
	if (count > 8) count = 8;
	for (int i = 0; i < count; i++) {
		ObjKind kind = OBJ_FRUIT;
		if (gRng.unit() < bombChance && !(bombUsed && mode != MODE_MINEFIELD)) {
			kind = OBJ_BOMB;
			bombUsed = true;
#ifdef AUTOTEST
		} else if (mode == MODE_ARCADE && gRng.unit() < 0.3f) {
			// Test builds cycle the power-ups so that every one gets exercised.
			static int nextPower = 0;
			kind = (ObjKind)(OBJ_FREEZE + nextPower);
			nextPower = (nextPower + 1) % 3;
#else
		} else if (mode == MODE_ARCADE && gRng.unit() < 0.075f) {
			kind = (ObjKind)(OBJ_FREEZE + gRng.below(3));
#endif
		} else if (gRng.unit() < 0.035f) {
			kind = OBJ_GOLD;
		}
		kinds[i] = kind;
	}
	// Bombs go first; every fruit is then placed against them.
	for (int pass = 0; pass < 2; pass++) {
		for (int i = 0; i < count; i++) {
			bool isBomb = kinds[i] == OBJ_BOMB;
			if ((pass == 0) != isBomb) continue;
			float x = 160, vx = 0, vy = -300;
			pickThrow(isBomb, stagger * i, x, vx, vy);
			int fruit = gRng.unit() < 0.09f ? kCoconut : gRng.below(kCoconut);
			queue(stagger * i, x, vx, vy, kinds[i], fruit);
		}
	}
	float interval = lerpf(2.1f, 0.95f, d);
	if (mode == MODE_BLITZ) interval *= 0.85f;
	if (mode == MODE_ZEN) interval *= 0.95f;
	waveTimer = interval + stagger * count;
}

// ---------------------------------------------------------------- effects
void Game::emitJuice(float x, float y, u32 color, int count, float dirX, float dirY) {
	for (int i = 0; i < count; i++) {
		Particle& p = parts[partCursor];
		partCursor = (partCursor + 1) % kMaxParticles;
		// Most drops spray along the stroke; the rest burst outward.
		float a, s;
		if (i % 3 != 0) {
			a = atan2f(dirY, dirX) + gRng.range(-0.55f, 0.55f);
			s = gRng.range(70, 240);
		} else {
			a = gRng.range(0, 6.28f);
			s = gRng.range(25, 110);
		}
		p = {x, y, cosf(a) * s, sinf(a) * s - 20, 0, gRng.range(0.3f, 0.65f), gRng.range(0.35f, 0.95f), 340, color, 0, true};
		p.life = p.maxLife;
	}
}

void Game::emitSparks(float x, float y, u32 color, int count, float speed) {
	for (int i = 0; i < count; i++) {
		Particle& p = parts[partCursor];
		partCursor = (partCursor + 1) % kMaxParticles;
		float a = gRng.range(0, 6.28f), s = gRng.range(speed * 0.3f, speed);
		p = {x, y, cosf(a) * s, sinf(a) * s, 0, gRng.range(0.35f, 0.8f), gRng.range(0.4f, 1.0f), 90, color, 1, true};
		p.life = p.maxLife;
	}
}

void Game::addPopup(const char* s, float x, float y, u32 color, float scale) {
	Popup& p = popups[popupCursor];
	popupCursor = (popupCursor + 1) % kMaxPopups;
	snprintf(p.text, sizeof p.text, "%s", s);
	p.x = clampf(x, 26, 294);
	p.y = clampf(y, 24, 220);
	p.life = 0.9f;
	p.color = color;
	p.scale = scale;
	p.on = true;
}

void Game::addRing(float x, float y, u32 color, float maxScale, float life) {
	Ring& r = rings[ringCursor];
	ringCursor = (ringCursor + 1) % kMaxRings;
	r = {x, y, life, life, maxScale, color, true};
}

void Game::setCallout(const char* s, u32 color, float size) {
	snprintf(callout, sizeof callout, "%s", s);
	fprintf(stderr, "[game] %s (score %lu)\n", callout, (unsigned long)score);
	calloutT = 1.4f;
	calloutColor = color;
	calloutSize = size;
}

// ---------------------------------------------------------------- scoring
void Game::award(int points) {
	if (mode == MODE_SUDDEN) points *= 2;
	if (doubleT > 0) points *= 2;
	if (feverT > 0) points *= 2;
	score += points;
	scorePulse = 1;
}

void Game::addFever(float amount) {
	if (feverT > 0 || phase != PHASE_PLAY) return;
	feverMeter += amount;
	feverIdle = 0;
	if (feverMeter >= 1) {
		feverMeter = 0;
		feverT = 7;
		feverTimer = 0;
		setCallout("FRUIT FEVER! X2", hueColor(0.1f), 0.86f);
		audio::play(SFX_FEVER, 1.0f);
		addRing(160, 120, rgba(255, 220, 90), 5, 0.7f);
		flash = 0.45f;
		flashColor = rgba(255, 210, 90);
	}
}

void Game::finishCombo() {
	int n = comboCount;
	comboCount = 0;
	comboTimer = 0;
	if (n < 3) return;

	// Every tier is louder, bigger and longer than the one before.
	struct Tier { int at; const char* name; Sfx sfx; int mult; u32 color; float size, slow, shakeAmount; int sparks, rings; };
	static const Tier kTiers[] = {
		{10, "LEGENDARY", SFX_COMBO10, 4, 0xFF40E0FFu, 1.0f, 0.9f, 0.55f, 80, 5},
		{8, "INSANE", SFX_COMBO8, 3, 0xFFE070FFu, 0.92f, 0.7f, 0.42f, 60, 4},
		{6, "SUPER", SFX_COMBO6, 2, 0xFFFFD060u, 0.84f, 0.4f, 0.25f, 40, 3},
		{5, "AWESOME", SFX_COMBO5, 2, 0xFF70F090u, 0.78f, 0.22f, 0.12f, 26, 2},
		{4, "GREAT", SFX_COMBO4, 1, 0xFF60D8FFu, 0.72f, 0.0f, 0.0f, 14, 1},
		{3, "NICE", SFX_COMBO3, 1, 0xFF5AD6FFu, 0.68f, 0.0f, 0.0f, 8, 1},
	};
	const Tier* t = &kTiers[5];
	for (const auto& candidate : kTiers) {
		if (n >= candidate.at) {
			t = &candidate;
			break;
		}
	}
	int bonus = n * t->mult;
	if (mode == MODE_COMBO) bonus *= 2;
	award(bonus);

	char buf[40];
	snprintf(buf, sizeof buf, "%s! %d COMBO +%d", t->name, n, bonus);
	setCallout(buf, t->color, t->size);
	audio::play(t->sfx);
	if (t->slow > 0) slowT = t->slow;
	if (t->shakeAmount > shake) shake = t->shakeAmount;
	for (int i = 0; i < t->rings; i++) addRing(comboX, comboY, hueColor(i * 0.18f), 2.0f + i * 1.3f, 0.35f + i * 0.12f);
	for (int i = 0; i < juiceCount(t->sparks); i++) {
		Particle& p = parts[partCursor];
		partCursor = (partCursor + 1) % kMaxParticles;
		float a = gRng.range(0, 6.28f), s = gRng.range(60, 300);
		p = {comboX, comboY, cosf(a) * s, sinf(a) * s - 40, 0, gRng.range(0.5f, 1.0f), gRng.range(0.5f, 1.1f), 200, hueColor(gRng.unit()), 1, true};
		p.life = p.maxLife;
	}
	if (n >= 8) {
		flash = 0.5f;
		flashColor = t->color;
	}
	snprintf(buf, sizeof buf, "+%d", bonus);
	addPopup(buf, comboX, comboY - 16, t->color, 0.7f + n * 0.05f);
	addFever(n * 0.035f);
	if (n > bestCombo) bestCombo = n;
}

void Game::addStrike(float x) {
	streak = 0;
	if (maxStrikes == 0 || feverT > 0) return;
	strikes++;
	strikePulse = 1;
	audio::play(SFX_STRIKE, 0.9f, (x - 160) / 200.0f);
	addPopup("MISS", x, 214, rgba(255, 90, 90), 0.62f);
	if (strikes >= maxStrikes) endGame();
}

void Game::sliceFruit(Obj& o, const Blade& blade) {
	const FruitDef& def = o.kind == OBJ_GOLD ? kGold : kFruit[o.fruit];
	float pan = (o.x - 160) / 200.0f;
	float dx = blade.x - blade.px, dy = blade.y - blade.py;
	float len = sqrtf(dx * dx + dy * dy);
	if (len < 0.01f) { dx = 1; dy = 0; len = 1; }
	dx /= len;
	dy /= len;

	if (o.hits > 1) {
		// Tough shell: the first hit only cracks it and knocks it back up.
		o.hits--;
		o.hitCooldown = 0.2f;
		o.vy -= 70;
		o.vx += dx * 40;
		o.rotv *= -1.4f;
		audio::play(SFX_KNOCK, 0.95f, pan, gRng.range(0.94f, 1.06f));
		emitSparks(o.x, o.y, rgba(255, 240, 190), juiceCount(10), 160);
		addRing(o.x, o.y, rgba(255, 240, 190), 1.3f, 0.2f);
		return;
	}

	// The half sprites are cut down their local Y axis; line that up with the stroke.
	float cutRot = atan2f(dy, dx) - kPi * 0.5f;
	float nx = -dy, ny = dx;
	// Sprite-local -X side must separate towards -normal after rotation.
	float lx = cosf(cutRot), ly = sinf(cutRot);
	if (lx * nx + ly * ny < 0) { nx = -nx; ny = -ny; }

	const float kick = 42;  // how fast the halves drift apart along the cut's normal
	for (int s = 0; s < 2; s++) {
		Half& h = halves[halfCursor];
		halfCursor = (halfCursor + 1) % kMaxHalves;
		float side = s == 0 ? -1.0f : 1.0f;
		h.on = true;
		h.x = o.x;
		h.y = o.y;
		// Halves keep the fruit's momentum, plus a nudge from the blade.
		h.vx = o.vx * 0.9f + nx * kick * side + dx * 30;
		h.vy = o.vy * 0.8f + ny * kick * side + dy * 30;
		h.rot = cutRot;
		h.rotv = side * gRng.range(1.5f, 4.0f);
		h.sprite = (short)(s == 0 ? def.left : def.right);
	}

	Splat& sp = splats[splatCursor];
	splatCursor = (splatCursor + 1) % kMaxSplats;
	sp = {o.x, o.y, gRng.range(0, 6.28f), gRng.range(0.7f, 1.05f), 3.2f, def.juice, (u8)gRng.below(3), true};
	emitJuice(o.x, o.y, def.juice, juiceCount(def.cut == CUT_MELON ? 22 : 14), dx, dy);

	// Two takes per fruit class, plus slight pitch variation, so repeat cuts don't sound identical.
	Sfx cut = SFX_CRISP0;
	switch (def.cut) {
		case CUT_CRISP: cut = (Sfx)(SFX_CRISP0 + gRng.below(2)); break;
		case CUT_JUICY: cut = (Sfx)(SFX_JUICY0 + gRng.below(2)); break;
		case CUT_SOFT: cut = (Sfx)(SFX_SOFT0 + gRng.below(2)); break;
		case CUT_MELON: cut = (Sfx)(SFX_MELON0 + gRng.below(2)); break;
		case CUT_SHELL: cut = SFX_SHELL; break;
	}
	audio::play(cut, 0.95f, pan, gRng.range(0.92f, 1.09f));

	int pts = o.kind == OBJ_GOLD ? 5 : (def.cut == CUT_SHELL ? 3 : 1);
	award(pts);
	sliced++;
	streak++;
	addFever(o.kind == OBJ_GOLD ? 0.1f : 0.02f);
	if (pts > 1) {
		char buf[8];
		snprintf(buf, sizeof buf, "+%d", pts);
		addPopup(buf, o.x, o.y - 18, rgba(255, 220, 90), 0.8f);
	}
	if (o.kind == OBJ_GOLD) {
		audio::play(SFX_GOLD, 0.9f, pan);
		emitSparks(o.x, o.y, rgba(255, 226, 120), juiceCount(14), 180);
	}
	if (streak % 15 == 0) {
		award(5);
		char buf[24];
		snprintf(buf, sizeof buf, "STREAK %d +5", streak);
		addPopup(buf, 160, 60, rgba(170, 240, 255), 0.6f);
	}

	if (comboStroke != blade.strokeId && comboCount > 0) finishCombo();
	comboStroke = blade.strokeId;
	comboCount++;
	comboTimer = 0.32f;
	comboX = o.x;
	comboY = o.y;
	if (comboCount >= 3) addRing(o.x, o.y, hueColor(comboCount * 0.11f), 1.2f + comboCount * 0.08f, 0.25f);
	o.on = false;
}

void Game::hitBomb(Obj& o) {
	o.on = false;
	flash = 1;
	flashColor = WHITE;
	shake = 0.6f;
	streak = 0;
	feverMeter *= 0.5f;
	emitSparks(o.x, o.y, rgba(255, 170, 60), juiceCount(42), 280);
	emitSparks(o.x, o.y, rgba(255, 240, 200), juiceCount(16), 180);
	addRing(o.x, o.y, rgba(255, 190, 90), 4, 0.45f);
	audio::play(SFX_BOOM, 1.0f, (o.x - 160) / 200.0f);
	switch (mode) {
		case MODE_ARCADE:
		case MODE_COMBO:
			score = score > 10 ? score - 10 : 0;
			addPopup("-10", o.x, o.y, rgba(255, 90, 90), 0.9f);
			setCallout("BOMB! -10", rgba(255, 110, 90));
			break;
		case MODE_BLITZ:
			addPopup("-5 SEC", o.x, o.y, rgba(255, 90, 90), 0.8f);
			if (timeLeft > 5) {
				timeLeft -= 5;
				setCallout("BOMB! -5 SECONDS", rgba(255, 110, 90));
			} else {
				timeLeft = 0;
				setCallout("TIME!", rgba(255, 230, 140));
				endGame();
			}
			break;
		default:
			setCallout("BOOM!", rgba(255, 110, 90));
			endGame();
			break;
	}
}

void Game::sliceObj(Obj& o, const Blade& blade) {
	float pan = (o.x - 160) / 200.0f;
	switch (o.kind) {
		case OBJ_BOMB: hitBomb(o); return;
		case OBJ_FRUIT:
		case OBJ_GOLD: sliceFruit(o, blade); return;
		case OBJ_FREEZE:
			freezeT = 6;
			audio::play(SFX_FREEZE, 1.0f, pan);
			setCallout("FROST! TIME SLOWS", rgba(150, 220, 255));
			break;
		case OBJ_DOUBLE:
			doubleT = 8;
			audio::play(SFX_POWER, 1.0f, pan);
			setCallout("DOUBLE POINTS!", rgba(255, 140, 190));
			break;
		case OBJ_FRENZY:
			frenzyT = 5;
			frenzyTimer = 0;
			audio::play(SFX_POWER, 1.0f, pan);
			setCallout("FRUIT STORM!", rgba(255, 180, 80));
			break;
	}
	emitSparks(o.x, o.y, WHITE, juiceCount(18), 200);
	addRing(o.x, o.y, WHITE, 2, 0.3f);
	o.on = false;
}

bool Game::hitTest(const Obj& o, const Blade& blade, float radius) const {
	float dx = blade.x - blade.px, dy = blade.y - blade.py;
	float len2 = dx * dx + dy * dy;
	float t = clampf(((o.x - blade.px) * dx + (o.y - blade.py) * dy) / len2, 0, 1);
	float qx = blade.px + dx * t - o.x, qy = blade.py + dy * t - o.y;
	return qx * qx + qy * qy <= radius * radius;
}

void Game::testSlices(Blade& blade) {
	if (!blade.active || blade.strokeStart) return;
	float dx = blade.x - blade.px, dy = blade.y - blade.py;
	if (dx * dx + dy * dy < 2.0f) return;  // a resting stylus never cuts
	if (blade.speed > 6 && blade.swishCooldown <= 0) {
		float pan = (blade.x - 160) / 200.0f;
		if (blade.speed > 17) audio::play(SFX_SHING, 0.34f, pan, gRng.range(0.95f, 1.08f));
		else audio::play((Sfx)(SFX_SWISH0 + gRng.below(3)), 0.5f, pan, gRng.range(0.9f, 1.12f));
		blade.swishCooldown = 0.2f;
	}
	// Fruit is tested first. If this movement cut a fruit, a nearby bomb only counts on a much
	// closer pass, so going for a fruit that's near a bomb is fair.
	bool cutFruit = false;
	for (auto& o : objs) {
		if (!o.on || o.kind == OBJ_BOMB || o.hitCooldown > 0) continue;
		if (hitTest(o, blade, o.radius)) {
			sliceObj(o, blade);
			cutFruit = true;
			if (phase != PHASE_PLAY) return;
		}
	}
	for (auto& o : objs) {
		if (!o.on || o.kind != OBJ_BOMB) continue;
		if (hitTest(o, blade, cutFruit ? 11.0f : o.radius)) {
			sliceObj(o, blade);
			if (phase != PHASE_PLAY) return;
		}
	}
}

// ---------------------------------------------------------------- finale fruit
static constexpr float kFinaleRise = 0.7f;
static constexpr float kFinaleWindow = 4.0f;
static constexpr float kFinaleRadius = 46.0f;
static constexpr float kFinaleTravelPerHit = 70.0f;

void Game::startFinale() {
	finishCombo();
	phase = PHASE_FINALE;
	phaseT = 0;
	finaleOn = true;
	finaleT = 0;
	finaleX = 160;
	finaleY = 300;
	finaleTravel = kFinaleTravelPerHit;  // the first touch counts straight away
	freezeT = doubleT = frenzyT = feverT = slowT = 0;
	for (auto& p : pending) p.on = false;
	for (auto& o : objs) {
		// Nothing should punish you here, so leftover bombs fizzle out.
		if (o.on && o.kind == OBJ_BOMB) {
			emitSparks(o.x, o.y, rgba(255, 190, 110), juiceCount(8), 110);
			o.on = false;
		}
	}
	audio::fuse(false);
	audio::play(SFX_FINALE_RISE);
	setCallout("TIME! SLICE THE SUNBURST!", rgba(255, 210, 90), 0.72f);
}

void Game::finaleHit(const Blade& blade) {
	finaleHits++;
	float pan = (finaleX - 160) / 200.0f;
	float dx = blade.x - blade.px, dy = blade.y - blade.py;
	float len = sqrtf(dx * dx + dy * dy);
	if (len < 0.01f) len = 1;
	dx /= len;
	dy /= len;

	award(1);
	finaleSwell = clampf(finaleHits * 0.007f, 0, 0.42f);
	finaleJoltX += dx * 7;
	finaleJoltY += dy * 7;
	scorePulse = 1;
	audio::play(SFX_FINALE_HIT, 0.85f, pan, clampf(0.8f + finaleHits * 0.022f, 0.8f, 2.0f));
	if (finaleHits % 3 == 0) audio::play((Sfx)(SFX_JUICY0 + gRng.below(2)), 0.55f, pan, gRng.range(0.95f, 1.2f));
	static const u32 kJuice[] = {0xFF2E9CFFu, 0xFF52E6FFu, 0xFF4A3CF0u};
	emitJuice(blade.x, blade.y, kJuice[gRng.below(3)], juiceCount(5), dx, dy);
	if (finaleHits >= 10) emitSparks(blade.x, blade.y, hueColor(finaleHits * 0.03f), juiceCount(2), 150);

	// Every ten hits is a bonus, and the milestones climb through the combo stingers.
	if (finaleHits % 10 == 0) {
		award(5);
		char buf[16];
		snprintf(buf, sizeof buf, "+5");
		addPopup(buf, blade.x, blade.y - 20, rgba(255, 226, 120), 0.8f);
		addRing(finaleX, finaleY, hueColor(finaleHits * 0.02f), 2.4f + finaleHits * 0.02f, 0.3f);
	}
	struct Stage { int at; const char* name; Sfx sfx; u32 color; float size; };
	static const Stage kStages[] = {
		{10, "NICE! 10 HITS", SFX_COMBO3, 0xFF5AD6FFu, 0.76f},
		{20, "AWESOME! 20 HITS", SFX_COMBO5, 0xFF70F090u, 0.82f},
		{30, "SUPER! 30 HITS", SFX_COMBO6, 0xFFFFD060u, 0.88f},
		{40, "INSANE! 40 HITS", SFX_COMBO8, 0xFFE070FFu, 0.94f},
		{60, "LEGENDARY! 60 HITS", SFX_COMBO10, 0xFF40E0FFu, 1.0f},
	};
	for (const auto& s : kStages) {
		if (finaleHits == s.at) {
			setCallout(s.name, s.color, s.size);
			audio::play(s.sfx, 0.8f);
			shake = 0.2f;
		}
	}
}

void Game::finaleBurst() {
	finaleOn = false;
	if (finaleHits == 0) {
		setCallout("IT GOT AWAY...", rgba(200, 190, 230));
		endGame();
		return;
	}
	int bonus = finaleHits * 2;
	award(bonus);
	if (finaleHits > bestCombo) bestCombo = finaleHits;
	char buf[40];
	snprintf(buf, sizeof buf, "SUNBURST! %d HITS +%d", finaleHits, bonus);
	setCallout(buf, rgba(255, 220, 90), 0.95f);
	calloutT = 2.2f;
	audio::play(SFX_FINALE_BURST);
	flash = 0.7f;
	flashColor = rgba(255, 220, 130);
	shake = 0.6f;
	for (int i = 0; i < 5; i++) addRing(finaleX, finaleY, hueColor(0.05f + i * 0.14f), 2.5f + i * 1.5f, 0.4f + i * 0.14f);
	emitJuice(finaleX, finaleY, 0xFF2E9CFFu, juiceCount(46), 0, -1);
	emitJuice(finaleX, finaleY, 0xFF52E6FFu, juiceCount(30), 0, 1);
	for (int i = 0; i < juiceCount(70); i++) {
		Particle& p = parts[partCursor];
		partCursor = (partCursor + 1) % kMaxParticles;
		float a = gRng.range(0, 6.28f), s = gRng.range(60, 340);
		p = {finaleX, finaleY, cosf(a) * s, sinf(a) * s - 50, 0, gRng.range(0.6f, 1.2f), gRng.range(0.5f, 1.1f), 220, hueColor(gRng.unit()), 1, true};
		p.life = p.maxLife;
	}
	Splat& sp = splats[splatCursor];
	splatCursor = (splatCursor + 1) % kMaxSplats;
	sp = {finaleX, finaleY, gRng.range(0, 6.28f), 2.1f, 3.2f, 0xFF2E9CFFu, (u8)gRng.below(3), true};
	endGame();
}

void Game::updateFinale(Blade& blade) {
	finaleT += kDt;
	updateObjects(kDt);  // leftover fruit falls away, with no strikes now
	float k = clampf(finaleT / kFinaleRise, 0, 1);
	finaleY = lerpf(300, 112, 1 - (1 - k) * (1 - k) * (1 - k));
	finaleJoltX *= 0.82f;
	finaleJoltY *= 0.82f;

	// Hits come from blade distance inside the fruit, so faster scrubbing really does score more.
	if (finaleT > kFinaleRise * 0.6f && blade.active && !blade.strokeStart) {
		float dx = blade.x - blade.px, dy = blade.y - blade.py;
		float len = sqrtf(dx * dx + dy * dy);
		Obj probe = {};
		probe.x = finaleX;
		probe.y = finaleY;
		if (len >= 1.5f && hitTest(probe, blade, kFinaleRadius + finaleSwell * 40)) {
			finaleTravel += len;
			if (blade.swishCooldown <= 0) {
				audio::play((Sfx)(SFX_SWISH0 + gRng.below(3)), 0.3f, 0, gRng.range(1.0f, 1.3f));
				blade.swishCooldown = 0.16f;
			}
			if (finaleTravel >= kFinaleTravelPerHit) {
				finaleTravel -= kFinaleTravelPerHit;
				finaleHit(blade);
			}
		}
	}
	if (finaleT >= kFinaleRise + kFinaleWindow) finaleBurst();
}

void Game::endGame() {
	if (phase != PHASE_PLAY && phase != PHASE_FINALE) return;
	phase = PHASE_DYING;
	phaseT = 0;
	audio::fuse(false);
	finishCombo();
}

// ---------------------------------------------------------------- simulation
void Game::updateObjects(float dt) {
	for (auto& o : objs) {
		if (!o.on) continue;
		o.vy += gravity * dt;
		o.x += o.vx * dt;
		o.y += o.vy * dt;
		o.rot += o.rotv * dt;
		o.age += dt;
		if (o.hitCooldown > 0) o.hitCooldown -= dt;
		if (o.vy > 0 && o.y > kKillY) {
			o.on = false;
			if ((o.kind == OBJ_FRUIT || o.kind == OBJ_GOLD) && phase == PHASE_PLAY) addStrike(o.x);
		}
	}
}

void Game::updateEffects(float dt) {
	for (auto& h : halves) {
		if (!h.on) continue;
		h.vy += gravity * dt;
		h.x += h.vx * dt;
		h.y += h.vy * dt;
		h.rot += h.rotv * dt;
		if (h.y > kKillY + 20) h.on = false;
	}
	for (auto& p : parts) {
		if (!p.on) continue;
		p.vy += p.gravity * dt;
		p.x += p.vx * dt;
		p.y += p.vy * dt;
		p.life -= dt;
		if (p.life <= 0) p.on = false;
	}
	for (auto& s : splats) {
		if (!s.on) continue;
		s.life -= dt;
		if (s.life <= 0) s.on = false;
	}
	for (auto& p : popups) {
		if (!p.on) continue;
		p.life -= dt;
		p.y -= 26 * dt;
		if (p.life <= 0) p.on = false;
	}
	for (auto& r : rings) {
		if (!r.on) continue;
		r.life -= dt;
		if (r.life <= 0) r.on = false;
	}
	if (flash > 0) flash -= dt * 2.4f;
	if (shake > 0) shake -= dt;
	if (calloutT > 0) calloutT -= dt;
	if (scorePulse > 0) scorePulse -= dt * 4;
	if (strikePulse > 0) strikePulse -= dt * 2.5f;
	float s = (shake > 0 && gSave.shake) ? shake * 9 : 0;
	gfx::setShake(s > 0 ? gRng.range(-s, s) : 0, s > 0 ? gRng.range(-s, s) : 0);
}

void Game::update(Blade& blade) {
	phaseT += kDt;
	updateEffects(kDt);

	if (phase == PHASE_READY) {
		if (phaseT >= 1.3f) {
			phase = PHASE_PLAY;
			phaseT = 0;
			audio::play(SFX_GO);
		}
		return;
	}

	if (phase == PHASE_PLAY) {
		elapsed += kDt;
		float objDt = freezeT > 0 ? kDt * 0.45f : kDt;
		if (slowT > 0) {
			objDt *= 0.3f;
			slowT -= kDt;
		}
		if (freezeT > 0) freezeT -= kDt;
		if (doubleT > 0) doubleT -= kDt;

		// The Fever meter drains unless you keep slicing.
		if (feverT <= 0) {
			feverIdle += kDt;
			if (feverIdle > kFeverGrace && feverMeter > 0) feverMeter = clampf(feverMeter - kFeverDrain * kDt, 0, 1);
		}

		if (timeLeft > 0) {
			timeLeft -= kDt;
			if (timeLeft <= 0) {
				timeLeft = 0;
				if (mode == MODE_ZEN) {
					setCallout("TIME!", rgba(255, 230, 140));
					endGame();
				} else {
					startFinale();  // Arcade, Blitz and Combo Rush end on the giant fruit
				}
			}
		}

		if (phase == PHASE_PLAY) {
			if (feverT > 0) {
				// Fever: a shower of bomb-free fruit, routed around anything already airborne.
				feverT -= kDt;
				feverTimer -= kDt;
				if (feverTimer <= 0) {
					feverTimer = 0.2f;
					float x = 160, vx = 0, vy = -300;
					pickThrow(false, 0, x, vx, vy);
					queue(0, x, vx, vy, OBJ_FRUIT, gRng.below(kCoconut));
				}
			} else if (frenzyT > 0) {
				frenzyT -= kDt;
				frenzyTimer -= kDt;
				if (frenzyTimer <= 0) {
					frenzyTimer = 0.16f;
					bool left = gRng.below(2);
					float x = left ? -18.0f : 338.0f;
					float vy = -gRng.range(270, 350);
					float vx = (left ? 1 : -1) * gRng.range(90, 150);
					// Try the other edge if this arc would cross a bomb.
					if (clearance(x, vx, vy, 0, false) < kWantClearance) {
						x = 320 - x;
						vx = -vx;
					}
					queue(0, x, vx, vy, OBJ_FRUIT, gRng.below(kCoconut));
				}
			} else {
				waveTimer -= objDt;
				if (waveTimer <= 0) spawnWave();
			}
			for (auto& p : pending) {
				if (!p.on) continue;
				p.delay -= objDt;
				if (p.delay <= 0) {
					launch(p);
					p.on = false;
				}
			}
		}
		updateObjects(objDt);
		if (phase == PHASE_PLAY) testSlices(blade);
		if (comboCount > 0) {
			comboTimer -= kDt;
			if (comboTimer <= 0 || !blade.active) finishCombo();
		}

		// The fuse sizzles whenever a bomb is in the air, panned towards it.
		const Obj* bomb = nullptr;
		for (const auto& o : objs)
			if (o.on && o.kind == OBJ_BOMB && o.y < 250) bomb = &o;
		audio::fuse(bomb != nullptr && phase == PHASE_PLAY, bomb ? (bomb->x - 160) / 200.0f : 0);

#ifdef AUTOTEST
		bool anyBomb = false, overlap = false;
		for (const auto& b : objs) {
			if (!b.on || b.kind != OBJ_BOMB || b.y > 240) continue;
			anyBomb = true;
			for (const auto& f : objs) {
				if (!f.on || f.kind == OBJ_BOMB || f.y > 240) continue;
				float dx = f.x - b.x, dy = f.y - b.y, r = f.radius + b.radius;
				if (dx * dx + dy * dy < r * r) overlap = true;
			}
		}
		statBombFrames += anyBomb;
		statOverlapFrames += overlap;
#endif
		return;
	}

	if (phase == PHASE_FINALE) {
		updateFinale(blade);
		return;
	}

	if (phase == PHASE_DYING) {
		updateObjects(kDt * 0.5f);
		if (phaseT >= (finaleHits > 0 ? 2.1f : 1.2f)) {
			phase = PHASE_OVER;
			phaseT = 0;
			if (score > gSave.best[mode]) {
				gSave.best[mode] = score;
				best = score;
				newBest = score > 0;
			}
			gSave.totalFruit += sliced;
			if ((u32)bestCombo > gSave.bestCombo) gSave.bestCombo = bestCombo;
			saveWrite();
			audio::play(newBest ? SFX_BEST : SFX_OVER);
		}
		return;
	}

	updateObjects(kDt);
}

int Game::activeObjects() const {
	int n = 0;
	for (const auto& o : objs) n += o.on;
	return n;
}
int Game::activeParticles() const {
	int n = 0;
	for (const auto& p : parts) n += p.on;
	return n;
}

const Obj* Game::botTarget(bool wantBomb) const {
	const Obj* bestO = nullptr;
	for (const auto& o : objs) {
		if (!o.on || o.y > 225 || o.y < 15 || o.x < 15 || o.x > 305) continue;
		if (o.hitCooldown > 0) continue;
		bool isBomb = o.kind == OBJ_BOMB;
		if (isBomb != wantBomb) continue;
		if (!bestO || o.y > bestO->y) bestO = &o;
	}
	return bestO;
}

// ---------------------------------------------------------------- drawing
void Game::drawField(const Blade& blade) const {
	gfx::image(SHEET_BGBOT, bgbot_board0_idx + gSave.board, -6, -6, 332, 252);
	if (freezeT > 0) gfx::rect(0, 0, 320, 240, rgba(120, 200, 255, 46));
	if (feverT > 0) gfx::rect(0, 0, 320, 240, withAlpha(hueColor(elapsed * 0.25f), 0.13f + sinf(elapsed * 7) * 0.03f));

	for (const auto& s : splats) {
		if (!s.on) continue;
		gfx::sprite(SHEET_SPRITES, sprites_splat0_idx + s.variant, s.x, s.y, s.scale, s.scale, s.rot,
		            withAlpha(s.color, clampf(s.life, 0, 1) * 0.75f));
	}
	for (const auto& h : halves) {
		if (h.on) gfx::sprite(SHEET_SPRITES, h.sprite, h.x, h.y, 1, 1, h.rot);
	}
	// Fruit first and bombs afterwards, so a bomb is never hidden behind a fruit.
	for (const auto& o : objs) {
		if (!o.on || o.kind == OBJ_BOMB) continue;
		switch (o.kind) {
			case OBJ_FRUIT:
				gfx::sprite(SHEET_SPRITES, kFruit[o.fruit].whole, o.x, o.y, 1, 1, o.rot);
				if (o.hits == 1 && o.fruit == kCoconut) gfx::sprite(SHEET_SPRITES, sprites_crack_idx, o.x, o.y, 1, 1, o.rot);
				break;
			case OBJ_GOLD: {
				float g = 1.1f + sinf(o.age * 9) * 0.1f;
				gfx::sprite(SHEET_SPRITES, sprites_glow_idx, o.x, o.y, g * 1.5f, g * 1.5f, 0, rgba(255, 220, 90, 200));
				gfx::sprite(SHEET_SPRITES, kGold.whole, o.x, o.y, 1, 1, o.rot);
				break;
			}
			case OBJ_FREEZE: gfx::sprite(SHEET_SPRITES, sprites_pw_freeze_idx, o.x, o.y, 1, 1, sinf(o.age * 3) * 0.3f); break;
			case OBJ_DOUBLE: gfx::sprite(SHEET_SPRITES, sprites_pw_double_idx, o.x, o.y, 1, 1, sinf(o.age * 3) * 0.3f); break;
			case OBJ_FRENZY: gfx::sprite(SHEET_SPRITES, sprites_pw_frenzy_idx, o.x, o.y, 1, 1, sinf(o.age * 3) * 0.3f); break;
			default: break;
		}
	}
	for (const auto& o : objs) {
		if (!o.on || o.kind != OBJ_BOMB) continue;
		float g = 1.25f + sinf(o.age * 14) * 0.25f;
		gfx::sprite(SHEET_SPRITES, sprites_glow_idx, o.x, o.y, g * 1.5f, g * 1.5f, 0, rgba(255, 50, 40, 190));
		gfx::sprite(SHEET_SPRITES, sprites_bomb_idx, o.x, o.y, 1, 1, o.rot);
	}
	if (finaleOn) {
		float urgency = clampf((finaleT - kFinaleRise) / kFinaleWindow, 0, 1);
		float g = 1.9f + finaleSwell * 3 + sinf(finaleT * (6 + urgency * 14)) * (0.12f + urgency * 0.2f);
		float s = 1.0f + finaleSwell + sinf(finaleT * 11) * 0.015f;
		float wobble = sinf(finaleT * 3.3f) * 0.1f + finaleJoltX * 0.012f;
		gfx::sprite(SHEET_SPRITES, sprites_glow_idx, finaleX, finaleY, g * 1.5f, g * 1.5f, 0, rgba(255, 190, 70, 170));
		gfx::sprite(SHEET_SPRITES, sprites_sunburst_idx, finaleX + finaleJoltX, finaleY + finaleJoltY, s, s, wobble);
		// Time left, as a bar along the top of the field.
		if (finaleT > kFinaleRise) {
			float left = 1 - (finaleT - kFinaleRise) / kFinaleWindow;
			gfx::rect(30, 8, 260, 8, rgba(16, 10, 34, 190));
			gfx::rect(30, 8, 260 * clampf(left, 0, 1), 8, left < 0.3f ? rgba(255, 90, 80) : rgba(255, 210, 90));
		}
	}
	for (const auto& r : rings) {
		if (!r.on) continue;
		float t = 1 - r.life / r.maxLife;
		float s = 0.2f + t * r.maxScale;
		gfx::sprite(SHEET_SPRITES, sprites_ring_idx, r.x, r.y, s, s, 0, withAlpha(r.color, (1 - t) * 0.9f));
	}
	for (const auto& p : parts) {
		if (!p.on) continue;
		float life = p.life / p.maxLife;
		gfx::sprite(SHEET_SPRITES, p.sprite ? sprites_spark_idx : sprites_drop_idx, p.x, p.y, p.size, p.size, 0,
		            withAlpha(p.color, clampf(life * 1.6f, 0, 1)));
	}
	blade.draw();
	for (const auto& p : popups) {
		if (!p.on) continue;
		gfx::text(p.text, p.x, p.y - 12, p.scale, withAlpha(p.color, clampf(p.life * 2.5f, 0, 1)), ALIGN_CENTER);
	}
	if (flash > 0) gfx::rect(0, 0, 320, 240, withAlpha(flashColor, clampf(flash, 0, 1)));
}

void Game::drawHud() const {
	char buf[32];
	// Status panel plane.
	gfx::setDepth(0.5f);
	gfx::image(SHEET_SPRITES, sprites_panel_idx, 10, 8, 380, 62, rgba(24, 16, 44, 165));

	// Score: closest plane.
	gfx::setDepth(1.3f);
	float pulse = 1.0f + clampf(scorePulse, 0, 1) * 0.16f;
	snprintf(buf, sizeof buf, "%lu", (unsigned long)score);
	gfx::text(buf, 24, 14 - (pulse - 1) * 20, 1.25f * pulse, feverT > 0 ? hueColor(elapsed * 0.6f) : rgba(255, 226, 120));

	gfx::setDepth(0.9f);
	gfx::text(kModes[mode].name, 386, 12, 0.5f, rgba(255, 246, 226), ALIGN_RIGHT);
	snprintf(buf, sizeof buf, "BEST %lu", (unsigned long)(score > best ? score : best));
	gfx::text(buf, 386, 30, 0.45f, rgba(200, 190, 230), ALIGN_RIGHT);

	if (maxStrikes > 0) {
		for (int i = 0; i < maxStrikes; i++) {
			bool used = i < strikes;
			float s = 0.7f + (used && i == strikes - 1 ? clampf(strikePulse, 0, 1) * 0.6f : 0);
			gfx::sprite(SHEET_SPRITES, used ? sprites_pip_on_idx : sprites_pip_off_idx,
			            378 - (maxStrikes - 1 - i) * 24, 57, s, s);
		}
	}
	if (timeLeft > 0 || mode == MODE_ARCADE || mode == MODE_ZEN || mode == MODE_BLITZ || mode == MODE_COMBO) {
		int secs = (int)ceilf(timeLeft);
		snprintf(buf, sizeof buf, "%d:%02d", secs / 60, secs % 60);
		bool urgent = secs <= 10 && phase == PHASE_PLAY;
		float s = urgent ? 0.62f + 0.06f * sinf(elapsed * 12) : 0.56f;
		gfx::text(buf, 386, 48, s, urgent ? rgba(255, 110, 100) : rgba(255, 246, 226), ALIGN_RIGHT);
	}

	// Streak and combo sit to the right of the score, stacked so they never overlap it.
	gfx::setDepth(0.7f);
	snprintf(buf, sizeof buf, "%lu", (unsigned long)score);
	float px = 24 + gfx::textWidth(buf, 1.25f) + 14;
	if (streak >= 5) {
		char line[20];
		snprintf(line, sizeof line, "STREAK %d", streak);
		gfx::text(line, px, 16, 0.42f, rgba(170, 240, 255));
	}
	if (comboCount >= 2) {
		char line[20];
		snprintf(line, sizeof line, "COMBO X%d", comboCount);
		gfx::text(line, px, 36, 0.42f + clampf(comboCount * 0.015f, 0, 0.12f), hueColor(comboCount * 0.09f));
	}

	// Fever meter.
	gfx::setDepth(0.6f);
	const float bx = 24, by = 74, bw = 352;
	gfx::rect(bx, by, bw, 7, rgba(16, 10, 34, 170));
	if (feverT > 0) {
		gfx::rect(bx, by, bw * clampf(feverT / 7, 0, 1), 7, hueColor(elapsed * 0.7f));
		gfx::text("FEVER X2", 200, by + 7, 0.42f, hueColor(elapsed * 0.7f), ALIGN_CENTER);
	} else {
		bool draining = feverIdle > kFeverGrace && feverMeter > 0;
		gfx::rect(bx, by, bw * clampf(feverMeter, 0, 1), 7, draining ? rgba(190, 150, 90) : rgba(255, 196, 70));
	}

	// Active power-up meters.
	struct Meter { float t, max; int spr; };
	const Meter meters[] = {{freezeT, 6, sprites_pw_freeze_idx}, {doubleT, 8, sprites_pw_double_idx}, {frenzyT, 5, sprites_pw_frenzy_idx}};
	float mx = 24;
	for (const auto& m : meters) {
		if (m.t <= 0) continue;
		gfx::sprite(SHEET_SPRITES, m.spr, mx + 8, 96, 0.42f, 0.42f);
		gfx::rect(mx + 22, 92, 60, 8, rgba(0, 0, 0, 120));
		gfx::rect(mx + 22, 92, 60 * clampf(m.t / m.max, 0, 1), 8, rgba(255, 246, 226));
		mx += 94;
	}

	if (phase == PHASE_FINALE && finaleHits > 0) {
		gfx::setDepth(1.9f);
		snprintf(buf, sizeof buf, "%d", finaleHits);
		float s = 1.5f + clampf(finaleHits * 0.012f, 0, 0.7f);
		gfx::text(buf, 200, 136, s * 0.85f, hueColor(finaleHits * 0.025f), ALIGN_CENTER);
	}

	if (phase == PHASE_READY) {
		gfx::setDepth(1.6f);
		bool go = phaseT > 0.9f;
		float s = go ? 1.8f : 1.2f + 0.1f * sinf(phaseT * 8);
		gfx::text(go ? "GO!" : "READY", 200, 118 - s * 20, s, go ? rgba(140, 240, 150) : rgba(255, 230, 140), ALIGN_CENTER);
	} else if (calloutT > 0) {
		gfx::setDepth(1.6f);
		float in = clampf((1.4f - calloutT) * 8, 0, 1);
		float s = calloutSize + (1 - in) * 0.5f;
		// Long combo lines must always fit on the top screen.
		float w = gfx::textWidth(callout, s);
		if (w > 380) s *= 380 / w;
		gfx::text(callout, 200, phase == PHASE_FINALE ? 88 : 120, s, withAlpha(calloutColor, clampf(calloutT * 2.5f, 0, 1)), ALIGN_CENTER);
	}
}
