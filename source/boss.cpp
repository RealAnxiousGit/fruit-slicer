// Orchard Blast's bosses: The Big Melon (room 5) and The Bomb King (room 10).
// A boss attacks on its own clock, so Armor and Max HP matter. It can only be sliced while its guard
// is down; slicing the guard costs a little HP ("Deflected!"). A clean slice leaves it confused for a
// few seconds, then the guard goes back up.
#include <math.h>
#include <stdio.h>

#include "audio.h"
#include "fruit.h"
#include "fruitids.h"
#include "game.h"
#include "model3d.h"
#include "render.h"
#include "sprites.h"

namespace {
enum BossAttack : u8 {
	ATK_NONE,
	MEL_SEEDS,   // Seed Spit: a spray of seeds; slice them before they land
	MEL_FLOP,    // Belly Flop: jumps and slams down, splashing juice. Guard drops when it lands
	MEL_CHARGE,  // Rolling Charge: rolls at the screen. Swipe it to block. Guard drops after
	KING_VOLLEY, // Cannon Volley: cannonballs that take two swipes. Guard drops after
	KING_LOB,    // Royal Lob: real bombs, and fruit that turn into bombs
	KING_CROWN,  // Crown Toss: the crown flies out and back. Guard is down while it's away
	KING_FUSE,   // Short Fuse: its fuse is lit; snuff it with three swipes, or it blows
};

constexpr float kCrownLife = 3.2f;

float segDist2(float px, float py, const Blade& b) {
	const float dx = b.x - b.px, dy = b.y - b.py;
	const float len2 = dx * dx + dy * dy;
	const float t = len2 > 0 ? clampf(((px - b.px) * dx + (py - b.py) * dy) / len2, 0, 1) : 0;
	const float qx = b.px + dx * t - px, qy = b.py + dy * t - py;
	return qx * qx + qy * qy;
}

int bossDamage(int base) {
	const int d = (int)(base * orchardLevel(gSave.difficulty).bossDamage + 0.5f);
	return d < 1 ? 1 : d;
}

float fieldMidY() { return kFieldTop + (kFieldH - kFieldTop) * 0.36f; }
}  // namespace

void Game::bossStart(int id) {
	Boss& b = orc.boss;
	b = {};
	b.id = (u8)id;
	b.on = true;
	b.guard = true;
	b.phase = 1;
	b.left = kBosses[id].slices;
	b.radius = (id == BOSS_MELON ? 50.0f : 44.0f) * (PORT_3DS ? 0.85f : 1.0f);
	b.x = kFieldW * 0.5f;
	b.baseY = fieldMidY();
	b.y = -b.radius * 2;
	b.enterT = 1.6f;
	b.nextAttack = 1.2f;
	for (auto& s : orc.shots) s.on = false;
	orc.fightT = 0;
	setCallout(kBosses[id].name, rgba(255, 200, 120), 0.8f);
	showCaption("A boss drops in.");
	audio::play(SFX_BOSSIN, 1.0f);
}

void Game::bossOpen(float seconds) {
	Boss& b = orc.boss;
	if (!b.on || b.beaten || b.dazeT > 0) return;
	float t = seconds + 0.25f * orc.upgrade[CARD_BLADE];
	if (orc.upgrade[CARD_GLASS]) t *= 2;
	const bool wasUp = b.guard;
	if (t > b.openT) b.openT = t;
	b.guard = false;
	if (wasUp) {
		addPopup("OPEN!", b.x, b.y - b.radius - 16, rgba(255, 230, 120), 0.75f);
		audio::play(SFX_OPEN, 0.9f, fieldPan(b.x));
		showCaption("Its guard is down. Slice it!");
	}
}

