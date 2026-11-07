#pragma once
#include "common.h"

// A single background worker. On a New 3DS it runs on the extra CPU core; anywhere else, or if the
// core isn't available, jobs simply run immediately on the calling thread.
namespace jobs {
using Fn = void (*)(void*);
void init();
void shutdown();
bool threaded();
void run(Fn fn, void* arg);  // queues a job; at most a handful per frame
void wait();                 // returns once every queued job has finished
void setEnabled(bool on);    // the speed test turns this off to stand in for an Old 3DS
}  // namespace jobs
