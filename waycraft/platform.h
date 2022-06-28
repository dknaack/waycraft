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
		u8 toggle_inventory;
	} controller;

	f32 dt;
	u32 width;
	u32 height;
	bool shift_down;
	bool ctrl_down;
	bool alt_down;
};

struct platform_memory {
    void *data;
    usize size;

    bool is_initialized;
	struct opengl_api *gl;
};

typedef void game_update_t(struct platform_memory *memory, struct game_input *input,
	struct game_window_manager *window_manager);

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
