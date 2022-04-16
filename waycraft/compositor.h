struct egl {
	EGLDisplay *display;
	EGLSurface *surface;
	EGLContext *context;
};

struct backend_memory;
struct game_window_manager;

static i32 compositor_init(struct backend_memory *memory, struct egl *egl,
	struct game_window_manager *wm, i32 keymap, i32 keymap_size);
static struct game_window_manager *compositor_update(struct backend_memory *memory);
static void compositor_send_key(struct backend_memory *memory, i32 key, i32 state);
static void compositor_send_button(struct backend_memory *memory, i32 key, i32 state);
static void compositor_send_motion(struct backend_memory *memory, i32 x, i32 y);
static void compositor_send_modifiers(struct backend_memory *memory,
	u32 depressed, u32 latched, u32 locked, u32 group);
static void compositor_finish(struct backend_memory *memory);
