#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "types.h"

struct egl;

struct compositor_window {
    u32 texture;
};

struct compositor_state {
    struct compositor_window *windows;
    u32 window_count;
    u32 active_window;
    u8 is_active;
    m4x4 transform;
};

void compositor_update(struct backend_memory *memory, struct egl *egl,
                       struct compositor_state *state);
void compositor_update_key(struct compositor_state *compositor, i32 key, i32 state);
void compositor_update_button(struct compositor_state *compositor, i32 key, i32 state);
void compositor_finish(struct backend_memory *memory);

#endif /* COMPOSITOR_H */ 
