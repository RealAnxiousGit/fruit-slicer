#pragma once
#include "common.h"

namespace gfx {
u32 logoLetterColor(int letterIdx, float yFrac, bool rainbow, float timeSec);
void drawVectorLogo(float cx, float cy, float scale, bool rainbow, float timeSec);
bool logoHitTest(float lx, float ly, float cx, float cy, float scale);
int vectorLogoTriCount();
}  // namespace gfx
