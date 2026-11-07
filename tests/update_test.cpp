// Host test for the update check and the Linux tar.gz install, against the real GitHub releases.
// It plays an older version, so the newest release is offered, then downloads and installs it into the
// folder the test runs from. Run it from a scratch copy: that folder's files are replaced.
// Build and run (Linux, SDL 3 from the game's build folder):
//   g++ -std=gnu++17 -Iport -Isource -I<sdl>/include tests/update_test.cpp source/update.cpp port/update_port.cpp \
//       <sdl-build>/libSDL3.a -ldl -lpthread -lm -o /tmp/upd/update_test && /tmp/upd/update_test 0.3.0-alpha
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"
#include "port_api.h"
#include "progress.h"
#include "update.h"

Settings gSave;
static char sDir[300];
void saveWrite() {}
namespace progress {
u32 today() { return 9999; }
}  // namespace progress
namespace port {
void dataPath(char* out, size_t size, const char* name) { snprintf(out, size, "%s%s", sDir, name); }
}  // namespace port

static bool wait(update::State busy) {
	for (int i = 0; i < 60 * 600 && update::state() == busy; i++) {
		update::tick();
		SDL_Delay(16);
	}
	return update::state() != busy;
}

int main(int argc, char** argv) {
	SDL_Init(0);
	snprintf(sDir, sizeof sDir, "%sdata/", SDL_GetBasePath());
#ifdef _WIN32
	mkdir(sDir);
#else
	mkdir(sDir, 0755);
#endif
	const char* version = argc > 1 ? argv[1] : "0.3.0-alpha";
	printf("playing version %s, installs as %s\n", version, port::updateAsset() ? port::updateAsset() : "(can't install)");
	update::boot(version);
	printf("first start asks: %s\n", update::state() == update::ASK ? "yes" : "NO");
	update::consent(true);
	if (!wait(update::CHECKING)) return printf("FAIL: the check never finished\n"), 1;
	if (update::state() == update::IDLE) return printf("nothing newer: the game stays quiet\n"), 0;
	if (update::state() != update::OFFER) return printf("FAIL: state %d, not an offer\n", (int)update::state()), 1;
	printf("offered %s; notes start: %.300s\n", update::newVersion(), update::notes());
	if (argc > 2 && strcmp(argv[2], "check") == 0) return 0;
	update::install();
	if (!wait(update::DOWNLOADING)) return printf("FAIL: the download never finished\n"), 1;
	printf("after download: %s (progress %.2f)\n", update::state() == update::READY ? "READY" : "FAILED", update::progress());
	return update::state() == update::READY ? 0 : 1;
}
