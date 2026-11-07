// Host test for source/save.cpp: 0.4 upgrade, 1.0 round trip, damaged files and older 1.x saves.
// Build and run (Linux):  g++ -std=gnu++17 -Iport -Isource tests/save_test.cpp source/save.cpp -o /tmp/save_test && /tmp/save_test
// Add -DTEST_3DS_LAYOUT to check a 60-byte 3DS 0.4 save instead of the 64-byte universal one.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "port_api.h"

const char* const kRendererNames[] = {"OpenGL", "Vulkan", "Software"};
const int kRendererCount = 3;
static char sDir[256];

namespace port {
void dataPath(char* out, size_t size, const char* name) { snprintf(out, size, "%s/%s", sDir, name); }
void ensureDataDir() { mkdir(sDir, 0755); }
}  // namespace port

static int sFails = 0;
#define CHECK(c) do { if (!(c)) { printf("  FAIL line %d: %s\n", __LINE__, #c); sFails++; } } while (0)

static u32 fnv(const u8* p, size_t n) {
	u32 h = 2166136261u;
	for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
	return h;
}

static void path(char* out, const char* name) { snprintf(out, 300, "%.200s/%.90s", sDir, name); }
static bool exists(const char* name) { char p[300]; path(p, name); return access(p, F_OK) == 0; }
static void rm(const char* name) { char p[300]; path(p, name); remove(p); }

// A 0.4 save exactly as 0.4.0 wrote it: "FLS4", the settings struct, FNV-1a.
static void write04(const char* name, int payload) {
	u8 b[64] = {0};
	u32 best[7] = {11, 22, 33, 44, 55, 66, 77};
	memcpy(b, best, 28);
	u32 tf = 1234, bc = 56;
	memcpy(b + 28, &tf, 4);
	memcpy(b + 32, &bc, 4);
	const u8 tail[] = {40, 30, 20, 3, 4, 1, 2, 1, 5, 2, 60, 70, 1, 1, 0, 1, 0, 33, 3, 80, 20, 90, 10, 5};
	memcpy(b + 36, tail, sizeof tail);
	if (payload > 60) b[60] = 2;  // renderer
	char p[300];
	path(p, name);
	FILE* f = fopen(p, "wb");
	u32 magic = 0x34534C46, sum = fnv(b, payload);
	fwrite(&magic, 4, 1, f);
	fwrite(b, payload, 1, f);
	fwrite(&sum, 4, 1, f);
	fclose(f);
}

int main() {
	snprintf(sDir, sizeof sDir, "/tmp/fs_save_test_%d", (int)getpid());
	mkdir(sDir, 0755);
#ifdef TEST_3DS_LAYOUT
	const int payload = 60;
#else
	const int payload = 64;
#endif

	printf("1. A 0.4 save upgrades with every score and setting kept\n");
	write04("save.bin", payload);
	saveLoad();
	CHECK(gSave.best[0] == 11 && gSave.best[6] == 77 && gSave.best[7] == 0);
	CHECK(gSave.totalFruit == 1234 && gSave.bestCombo == 56);
	CHECK(gSave.sfx == 40 && gSave.music == 30 && gSave.menuSfx == 20 && gSave.menuTrack == 3 && gSave.gameTrack == 4);
	CHECK(gSave.artStyle == 1 && gSave.board == 2 && gSave.scene == 1 && gSave.blade == 5 && gSave.effects == 2);
	CHECK(gSave.depth == 60 && gSave.shake == 70 && gSave.bgDetail == 1 && gSave.detail == 0 && gSave.sliceStyle == 1 && gSave.transitions == 0);
	CHECK(gSave.stylus == 33 && gSave.difficulty == 3 && gSave.fruitSpeed == 80 && gSave.bombs == 20);
	CHECK(gSave.forgiveness == 90 && gSave.frenzyDrain == 10 && gSave.misses == 5);
	CHECK(gSave.coins == 0 && gSave.ownedThemes == 1 && gSave.uiScale == 0 && gSave.snapAssist == 1 && gSave.stickSpeed == 50);
#ifndef TEST_3DS_LAYOUT
	CHECK(gSave.renderer == 2 && gSave.vsync == 1 && gSave.preset == 1 && gSave.hwChecked == 0);
#endif

	printf("2. The first 1.0 write keeps the 0.4 file as save04.bak\n");
	gSave.coins = 99;
	saveWrite();
	CHECK(exists("save04.bak") && exists("save.bin") && !exists("save.tmp"));

	printf("3. A 1.0 save reads back exactly\n");
	Settings before = gSave;
	memset((void*)&gSave, 0xAB, sizeof gSave);
	saveLoad();
	CHECK(memcmp(&before, &gSave, sizeof gSave) == 0);

	printf("4. A second write doesn't touch save04.bak again\n");
	rm("save04.bak");
	saveWrite();
	CHECK(!exists("save04.bak"));

	printf("5. A damaged save.bin falls back to save.tmp\n");
	{
		char b[300], t[300];
		path(b, "save.bin");
		path(t, "save.tmp");
		FILE* in = fopen(b, "rb");
		static u8 buf[4096];
		size_t n = fread(buf, 1, sizeof buf, in);
		fclose(in);
		FILE* out = fopen(t, "wb");
		fwrite(buf, 1, n, out);
		fclose(out);
		buf[20] ^= 0xFF;  // break save.bin's checksum
		out = fopen(b, "wb");
		fwrite(buf, 1, n, out);
		fclose(out);
		saveLoad();
		CHECK(gSave.coins == 99 && gSave.best[0] == 11);
		rm("save.tmp");
	}

	printf("6. A damaged save and no temp file gives clean defaults\n");
	saveLoad();
	CHECK(gSave.coins == 0 && gSave.best[0] == 0 && gSave.sfx == 100);

	printf("7. A shorter 1.x save loads, with the rest at defaults\n");
	{
		saveLoad();
		gSave.coins = 7;
		gSave.best[2] = 500;
		const u16 shortSize = 200;  // as if an older 1.x save ended here
		char b[300];
		path(b, "save.bin");
		FILE* f = fopen(b, "wb");
		u32 magic = 0x35534C46, sum = fnv((const u8*)&gSave, shortSize);
		u16 head[2] = {1, shortSize};
		fwrite(&magic, 4, 1, f);
		fwrite(head, 4, 1, f);
		fwrite(&gSave, shortSize, 1, f);
		fwrite(&sum, 4, 1, f);
		fclose(f);
		gSave.best[2] = 0;
		saveLoad();
		CHECK(gSave.best[2] == 500 && gSave.sfx == 100);
	}

	printf("8. Reset Everything keeps coins and Shop items\n");
	gSave.coins = 42;
	gSave.ownedTrails = 0x7;
	gSave.best[0] = 9;
	saveDefaults(false, SEC_ALL);
	CHECK(gSave.coins == 42 && gSave.ownedTrails == 0x7 && gSave.best[0] == 0);

	char cmd[300];
	snprintf(cmd, sizeof cmd, "rm -rf %s", sDir);
	system(cmd);
	printf("%s (Settings is %d bytes)\n", sFails ? "FAILED" : "ALL PASSED", (int)sizeof(Settings));
	return sFails ? 1 : 0;
}
