#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server.h>
#include <xdg-shell-protocol.h>

#include "types.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "game.h"
#include "math.h"
#include "gl.h"
#include "x11.h"

#include "camera.c"
#include "game.c"
#include "gl.c"
#include "math.c"
#include "memory.c"
#include "noise.c"
#include "world.c"
#include "x11.c"
#include "xdg-shell-protocol.c"

#define WL_COMPOSITOR_VERSION 5
#define WL_REGION_VERSION 1
#define WL_SEAT_VERSION 7
#define WL_SURFACE_VERSION 5
#define XDG_WM_BASE_VERSION 4
#define XDG_TOPLEVEL_VERSION 4
#define XDG_SURFACE_VERSION 4

struct server {
    struct wl_display *display;

    struct wl_list clients;
    struct wl_list surfaces;

    struct wl_list free_clients;
    struct wl_list free_surfaces;
};

struct surface {
    struct wl_resource *surface;
    struct wl_resource *buffer;
    struct wl_resource *xdg_surface;
    struct wl_resource *xdg_toplevel;
    struct wl_resource *frame_callback;
    i32 x, y;
    i32 width, height;

    struct wl_client *client;
    struct wl_list node;
};

static struct surface *
server_create_surface(struct server *server)
{
    struct surface *surface = calloc(1, sizeof(struct surface *));

    wl_list_insert(server->surfaces, surface);
    return surface;
}

static void
wl_region_destroy(struct wl_client *client, struct wl_resource *resource)
{
    /* TODO */
}

static void
wl_region_add(struct wl_client *client, struct wl_resource *resource,
        i32 x, i32 y, i32 width, i32 height)
{
    /* TODO */
}

static void
wl_region_subtract(struct wl_client *client, struct wl_resource *resource,
        i32 x, i32 y, i32 width, i32 height)
{
    /* TODO */
}

static const struct wl_region_interface wl_region_implementation = {
    .destroy  = wl_region_destroy,
    .add      = wl_region_add,
    .subtract = wl_region_subtract,
};

static void
wl_region_resource_destroy(struct wl_resource *resource)
{
    /* TODO */
}

/*
 * wl_surface functions
 */

static void wl_surface_destroy(struct wl_client *client,
        struct wl_resource *resource)
{
    struct surface *surface = wl_resource_get_user_data(resource);

    wl_list_remove(&surface->node);
    free(surface);
}

static void wl_surface_attach(struct wl_client *client,
        struct wl_resource *resource, struct wl_resource *buffer, i32 x, i32 y)
{
    struct surface *surface = wl_resource_get_user_data(resource);
    surface->buffer = buffer;
}

static void wl_surface_damage(struct wl_client *client,
        struct wl_resource *resource,
        i32 x,
        i32 y,
        i32 width,
        i32 height)
{
	/* TODO */
}

static void wl_surface_frame(struct wl_client *client,
        struct wl_resource *resource, u32 callback)
{
    struct surface *surface = wl_resource_get_user_data(resource);

    surface->frame_callback = wl_resource_create(client,
            &wl_callback_interface, 1, callback);
}

static void wl_surface_set_opaque_region(struct wl_client *client,
        struct wl_resource *resource, struct wl_resource *region)
{
	/* TODO */
}

static void wl_surface_set_input_region(struct wl_client *client,
        struct wl_resource *resource, struct wl_resource *region)
{
	/* TODO */
}

static void wl_surface_commit(struct wl_client *client,
        struct wl_resource *resource)
{
    struct surface *surface = wl_resource_get_user_data(resource);
    if (!surface->buffer) {
        xdg_surface_send_configure(surface->xdg_surface, 0);
    }

    // TODO: draw the texture
}

static void wl_surface_set_buffer_transform(struct wl_client *client,
        struct wl_resource *resource, i32 transform)
{
	/* TODO */
}

static void wl_surface_set_buffer_scale(struct wl_client *client,
        struct wl_resource *resource, i32 scale)
{
	/* TODO */
}

static void wl_surface_damage_buffer(struct wl_client *client,
        struct wl_resource *resource, i32 x, i32 y, i32 width, i32 height)
{
	/* TODO */
}

static void wl_surface_offset(struct wl_client *client,
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
wl_compositor_create_surface(struct wl_client *client,
        struct wl_resource *resource, u32 id)
{
    struct server *server = wl_resource_get_user_data(resource);
    struct surface *surface = server_create_surface(server);
    if (!surface) {
        return;
    }

    surface->client = client;
    surface->surface = wl_resource_create(client, 
            &wl_surface_interface, WL_SURFACE_VERSION, id);
    wl_resource_set_implementation(surface->surface,
        &wl_surface_implementation, surface, &wl_surface_resource_destroy);
}

static void
wl_compositor_create_region(struct wl_client *client,
        struct wl_resource *resource, u32 id)
{
    struct wl_resource *region = wl_resource_create(client,
            &wl_region_interface, WL_REGION_VERSION, id);
    wl_resource_set_implementation(region, &wl_region_implementation,
            0, wl_region_resource_destroy);
}

static const struct wl_compositor_interface wl_compositor_implementation = {
    .create_surface = wl_compositor_create_surface,
    .create_region  = wl_compositor_create_region,
};

static void
wl_compositor_resource_destroy(struct wl_resource *resource)
{
    /* TODO */
}

static void
wl_compositor_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
    struct server *server = data;
    struct wl_resource *compositor = wl_resource_create(client,
            &wl_compositor_interface, WL_COMPOSITOR_VERSION, id);
    wl_resource_set_implementation(compositor, &wl_compositor_implementation,
            server, wl_compositor_resource_destroy);
}