void Game::bossFire(int kind, float x, float y, float vx, float vy) {
	for (auto& s : orc.shots) {
		if (s.on) continue;
		s = {};
		s.on = true;
		s.kind = (u8)kind;
		s.x = x;
		s.y = y;
		s.vx = vx;
		s.vy = vy;
		s.hits = kind == SHOT_CANNON ? 2 : 1;
		s.life = kind == SHOT_CROWN ? kCrownLife : 0;
		return;
	}
}

// Picks what comes next. Every other attack at most is one without an opening, so the guard drops often.
void Game::bossNextAttack() {
	Boss& b = orc.boss;
	u8 choices[4];
	int n = 0;
	if (b.id == BOSS_MELON) {
		if (b.lastAttack != MEL_SEEDS) choices[n++] = MEL_SEEDS;
		choices[n++] = MEL_FLOP;
		if (b.phase >= 2) choices[n++] = MEL_CHARGE;
	} else {
		choices[n++] = KING_VOLLEY;
		choices[n++] = KING_CROWN;
		if (b.phase >= 2 && b.lastAttack != KING_LOB) choices[n++] = KING_LOB;
		if (b.phase >= 3 && b.lastAttack != KING_FUSE) choices[n++] = KING_FUSE;
	}
	int pick = gRng.below(n);
	if (choices[pick] == b.lastAttack && n > 1) pick = (pick + 1) % n;
	const u8 a = choices[pick];
	b.attack = a;
	b.lastAttack = a;
	b.attackT = 0;
	b.volleyT = 0;
	switch (a) {
		case MEL_SEEDS:
			b.volley = 3 + b.phase;
			addPopup("SEED SPIT", b.x, b.y - b.radius - 14, rgba(255, 200, 160), 0.55f);
			break;
		case MEL_FLOP:
			b.volley = b.phase >= 3 ? 2 : 1;
			addPopup("BELLY FLOP", b.x, b.y - b.radius - 14, rgba(255, 200, 160), 0.55f);
			break;
		case MEL_CHARGE:
			b.volley = 0;
			addPopup("ROLLING CHARGE", b.x, b.y - b.radius - 14, rgba(255, 170, 140), 0.55f);
			showCaption("It's charging. Swipe it to block.");
			break;
		case KING_VOLLEY:
			b.volley = 2 + b.phase;
			addPopup("CANNON VOLLEY", b.x, b.y - b.radius - 14, rgba(255, 200, 160), 0.55f);
			break;
		case KING_LOB:
			addPopup("ROYAL LOB", b.x, b.y - b.radius - 14, rgba(255, 200, 160), 0.55f);
			showCaption("Some of that fruit is a trick.");
			break;
		case KING_CROWN:
			addPopup("CROWN TOSS", b.x, b.y - b.radius - 14, rgba(255, 220, 120), 0.55f);
			break;
		case KING_FUSE:
			b.fuseT = 5.0f - 0.5f * (gSave.difficulty >= DIFF_HARD);
			b.fuseCuts = 0;
			b.deflectStroke = 0;
			setCallout("SNUFF THE FUSE!", rgba(255, 170, 90));
			showCaption("Its fuse is lit. Swipe the fuse three times.");
			audio::fuse(true, fieldPan(b.x));
			break;
		default: break;
	}
}

