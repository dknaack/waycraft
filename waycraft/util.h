struct timer {
	const char *name;
	f64 start;
};

enum log_level {
	LOG_INFO,
	LOG_DEBUG,
	LOG_WARN,
	LOG_ERR,
	LOG_LEVEL_COUNT
};

struct memory_arena {
	u8 *data;
	usize size;
	usize used;
};

#define arena_alloc(arena, count, type) \
    ((type *)arena_alloc_(arena, count * sizeof(type)))

#define log_info(...) log_(LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_debug(...) log_(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_warn(...) log_(LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_err(...) log_(LOG_ERR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#if 0
#define timer_name(name) __timer_ ## name
#define timer_begin(name) struct timer timer_name(name) = timer_begin_(""#name)
#define timer_end(name) timer_end_(&timer_name(name))
#define timer_begin_func() struct timer __timer_func = timer_begin_(__func__)
#define timer_end_func() timer_end_(&__timer_func)
#else
#define timer_name(name)
#define timer_begin(name)
#define timer_end(name)
#define timer_begin_func()
#define timer_end_func()
#endif
