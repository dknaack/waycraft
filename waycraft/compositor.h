struct egl {
	EGLDisplay *display;
	EGLSurface *surface;
	EGLContext *context;
};

enum surface_role {
	SURFACE_ROLE_NONE,
	SURFACE_ROLE_TOPLEVEL,
	SURFACE_ROLE_SUBSURFACE,
	SURFACE_ROLE_XWAYLAND,
	SURFACE_ROLE_CURSOR,
	SURFACE_ROLE_COUNT
};

enum surface_flags {
	SURFACE_NEW_BUFFER = 1 << 0,
	SURFACE_NEW_ROLE   = 1 << 1,
	SURFACE_NEW_FRAME  = 1 << 2,
};

struct surface_state {
	u32 flags;
	u32 role;

	struct wl_resource *buffer;
	struct wl_resource *frame_callback;
};

struct surface {
	u32 texture;

	struct wl_resource *resource;
	union {
		struct {
			struct wl_resource *surface;
			struct wl_resource *toplevel;
		} xdg;

		struct wl_resource *parent_surface;
		struct xwayland_surface xwayland_surface;

		struct {
			f32 hotspot_x;
			f32 hotspot_y;
		} cursor;
	};

	u32 width;
	u32 height;

	struct game_window *window;
	struct compositor *compositor;
	struct surface_state pending;
	struct surface_state current;
};

struct compositor {
	struct game_window_manager window_manager;
	struct memory_arena arena;
	struct xwayland xwayland;
	struct wl_display *display;
	EGLDisplay *egl_display;

	struct wl_global *compositor;
	struct wl_global *output;
	struct wl_global *xdg_wm_base;
	struct wl_global *subcompositor;
	struct wl_global *data_device_manager;
	struct wl_global *seat;

	struct wl_list keyboards;
	struct wl_list pointers;
	struct wl_signal new_surface;
	struct wl_listener xwayland_surface_destroy;
	struct surface *surfaces;
	u32 surface_count;
	u32 focused_surface;

	struct {
		u32 depressed;
		u32 latched;
		u32 locked;
		u32 group;
	} modifiers;

	i32 keymap;
	i32 keymap_size;
};