void Game::bossAttack(float dt) {
	Boss& b = orc.boss;
	const OrchardLevel& lv = orchardLevel(gSave.difficulty);
	b.attackT += dt;
	b.volleyT -= dt;
	const float floorY = kFieldH - b.radius * 0.75f;
	switch (b.attack) {
		case MEL_SEEDS:
			if (b.volley > 0 && b.volleyT <= 0) {
				const float tx = gRng.range(30, kFieldW - 30), ty = (float)kFieldH;
				const float dx = tx - b.x, dy = ty - b.y, len = sqrtf(dx * dx + dy * dy) + 1;
				const float speed = (95 + 15 * b.phase) * kFieldK;
				bossFire(SHOT_SEED, b.x + dx / len * b.radius * 0.7f, b.y + dy / len * b.radius * 0.7f, dx / len * speed, dy / len * speed);
				audio::play(SFX_THROW, 0.5f, fieldPan(b.x), 1.4f);
				b.squash = 0.4f;
				b.volley--;
				b.volleyT = 0.42f - 0.06f * b.phase;
			}
			if (b.volley <= 0 && b.volleyT < -0.5f) b.attack = ATK_NONE;
			break;
		case MEL_FLOP: {
			const float t = b.attackT;
			if (t < 0.55f) {
				b.y = lerpf(b.y, kFieldTop + b.radius * 0.6f, 1 - powf(0.0005f, dt));
			} else if (t < 0.85f) {
				// hangs at the top: the tell
			} else if (t < 1.1f) {
				b.y = lerpf(kFieldTop + b.radius * 0.6f, floorY, (t - 0.85f) / 0.25f);
			} else if (b.volleyT > -100) {
				b.y = floorY;
				b.squash = 1;
				shake = 0.5f;
				audio::play(SFX_BOOM, 0.6f, fieldPan(b.x), 0.7f);
				PORT_VIBRATE(0.4f, 90);
				for (int i = 0; i < 3; i++) {
					const float side = (i - 1) * 70.0f * acrossScale();
					bossFire(SHOT_SPLASH, b.x + (i - 1) * 20, floorY - b.radius * 0.5f, side, -gRng.range(260, 320) * kFieldK);
				}
				if (--b.volley > 0) {
					b.attackT = 0;  // the second flop
				} else {
					b.volleyT = -1000;  // landed for good
					bossOpen(lv.openTime + 0.5f);
					b.attack = ATK_NONE;
				}
			}
			break;
		}
		case MEL_CHARGE: {
			const float t = b.attackT;
			b.x = lerpf(b.x, kFieldW * 0.5f, 1 - powf(0.02f, dt));
			b.rx += dt * 9;
			if (t > 0.5f) b.squash = -clampf((t - 0.5f) / 1.3f, 0, 1);  // negative squash: it grows toward the screen
			if (t >= 1.8f) {
				b.squash = 0;
				shake = 0.8f;
				orcHurt(bossDamage(3), b.x, b.y, "CHARGE");
				bossOpen(lv.openTime);
				b.attack = ATK_NONE;
			}
			break;
		}
		case KING_VOLLEY:
			if (b.volley > 0 && b.volleyT <= 0) {
				const float tx = gRng.range(40, kFieldW - 40), ty = (float)kFieldH;
				const float dx = tx - b.x, dy = ty - b.y, len = sqrtf(dx * dx + dy * dy) + 1;
				const float speed = (70 + 10 * b.phase) * kFieldK;
				bossFire(SHOT_CANNON, b.x + dx / len * b.radius, b.y + dy / len * b.radius, dx / len * speed, dy / len * speed);
				audio::play(SFX_BOOM, 0.35f, fieldPan(b.x), 1.5f);
				b.squash = 0.6f;
				b.volley--;
				b.volleyT = 0.65f;
			}
			if (b.volley <= 0 && b.volleyT < -0.6f) {
				bossOpen(lv.openTime);
				b.attack = ATK_NONE;
			}
			break;
		case KING_LOB:
			if (b.attackT < dt * 1.5f) {
				const int bombs = 1 + (b.phase >= 3), decoys = 2;
				for (int i = 0; i < bombs + decoys; i++) {
					float x = 160, vx = 0, vy = -300;
					const bool bomb = i < bombs;
					pickThrow(bomb, 0.25f * i, x, vx, vy);
					// A decoy is queued as a fruit with the top bit set; launch() starts its timer.
					queue(0.25f * i, x, vx, vy, bomb ? OBJ_BOMB : OBJ_FRUIT, bomb ? 0 : (normalFruitOf(gSpawn) | 0x80));
				}
			}
			if (b.attackT > 1.4f) b.attack = ATK_NONE;
			break;
		case KING_CROWN:
			if (b.attackT < dt * 1.5f) {
				bossFire(SHOT_CROWN, b.x, b.y - b.radius, 0, 0);
				b.crownOut = kCrownLife;
				bossOpen(kCrownLife - 0.4f);
				audio::play(SFX_SWISH0, 0.8f, fieldPan(b.x), 0.8f);
			}
			if (b.attackT > 0.5f) b.attack = ATK_NONE;
			break;
		case KING_FUSE:
			b.fuseT -= dt;
			if (b.fuseCuts >= 3) {
				audio::fuse(false);
				setCallout("SNUFFED!", rgba(170, 230, 255));
				bossOpen(lv.openTime * 1.5f);
				b.attack = ATK_NONE;
			} else if (b.fuseT <= 0) {
				audio::fuse(false);
				audio::play(SFX_BOOM, 1.0f, fieldPan(b.x));
				flash = 0.6f;
				flashColor = rgba(255, 210, 150);
				shake = 1;
				emitSparks(b.x, b.y - b.radius, rgba(255, 190, 90), juiceAmount(24), 220);
				orcHurt(bossDamage(5), b.x, b.y, "BLAST");
				b.attack = ATK_NONE;
			}
			break;
		default: b.attack = ATK_NONE; break;
	}
	if (b.attack == ATK_NONE) b.nextAttack = lerpf(3.0f, 1.8f, (b.phase - 1) * 0.5f) * (gSave.difficulty == DIFF_EASY ? 1.25f : 1.0f);
}

