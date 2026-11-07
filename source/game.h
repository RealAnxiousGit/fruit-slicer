#pragma once
#include "blade.h"
#include "common.h"

// All gameplay objects live in fixed arrays: nothing here allocates after boot,
// so rapid spawning can't fragment the heap.
enum ObjKind : u8 { OBJ_FRUIT, OBJ_GOLD, OBJ_BOMB, OBJ_FREEZE, OBJ_DOUBLE, OBJ_FRENZY };

struct Obj {
	float x, y, vx, vy, rot, rotv, radius, age;
	float hitCooldown;  // tough fruit can't be hit twice by one swipe
	ObjKind kind;
	u8 fruit;
	u8 hits;            // slices still needed
	bool on;
};
struct Half {
	float x, y, vx, vy, rot, rotv;
	short sprite;
	bool on;
};
struct Particle {
	float x, y, vx, vy, life, maxLife, size, gravity;
	u32 color;
	u8 sprite;  // 0 drop, 1 spark
	bool on;
};
struct Splat {
	float x, y, rot, scale, life;
	u32 color;
	u8 variant;
	bool on;
};
struct Popup {
	char text[24];
	float x, y, life, scale;
	u32 color;
	bool on;
};
struct Ring {
	float x, y, life, maxLife, maxScale;
	u32 color;
	bool on;
};
struct Pending {
	float delay, x, vx, vy;
	ObjKind kind;
	u8 fruit;
	bool on;
};

enum GamePhase { PHASE_READY, PHASE_PLAY, PHASE_FINALE, PHASE_DYING, PHASE_OVER };

struct Game {
	static constexpr int kMaxObjs = 40;
	static constexpr int kMaxHalves = 64;
	static constexpr int kMaxParticles = 260;
	static constexpr int kMaxSplats = 18;
	static constexpr int kMaxPopups = 14;
	static constexpr int kMaxRings = 8;
	static constexpr int kMaxPending = 28;

	Obj objs[kMaxObjs];
	Half halves[kMaxHalves];
	Particle parts[kMaxParticles];
	Splat splats[kMaxSplats];
	Popup popups[kMaxPopups];
	Ring rings[kMaxRings];
	Pending pending[kMaxPending];
	int partCursor, splatCursor, halfCursor, popupCursor, ringCursor;

	Mode mode;
	GamePhase phase;
	float phaseT;       // seconds in the current phase
	float elapsed;      // seconds of play
	float timeLeft;     // for timed modes
	float waveTimer;
	float gravity;
	float flash;        // white flash from bomb
	u32 flashColor;
	float shake;
	u32 score;
	u32 best;
	bool newBest;
	int strikes;
	int maxStrikes;     // 0 means no strike rule
	int sliced;
	int bestCombo;
	int streak;
	int comboCount;
	float comboTimer;
	u32 comboStroke;
	float comboX, comboY;
	char callout[40];   // large top-screen message
	float calloutT;
	float calloutSize;
	u32 calloutColor;
	float freezeT, doubleT, frenzyT;
	float frenzyTimer;
	float feverMeter;   // 0..1, fills as you slice
	float feverIdle;    // seconds since the meter last gained anything
	float feverT;       // seconds of Fever left
	float feverTimer;
	float slowT;        // brief slow motion after a huge combo
	float scorePulse;
	float strikePulse;
	// The giant fruit that ends a timed run.
	float finaleT;      // seconds since it was thrown
	float finaleX, finaleY;
	float finaleSwell;  // grows with every hit
	float finaleJoltX, finaleJoltY;
	float finaleTravel; // blade distance inside the fruit since the last hit
	int finaleHits;
	bool finaleOn;
#ifdef AUTOTEST
	// Measures how often a fruit is actually drawn on top of a bomb.
	u32 statBombFrames, statOverlapFrames, statWaves;
#endif

	void start(Mode m);
	void update(Blade& blade);
	void drawField(const Blade& blade) const;   // bottom screen
	void drawHud() const;                       // top screen, one eye at a time
	int activeObjects() const;
	int activeParticles() const;
	// First live object the AUTOTEST bot should aim for (or avoid).
	const Obj* botTarget(bool wantBomb) const;

private:
	void spawnWave();
	bool spawnPattern(float d);
	void pickThrow(bool bomb, float delay, float& x, float& vx, float& vy);
	float clearance(float x, float vx, float vy, float delay, bool bomb) const;
	bool bombAround() const;
	void queue(float delay, float x, float vx, float vy, ObjKind kind, int fruit);
	void launch(const Pending& p);
	void updateObjects(float dt);
	void updateEffects(float dt);
	void testSlices(Blade& blade);
	bool hitTest(const Obj& o, const Blade& blade, float radius) const;
	void sliceObj(Obj& o, const Blade& blade);
	void sliceFruit(Obj& o, const Blade& blade);
	void hitBomb(Obj& o);
	void award(int points);
	void addStrike(float x);
	void finishCombo();
	void addFever(float amount);
	void emitJuice(float x, float y, u32 color, int count, float dirX, float dirY);
	void emitSparks(float x, float y, u32 color, int count, float speed);
	void addPopup(const char* s, float x, float y, u32 color, float scale);
	void addRing(float x, float y, u32 color, float maxScale, float life);
	void setCallout(const char* s, u32 color, float size = 0.72f);
	void endGame();
	void startFinale();
	void updateFinale(Blade& blade);
	void finaleHit(const Blade& blade);
	void finaleBurst();
	float difficulty() const;
};

u32 hueColor(float t);
