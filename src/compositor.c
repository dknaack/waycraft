#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>
#include <xdg-shell-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include <time.h>

#include "backend.h"
#include "compositor.h"
#include "egl.h"
#include "game.h"
#include "gl.h"

#define WL_COMPOSITOR_VERSION 5
#define WL_REGION_VERSION 1
#define WL_SEAT_VERSION 7
#define WL_SURFACE_VERSION 5
#define XDG_WM_BASE_VERSION 4
#define XDG_TOPLEVEL_VERSION 4
#define XDG_SURFACE_VERSION 4

#define MAX_SURFACE_COUNT 256
#define MAX_CLIENT_COUNT  256

static PFNEGLQUERYWAYLANDBUFFERWLPROC eglQueryWaylandBufferWL = 0;
static PFNEGLBINDWAYLANDDISPLAYWLPROC eglBindWaylandDisplayWL = 0;

struct wc_client {
    struct wl_client *client;

    struct wl_resource *pointer;

    struct wl_list link;
};

struct wc_surface {
    struct wl_resource *surface;
    struct wl_resource *buffer;
    struct wl_resource *xdg_surface;
    struct wl_resource *xdg_toplevel;
    struct wl_resource *frame_callback;

    i32 x;
    i32 y;
    i32 width;
    i32 height;
    u32 texture;

    struct wc_compositor *server;
    struct wc_client *client;
    struct wl_list link;
};

struct wc_compositor {
    struct compositor base;

    struct wl_display *display;
    struct wl_event_loop *event_loop;
    EGLDisplay *egl_display;

    struct memory_arena arena;
    struct wc_surface *surfaces;
    struct wc_client *clients;

    // NOTE: surface_count is the same as window_count
    u32 client_count;
};

static struct wc_surface *
server_create_surface(struct wc_compositor *compositor)
{
    u32 surface_count = compositor->base.window_count;
    struct wc_surface *surface = compositor->surfaces + surface_count++;
    surface->server = compositor;
    compositor->base.window_count = surface_count;
    assert(surface_count < MAX_SURFACE_COUNT);

    return surface;
}

static struct wc_client *
server_create_client(struct wc_compositor *compositor)
{
    struct wc_client *client = calloc(1, sizeof(struct wc_client));
    compositor->client_count++;
    assert(compositor->client_count < MAX_CLIENT_COUNT);

    return client;
}

static void
do_nothing()
{
}

static const struct wl_region_interface wl_region_implementation = {
    .destroy  = do_nothing,
    .add      = do_nothing,
    .subtract = do_nothing,
};

static void
wl_region_resource_destroy(struct wl_resource *resource)
{
    /* TODO */
}

/*
 * wl_surface functions
 */

static void
wl_surface_destroy(struct wl_client *client,
                   struct wl_resource *resource)
{
    struct wc_surface *surface = wl_resource_get_user_data(resource);
    wl_list_remove(&surface->link);
}

static void
wl_surface_attach(struct wl_client *client, struct wl_resource *resource, 
                  struct wl_resource *buffer, i32 x, i32 y)
{
    struct wc_surface *surface = wl_resource_get_user_data(resource);
    surface->buffer = buffer;
}

static void
wl_surface_damage(struct wl_client *client,
                  struct wl_resource *resource,
                  i32 x,
                  i32 y,
                  i32 width,
                  i32 height)
{
    /* TODO */
}

static void
wl_surface_frame(struct wl_client *client,
                 struct wl_resource *resource, u32 callback)
{
    puts("wl_surface::frame");
    struct wc_surface *surface = wl_resource_get_user_data(resource);

    surface->frame_callback = 
        wl_resource_create(client, &wl_callback_interface, 1, callback);
}

static void
wl_surface_set_opaque_region(struct wl_client *client,
                             struct wl_resource *resource,
                             struct wl_resource *region)
{
    /* TODO */
}

static void
wl_surface_set_input_region(struct wl_client *client,
                            struct wl_resource *resource,
                            struct wl_resource *region)
{
    /* TODO */
}

static void
wl_surface_commit(struct wl_client *client,
                  struct wl_resource *resource)
{
    struct wc_surface *surface = wl_resource_get_user_data(resource);
    struct wc_compositor *server = surface->server;
    EGLDisplay *egl_display = server->egl_display;
    i32 texture_format;

