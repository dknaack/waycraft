#ifndef TIMER_H
#define TIMER_H

#include <waycraft/types.h>

struct timer {
    const char *name;
    f64 start;
};

static struct timer timer_begin_(const char *name);
static void timer_end_(struct timer *timer);

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

#endif /* TIMER_H */
