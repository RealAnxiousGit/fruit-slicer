// Updates for Windows, Linux and Android: fetching from GitHub on a background thread, and installing.
//   Windows: WinHTTP (part of Windows). The zip is unpacked with Windows' own tar, and a small script swaps
//            the files in once the game has closed, then starts it again.
//   Linux:   the system's libcurl, loaded when first needed, so the game still runs without it. An AppImage
//            is replaced in place; the tar.gz build has its folder's files replaced.
//   Android: Java (FruitSlicerActivity) downloads, and Android's package installer asks the player.
// Every download goes to a file first and must match GitHub's size before anything is replaced.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"
#include "port.h"
#include "port_api.h"

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#elif PORT_ANDROID
#include <jni.h>
#elif PORT_WEB
#include <emscripten.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace {
enum Job { JOB_TEXT, JOB_FILE };
SDL_Thread* sThread;
SDL_AtomicInt sResult;  // 0 working, 1 done, -1 failed
SDL_AtomicInt sGot;     // bytes so far
Job sJob;
char sUrl[600];
char sFile[700];        // where the download goes
unsigned long long sExpect;
char* sText;
bool sRestart;
char sRestartPath[700];

void updateDir(char* out, size_t size, const char* name) {
	char base[600];
	port::dataPath(base, sizeof base, "");
	char dir[640];
	snprintf(dir, sizeof dir, "%supdate", base);
	SDL_CreateDirectory(dir);
#if defined(_WIN32)
	snprintf(out, size, "%s\\%s", dir, name);  // cmd reads a forward slash as a switch
#else
	snprintf(out, size, "%s/%s", dir, name);
#endif
}

// ---------------------------------------------------------------- fetching to a file
#if defined(_WIN32)
bool fetch(const char* url, FILE* out, const char* postJson = nullptr) {
	wchar_t wurl[700], host[256], path[1400];
	if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 700)) return false;
	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof uc;
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 1400;
	if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) return false;
	bool ok = false;
	HINTERNET s = WinHttpOpen(L"FruitSlicer", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	HINTERNET c = s ? WinHttpConnect(s, host, uc.nPort, 0) : nullptr;
	HINTERNET r = c ? WinHttpOpenRequest(c, postJson ? L"POST" : L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
	if (r) {
		WinHttpSetTimeouts(r, 20000, 20000, 30000, 30000);
		const wchar_t* hdr = postJson ? L"Content-Type: application/json\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS;
		const DWORD hdrLen = postJson ? (DWORD)wcslen(hdr) : 0;
		const DWORD bodyLen = postJson ? (DWORD)strlen(postJson) : 0;
		DWORD code = 0, len = sizeof code;
		if (WinHttpSendRequest(r, hdr, hdrLen, (LPVOID)postJson, bodyLen, bodyLen, 0) && WinHttpReceiveResponse(r, nullptr) &&
		    WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &len, WINHTTP_NO_HEADER_INDEX) && code == 200) {
			static char buf[64 * 1024];
			ok = true;
			for (;;) {
				DWORD got = 0;
				if (!WinHttpReadData(r, buf, sizeof buf, &got)) {
					ok = false;
					break;
				}
				if (got == 0) break;
				if (fwrite(buf, 1, got, out) != got) {
					ok = false;
					break;
				}
				SDL_AddAtomicInt(&sGot, (int)got);
			}
		}
	}
	if (r) WinHttpCloseHandle(r);
	if (c) WinHttpCloseHandle(c);
	if (s) WinHttpCloseHandle(s);
	return ok;
}
#elif PORT_ANDROID
jclass sActivityClass;  // found on the main thread: other threads can't look up the game's classes
jmethodID sFetch, sFetchBody, sInstall;

