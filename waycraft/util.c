#include <time.h>
#include <string.h>

static f64
get_time_sec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static struct arena
arena_init(void *data, u64 size)
{
	struct arena arena = {0};
	arena.data = data;
	arena.size = size;
	arena.used = 0;
	return arena;
}

static void *
alloc(struct arena *arena, usz count, usz size)
{
	assert(arena->used + count * size < arena->size);
	void *ptr = arena->data + arena->used;
	arena->used += count * size;
	return ptr;
}

static struct arena
arena_create(usz size, struct arena *arena)
{
	struct arena result;
	result.data = alloc(arena, 1, size);
	result.size = size;
	return result;
}

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

static const char *log_str[LOG_LEVEL_COUNT] = {
	[LOG_INFO] = "info",
	[LOG_DEBUG] = "debug",
	[LOG_WARN] = "warn",
	[LOG_ERR] = "error",
};

static u64
log_time(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void
log_(enum log_level level, const char *file, u32 line, const char *func,
    const char *fmt, ...)
{
	va_list ap;

	u64 time = log_time();
	fprintf(stderr, "[%7ld.%-3ld] %5s %s:%d:%s: ", time / 1000, time % 1000,
	    log_str[level], file, line, func);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}
}