    if (!surface->buffer) {
        xdg_surface_send_configure(surface->xdg_surface, 0);
    } else if (eglQueryWaylandBufferWL(egl_display, surface->buffer,
                                       EGL_TEXTURE_FORMAT, &texture_format)) {
        i32 width, height;
        eglQueryWaylandBufferWL(egl_display, surface->buffer, EGL_WIDTH, &width);
        eglQueryWaylandBufferWL(egl_display, surface->buffer, EGL_HEIGHT, &height);

        i64 attributes[] = { EGL_NONE };
        EGLImage image = eglCreateImage(egl_display, EGL_NO_CONTEXT, 
                                        EGL_WAYLAND_BUFFER_WL, surface->buffer,
                                        attributes);

        if (surface->texture) {
            gl.DeleteTextures(1, &surface->texture);
        }

        u32 texture = 0;
        gl.GenTextures(1, &texture);
        gl.BindTexture(GL_TEXTURE_2D, texture);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.EGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
        gl.BindTexture(GL_TEXTURE_2D, 0);
        surface->texture = texture;

        wl_buffer_send_release(surface->buffer);
    } else {
        struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get(surface->buffer);
        u32 width = wl_shm_buffer_get_width(shm_buffer);
        u32 height = wl_shm_buffer_get_height(shm_buffer);
        void *data = wl_shm_buffer_get_data(shm_buffer);

        if (surface->texture) {
            gl.DeleteTextures(1, &surface->texture);
        }

        u32 texture = 0;
        gl.GenTextures(1, &texture);
        gl.BindTexture(GL_TEXTURE_2D, texture);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
                      GL_BGRA, GL_UNSIGNED_BYTE, data);
        gl.BindTexture(GL_TEXTURE_2D, 0);
        surface->texture = texture;

        wl_buffer_send_release(surface->buffer);
    }
}

static void
wl_surface_set_buffer_transform(struct wl_client *client,
                                struct wl_resource *resource, i32 transform)
{
    /* TODO */
}

static void
wl_surface_set_buffer_scale(struct wl_client *client,
                            struct wl_resource *resource, i32 scale)
{
    /* TODO */
}

static void
wl_surface_damage_buffer(struct wl_client *client,
                         struct wl_resource *resource,
                         i32 x, i32 y, i32 width, i32 height)
{
    /* TODO */
}

static void
wl_surface_offset(struct wl_client *client,
                  struct wl_resource *resource, i32 x, i32 y)
{
    /* TODO */
}

static const struct wl_surface_interface wl_surface_implementation = {
    .destroy              = wl_surface_destroy,
    .attach               = wl_surface_attach,
    .damage               = wl_surface_damage,
    .frame                = wl_surface_frame ,
    .set_opaque_region    = wl_surface_set_opaque_region,
    .set_input_region     = wl_surface_set_input_region,
    .commit               = wl_surface_commit,
    .set_buffer_transform = wl_surface_set_buffer_transform,
    .set_buffer_scale     = wl_surface_set_buffer_scale,
    .damage_buffer        = wl_surface_damage_buffer,
    .offset               = wl_surface_offset,
};

static void
wl_surface_resource_destroy(struct wl_resource *resource)
{
    /* TODO */
}

/*
 * wl_compositor functions
 */

static void
wl_compositor_create_surface(struct wl_client *wl_client,
                             struct wl_resource *resource, u32 id)
{
    struct wc_compositor *server = wl_resource_get_user_data(resource);
    struct wc_surface *surface = server_create_surface(server);
    struct wc_client *client = server_create_client(server);
    if (!surface || !client) {
        return;
    }

    struct wl_resource *wl_surface = wl_resource_create(
        wl_client, &wl_surface_interface, WL_SURFACE_VERSION, id);

    wl_resource_set_implementation(
        wl_surface, &wl_surface_implementation,
        surface, &wl_surface_resource_destroy);

    surface->client = client;
    surface->surface = wl_surface;
    client->client = wl_client;
}

static void
wl_compositor_create_region(struct wl_client *client,
                            struct wl_resource *resource, u32 id)
{
    struct wl_resource *region = 
        wl_resource_create(client, &wl_region_interface, WL_REGION_VERSION, id);
    wl_resource_set_implementation(region, &wl_region_implementation, 0,
                                    wl_region_resource_destroy);
}

static const struct wl_compositor_interface wl_compositor_implementation = {
    .create_surface = wl_compositor_create_surface,
    .create_region  = wl_compositor_create_region,
};

