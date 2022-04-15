#ifndef WAYCRAFT_XWAYLAND_H
#define WAYCRAFT_XWAYLAND_H

#include <xcb/xcb.h>

struct xwayland_surface {
	xcb_window_t window;
	u32 wl_surface;
};

enum xwm_atoms {
    XWM_NET_WM_NAME,
	XWM_NET_SUPPORTING_WM_CHECK,
	XWM_NET_ACTIVE_WINDOW,
	XWM_WINDOW,
	XWM_WL_SURFACE_ID,
	XWM_WM_PROTOCOLS,
	XWM_WM_S0,
    XWM_ATOM_COUNT
};

struct xwm {
    xcb_connection_t *connection;
    xcb_screen_t *screen;
    xcb_window_t window;
    xcb_atom_t atoms[XWM_ATOM_COUNT];

	struct xwayland_surface unpaired_surfaces[256];
	u32 unpaired_surface_count;

    struct wl_client *client;
    struct wl_event_source *event_source;
	struct wl_listener destroy_listener;
	struct wl_listener new_surface;
	struct wl_signal destroy_notify;
};

struct xwayland {
    struct xwm xwm;

    struct wl_display *display;
	struct wl_event_source *usr1_source;

    u32 display_number;
    i32 x_fd[2];
    i32 wm_fd[2];
    i32 wl_fd[2];
};

static i32 xwayland_init(struct xwayland *xwayland, struct compositor *compositor);
static void xwayland_finish(struct xwayland *xwayland);

static void xwm_focus(struct xwm *xwm, struct xwayland_surface *surface);

#endif /* WAYCRAFT_XWAYLAND_H */
