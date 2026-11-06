#include "game.h"

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "render.h"
#include "sprites.h"

Rng gRng;

const ModeInfo kModes[MODE_COUNT] = {
	{"Classic", "Three misses and you are out.", "A bomb ends the run."},
	{"Arcade", "60 seconds. Bombs cost 10 points.", "Grab power-ups to score big."},
	{"Zen", "90 calm seconds. No bombs.", "No misses. Just slice."},
	{"Blitz", "30 seconds at full speed.", "Bombs take 5 seconds away."},
	{"Sudden Death", "One miss or one bomb.", "Fruit is worth double."},
	{"Minefield", "Bombs everywhere. Misses are free.", "How long can you last?"},
};

struct FruitDef {
	int whole, left, right;
	u32 juice;
};
static const FruitDef kFruit[] = {
	{sprites_apple_idx, sprites_apple_l_idx, sprites_apple_r_idx, 0xFF6AD8F5u},
	{sprites_orange_idx, sprites_orange_l_idx, sprites_orange_r_idx, 0xFF2E9CFFu},
	{sprites_lemon_idx, sprites_lemon_l_idx, sprites_lemon_r_idx, 0xFF52E6FFu},
	{sprites_pear_idx, sprites_pear_l_idx, sprites_pear_r_idx, 0xFF8AEBDDu},
	{sprites_plum_idx, sprites_plum_l_idx, sprites_plum_r_idx, 0xFF9A4AC2u},
	{sprites_melon_idx, sprites_melon_l_idx, sprites_melon_r_idx, 0xFF4A3CF0u},
	{sprites_kiwi_idx, sprites_kiwi_l_idx, sprites_kiwi_r_idx, 0xFF52D69Au},
	{sprites_berry_idx, sprites_berry_l_idx, sprites_berry_r_idx, 0xFF5A32F0u},
};
static constexpr int kFruitKinds = sizeof(kFruit) / sizeof(kFruit[0]);
static const FruitDef kGold = {sprites_gold_idx, sprites_gold_l_idx, sprites_gold_r_idx, 0xFF40D8FFu};

static const u32 kBladeColors[] = {0xFF50D8FFu, 0xFFFFD060u, 0xFFA070FFu, 0xFF70F090u};

static constexpr float kSpawnY = 262.0f;
static constexpr float kKillY = 276.0f;

// ---------------------------------------------------------------- lifecycle
void Game::start(Mode m) {
	memset((void*)this, 0, sizeof(Game));
	mode = m;
	phase = PHASE_READY;
	best = gSave.best[m];
	gravity = 400;
	waveTimer = 0.2f;
	switch (m) {
		case MODE_CLASSIC: maxStrikes = 3; break;
		case MODE_ARCADE: timeLeft = 60; break;
		case MODE_ZEN: timeLeft = 90; break;
		case MODE_BLITZ: timeLeft = 30; break;
		case MODE_SUDDEN: maxStrikes = 1; break;
		case MODE_MINEFIELD: break;
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
		o.radius = p.kind == OBJ_BOMB ? 22 : 23;
		audio::play(SFX_THROW, 0.35f, (p.x - 160) / 200.0f);
		return;
	}
}

