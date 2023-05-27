#include <EGL/egl.h>
#include <wayland-server.h>
#include <xdg-shell-server-protocol.h>
#include <xcb/xcb.h>
#include <xcb/composite.h>
#include <xcb/xfixes.h>

#include <waycraft/xwayland.h>

#define WL_COMPOSITOR_VERSION 5
#define WL_DATA_DEVICE_MANAGER_VERSION 3
#define WL_OUTPUT_VERSION 4
#define WL_SEAT_VERSION 7
#define WL_SUBCOMPOSITOR_VERSION 1
#define XDG_WM_BASE_VERSION 4

#define MAX_SURFACE_COUNT 256
#define MAX_WINDOW_COUNT 256
#define MAX_RECT_COUNT 8

struct wl_resource;

enum surface_role {
	SURFACE_ROLE_NONE,
	SURFACE_ROLE_XDG_TOPLEVEL,
	SURFACE_ROLE_XDG_POPUP,
	SURFACE_ROLE_SUBSURFACE,
	SURFACE_ROLE_DRAG_AND_DROP_ICON,
	SURFACE_ROLE_XWAYLAND,
	SURFACE_ROLE_CURSOR,
	SURFACE_ROLE_COUNT
};

enum surface_flags {
	SURFACE_NEW_BUFFER = 1 << 0,
	SURFACE_NEW_FRAME  = 1 << 1,
};

struct surface_state {
	u32 flags;
	struct wl_resource *buffer;
	struct wl_resource *frame_callback;
};

struct surface {
	u32 role;
	struct wl_resource *resource;

	union {
		struct {
			struct wl_resource *surface;
			struct wl_resource *toplevel;
		} xdg_toplevel;

		struct {
			struct wl_resource *surface;
			struct wl_resource *parent;
			struct wl_resource *positioner;
		} xdg_popup;

		struct {
			struct wl_resource *parent;
		} subsurface;

		struct xwayland_surface xwayland_surface;

		struct {
			f32 hotspot_x;
			f32 hotspot_y;
		} cursor;
	};

	u32 texture;
	u32 width;
	u32 height;

	struct game_window *window;
	struct surface_state pending;
	struct surface_state current;
};

enum region_mode {
	REGION_ADD,
	REGION_SUBTRACT,
};

struct region {
	struct {
		box2 rect;
		u8 mode;
	} entries[MAX_RECT_COUNT];
	u8 count;
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

static struct opengl_api gl;

static bool surface_set_role(struct surface *surface, u32 role,
	struct wl_resource *resource, u32 error);
static void surface_destroy_resource(struct wl_resource *resource);
