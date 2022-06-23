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
		u8 modifiers;
		u8 jump;
		u8 toggle_inventory;
	} controller;

	f32 dt;
	u32 width;
	u32 height;
};

struct backend_memory {
    void *data;
    usize size;

    u32 is_initialized;
};

void game_update(struct backend_memory *memory, struct game_input *input,
	struct game_window_manager *window_manager);