void Game::shotsUpdate(float dt) {
	Boss& b = orc.boss;
	for (auto& s : orc.shots) {
		if (!s.on) continue;
		s.age += dt;
		s.rot += dt * 6;
		if (s.kind == SHOT_CROWN) {
			// Out and back in a loop, like a thrown boomerang.
			const float k = s.age / kCrownLife * 6.2832f;
			s.x = b.x + sinf(k) * kFieldW * 0.38f;
			s.y = b.y - b.radius + (1 - cosf(k)) * (kFieldH - b.y) * 0.42f;
			s.life -= dt;
			if (s.life <= 0) {
				s.on = false;
				b.crownOut = 0;
				orcHurt(bossDamage(2), b.x, b.y - b.radius, "CROWN");
			}
			continue;
		}
		if (s.kind == SHOT_SPLASH) s.vy += gravity * 0.8f * dt;
		if (s.kind == SHOT_SEED) s.vy += 40 * kFieldK * dt;
		s.x += s.vx * dt;
		s.y += s.vy * dt;
		if (s.x < -40 || s.x > kFieldW + 40) {
			s.on = false;
			continue;
		}
		if (s.y > kFieldH - 4 && s.vy > 0) {
			s.on = false;
			const int dmg = s.kind == SHOT_CANNON ? 2 : 1;
			emitJuice(s.x, (float)kFieldH - 6, s.kind == SHOT_CANNON ? rgba(90, 90, 100) : rgba(255, 90, 110), juiceAmount(6), 0, -1);
			orcHurt(bossDamage(dmg), s.x, kFieldH - 20.0f, s.kind == SHOT_CANNON ? "CANNON" : (s.kind == SHOT_SEED ? "SEED" : "SPLASH"));
		}
	}
	if (b.crownOut > 0) b.crownOut -= dt;
}

