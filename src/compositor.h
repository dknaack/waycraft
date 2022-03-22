#ifndef COMPOSITOR_H
#define COMPOSITOR_H

struct server {
    struct wl_display *display;
    EGLDisplay *egl_display;

    struct wl_list clients;
    struct wl_list surfaces;
};

struct egl;

i32 compositor_init(struct server *server, struct egl *egl);
void compositor_finish(struct server *server);

#endif /* COMPOSITOR_H */ 