static void
wl_compositor_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
    struct compositor *server = data;
    struct wl_resource *compositor = wl_resource_create(
        client, &wl_compositor_interface, WL_COMPOSITOR_VERSION, id);
    wl_resource_set_implementation(compositor, &wl_compositor_implementation,
                                   server, 0);
}

/*
 * xdg toplevel functions
 */

static void
xdg_toplevel_destroy(struct wl_client *client,
                     struct wl_resource *resource)
{
    printf("destroy\n");
}

static void
xdg_toplevel_set_parent(struct wl_client *client,
                        struct wl_resource *resource,
                        struct wl_resource *parent)
{
    printf("parent\n");
}

static void
xdg_toplevel_set_title(struct wl_client *client,
                       struct wl_resource *resource,
                       const char *title)
{
    printf("title: %s\n", title);
}

static void
xdg_toplevel_set_app_id(struct wl_client *client,
                        struct wl_resource *resource,
                        const char *app_id)
{
    printf("app id: %s\n", app_id);
}

static void
xdg_toplevel_show_window_menu(struct wl_client *client,
                              struct wl_resource *resource,
                              struct wl_resource *seat, u32 serial, i32 x, i32 y)
{
    /* TODO */
}

static void
xdg_toplevel_move(struct wl_client *client, struct wl_resource *resource,
                  struct wl_resource *seat, u32 serial)
{
    /* TODO */
}

static void
xdg_toplevel_resize(struct wl_client *client, struct wl_resource *resource,
                    struct wl_resource *seat, u32 serial, u32 edges)
{
    /* TODO */
}

static void
xdg_toplevel_set_max_size(struct wl_client *client,
                          struct wl_resource *resource, i32 width, i32 height)
{
    printf("min_size: (%d, %d)\n", width, height);
}

static void
xdg_toplevel_set_min_size(struct wl_client *client,
                          struct wl_resource *resource, i32 width, i32 height)
{
    printf("max_size: (%d, %d)\n", width, height);
}

static void
xdg_toplevel_set_maximized(struct wl_client *client,
                           struct wl_resource *resource)
{
    printf("maximize\n");
}

static void
xdg_toplevel_unset_maximized(struct wl_client *client,
                             struct wl_resource *resource)
{
    printf("unmaximize\n");
}

static void
xdg_toplevel_set_fullscreen(struct wl_client *client,
                            struct wl_resource *resource,
                            struct wl_resource *output)
{
    /* TODO */
}

static void
xdg_toplevel_unset_fullscreen(struct wl_client *client,
                              struct wl_resource *resource)
{
    /* TODO */
}

static void
xdg_toplevel_set_minimized(struct wl_client *client,
                           struct wl_resource *resource)
{
    printf("minimize\n");
}

static const struct xdg_toplevel_interface xdg_toplevel_implementation = {
    .destroy          = xdg_toplevel_destroy,
    .set_parent       = xdg_toplevel_set_parent,
    .set_title        = xdg_toplevel_set_title,
    .set_app_id       = xdg_toplevel_set_app_id,
    .show_window_menu = xdg_toplevel_show_window_menu,
    .move             = xdg_toplevel_move,
    .resize           = xdg_toplevel_resize,
    .set_max_size     = xdg_toplevel_set_max_size,
    .set_min_size     = xdg_toplevel_set_min_size,
    .set_maximized    = xdg_toplevel_set_maximized,
    .unset_maximized  = xdg_toplevel_unset_maximized,
    .set_fullscreen   = xdg_toplevel_set_fullscreen,
    .unset_fullscreen = xdg_toplevel_unset_fullscreen,
    .set_minimized    = xdg_toplevel_set_minimized,
};

/*
 * xdg surface functions
 */

static void
xdg_surface_destroy(struct wl_client *client, struct wl_resource *resource)
{
    /* TODO */
}

static void
xdg_surface_get_toplevel(struct wl_client *client,
                         struct wl_resource *resource, u32 id)
{
    struct wl_resource *xdg_toplevel = wl_resource_create(client,
                                                          &xdg_toplevel_interface, XDG_TOPLEVEL_VERSION, id);

    // TODO: handle resource destroy?
    wl_resource_set_implementation(xdg_toplevel, &xdg_toplevel_implementation,
                                   0, 0);
}

static void
xdg_surface_get_popup(struct wl_client *client, struct wl_resource *resource,
                      u32 id, struct wl_resource *parent, struct wl_resource *positioner)
{
    /* TODO */
}