void Game::bossUpdate(float dt) {
	Boss& b = orc.boss;
	if (!b.on) return;
	b.t += dt;
	if (b.hurtT > 0) b.hurtT -= kDt;
	if (b.deflectT > 0) b.deflectT -= kDt;
	b.squash *= powf(0.02f, kDt);
	if (b.beaten) {
		b.leaveT += kDt;
		b.y -= 40 * kDt;
		if (b.leaveT > 1.6f) {
			b.on = false;
			orcNextRoom();
		}
		return;
	}
	if (b.enterT > 0) {
		b.enterT -= kDt;
		b.y = lerpf(b.baseY, -b.radius * 2, clampf(b.enterT / 1.6f, 0, 1) * clampf(b.enterT / 1.6f, 0, 1));
		return;
	}
	shotsUpdate(dt);
	if (b.dazeT > 0) {
		b.dazeT -= dt;
		b.x += sinf(b.t * 9) * 20 * dt;  // wobbles, dizzy
		if (b.dazeT <= 0) {
			b.dazeT = 0;
			b.guard = true;
			b.openT = 0;
			b.nextAttack = 0.8f;
		}
		return;
	}
	if (b.openT > 0) {
		b.openT -= dt;
		if (b.openT <= 0 && b.crownOut <= 0) {
			b.openT = 0;
			b.guard = true;
			addPopup("GUARD UP", b.x, b.y - b.radius - 14, rgba(150, 200, 255), 0.5f);
		}
	}
	if (b.roarT > 0) {
		b.roarT -= dt;
		return;
	}
	// Drifts about between attacks, and floats back up after a flop.
	if (b.attack != MEL_FLOP && b.attack != MEL_CHARGE) {
		const float speed = b.id == BOSS_KING ? 0.9f : 0.6f;
		const float tx = kFieldW * 0.5f + sinf(b.t * speed) * kFieldW * 0.28f;
		b.x = lerpf(b.x, tx, 1 - powf(0.2f, dt));
		if (b.openT <= 0) b.y = lerpf(b.y, b.baseY + sinf(b.t * 1.3f) * 6, 1 - powf(0.1f, dt));
	}
	if (b.attack != ATK_NONE) {
		bossAttack(dt);
	} else if (b.openT <= 0) {
		b.nextAttack -= dt;
		if (b.nextAttack <= 0) bossNextAttack();
	}
}

