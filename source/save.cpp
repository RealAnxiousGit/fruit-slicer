#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"

Settings gSave;
bool gIsNew3ds = false;
bool gHas3dScreen = true;

static constexpr u32 kMagic = 0x33534C46;    // "FLS3"
static constexpr u32 kMagicV2 = 0x32534C46;  // "FLS2", written by 0.2.0
static constexpr u32 kMagicV1 = 0x31534C46;  // "FLS1", written by 0.1.0
static const char* const kDir = "sdmc:/3ds/FruitSlicer";
static const char* const kFile = "sdmc:/3ds/FruitSlicer/save.bin";
static const char* const kTemp = "sdmc:/3ds/FruitSlicer/save.tmp";

static u32 checksum(const void* data, size_t size) {
	const u8* p = (const u8*)data;
	u32 h = 2166136261u;
	for (size_t i = 0; i < size; i++) h = (h ^ p[i]) * 16777619u;
	return h;
}

void saveDefaults(bool keepScores, int section) {
	if (!keepScores) {
		memset(gSave.best, 0, sizeof gSave.best);
		gSave.totalFruit = 0;
		gSave.bestCombo = 0;
	}
	if (section < 0 || section == 0) {
		gSave.sfx = 100;
		gSave.music = 75;
		gSave.menuSfx = 80;
		gSave.menuTrack = gSave.gameTrack = 0;
	}
	if (section < 0 || section == 1) {
		// A New 3DS gets the full look. An Old 3DS starts on the settings it can hold 60 with.
		gSave.artStyle = gIsNew3ds ? ART_3D : ART_2D;
		gSave.board = gSave.scene = gSave.blade = 0;
		gSave.effects = gIsNew3ds ? FX_FULL : FX_MEDIUM;
		gSave.depth = 50;
		gSave.shake = 100;
		gSave.bgDetail = gIsNew3ds ? 1 : 0;
		gSave.topFruit = 1;
		gSave.detail = gIsNew3ds ? 1 : 0;
		gSave.sliceStyle = gIsNew3ds ? 1 : 0;
		gSave.transitions = 1;
	}
	if (section < 0 || section == 2) {
		gSave.stylus = 40;
		gSave.difficulty = DIFF_NORMAL;
		gSave.fruitSpeed = 50;
		gSave.bombs = 50;
		gSave.forgiveness = 50;
		gSave.frenzyDrain = 50;
		gSave.misses = 3;
	}
}

// The 0.2.0 layout.
struct SettingsV2 {
	u32 best[MODE_COUNT];
	u32 totalFruit;
	u32 bestCombo;
	u8 sfx, music, blade, board, scene, juice, depth, shake;
};
// The 0.1.0 layout: six best scores followed by three settings bytes.
struct SettingsV1 {
	u32 best[6];
	u8 sfx, music, blade, pad;
};

void saveLoad() {
	saveDefaults(false, -1);
	FILE* f = fopen(kFile, "rb");
	if (!f) f = fopen(kTemp, "rb");  // power was lost between the two steps of saveWrite
	if (!f) return;
	u32 magic = 0, sum = 0;
	if (fread(&magic, 4, 1, f) == 1) {
		if (magic == kMagic) {
			Settings tmp;
			if (fread(&tmp, sizeof tmp, 1, f) == 1 && fread(&sum, 4, 1, f) == 1 && sum == checksum(&tmp, sizeof tmp)) gSave = tmp;
			else fprintf(stderr, "[save] ignoring invalid save file\n");
		} else if (magic == kMagicV2) {
			SettingsV2 old;
			if (fread(&old, sizeof old, 1, f) == 1 && fread(&sum, 4, 1, f) == 1 && sum == checksum(&old, sizeof old)) {
				memcpy(gSave.best, old.best, sizeof old.best);
				gSave.totalFruit = old.totalFruit;
				gSave.bestCombo = old.bestCombo;
				gSave.sfx = old.sfx > 4 ? 100 : old.sfx * 25;
				gSave.music = old.music > 4 ? 75 : old.music * 25;
				gSave.blade = old.blade;
				gSave.board = old.board;
				gSave.scene = old.scene;
				if (old.juice == 0) gSave.effects = FX_LOW;
				else if (old.juice == 2) gSave.effects = FX_FULL;
				gSave.depth = old.depth == 0 ? 0 : (old.depth == 1 ? 50 : 90);
				gSave.shake = old.shake ? 100 : 0;
				fprintf(stderr, "[save] upgraded 0.2.0 save\n");
			}
		} else if (magic == kMagicV1) {
			SettingsV1 old;
			if (fread(&old, sizeof old, 1, f) == 1 && fread(&sum, 4, 1, f) == 1 && sum == checksum(&old, sizeof old)) {
				memcpy(gSave.best, old.best, sizeof old.best);
				gSave.sfx = old.sfx ? 100 : 0;
				gSave.music = old.music ? 75 : 0;
				gSave.blade = old.blade;
				fprintf(stderr, "[save] upgraded 0.1.0 save\n");
			}
		}
	}
	fclose(f);
	gSave.board %= kBoardCount;
	gSave.scene %= kSceneCount;
	gSave.blade %= kBladeCount;
	gSave.artStyle &= 1;
	gSave.effects &= 3;
	gSave.difficulty %= DIFF_COUNT;
	gSave.misses = gSave.misses < 1 ? 1 : (gSave.misses > 5 ? 5 : gSave.misses);
	gSave.bgDetail &= 1;
	gSave.topFruit &= 1;
	gSave.detail &= 1;
	gSave.sliceStyle &= 1;
	gSave.transitions &= 1;
	gSave.menuTrack %= kTrackCount + 1;
	gSave.gameTrack %= kTrackCount + 1;
	u8* const sliders[] = {&gSave.sfx, &gSave.music, &gSave.menuSfx, &gSave.depth, &gSave.shake, &gSave.stylus, &gSave.fruitSpeed, &gSave.bombs, &gSave.forgiveness, &gSave.frenzyDrain};
	for (u8* v : sliders)
		if (*v > 100) *v = 100;
}

void saveWrite() {
	mkdir("sdmc:/3ds", 0777);
	mkdir(kDir, 0777);
	// Written beside the old save and swapped in afterwards, so a power cut can't lose both.
	FILE* f = fopen(kTemp, "wb");
	if (!f) {
		fprintf(stderr, "[save] could not write %s\n", kTemp);
		return;
	}
	u32 sum = checksum(&gSave, sizeof gSave);
	fwrite(&kMagic, 4, 1, f);
	fwrite(&gSave, sizeof gSave, 1, f);
	fwrite(&sum, 4, 1, f);
	fclose(f);
	remove(kFile);
	rename(kTemp, kFile);
}
