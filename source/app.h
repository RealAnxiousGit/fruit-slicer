// The shared game, as seen by each port's main loop.
#pragma once
#include "common.h"

namespace app {
bool init();           // false if the assets are missing and there is nothing to draw
void showLoading();    // runs the loading screen until everything has arrived
bool frame(Input& in); // one update and draw; false when the game should quit
void shutdown();
void notifySuspended();  // the game was interrupted, so pause it
bool inFieldView();      // touch coordinates are in the field rather than the menu layout
}  // namespace app
