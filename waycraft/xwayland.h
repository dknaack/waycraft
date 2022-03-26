#ifndef WAYCRAFT_XWAYLAND_H
#define WAYCRAFT_XWAYLAND_H

#include <xcb/xcb.h>

enum xwm_atoms {
    XWM_NET_WM_NAME,
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

    u32 display_number;
    i32 x_fd[2];
    i32 wm_fd[2];
    i32 wl_fd[2];
};

i32 xwayland_init(struct xwayland *xwayland, struct wl_display *display);
void xwayland_finish(struct xwayland *xwayland);

#endif /* WAYCRAFT_XWAYLAND_H */ 