void Game::bossTest(Blade& blade) {
	Boss& b = orc.boss;
	if (!b.on || b.beaten || b.enterT > 0 || !blade.active || blade.strokeStart) return;
	const float reach = orcReach();
	// Shots first: they're small and quick.
	for (auto& s : orc.shots) {
		if (!s.on || s.stroke == blade.strokeId) continue;
		const float r = (s.kind == SHOT_CANNON ? 15.0f : (s.kind == SHOT_CROWN ? 18.0f : 10.0f)) * reach + 4;
		if (segDist2(s.x, s.y, blade) > r * r) continue;
		s.stroke = blade.strokeId;
		if (--s.hits > 0) {
			audio::play(SFX_KNOCK, 0.8f, fieldPan(s.x), 0.8f);
			emitSparks(s.x, s.y, rgba(200, 200, 210), juiceAmount(6), 90);
			continue;
		}
		s.on = false;
		if (s.kind == SHOT_CROWN) {
			b.crownOut = 0;
			addPopup("CROWN KNOCKED!", s.x, s.y - 14, rgba(255, 220, 120), 0.6f);
			audio::play(SFX_GOLD, 0.8f, fieldPan(s.x));
		} else {
			audio::play((Sfx)(SFX_JUICY0 + gRng.below(2)), 0.7f, fieldPan(s.x), 1.3f);
		}
		emitJuice(s.x, s.y, s.kind == SHOT_CANNON ? rgba(110, 110, 120) : rgba(255, 110, 120), juiceAmount(8), blade.x - blade.px, blade.y - blade.py);
		orcGainXp(s.kind == SHOT_CANNON ? 2 : 1);
	}
	if (b.dazeT > 0) return;  // confused: swipes pass straight through
	// The Bomb King's fuse sits on top of it.
	if (b.attack == KING_FUSE) {
		const float fx = b.x, fy = b.y - b.radius - 8, r = 18 * reach;
		if (segDist2(fx, fy, blade) <= r * r) {
			if (blade.strokeId != b.deflectStroke) {
				b.deflectStroke = blade.strokeId;
				b.fuseCuts++;
				emitSparks(fx, fy, rgba(255, 200, 90), juiceAmount(8), 120);
				audio::play(SFX_KNOCK, 0.6f, fieldPan(fx), 1.6f);
			}
			return;
		}
	}
	float scale = 1 - (b.squash < 0 ? b.squash * 0.8f : 0);
	const float r = b.radius * scale * 0.92f * reach;
	if (segDist2(b.x, b.y, blade) > r * r) return;
	if (b.attack == MEL_CHARGE && b.attackT > 0.6f) {
		// Blocked the charge: it bounces off and its guard drops for longer.
		b.attack = ATK_NONE;
		b.squash = 0;
		b.nextAttack = 2;
		setCallout("BLOCKED!", rgba(170, 230, 255));
		audio::play(SFX_CLANG, 1.0f, fieldPan(b.x), 0.8f);
		bossOpen(orchardLevel(gSave.difficulty).openTime * 1.3f);
		return;
	}
	if (b.guard && !orc.cleaver) {
		// Deflected!: once per swipe, and at most every half second.
		if (blade.strokeId == b.deflectStroke || b.deflectT > 0) return;
		b.deflectStroke = blade.strokeId;
		b.deflectT = 0.5f;
		audio::play(SFX_CLANG, 1.0f, fieldPan(b.x));
		emitSparks(blade.x, blade.y, rgba(170, 210, 255), juiceAmount(10), 150);
		blade.speed *= 0.3f;
		if (orc.lastStandT > 0 || gRng.unit() < 0.1f * orc.upgrade[CARD_PARRY]) {
			addPopup("PARRIED!", b.x, b.y - b.radius - 14, rgba(170, 230, 255), 0.6f);
			return;
		}
		orcHurt(orchardLevel(gSave.difficulty).deflectDamage, blade.x, blade.y, "DEFLECTED!");
		return;
	}
	// A clean slice.
	if (orc.cleaver && b.guard) addPopup("CLEAVED!", b.x, b.y - b.radius - 14, rgba(255, 230, 150), 0.6f);
	orc.cleaver = false;
	b.left--;
	b.hurtT = 0.4f;
	b.dazeT = 3.0f;
	b.openT = 0;
	b.guard = false;
	b.crownOut = 0;
	for (auto& s : orc.shots)
		if (s.kind == SHOT_CROWN) s.on = false;
	if (b.attack == KING_FUSE) audio::fuse(false);
	b.attack = ATK_NONE;
	shake = 0.6f;
	flash = 0.15f;
	flashColor = rgba(255, 255, 255);
	const float dx = blade.x - blade.px, dy = blade.y - blade.py;
	emitJuice(b.x, b.y, b.id == BOSS_MELON ? rgba(255, 70, 90) : rgba(255, 180, 80), juiceAmount(30), dx, dy);
	audio::play(b.id == BOSS_MELON ? SFX_MELON0 : SFX_SHELL, 1.0f, fieldPan(b.x), 0.8f);
	PORT_VIBRATE(0.6f, 140);
	orcGainXp(6);
	award(150);
	const int done = kBosses[b.id].slices - b.left;
	if (b.left == 0) {
		b.beaten = true;
		b.leaveT = 0;
		for (auto& s : orc.shots) s.on = false;
		orc.bossesBeaten++;
		gSave.bossesBeaten |= 1u << b.id;
		char buf[40];
		snprintf(buf, sizeof buf, "%s BEATEN!", b.id == BOSS_MELON ? "BIG MELON" : "BOMB KING");
		setCallout(buf, rgba(255, 230, 140), 0.8f);
		audio::play(SFX_VICTORY, 1.0f);
		orcGainXp(20);
		award(500 * (b.id + 1));
		return;
	}
	const int phase = 1 + done * kBosses[b.id].phases / kBosses[b.id].slices;
	if (phase > b.phase) {
		b.phase = (u8)phase;
		b.roarT = 1.2f;
		setCallout(phase >= 3 ? "IT'S FURIOUS!" : "IT'S ANGRY!", rgba(255, 140, 110));
		audio::play(SFX_ROAR, 1.0f, fieldPan(b.x));
	} else {
		addPopup("SLICED!", b.x, b.y - b.radius - 14, rgba(255, 240, 160), 0.7f);
	}
}

