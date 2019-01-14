#include "../gx_gui.h"
#include <sys/time.h>
#include <time.h>

uint64_t timeMillis() {
	uint64_t now;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	now = tv.tv_sec * (uint64_t)1000;
	now += tv.tv_usec / (uint64_t)1000;
	return now;
}

void sleepMillis(int64_t millis) {
	struct timespec ts;
	ts.tv_sec = millis / 1000;
	ts.tv_nsec = (millis % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

void parkThread() {
	// FIXME: sched_yield();
	sleepMillis(0);
}
