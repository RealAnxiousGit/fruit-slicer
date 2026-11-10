// Custom Music (Windows, Linux and Android only): the player's own songs, from a "Custom Music" folder
// beside the save. Each one is played in Beat Slice once its beat has been found, by the Beat Tool's
// analysis and any nudges the player makes. MP3, Ogg Vorbis, FLAC and 16-bit WAV are read.
#pragma once

namespace custom {
static constexpr int kMax = 64;

void scan();               // reads the folder again, making it the first time
int count();
const char* name(int i);   // as shown: the file name without its extension, plain letters only
const char* path(int i);   // the whole path, for opening
bool isWav(int i);
const char* folder();      // the folder itself
bool openFolder();         // shows it in the system's file manager, where there is one

// Add Songs: the system's file picker, where any number of songs can be picked at once. Each is copied
// into the folder. False if no picker could be shown.
bool importSongs();
// Once the picker has closed: how many songs were copied (0 if none, or it was cancelled), and the file
// name of the first, for the list to pick it. -1 while nothing has come back.
int importDone(char* first, int size);
bool importOpen();         // the picker is on screen
int importFiles(const char* const* files);  // what the picker does with the files picked (null-ended); the Lab calls it directly

// The beat: tempo, and where the first beat of a bar falls. False until it has been found.
bool beat(int i, float* bpm, float* first);
void setBeat(int i, float bpm, float first);  // kept in "beats.txt" in the folder

// Finding the beat, a few milliseconds each frame so the game never stalls.
bool analyzeStart(int i);
float analyzeStep();       // progress, 0 to 1; 1 means found (and saved), below 0 means it could not be read
void analyzeCancel();
}  // namespace custom