// ---------------------------------------------------------------- drawing
static void drawCrown(float x, float y, float s, u32 gold) {
	const u32 dark = rgba(200, 140, 30, (gold >> 24) & 0xFF);
	gfx::rect(x - 14 * s, y - 2 * s, 28 * s, 8 * s, dark);
	for (int i = -1; i <= 1; i++) {
		const float cx = x + i * 10 * s;
		gfx::tri(cx - 6 * s, y, gold, cx + 6 * s, y, gold, cx, y - (i == 0 ? 15 : 11) * s, gold);
	}
	gfx::rect(x - 14 * s, y, 28 * s, 5 * s, gold);
}

void Game::drawShots() const {
	for (const auto& s : orc.shots) {
		if (!s.on) continue;
		switch (s.kind) {
			case SHOT_SEED: gfx::sprite(SHEET_SPRITES, sprites_drop_idx, s.x, s.y, 0.9f, 1.2f, atan2f(s.vy, s.vx) - 1.5708f, rgba(70, 40, 30)); break;
			case SHOT_SPLASH: gfx::sprite(SHEET_SPRITES, sprites_drop_idx, s.x, s.y, 1.5f, 1.5f, s.rot * 0.3f, rgba(255, 80, 110)); break;
			case SHOT_CANNON:
				gfx::sprite(SHEET_FRUIT, fruit_bomb_idx, s.x, s.y, 0.8f, 0.8f, s.rot, s.hits > 1 ? rgba(120, 120, 130) : rgba(200, 120, 110));
				break;
			case SHOT_CROWN: drawCrown(s.x, s.y, 1.1f, rgba(255, 210, 70)); break;
			default: break;
		}
	}
}