bool javaReady() {
	if (sActivityClass) return true;
	JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
	jobject act = (jobject)SDL_GetAndroidActivity();
	if (!env || !act) return false;
	jclass c = env->GetObjectClass(act);
	env->DeleteLocalRef(act);
	sFetch = env->GetStaticMethodID(c, "fetch", "(Ljava/lang/String;Ljava/lang/String;)Z");
	sFetchBody = env->GetStaticMethodID(c, "fetchBody", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z");
	sInstall = env->GetStaticMethodID(c, "installApk", "(Ljava/lang/String;)Z");
	if (!sFetch || !sFetchBody || !sInstall) {
		env->ExceptionClear();
		env->DeleteLocalRef(c);
		return false;
	}
	sActivityClass = (jclass)env->NewGlobalRef(c);
	env->DeleteLocalRef(c);
	return true;
}

bool callJava(jmethodID m, const char* a, const char* b, const char* c = nullptr) {
	JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();  // attaches this thread
	if (!env) return false;
	jstring ja = env->NewStringUTF(a);
	jstring jb = b ? env->NewStringUTF(b) : nullptr;
	jstring jc = c ? env->NewStringUTF(c) : nullptr;
	const jboolean ok = c ? env->CallStaticBooleanMethod(sActivityClass, m, ja, jb, jc)
	                      : (b ? env->CallStaticBooleanMethod(sActivityClass, m, ja, jb)
	                           : env->CallStaticBooleanMethod(sActivityClass, m, ja));
	if (env->ExceptionCheck()) env->ExceptionClear();
	env->DeleteLocalRef(ja);
	if (jb) env->DeleteLocalRef(jb);
	if (jc) env->DeleteLocalRef(jc);
	return ok;
}
#elif PORT_WEB
// Browser builds use window.fetch() directly for leaderboards and skip GitHub self-updates.
#else
struct Curl {
	void* (*init)();
	int (*setopt)(void*, int, ...);
	int (*perform)(void*);
	void (*cleanup)(void*);
	void* (*slistAppend)(void*, const char*);
	void (*slistFreeAll)(void*);
};
Curl sCurl;

bool curlReady() {
	if (sCurl.init) return true;
	void* lib = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
	if (!lib) lib = dlopen("libcurl-gnutls.so.4", RTLD_NOW | RTLD_LOCAL);
	if (!lib) return false;
	sCurl.setopt = (int (*)(void*, int, ...))dlsym(lib, "curl_easy_setopt");
	sCurl.perform = (int (*)(void*))dlsym(lib, "curl_easy_perform");
	sCurl.cleanup = (void (*)(void*))dlsym(lib, "curl_easy_cleanup");
	sCurl.init = (void* (*)())dlsym(lib, "curl_easy_init");
	sCurl.slistAppend = (void* (*)(void*, const char*))dlsym(lib, "curl_slist_append");
	sCurl.slistFreeAll = (void (*)(void*))dlsym(lib, "curl_slist_free_all");
	return sCurl.init && sCurl.setopt && sCurl.perform && sCurl.cleanup;
}

size_t toFile(char* p, size_t size, size_t n, void* user) {
	const size_t got = fwrite(p, 1, size * n, (FILE*)user);
	SDL_AddAtomicInt(&sGot, (int)got);
	return got;
}

bool fetch(const char* url, FILE* out, const char* postJson = nullptr) {
	void* c = sCurl.init();
	if (!c) return false;
	// libcurl's option numbers, which never change: see curl.h.
	sCurl.setopt(c, 10002, url);                      // URL
	sCurl.setopt(c, 20011, toFile);                   // WRITEFUNCTION
	sCurl.setopt(c, 10001, out);                      // WRITEDATA
	sCurl.setopt(c, 52, 1L);                          // FOLLOWLOCATION
	sCurl.setopt(c, 10018, "FruitSlicer");            // USERAGENT
	sCurl.setopt(c, 78, 20L);                         // CONNECTTIMEOUT
	sCurl.setopt(c, 19, 512L);                        // LOW_SPEED_LIMIT: give up under 512 bytes a second
	sCurl.setopt(c, 20, 30L);                         // LOW_SPEED_TIME: for 30 seconds
	sCurl.setopt(c, 45, 1L);                          // FAILONERROR
	sCurl.setopt(c, 99, 1L);                          // NOSIGNAL: safe on a thread
	void* hdrs = nullptr;
	if (postJson) {
		sCurl.setopt(c, 10015, postJson);             // POSTFIELDS
		if (sCurl.slistAppend) {
			hdrs = sCurl.slistAppend(hdrs, "Content-Type: application/json");
			sCurl.setopt(c, 10023, hdrs);             // HTTPHEADER
		}
	}
	const bool ok = sCurl.perform(c) == 0;
	if (hdrs && sCurl.slistFreeAll) sCurl.slistFreeAll(hdrs);
	sCurl.cleanup(c);
	return ok;
}
#endif

bool fetchTo(const char* url, const char* path, const char* postJson = nullptr) {
#if PORT_ANDROID
	if (postJson) return callJava(sFetchBody, url, postJson, path);
	return callJava(sFetch, url, path);
#elif PORT_WEB
	(void)url;
	(void)path;
	(void)postJson;
	return false;
#else
	FILE* f = fopen(path, "wb");
	if (!f) return false;
	bool ok = fetch(url, f, postJson);
	if (fclose(f) != 0) ok = false;
	return ok;
#endif
}

long long fileSize(const char* path) {
	struct stat st;
	return stat(path, &st) == 0 ? (long long)st.st_size : -1;
}

// ---------------------------------------------------------------- installing
#if defined(_WIN32)
bool runAndWait(const char* cmd) {
	char line[2400];
	snprintf(line, sizeof line, "%s", cmd);
	STARTUPINFOA si = {};
	si.cb = sizeof si;
	PROCESS_INFORMATION pi = {};
	if (!CreateProcessA(nullptr, line, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
	WaitForSingleObject(pi.hProcess, 5 * 60 * 1000);
	DWORD code = 1;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return code == 0;
}

// Unpacks the zip beside the game's data, then writes the script that moves it into place.
bool install() {
	char newDir[700], script[700];
	updateDir(newDir, sizeof newDir, "new");
	updateDir(script, sizeof script, "swap.cmd");
	char cmd[2400], sys[MAX_PATH];
	GetSystemDirectoryA(sys, sizeof sys);
	snprintf(cmd, sizeof cmd, "cmd.exe /c rd /s /q \"%s\" & mkdir \"%s\"", newDir, newDir);
	runAndWait(cmd);
	snprintf(cmd, sizeof cmd, "\"%s\\tar.exe\" -xf \"%s\" -C \"%s\"", sys, sFile, newDir);
	if (!runAndWait(cmd)) return false;
	// The zip holds one folder with the game in it.
	char find[720], inner[1000];
	snprintf(find, sizeof find, "%s\\*", newDir);
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA(find, &fd);
	inner[0] = 0;
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.') snprintf(inner, sizeof inner, "%s\\%s", newDir, fd.cFileName);
		} while (!inner[0] && FindNextFileA(h, &fd));
		FindClose(h);
	}
	char exe[1100];
	snprintf(exe, sizeof exe, "%s\\FruitSlicer.exe", inner);
	if (!inner[0] || fileSize(exe) <= 0) return false;
	char base[700];
	snprintf(base, sizeof base, "%s", SDL_GetBasePath() ? SDL_GetBasePath() : "");
	const size_t baseLen = strlen(base);
	if (baseLen > 3 && base[baseLen - 1] == '\\') base[baseLen - 1] = 0;  // a \ before a closing quote escapes it
	FILE* f = fopen(script, "w");
	if (!f || !base[0]) {
		if (f) fclose(f);
		return false;
	}
	const DWORD pid = GetCurrentProcessId();
	// Wait for this copy of the game to close, copy the new files over it, then start it again. If the copy
	// fails, the old game starts as it was.
	fprintf(f, "@echo off\r\n:wait\r\ntasklist /FI \"PID eq %lu\" 2>nul | find \"%lu\" >nul && (ping -n 2 127.0.0.1 >nul & goto wait)\r\n", (unsigned long)pid, (unsigned long)pid);
	fprintf(f, "xcopy /E /Y /Q /I \"%s\\*\" \"%s\" >nul\r\n", inner, base);
	fprintf(f, "start \"\" \"%s\\FruitSlicer.exe\"\r\n", base);
	fprintf(f, "rd /s /q \"%s\"\r\ndel \"%s\"\r\n", newDir, sFile);
	fclose(f);
	snprintf(sRestartPath, sizeof sRestartPath, "%s", script);
	return true;
}
#elif PORT_ANDROID
bool install() { return callJava(sInstall, sFile, nullptr); }
#elif PORT_WEB
bool install() { return false; }
#else
bool install() {
	if (const char* image = SDL_getenv("APPIMAGE")) {
		// The download sits beside the AppImage; renaming it over the old one is all-or-nothing.
		chmod(sFile, 0755);
		if (rename(sFile, image) != 0) return false;
		snprintf(sRestartPath, sizeof sRestartPath, "%s", image);
		return true;
	}
	char newDir[700], cmd[3600];
	updateDir(newDir, sizeof newDir, "new");
	const char* base = SDL_GetBasePath();
	if (!base) return false;
	snprintf(cmd, sizeof cmd, "rm -rf '%s' && mkdir -p '%s' && tar -xzf '%s' --strip-components=1 -C '%s' && test -x '%s/FruitSlicer'", newDir, newDir, sFile, newDir, newDir);
	if (system(cmd) != 0) return false;
	// Each file is replaced by a new one rather than written into, so the running game is never disturbed.
	snprintf(cmd, sizeof cmd, "cp -rf --remove-destination '%s'/. '%s' && rm -rf '%s' '%s'", newDir, base, newDir, sFile);
	if (system(cmd) != 0) return false;
	snprintf(sRestartPath, sizeof sRestartPath, "%sFruitSlicer", base);
	return true;
}
#endif

int work(void*) {
	bool ok = fetchTo(sUrl, sFile);
	if (sJob == JOB_TEXT) {
		size_t len = 0;
		sText = ok ? (char*)SDL_LoadFile(sFile, &len) : nullptr;
		remove(sFile);
		ok = sText != nullptr;
	} else {
		ok = ok && (unsigned long long)fileSize(sFile) == sExpect && install();
		if (!ok) remove(sFile);
	}
	SDL_SetAtomicInt(&sResult, ok ? 1 : -1);
	return 0;
}

bool begin(Job job, const char* url) {
	if (sThread) {
		if (SDL_GetAtomicInt(&sResult) == 0) return false;  // one at a time
		SDL_WaitThread(sThread, nullptr);
		sThread = nullptr;
	}
#if PORT_ANDROID
	if (!javaReady()) return false;
#elif PORT_WEB
	(void)job;
	(void)url;
	return false;
#elif !defined(_WIN32)
	if (!curlReady()) return false;
#endif
	SDL_free(sText);
	sText = nullptr;
	sJob = job;
	snprintf(sUrl, sizeof sUrl, "%s", url);
	SDL_SetAtomicInt(&sResult, 0);
	SDL_SetAtomicInt(&sGot, 0);
	sThread = SDL_CreateThread(work, "update", nullptr);
	return sThread != nullptr;
}

// Can this build replace itself? Only where its own files can be written.
bool writable(const char* dir) {
	if (!dir) return false;
	char probe[700];
	snprintf(probe, sizeof probe, "%s.fs-write-test", dir);
	FILE* f = fopen(probe, "wb");
	if (!f) return false;
	fclose(f);
	remove(probe);
	return true;
}
}  // namespace