static void
xdg_surface_set_window_geometry(struct wl_client *client,
                                struct wl_resource *resource,
                                i32 x, i32 y, i32 width, i32 height)
{
    struct wc_surface *surface = wl_resource_get_user_data(resource);
    surface->width = width;
    surface->height = height;
    surface->x = x;
    surface->y = y;
}

static void
xdg_surface_ack_configure(struct wl_client *client,
                          struct wl_resource *resource, u32 serial)
{
    /* TODO */
}

static const struct xdg_surface_interface xdg_surface_implementation = {
    .destroy             = xdg_surface_destroy,
    .get_toplevel        = xdg_surface_get_toplevel,
    .get_popup           = xdg_surface_get_popup,
    .set_window_geometry = xdg_surface_set_window_geometry,
    .ack_configure       = xdg_surface_ack_configure,
};

/*
 * xdg_wm_base functions
 */

static void
xdg_wm_base_destroy(struct wl_client *client, struct wl_resource *resource)
{
    /* TODO */
}

static void
xdg_wm_base_create_positioner(struct wl_client *client,
                              struct wl_resource *resource, u32 id)
{
    /* TODO */
}

static void
xdg_wm_base_get_xdg_surface(struct wl_client *client,
                            struct wl_resource *resource, 
                            u32 id, struct wl_resource *wl_surface)
{
    struct wc_surface *surface = wl_resource_get_user_data(wl_surface);
    struct wl_resource *xdg_surface = wl_resource_create(
        client, &xdg_surface_interface, XDG_SURFACE_VERSION, id);
    surface->xdg_surface = xdg_surface;

    // TODO: handle resource destroy
    wl_resource_set_implementation(xdg_surface, &xdg_surface_implementation, 
                                   surface, 0);
}

static void
xdg_wm_base_pong(struct wl_client *client, struct wl_resource *resource,
                 u32 serial)
{
    /* TODO */
}

static const struct xdg_wm_base_interface xdg_wm_base_implementation = {
    .destroy           = xdg_wm_base_destroy,
    .create_positioner = xdg_wm_base_create_positioner,
    .get_xdg_surface   = xdg_wm_base_get_xdg_surface,
    .pong              = xdg_wm_base_pong,
};

static void
xdg_wm_base_resource_destroy(struct wl_resource *resource)
{
    /* TODO */
}

static void
xdg_wm_base_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
    struct compositor *server = data;
    struct wl_resource *resource = wl_resource_create(
        client, &xdg_wm_base_interface, XDG_WM_BASE_VERSION, id);
    wl_resource_set_implementation(resource, &xdg_wm_base_implementation,
                                   server, xdg_wm_base_resource_destroy);
}

/*
 * wl_pointer functions
 */

static void 
wl_pointer_set_cursor(struct wl_client *client, struct wl_resource *resource, 
                      u32 serial, struct wl_resource *surface, i32 hotspot_x, i32 hotspot_y)
{
    /* TODO */
}

static void 
wl_pointer_release(struct wl_client *client, struct wl_resource *resource)
{
}

static const struct wl_pointer_interface wl_pointer_implementation = {
    .set_cursor = wl_pointer_set_cursor,
    .release    = wl_pointer_release,
};

/* 
 * wl_keyboard functions
 */
static void
wl_keyboard_release(struct wl_client *client, struct wl_resource *resource)
{
}

static const struct wl_keyboard_interface wl_keyboard_implementation = {
    .release = wl_keyboard_release,
};

/*
 * wl seat functions
 */

static void
wl_seat_get_pointer(struct wl_client *client, struct wl_resource *resource,
                    u32 id)
{
    (void)wl_pointer_implementation;
#if 0
    struct wl_resource *pointer = 
        wl_resource_create(client, &wl_pointer_interface, 7, id);
    wl_resource_set_implementation(pointer, &wl_pointer_implementation, 0, 0);
    struct wc_client *it;
    wl_list_for_each(it, &server.clients, link) {
        if (it->client == client) {
            it->pointer = pointer;
        }
    }
#endif
}

static void
wl_seat_get_keyboard(struct wl_client *client, struct wl_resource *resource,
                     u32 id)
{
    struct wl_resource *keyboard =
        wl_resource_create(client, &wl_keyboard_interface, 7, id);
    wl_resource_set_implementation(keyboard, 
                                   &wl_keyboard_implementation, 0, 0);

    //i32 fd, size;
    //// TODO: get the xkb keymap fd and size
    //wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, size);
}

static void
wl_seat_get_touch(struct wl_client *client, struct wl_resource *resource,
                  u32 id)
{
    /* TODO */
}

