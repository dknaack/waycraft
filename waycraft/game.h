#include <waycraft/types.h>
#include <waycraft/backend.h>
#include <waycraft/memory.h>
#include <waycraft/gl.h>
#include <waycraft/timer.h>
#include <waycraft/log.h>
#include <waycraft/block.h>
#include <waycraft/inventory.h>
#include <waycraft/renderer.h>
#include <waycraft/world.h>

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

struct game_state {
	struct world world;
	struct camera camera;
	struct memory_arena arena;
	struct renderer renderer;
	struct game_window *hot_window;

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
