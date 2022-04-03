#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <waycraft/log.h>

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

	clock_gettime(CLOCK_MONOTONIC, &ts);

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

	fputc('\n', stderr);
}
