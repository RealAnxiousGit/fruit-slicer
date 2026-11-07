// These platforms have time to spare, so jobs simply run where they are asked for.
#include "jobs.h"

namespace jobs {
void init() {}
void shutdown() {}
bool threaded() { return false; }
void run(Fn fn, void* arg) { fn(arg); }
void wait() {}
void setEnabled(bool) {}
}  // namespace jobs