void Game::spawnWave() {
	float d = difficulty();
	// Low gravity gives long, floaty arcs with plenty of hang time near the top.
	gravity = lerpf(300, 430, d) * (mode == MODE_ZEN ? 0.9f : 1.0f);

	float bombChance = 0;
	int maxExtra = (int)lerpf(1, 5, d);
	switch (mode) {
		case MODE_CLASSIC: bombChance = lerpf(0.05f, 0.24f, d); break;
		case MODE_ARCADE: bombChance = lerpf(0.08f, 0.20f, d); maxExtra += 1; break;
		case MODE_BLITZ: bombChance = 0.18f; maxExtra += 2; break;
		case MODE_SUDDEN: bombChance = lerpf(0.05f, 0.18f, d); maxExtra = (int)lerpf(0, 3, d); break;
		case MODE_MINEFIELD: bombChance = lerpf(0.38f, 0.62f, d); maxExtra += 1; break;
		default: break;
	}
	int count = 1 + gRng.below(maxExtra + 1);
	float stagger = gRng.below(3) == 0 ? lerpf(0.22f, 0.11f, d) : 0.0f;
	bool bombUsed = false;

	for (int i = 0; i < count; i++) {
		float apexY = gRng.range(20, 90);
		float vy = -sqrtf(2.0f * gravity * (kSpawnY - apexY));
		float x = gRng.range(36, 284);
		float air = -2.0f * vy / gravity;
		float landX = gRng.range(50, 270);
		float vx = clampf((landX - x) / air, -75, 75);
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
		queue(stagger * i, x, vx, vy, kind, gRng.below(kFruitKinds));
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

void Game::setCallout(const char* s, u32 color) {
	snprintf(callout, sizeof callout, "%s", s);
	fprintf(stderr, "[game] %s (score %lu)\n", callout, (unsigned long)score);
	calloutT = 1.4f;
	calloutColor = color;
}

// ---------------------------------------------------------------- scoring
void Game::award(int points) {
	if (mode == MODE_SUDDEN) points *= 2;
	if (doubleT > 0) points *= 2;
	score += points;
	scorePulse = 1;
}

void Game::finishCombo() {
	if (comboCount >= 3) {
		int bonus = comboCount;
		award(bonus);
		char buf[40];
		snprintf(buf, sizeof buf, "%d FRUIT COMBO +%d", comboCount, bonus);
		setCallout(buf, rgba(255, 214, 90));
		audio::play(comboCount >= 5 ? SFX_COMBO5 : comboCount == 4 ? SFX_COMBO4 : SFX_COMBO3);
		if (comboCount > bestCombo) bestCombo = comboCount;
	}
	comboCount = 0;
	comboTimer = 0;
}

void Game::addStrike(float x) {
	streak = 0;
	if (maxStrikes == 0) return;
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
	emitJuice(o.x, o.y, def.juice, 14, dx, dy);
	audio::play((Sfx)(SFX_SQUISH0 + gRng.below(3)), 0.85f, pan);

	int pts = o.kind == OBJ_GOLD ? 5 : 1;
	award(pts);
	sliced++;
	streak++;
	if (o.kind == OBJ_GOLD) {
		audio::play(SFX_GOLD, 0.9f, pan);
		addPopup("+5", o.x, o.y - 18, rgba(255, 220, 90), 0.8f);
		emitSparks(o.x, o.y, rgba(255, 226, 120), 14, 180);
	}
	if (streak > 0 && streak % 15 == 0) {
		award(5);
		char buf[24];
		snprintf(buf, sizeof buf, "STREAK %d +5", streak);
		addPopup(buf, 160, 60, rgba(170, 240, 255), 0.6f);
	}

	if (comboStroke != blade.strokeId && comboCount > 0) finishCombo();
	comboStroke = blade.strokeId;
	comboCount++;
	comboTimer = 0.32f;
	o.on = false;
}

void Game::hitBomb(Obj& o) {
	o.on = false;
	flash = 1;
	shake = 0.6f;
	streak = 0;
	emitSparks(o.x, o.y, rgba(255, 170, 60), 42, 280);
	emitSparks(o.x, o.y, rgba(255, 240, 200), 16, 180);
	audio::play(SFX_BOOM, 1.0f, (o.x - 160) / 200.0f);
	switch (mode) {
		case MODE_ARCADE:
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
	emitSparks(o.x, o.y, WHITE, 18, 200);
	o.on = false;
}

void Game::testSlices(Blade& blade) {
	if (!blade.active || blade.strokeStart) return;
	float dx = blade.x - blade.px, dy = blade.y - blade.py;
	float len2 = dx * dx + dy * dy;
	if (len2 < 2.0f) return;  // a resting stylus never cuts
	if (blade.speed > 7 && blade.swishCooldown <= 0) {
		audio::play((Sfx)(SFX_SWISH0 + gRng.below(3)), 0.5f, (blade.x - 160) / 200.0f);
		blade.swishCooldown = 0.2f;
	}
	for (auto& o : objs) {
		if (!o.on) continue;
		float t = clampf(((o.x - blade.px) * dx + (o.y - blade.py) * dy) / len2, 0, 1);
		float qx = blade.px + dx * t - o.x, qy = blade.py + dy * t - o.y;
		if (qx * qx + qy * qy <= o.radius * o.radius) {
			sliceObj(o, blade);
			if (phase != PHASE_PLAY) return;
		}
	}
}

void Game::endGame() {
	if (phase != PHASE_PLAY) return;
	phase = PHASE_DYING;
	phaseT = 0;
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
	if (flash > 0) flash -= dt * 2.4f;
	if (shake > 0) shake -= dt;
	if (calloutT > 0) calloutT -= dt;
	if (scorePulse > 0) scorePulse -= dt * 4;
	if (strikePulse > 0) strikePulse -= dt * 2.5f;
	float s = shake > 0 ? shake * 9 : 0;
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
		if (freezeT > 0) freezeT -= kDt;
		if (doubleT > 0) doubleT -= kDt;

		if (timeLeft > 0) {
			timeLeft -= kDt;
			if (timeLeft <= 0) {
				timeLeft = 0;
				setCallout("TIME!", rgba(255, 230, 140));
				endGame();
			}
		}

		if (phase == PHASE_PLAY) {
			if (frenzyT > 0) {
				frenzyT -= kDt;
				frenzyTimer -= kDt;
				if (frenzyTimer <= 0) {
					frenzyTimer = 0.16f;
					bool left = gRng.below(2);
					float x = left ? -18.0f : 338.0f;
					float vx = (left ? 1 : -1) * gRng.range(90, 150);
					float vy = -gRng.range(270, 350);
					queue(0, x, vx, vy, OBJ_FRUIT, gRng.below(kFruitKinds));
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
		return;
	}

	if (phase == PHASE_DYING) {
		updateObjects(kDt * 0.5f);
		if (phaseT >= 1.2f) {
			phase = PHASE_OVER;
			phaseT = 0;
			if (score > gSave.best[mode]) {
				gSave.best[mode] = score;
				best = score;
				newBest = score > 0;
				saveWrite();
			}
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
		bool isBomb = o.kind == OBJ_BOMB;
		if (isBomb != wantBomb) continue;
		if (!bestO || o.y > bestO->y) bestO = &o;
	}
	return bestO;
}

// ---------------------------------------------------------------- drawing
void Game::drawField(const Blade& blade) const {
	gfx::image(SHEET_BGBOT, 0, -6, -6, 332, 252);
	if (freezeT > 0) gfx::rect(0, 0, 320, 240, rgba(120, 200, 255, 46));

	for (const auto& s : splats) {
		if (!s.on) continue;
		gfx::sprite(SHEET_SPRITES, sprites_splat0_idx + s.variant, s.x, s.y, s.scale, s.scale, s.rot,
		            withAlpha(s.color, clampf(s.life, 0, 1) * 0.75f));
	}
	for (const auto& h : halves) {
		if (h.on) gfx::sprite(SHEET_SPRITES, h.sprite, h.x, h.y, 1, 1, h.rot);
	}
	for (const auto& o : objs) {
		if (!o.on) continue;
		switch (o.kind) {
			case OBJ_FRUIT: gfx::sprite(SHEET_SPRITES, kFruit[o.fruit].whole, o.x, o.y, 1, 1, o.rot); break;
			case OBJ_GOLD: {
				float g = 1.1f + sinf(o.age * 9) * 0.1f;
				gfx::sprite(SHEET_SPRITES, sprites_glow_idx, o.x, o.y, g * 1.5f, g * 1.5f, 0, rgba(255, 220, 90, 200));
				gfx::sprite(SHEET_SPRITES, kGold.whole, o.x, o.y, 1, 1, o.rot);
				break;
			}
			case OBJ_BOMB: {
				float g = 1.25f + sinf(o.age * 14) * 0.25f;
				gfx::sprite(SHEET_SPRITES, sprites_glow_idx, o.x, o.y, g * 1.5f, g * 1.5f, 0, rgba(255, 50, 40, 190));
				gfx::sprite(SHEET_SPRITES, sprites_bomb_idx, o.x, o.y, 1, 1, o.rot);
				break;
			}
			case OBJ_FREEZE: gfx::sprite(SHEET_SPRITES, sprites_pw_freeze_idx, o.x, o.y, 1, 1, sinf(o.age * 3) * 0.3f); break;
			case OBJ_DOUBLE: gfx::sprite(SHEET_SPRITES, sprites_pw_double_idx, o.x, o.y, 1, 1, sinf(o.age * 3) * 0.3f); break;
			case OBJ_FRENZY: gfx::sprite(SHEET_SPRITES, sprites_pw_frenzy_idx, o.x, o.y, 1, 1, sinf(o.age * 3) * 0.3f); break;
		}
	}
	for (const auto& p : parts) {
		if (!p.on) continue;
		float life = p.life / p.maxLife;
		gfx::sprite(SHEET_SPRITES, p.sprite ? sprites_spark_idx : sprites_drop_idx, p.x, p.y, p.size, p.size, 0,
		            withAlpha(p.color, clampf(life * 1.6f, 0, 1)));
	}
	blade.draw(kBladeColors[gSave.blade % 4]);
	for (const auto& p : popups) {
		if (!p.on) continue;
		gfx::text(p.text, p.x, p.y - 12, p.scale, withAlpha(p.color, clampf(p.life * 2.5f, 0, 1)), ALIGN_CENTER);
	}
	if (flash > 0) gfx::rect(0, 0, 320, 240, rgba(255, 255, 255, (u8)(clampf(flash, 0, 1) * 255)));
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
	gfx::text(buf, 24, 14 - (pulse - 1) * 20, 1.25f * pulse, rgba(255, 226, 120));

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
	if (timeLeft > 0 || mode == MODE_ARCADE || mode == MODE_ZEN || mode == MODE_BLITZ) {
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
		gfx::text(line, px, 36, 0.42f, rgba(255, 214, 90));
	}

	// Active power-up meters.
	struct Meter { float t, max; int spr; };
	const Meter meters[] = {{freezeT, 6, sprites_pw_freeze_idx}, {doubleT, 8, sprites_pw_double_idx}, {frenzyT, 5, sprites_pw_frenzy_idx}};
	float mx = 24;
	gfx::setDepth(0.6f);
	for (const auto& m : meters) {
		if (m.t <= 0) continue;
		gfx::sprite(SHEET_SPRITES, m.spr, mx + 8, 88, 0.42f, 0.42f);
		gfx::rect(mx + 22, 84, 60, 8, rgba(0, 0, 0, 120));
		gfx::rect(mx + 22, 84, 60 * clampf(m.t / m.max, 0, 1), 8, rgba(255, 246, 226));
		mx += 94;
	}

	if (phase == PHASE_READY) {
		gfx::setDepth(1.6f);
		bool go = phaseT > 0.9f;
		float s = go ? 1.8f : 1.2f + 0.1f * sinf(phaseT * 8);
		gfx::text(go ? "GO!" : "READY", 200, 118 - s * 20, s, go ? rgba(140, 240, 150) : rgba(255, 230, 140), ALIGN_CENTER);
	} else if (calloutT > 0) {
		gfx::setDepth(1.6f);
		float in = clampf((1.4f - calloutT) * 8, 0, 1);
		float s = 0.72f + (1 - in) * 0.5f;
		gfx::text(callout, 200, 120, s, withAlpha(calloutColor, clampf(calloutT * 2.5f, 0, 1)), ALIGN_CENTER);
	}
}