static void
wl_seat_release(struct wl_client *client, struct wl_resource *resource)
{
    /* TODO */
}

static const struct wl_seat_interface wl_seat_implementation = {
    .get_pointer  = wl_seat_get_pointer,
    .get_keyboard = wl_seat_get_keyboard,
    .get_touch    = wl_seat_get_touch,
    .release      = wl_seat_release,
};

static void
wl_seat_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
    struct compositor *server = data;

    struct wl_resource *seat = 
        wl_resource_create(client, &wl_seat_interface, WL_SEAT_VERSION, id);
    wl_resource_set_implementation(seat, &wl_seat_implementation, server, 0);

    u32 caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
    wl_seat_send_capabilities(seat, caps);
}

static u32
compositor_time_msec(void)
{
    struct timespec ts = {0};

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void
compositor_update(struct compositor *base)
{
    struct wc_compositor *compositor = CONTAINER_OF(base, struct wc_compositor, base);

    wl_event_loop_dispatch(compositor->event_loop, 0);
    wl_display_flush_clients(compositor->display);

    u32 window_count = base->window_count;
    struct wc_surface *surface = compositor->surfaces;
    struct compositor_window *window = compositor->base.windows;
    while (window_count-- > 0) {
        window->texture = surface->texture;

        if (surface->frame_callback) {
            wl_callback_send_done(surface->frame_callback,
                                  compositor_time_msec());
            surface->frame_callback = 0;
        }

        surface++;
        window++;
    }
}

static void
compositor_send_key(struct compositor *compositor, i32 key, i32 state)
{
    u32 active_window = compositor->active_window;
    if (compositor->is_active && active_window) {
        struct wl_resource *keyboard = 0;
        wl_keyboard_send_key(keyboard, 0, compositor_time_msec(), key, state);
    }
}

static void
compositor_send_button(struct compositor *compositor, i32 button, i32 state)
{
    u32 active_window = compositor->active_window;
    if (active_window) {
        struct wl_resource *pointer = 0;
        wl_pointer_send_button(pointer, 0, compositor_time_msec(), button, state);
    }
}

static void
compositor_finish(struct compositor *base)
{
    struct wc_compositor *compositor = CONTAINER_OF(base, struct wc_compositor, base);

    wl_display_destroy(compositor->display);
}

struct compositor *
compositor_init(struct backend_memory *memory, struct egl *egl)
{
    struct wc_compositor *compositor = memory->data;
    struct memory_arena *arena = &compositor->arena;
    struct wl_display *display;
    const char *socket;

    eglQueryWaylandBufferWL = (PFNEGLQUERYWAYLANDBUFFERWLPROC) 
        eglGetProcAddress("eglQueryWaylandBufferWL");
    eglBindWaylandDisplayWL = (PFNEGLBINDWAYLANDDISPLAYWLPROC) 
        eglGetProcAddress("eglBindWaylandDisplayWL");

    if (!(display = wl_display_create())) {
        fprintf(stderr, "Failed to initialize display\n");
        return 0;
    }

    eglBindWaylandDisplayWL(egl->display, display);

    if (!(socket = wl_display_add_socket_auto(display))) {
        fprintf(stderr, "Failed to add a socket to the display\n");
        return 0;
    }

    wl_global_create(display, &wl_compositor_interface, WL_COMPOSITOR_VERSION,
                     compositor, wl_compositor_bind);
    wl_global_create(display, &xdg_wm_base_interface, XDG_WM_BASE_VERSION,
                     compositor, xdg_wm_base_bind);
    wl_global_create(display, &wl_seat_interface, WL_SEAT_VERSION,
                     compositor, &wl_seat_bind);
    wl_display_init_shm(display);

    compositor->display = display;
    compositor->event_loop = wl_display_get_event_loop(display);
    compositor->egl_display = egl->display;

    arena_init(arena, compositor + 1, memory->size - sizeof(*compositor));
    compositor->surfaces = arena_alloc(
        arena, MAX_SURFACE_COUNT, struct wc_surface);
    compositor->clients = arena_alloc(
        arena, MAX_CLIENT_COUNT, struct wc_client);
    compositor->base.windows = arena_alloc(
        &compositor->arena, MAX_SURFACE_COUNT, struct compositor_window);

    compositor->base.update = compositor_update;
    compositor->base.finish = compositor_finish;
    compositor->base.send_key = compositor_send_key;
    compositor->base.send_button = compositor_send_button;

    return &compositor->base;
}
