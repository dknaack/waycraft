#ifndef GAME_H
#define GAME_H

#include <waycraft/types.h>
#include <waycraft/world.h>
#include <waycraft/memory.h>
#include <waycraft/inventory.h>
#include <waycraft/renderer.h>

struct game_window {
	v3 position;
	v3 x_axis;
	v3 y_axis;
	v3 z_axis;
	u32 texture;
	u32 is_initialized;
};

struct game_window_manager {
	struct game_window *windows;
	struct game_window *hot_window;
	struct game_window *active_window;

	u32 window_count;
	u32 is_active;
	v2 cursor_pos;
};

struct camera {
	m4x4 view;
	m4x4 projection;
	v3 position;
	v3 front;
	v3 up;
	v3 right;
	f32 yaw;
	f32 pitch;
	f32 fov;
};

enum game_input_modifiers {
	MOD_SHIFT = 0x1,
	MOD_CTRL  = 0x2,
	MOD_ALT   = 0x4,
};

struct game_input {
	struct {
		f32 dx, dy;
		f32 x, y;
		u8 buttons[8];
	} mouse;

	struct {
		u8 move_up;
		u8 move_down;
		u8 move_left;
		u8 move_right;
		u8 jump;
		u8 modifiers;
		u8 toggle_inventory;
	} controller;

	f32 dt;
	u32 width;
	u32 height;
};

struct game_state {
	struct world world;
	struct camera camera;
	struct memory_arena arena;
	struct renderer renderer;

	struct player {
		f32 speed;
		v3 position;
		v3 velocity;
		u8 is_jumping;
		u8 frames_since_jump;

		struct inventory inventory;
		i8 hotbar_selection;
	} player;

	u32 window_vertex_array;
	u32 window_vertex_buffer;
	u32 window_index_buffer;
	v3 mouse_pos;
	v2 cursor_pos;

	u32 cursor;
};

struct backend_memory;
struct compositor;

static inline i32
button_was_pressed(u8 button)
{
	return (button & 0x3) == 0x1;
}

static inline i32
button_was_released(u8 button)
{
	return (button & 0x3) == 0x2;
}

static inline i32
button_is_down(u8 button)
{
	return button & 0x1;
}

void game_update(struct backend_memory *memory, struct game_input *input,
	struct game_window_manager *window_manager);

#endif /* GAME_H */
