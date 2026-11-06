#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"

Settings gSave;

static constexpr u32 kMagic = 0x31534C46;  // "FLS1"
static const char* const kDir = "sdmc:/3ds/FruitSlicer";
static const char* const kFile = "sdmc:/3ds/FruitSlicer/save.bin";

static u32 checksum(const Settings& s) {
	const u8* p = (const u8*)&s;
	u32 h = 2166136261u;
	for (size_t i = 0; i < sizeof s; i++) h = (h ^ p[i]) * 16777619u;
	return h;
}

static void defaults() {
	memset(&gSave, 0, sizeof gSave);
	gSave.sfx = 1;
	gSave.music = 1;
}

void saveLoad() {
	defaults();
	FILE* f = fopen(kFile, "rb");
	if (!f) return;
	u32 magic = 0, sum = 0;
	Settings tmp;
	bool ok = fread(&magic, 4, 1, f) == 1 && magic == kMagic && fread(&tmp, sizeof tmp, 1, f) == 1 &&
	          fread(&sum, 4, 1, f) == 1 && sum == checksum(tmp);
	fclose(f);
	if (ok) gSave = tmp;
	else fprintf(stderr, "[save] ignoring invalid save file\n");
}

void saveWrite() {
	mkdir("sdmc:/3ds", 0777);
	mkdir(kDir, 0777);
	FILE* f = fopen(kFile, "wb");
	if (!f) {
		fprintf(stderr, "[save] could not write %s\n", kFile);
		return;
	}
	u32 sum = checksum(gSave);
	fwrite(&kMagic, 4, 1, f);
	fwrite(&gSave, sizeof gSave, 1, f);
	fwrite(&sum, 4, 1, f);
	fclose(f);
}
