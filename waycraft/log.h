#ifndef WAYCRAFT_LOG_H
#define WAYCRAFT_LOG_H

enum log_level {
	LOG_INFO,
	LOG_DEBUG,
	LOG_WARN,
	LOG_ERR,
	LOG_LEVEL_COUNT
};

static void log_(enum log_level level, const char *file, u32 line,
	const char *func, const char *fmt, ...);

#define log_info(...) log_(LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_debug(...) log_(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_warn(...) log_(LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_err(...) log_(LOG_ERR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif /* WAYCRAFT_LOG_H */
