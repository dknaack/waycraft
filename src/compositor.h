#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "types.h"

struct egl;

struct compositor_window {
    u32 texture;
};

struct compositor {
    void (*update)(struct compositor *compositor);
    void (*send_key)(struct compositor *compositor, i32 key, i32 state);
    void (*send_button)(struct compositor *compositor, i32 key, i32 state);
    void (*finish)(struct compositor *compositor);

    struct compositor_window *windows;
    u32 window_count;
    u32 active_window;
    u8 is_active;
    m4x4 transform;
};

struct backend_memory;
struct compositor *compositor_init(struct backend_memory *memory, 
                                   struct egl *egl);

#endif /* COMPOSITOR_H */ 