/*
 * xdg toplevel functions
 */

static void
xdg_toplevel_destroy(struct wl_client *client,
        struct wl_resource *resource)
{
	/* TODO */
}

static void
xdg_toplevel_set_parent(struct wl_client *client,
        struct wl_resource *resource,
        struct wl_resource *parent)
{
	/* TODO */
}

static void
xdg_toplevel_set_title(struct wl_client *client,
        struct wl_resource *resource,
        const char *title)
{
	/* TODO */
}

static void
xdg_toplevel_set_app_id(struct wl_client *client,
        struct wl_resource *resource,
        const char *app_id)
{
	/* TODO */
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
	/* TODO */
}

static void
xdg_toplevel_set_min_size(struct wl_client *client,
        struct wl_resource *resource, i32 width, i32 height)
{
	/* TODO */
}

static void
xdg_toplevel_set_maximized(struct wl_client *client,
        struct wl_resource *resource)
{
	/* TODO */
}

static void
xdg_toplevel_unset_maximized(struct wl_client *client,
        struct wl_resource *resource)
{
	/* TODO */
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
	/* TODO */
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
        struct wl_resource *resource, i32 x, i32 y, i32 width, i32 height)
{
    /* TODO */
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
        struct wl_resource *resource, u32 id, struct wl_resource *surface)
{
    struct wl_resource *xdg_surface = wl_resource_create(client, 
            &xdg_surface_interface, XDG_SURFACE_VERSION, id);

    // TODO: handle resource destroy
    wl_resource_set_implementation(xdg_surface, &xdg_surface_implementation, 0, 0);
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
    struct server *server = data;
    struct wl_resource *resource = wl_resource_create(client,
            &xdg_wm_base_interface, XDG_WM_BASE_VERSION, id);
    wl_resource_set_implementation(resource, &xdg_wm_base_implementation,
            server, xdg_wm_base_resource_destroy);
}

/*
 * wl seat functions
 */

static void
wl_seat_get_pointer(struct wl_client *client, struct wl_resource *resource,
        u32 id)
{
    /* TODO */
}

static void
wl_seat_get_keyboard(struct wl_client *client, struct wl_resource *resource,
        u32 id)
{
    /* TODO */
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
wl_seat_resource_destroy(struct wl_resource *resource)
{
    /* TODO */
}

static void
wl_seat_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
    struct server *server = data;

    struct wl_resource *resource = wl_resource_create(client,
            &wl_seat_interface, WL_SEAT_VERSION, id);
    wl_resource_set_implementation(resource, &wl_seat_implementation,
            server, wl_seat_resource_destroy);

    u32 caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
    wl_seat_send_capabilities(resource, caps);
}

int
main(void)
{
    struct server server = {0};
    struct x11_window window = {0};
    struct game_state game = {0};
    struct game_input input = {0};
    struct wl_display *display;
    const char *socket;

    if (x11_window_init(&window) != 0) {
        fprintf(stderr, "Failed to initialize window\n");
        return 1;
    }

    if (!(display = wl_display_create())) {
        fprintf(stderr, "Failed to initialize display\n");
        return 1;
    }

    if (!(socket = wl_display_add_socket_auto(display))) {
        fprintf(stderr, "Failed to add a socket to the display\n");
        return 1;
    }

    struct wl_event_loop *event_loop = 
        wl_display_get_event_loop(display);

    wl_global_create(display, &wl_compositor_interface, WL_COMPOSITOR_VERSION, 
            &server, wl_compositor_bind);
    wl_global_create(display, &xdg_wm_base_interface, XDG_WM_BASE_VERSION,
            &server, xdg_wm_base_bind);
    wl_global_create(display, &wl_seat_interface, WL_SEAT_VERSION, 
            &server, &wl_seat_bind);
    wl_display_init_shm(display);

    if (gl_context_init(&gl, &window) != 0) {
        fprintf(stderr, "Failed to load the functions for OpenGL\n");
        return 1;
    }

    if (game_init(&game) != 0) {
        fprintf(stderr, "Failed to initialize the game\n");
        return 1;
    }

    // TODO: fix timestep
    struct timespec wait_time = { 0, 1000000 };
    while (window.is_open) {
        wl_event_loop_dispatch(event_loop, 0);

        input.mouse.dx = input.mouse.dy = 0;
        memset(&input.controller, 0, sizeof(input.controller));
        x11_window_poll_events(&window, &input);

        input.dt = 0.01;
        input.width = window.width;
        input.height = window.height;
        gl.Viewport(0, 0, window.width, window.height);
        if (game_update(&game, &input) != 0) {
            window.is_open = 0;
        }

        wl_display_flush_clients(display);
        glXSwapBuffers(window.display, window.drawable);
        nanosleep(&wait_time, 0);
    }

    gl_context_finish(&gl, &window);
    wl_display_destroy(display);
    x11_window_finish(&window);
    return 0;
}
