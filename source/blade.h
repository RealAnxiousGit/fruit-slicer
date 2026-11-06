#pragma once
#include "common.h"

// Turns raw resistive touch samples into a smooth blade stroke.
struct Blade {
	static constexpr int kTrail = 32;
	struct Point {
		float x, y, age;
	};

	Point trail[kTrail];
	int head = 0;
	int count = 0;

	bool active = false;     // finger is down this frame
	bool strokeStart = false;
	float x = 0, y = 0;      // filtered position
	float px = 0, py = 0;    // filtered position last frame
	float speed = 0;         // px per frame
	float rawX = 0, rawY = 0;
	float swishCooldown = 0;
	u32 strokeId = 0;

	void reset();
	void update(const Input& in);
	void draw(u32 color) const;

private:
	void pushPoint(float nx, float ny);
	float hx[3], hy[3];  // recent filtered samples, for curve interpolation
	int hn = 0;
};
