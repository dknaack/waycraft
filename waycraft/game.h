enum game_window_flags {
	WINDOW_INITIALIZED = 1 << 0,
	WINDOW_VISIBLE     = 1 << 1,
	WINDOW_DESTROYED   = 1 << 2,
};

/*
 * NOTE: if the window has no parent then it is a toplevel surface. A toplevel
 * surface should be the only one that can be moved by the window manager. The
 * positions of the other windows are determined by the compositor.
 */
struct game_window {
	u32 flags;
	u32 texture;
	u32 parent;

	v3 position;
	m3x3 rotation;
	v2 scale;
};

struct game_window_manager {
	struct game_window *windows;
	u32 focused_window;
	u32 window_count;
	u32 is_active;

	struct game_cursor {
		v2 position;
		v2 offset;
		v2 scale;
		u32 texture;
	} cursor;
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
	MOD_SHIFT = 1 << 0,
	MOD_CTRL  = 1 << 1,
	MOD_ALT   = 1 << 2,
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

struct backend_memory {
    void *data;
    usize size;

    u32 is_initialized;
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

static inline struct game_window *
window_manager_get_window(struct game_window_manager *wm, u32 id)
{
	return id ? wm->windows + id - 1 : 0;
}

static inline struct game_window *
window_manager_get_focused_window(struct game_window_manager *wm)
{
	return window_manager_get_window(wm, wm->focused_window);
}

static inline u32
window_manager_get_window_id(struct game_window_manager *wm,
		struct game_window *window)
{
	assert(!window || window - wm->windows < wm->window_count);

	return window ? window - wm->windows + 1 : 0;
}

void game_update(struct backend_memory *memory, struct game_input *input,
	struct game_window_manager *window_manager);
