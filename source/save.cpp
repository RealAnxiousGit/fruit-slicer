#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"

Settings gSave;

static constexpr u32 kMagic = 0x32534C46;    // "FLS2"
static constexpr u32 kMagicV1 = 0x31534C46;  // "FLS1", written by 0.1.0
static const char* const kDir = "sdmc:/3ds/FruitSlicer";
static const char* const kFile = "sdmc:/3ds/FruitSlicer/save.bin";

static u32 checksum(const void* data, size_t size) {
	const u8* p = (const u8*)data;
	u32 h = 2166136261u;
	for (size_t i = 0; i < size; i++) h = (h ^ p[i]) * 16777619u;
	return h;
}

static void defaults() {
	memset(&gSave, 0, sizeof gSave);
	gSave.sfx = 4;
	gSave.music = 3;
	gSave.juice = 1;
	gSave.depth = 1;
	gSave.shake = 1;
}

// The 0.1.0 layout: six best scores followed by three settings bytes.
struct SettingsV1 {
	u32 best[6];
	u8 sfx, music, blade, pad;
};

void saveLoad() {
	defaults();
	FILE* f = fopen(kFile, "rb");
	if (!f) return;
	u32 magic = 0, sum = 0;
	if (fread(&magic, 4, 1, f) == 1) {
		if (magic == kMagic) {
			Settings tmp;
			if (fread(&tmp, sizeof tmp, 1, f) == 1 && fread(&sum, 4, 1, f) == 1 && sum == checksum(&tmp, sizeof tmp)) gSave = tmp;
			else fprintf(stderr, "[save] ignoring invalid save file\n");
		} else if (magic == kMagicV1) {
			SettingsV1 old;
			if (fread(&old, sizeof old, 1, f) == 1 && fread(&sum, 4, 1, f) == 1 && sum == checksum(&old, sizeof old)) {
				memcpy(gSave.best, old.best, sizeof old.best);
				gSave.sfx = old.sfx ? 4 : 0;
				gSave.music = old.music ? 3 : 0;
				gSave.blade = old.blade % kBladeCount;
				fprintf(stderr, "[save] upgraded 0.1.0 save\n");
			}
		}
	}
	fclose(f);
	gSave.board %= kBoardCount;
	gSave.scene %= kSceneCount;
	gSave.blade %= kBladeCount;
}

void saveWrite() {
	mkdir("sdmc:/3ds", 0777);
	mkdir(kDir, 0777);
	FILE* f = fopen(kFile, "wb");
	if (!f) {
		fprintf(stderr, "[save] could not write %s\n", kFile);
		return;
	}
	u32 sum = checksum(&gSave, sizeof gSave);
	fwrite(&kMagic, 4, 1, f);
	fwrite(&gSave, sizeof gSave, 1, f);
	fwrite(&sum, 4, 1, f);
	fclose(f);
}
