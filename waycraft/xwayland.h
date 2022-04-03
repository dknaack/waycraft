#ifndef WAYCRAFT_XWAYLAND_H
#define WAYCRAFT_XWAYLAND_H

#include <xcb/xcb.h>

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

    struct wl_event_source *event_source;
};

struct xwayland {
    struct xwm xwm;

    struct wl_display *display;
    struct wl_client *client;
	struct wl_event_source *usr1_source;

    u32 display_number;
    i32 x_fd[2];
    i32 wm_fd[2];
    i32 wl_fd[2];
};

static i32 xwayland_init(struct xwayland *xwayland, struct wl_display *display);
static void xwayland_finish(struct xwayland *xwayland);

#endif /* WAYCRAFT_XWAYLAND_H */
