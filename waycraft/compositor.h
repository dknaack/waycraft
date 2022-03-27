#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "types.h"

struct egl;
struct backend_memory;

static i32 compositor_init(struct backend_memory *memory, struct egl *egl,
	i32 keymap, i32 keymap_size);
static void compositor_update(struct backend_memory *memory);
static void compositor_send_key(struct backend_memory *memory, i32 key, i32 state);
static void compositor_send_button(struct backend_memory *memory, i32 key, i32 state);
static void compositor_send_motion(struct backend_memory *memory, i32 x, i32 y);
static void compositor_send_modifiers(struct backend_memory *memory,
	u32 depressed, u32 latched, u32 locked, u32 group);
static void compositor_finish(struct backend_memory *memory);

#endif /* COMPOSITOR_H */