void Game::drawBoss(bool solid) const {
	(void)solid;
	const Boss& b = orc.boss;
	if (!b.on) return;
	float alpha = 1;
	if (b.beaten) alpha = clampf(1 - b.leaveT / 1.6f, 0, 1);
	if (b.dazeT > 0) {
		// Confused: a flicker no faster than three times a second, or a steady see-through look with Calm Effects.
		alpha *= gSave.calmFx ? 0.55f : (((int)(b.dazeT * 6)) & 1 ? 0.35f : 0.9f);
	}
	const float grow = b.squash < 0 ? -b.squash * 0.8f : 0;
	const float flat = b.squash > 0 ? b.squash * 0.25f : 0;
	const float sx = (1 + grow) * (1 + flat), sy = (1 + grow) * (1 - flat);
	const bool hurt = b.hurtT > 0;
	const float wob = b.dazeT > 0 ? sinf(b.t * 7) * 0.25f : sinf(b.t * 1.7f) * 0.06f;
	// The guard: a cool ring while it's up, a warm one while it's open.
	if (!b.beaten && b.enterT <= 0 && b.dazeT <= 0) {
		const float ring = b.radius * 1.5f * (1 + grow) / 34;
		const float pulse = 0.65f + 0.25f * sinf(b.t * 5);
		if (b.guard) gfx::sprite(SHEET_SPRITES, sprites_ring_idx, b.x, b.y, ring, ring, 0, rgba(140, 200, 255, (int)(200 * pulse * alpha)));
		else gfx::sprite(SHEET_SPRITES, sprites_ring_idx, b.x, b.y, ring * 1.08f, ring * 1.08f, 0, rgba(255, 220, 110, (int)(230 * pulse * alpha)));
	}
	if (drawing3D()) {
		m3d::begin(kFieldW, gfx::stereoPerZ(), true);
		m3d::Skin skin = {hurt ? 0.6f : 0.0f, 1.0f, 0.35f, 0.35f, alpha, 0.3f};
		m3d::setSkin(&skin);
		const float s = b.radius / (b.id == BOSS_MELON ? 23.0f : 19.0f);
		m3d::Pose pose = {b.x, b.y, 30, b.rx, b.t * 0.4f, wob, s * (1 + grow), 0, 0, 0};
		m3d::draw(b.id == BOSS_MELON ? m3d::MESH_MELON : m3d::MESH_BOMB, m3d::PART_WHOLE, pose);
		m3d::setSkin(nullptr);
		m3d::end();
	} else {
		const u32 tint = hurt ? rgba(255, 140, 140, (int)(255 * alpha)) : rgba(255, 255, 255, (int)(255 * alpha));
		const float s = b.radius / (b.id == BOSS_MELON ? 23.0f : 19.0f);
		gfx::sprite(SHEET_FRUIT, b.id == BOSS_MELON ? fruit_melon_idx : fruit_bomb_idx, b.x, b.y, s * sx, s * sy, wob, tint);
	}
	// A face, so it reads as a character and not a big fruit: angry brows, and dizzy ones while confused.
	const u32 ink = rgba(40, 20, 20, (int)(230 * alpha));
	const float fs = b.radius / 50 * (1 + grow);
	const float ey = b.y - b.radius * 0.15f * sy;
	for (int side = -1; side <= 1; side += 2) {
		const float ex = b.x + side * b.radius * 0.32f * sx;
		if (b.dazeT > 0) {
			gfx::text("x", ex, ey - 8 * fs, 0.9f * fs, ink, ALIGN_CENTER);
		} else {
			gfx::rect(ex - 4 * fs, ey - 4 * fs, 8 * fs, 9 * fs, ink);
			gfx::tri(ex - side * 11 * fs, ey - 16 * fs, ink, ex + side * 9 * fs, ey - 9 * fs, ink, ex - side * 11 * fs, ey - 11 * fs, ink);
		}
	}
	if (b.id == BOSS_KING && b.crownOut <= 0) drawCrown(b.x, b.y - b.radius * sy - 2, b.radius / 40, rgba(255, 210, 70, (int)(255 * alpha)));
	if (b.attack == KING_FUSE) {
		const float fx = b.x, fy = b.y - b.radius - 8;
		const bool lit = ((int)(b.t * 10)) & 1;
		gfx::sprite(SHEET_SPRITES, sprites_ring_idx, fx, fy, 0.9f, 0.9f, 0, rgba(255, 170, 60, lit ? 255 : 160));
		char buf[8];
		snprintf(buf, sizeof buf, "%d", 3 - b.fuseCuts);
		gfx::text(buf, fx, fy - 30, 0.7f, rgba(255, 220, 120), ALIGN_CENTER);
	}
	if (b.attack == MEL_CHARGE && b.attackT < 0.6f && (((int)(b.attackT * 8)) & 1)) {
		gfx::text("!", b.x, b.y - b.radius - 34, 1.2f, rgba(255, 120, 90), ALIGN_CENTER);
	}
	// While the guard is down, a bar shows how long it stays down.
	if (!b.guard && !b.beaten && b.dazeT <= 0 && b.openT > 0) {
		const float w = b.radius * 1.6f, k = clampf(b.openT / 3.0f, 0, 1);
		gfx::rect(b.x - w * 0.5f, b.y + b.radius * sy + 8, w, 5, rgba(40, 30, 20, 160));
		gfx::rect(b.x - w * 0.5f, b.y + b.radius * sy + 8, w * k, 5, rgba(255, 220, 110));
	}
}
