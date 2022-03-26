#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "types.h"

struct egl;

struct compositor_surface {
    u32 texture;
};

struct compositor {
    void (*update)(struct compositor *compositor);
    void (*flush)(struct compositor *compositor);
    void (*send_key)(struct compositor *compositor, i32 key, i32 state);
    void (*send_button)(struct compositor *compositor, i32 key, i32 state);
    void (*send_motion)(struct compositor *compositor, i32 x, i32 y);
    void (*send_modifiers)(struct compositor *compositor, u32 depressed,
                           u32 latched, u32 locked, u32 group);
    void (*finish)(struct compositor *compositor);

    struct compositor_surface *windows;
    struct compositor_surface *active_window;
    u32 window_count;
    u8 is_active;
    m4x4 transform;
    v2 cursor_pos;
};

struct backend_memory;
struct compositor *compositor_init(struct backend_memory *memory, 
                                   struct egl *egl, i32 keymap, i32 keymap_size);

#endif /* COMPOSITOR_H */ 
