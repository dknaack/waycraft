#include <time.h>

#include <waycraft/timer.h>

static i32 timer_initialized = 0;
static FILE *timer_output;

static void
timer_finish(void)
{
	fprintf(timer_output, "{}]\n");
}

struct timer
timer_begin_(const char *name)
{
	struct timer timer;
	struct timespec ts;

	if (!timer_initialized) {
		atexit(timer_finish);
		timer_initialized = 1;
		timer_output = fopen("trace.json", "w");

		fprintf(timer_output, "[\n");
	}

	clock_gettime(CLOCK_REALTIME, &ts);

	timer.name = name;
	timer.start = ts.tv_sec * 1e6 + 1e-3 * ts.tv_nsec;
	return timer;
}

void
timer_end_(struct timer *timer)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	f64 current_time = ts.tv_sec * 1e6 + 1e-3 * ts.tv_nsec;
	f64 start_time = timer->start;
	f64 duration = current_time - start_time;

	fprintf(timer_output, "{"
		"\"ph\": \"X\","
		"\"cat\": \"PERF\","
		"\"tid\": 1,"
		"\"pid\": 1,"
		"\"name\": \"%s\","
		"\"ts\": %f,"
		"\"dur\": %f"
		"},",
		timer->name, timer->start, duration);
}