namespace port {
const char* updateRepo() { return "RealAnxiousGit/fruit-slicer"; }

const char* updateAsset() {
	static int known = -1;
	static const char* asset = nullptr;
	if (known < 0) {
		known = 1;
#if defined(_WIN32)
		if (writable(SDL_GetBasePath())) asset = "-windows-x64.zip";
#elif PORT_ANDROID
		asset = "-android.apk";
#else
		if (const char* image = SDL_getenv("APPIMAGE")) {
			char dir[700];
			snprintf(dir, sizeof dir, "%s", image);
			char* slash = strrchr(dir, '/');
			if (slash) slash[1] = 0;
			if (slash && writable(dir)) asset = "-x86_64.AppImage";
		} else if (writable(SDL_GetBasePath())) {
			asset = "-linux-x86_64.tar.gz";
		}
#endif
	}
	return asset;
}

bool netGetText(const char* url) {
	updateDir(sFile, sizeof sFile, "release.json");
	return begin(JOB_TEXT, url);
}

bool netDownload(const char* url, unsigned long long size) {
	if (!updateAsset()) return false;
#if !defined(_WIN32) && !PORT_ANDROID
	if (const char* image = SDL_getenv("APPIMAGE")) {
		snprintf(sFile, sizeof sFile, "%s.new", image);
	} else
#endif
		updateDir(sFile, sizeof sFile, "download");
	sExpect = size;
	return begin(JOB_FILE, url);
}

int netPoll(float* progress) {
	const int r = SDL_GetAtomicInt(&sResult);
#if PORT_ANDROID
	const long long got = fileSize(sFile);  // Java writes it, so its size is the progress
#else
	const long long got = SDL_GetAtomicInt(&sGot);
#endif
	if (progress) *progress = sExpect && got > 0 ? (float)got / (float)sExpect : 0;
	if (r != 0 && sThread) {
		SDL_WaitThread(sThread, nullptr);
		sThread = nullptr;
	}
	return r;
}

char* netText() { return sText; }

void updateRestart() { sRestart = sRestartPath[0] != 0; }

// Called by the main loop once the game has shut down.
void updateAfterQuit() {
	if (!sRestart) return;
#if defined(_WIN32)
	char cmd[800];
	snprintf(cmd, sizeof cmd, "cmd.exe /c \"%s\"", sRestartPath);
	STARTUPINFOA si = {};
	si.cb = sizeof si;
	PROCESS_INFORMATION pi = {};
	if (CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
#elif !PORT_ANDROID && !PORT_WEB
	char* argv[] = {sRestartPath, nullptr};
	execv(sRestartPath, argv);
#endif
}

// Independent HTTP/HTTPS slot for Online Leaderboards (never blocks or cancels update checks).
static char* sLbText;
#if PORT_WEB
bool lbRequest(const char* url, const char* jsonBody) {
	if (!url || !url[0]) return false;
	SDL_free(sLbText);
	sLbText = nullptr;
	const char* body = (jsonBody && jsonBody[0]) ? jsonBody : "";
	EM_ASM({
		const url = UTF8ToString($0);
		const body = UTF8ToString($1);
		window._fsLbState = 0;
		window._fsLbBytes = null;
		var o = new Object();
		o.method = body ? 'POST' : 'GET';
		if (body) {
			var h = new Headers();
			h.append('Content-Type', 'application/json');
			o.headers = h;
			o.body = body;
		}
		fetch(url, o)
			.then(r => { if (!r.ok) throw new Error('http ' + r.status); return r.text(); })
			.then(txt => { window._fsLbBytes = new TextEncoder().encode(txt); window._fsLbState = 1; })
			.catch(() => { window._fsLbState = -1; });
	}, url, body);
	return true;
}

int lbPoll() {
	const int r = EM_ASM_INT({ return window._fsLbState || 0; });
	if (r > 0 && !sLbText) {
		const int len = EM_ASM_INT({ return window._fsLbBytes ? window._fsLbBytes.length : 0; });
		sLbText = (char*)SDL_malloc((size_t)len + 1);
		if (!sLbText) return -1;
		EM_ASM({
			if (window._fsLbBytes) HEAPU8.set(window._fsLbBytes, $0);
			HEAPU8[$0 + $1] = 0;
		}, sLbText, len);
	}
	return r;
}

const char* lbText() { return sLbText; }
#else
static SDL_Thread* sLbThread;
static SDL_AtomicInt sLbResult;
static char sLbUrl[600], sLbBody[512], sLbFile[700];
static bool sLbHasBody;

static int lbWork(void*) {
	const bool ok = fetchTo(sLbUrl, sLbFile, sLbHasBody ? sLbBody : nullptr);
	size_t len = 0;
	sLbText = ok ? (char*)SDL_LoadFile(sLbFile, &len) : nullptr;
	remove(sLbFile);
	SDL_SetAtomicInt(&sLbResult, sLbText != nullptr ? 1 : -1);
	return 0;
}

bool lbRequest(const char* url, const char* jsonBody) {
	if (sLbThread) {
		if (SDL_GetAtomicInt(&sLbResult) == 0) return false;
		SDL_WaitThread(sLbThread, nullptr);
		sLbThread = nullptr;
	}
#if PORT_ANDROID
	if (!javaReady()) return false;
#elif !defined(_WIN32)
	if (!curlReady()) return false;
#endif
	SDL_free(sLbText);
	sLbText = nullptr;
	updateDir(sLbFile, sizeof sLbFile, "leaderboard.json");
	snprintf(sLbUrl, sizeof sLbUrl, "%s", url ? url : "");
	sLbHasBody = jsonBody && jsonBody[0];
	if (sLbHasBody) snprintf(sLbBody, sizeof sLbBody, "%s", jsonBody);
	else sLbBody[0] = 0;
	SDL_SetAtomicInt(&sLbResult, 0);
	sLbThread = SDL_CreateThread(lbWork, "lb", nullptr);
	return sLbThread != nullptr;
}

int lbPoll() {
	const int r = SDL_GetAtomicInt(&sLbResult);
	if (r != 0 && sLbThread) {
		SDL_WaitThread(sLbThread, nullptr);
		sLbThread = nullptr;
	}
	return r;
}

const char* lbText() { return sLbText; }
#endif
}  // namespace port
