#include "jobs.h"

#include <stdio.h>

namespace jobs {
static constexpr int kQueue = 32;
struct Job {
	Fn fn;
	void* arg;
};
static Job sQueue[kQueue];
static volatile int sHead, sTail;
static LightEvent sWake, sDone;
static Thread sThread;
static volatile bool sQuit;
static bool sEnabled = true;

static void workerMain(void*) {
	while (true) {
		LightEvent_Wait(&sWake);
		if (sQuit) return;
		while (sTail != sHead) {
			Job j = sQueue[sTail];
			j.fn(j.arg);
			__dmb();
			sTail = (sTail + 1) % kQueue;
		}
		LightEvent_Signal(&sDone);
	}
}

void init() {
	bool isNew = false;
	APT_CheckNew3DS(&isNew);
	if (!isNew) return;
	LightEvent_Init(&sWake, RESET_ONESHOT);
	LightEvent_Init(&sDone, RESET_ONESHOT);
	s32 prio = 0x30;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	sThread = threadCreate(workerMain, nullptr, 32 * 1024, prio - 1, 2, false);
	DLOG("[jobs] worker on core 2: %s\n", sThread ? "yes" : "no, running inline");
}

void shutdown() {
	if (!sThread) return;
	sQuit = true;
	LightEvent_Signal(&sWake);
	threadJoin(sThread, U64_MAX);
	threadFree(sThread);
	sThread = nullptr;
}

bool threaded() { return sThread != nullptr && sEnabled; }

void setEnabled(bool on) {
	wait();
	sEnabled = on;
}

void run(Fn fn, void* arg) {
	if (!sThread || !sEnabled) {
		fn(arg);
		return;
	}
	int next = (sHead + 1) % kQueue;
	while (next == sTail) svcSleepThread(100000);  // queue full: wait for the worker to catch up
	sQueue[sHead] = {fn, arg};
	__dmb();
	sHead = next;
	LightEvent_Signal(&sWake);
}

void wait() {
	if (!sThread) return;
	while (sTail != sHead) {
		LightEvent_WaitTimeout(&sDone, 2000000);
	}
}
}  // namespace jobs
