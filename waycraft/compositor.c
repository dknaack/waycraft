#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>
#include <X11/Xlib.h>
#include <time.h>

#include <waycraft/backend.h>
#include <waycraft/compositor.h>
#include <waycraft/egl.h>
#include <waycraft/game.h>
#include <waycraft/gl.h>
#include <waycraft/xdg-shell-protocol.h>
#include <waycraft/xwayland.h>

#define WL_POINTER_VERSION 7
#define WL_KEYBOARD_VERSION 7
#define WL_COMPOSITOR_VERSION 5
#define WL_SUBCOMPOSITOR_VERSION 1
#define WL_REGION_VERSION 1
#define WL_SEAT_VERSION 7
#define WL_SURFACE_VERSION 5
#define WL_SUBSURFACE_VERSION 1
#define WL_DATA_DEVICE_MANAGER_VERSION 3
#define XDG_WM_BASE_VERSION 4
#define XDG_TOPLEVEL_VERSION 4
#define XDG_SURFACE_VERSION 4

#define MAX_WINDOW_COUNT  256
#define MAX_SURFACE_COUNT 256
#define MAX_CLIENT_COUNT  256

static PFNEGLQUERYWAYLANDBUFFERWLPROC eglQueryWaylandBufferWL = 0;
static PFNEGLBINDWAYLANDDISPLAYWLPROC eglBindWaylandDisplayWL = 0;

struct client {
	struct wl_client *wl_client;

	struct wl_resource *wl_pointer;
	struct wl_resource *wl_keyboard;
	struct wl_list link;
};

struct subsurface {
	struct wl_resource *wl_subsurface;

	struct surface *surface;
	struct surface *parent;
	struct wl_list link;
};

struct surface {
	struct wl_resource *wl_surface;
	struct wl_resource *wl_buffer;
	struct wl_resource *xdg_surface;
	struct wl_resource *xdg_toplevel;
	struct wl_resource *wl_frame_callback;

	i32 x;
	i32 y;
	i32 width;
	i32 height;
	u32 texture;

	struct compositor *compositor;
	struct surface *parent;
	struct client *client;
	struct wl_list link;
	struct wl_list subsurfaces;
};

struct compositor {
	struct game_window_manager *wm;
	struct xwayland xwayland;
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	EGLDisplay *egl_display;

	struct memory_arena arena;
	struct wl_list surfaces;
	struct wl_list clients;
	struct wl_list free_surfaces;
	struct wl_list free_clients;

	struct surface *active_surface;

	i32 keymap;
	i32 keymap_size;
	struct {
		u32 depressed;
		u32 latched;
		u32 locked;
		u32 group;
	} modifiers;
};

static struct surface *
compositor_create_surface(struct compositor *compositor)
{
	struct wl_list *free_surfaces = &compositor->free_surfaces;
	struct wl_list *surfaces = &compositor->surfaces;
	struct surface *surface = 0;
	if (wl_list_empty(free_surfaces)) {
		surface = arena_alloc(&compositor->arena, 1, struct surface);
		surface->compositor = compositor;
		wl_list_insert(surfaces, &surface->link);
		wl_list_init(&surface->subsurfaces);
	} else {
		struct surface *surface = wl_container_of(
			free_surfaces->next, surface, link);
		wl_list_remove(&surface->link);
	}

	return surface;
}

static struct client *
compositor_get_client(struct compositor *compositor,
	struct wl_client *wl_client)
{
	struct client *result = 0;
	struct client *client;
	wl_list_for_each(client, &compositor->clients, link) {
		if (wl_client == client->wl_client) {
			result = client;
			break;
		}
	}

	if (!result) {
		result = arena_alloc(&compositor->arena, 1, struct client);
		result->wl_client = wl_client;
		wl_list_insert(&compositor->clients, &result->link);
	}

	return result;
}

static void
surface_activate(struct surface *surface)
{
	if (!surface) {
		return;
	}

	struct wl_array array;
	wl_array_init(&array);

	struct wl_resource *keyboard = surface->client->wl_keyboard;
	if (keyboard) {
		wl_keyboard_send_enter(keyboard, 0, surface->wl_surface, &array);
		wl_keyboard_send_modifiers(keyboard, 0, 0, 0, 0, 0);
	}

	struct wl_resource *pointer = surface->client->wl_pointer;
	if (pointer) {
		wl_pointer_send_enter(pointer, 0, surface->wl_surface, 0, 0);
	}

	struct wl_resource *xdg_toplevel = surface->xdg_toplevel;
	if (xdg_toplevel) {
		i32 *state = wl_array_add(&array, sizeof(i32));
		*state = XDG_TOPLEVEL_STATE_ACTIVATED;
		xdg_toplevel_send_configure(xdg_toplevel, 0, 0, &array);
	}
}

static void
surface_deactivate(struct surface *surface)
{
	if (!surface) {
		return;
	}

	struct wl_resource *keyboard = surface->client->wl_keyboard;
	if (keyboard) {
		wl_keyboard_send_leave(keyboard, 0, surface->wl_surface);
	}

	struct wl_resource *pointer = surface->client->wl_pointer;
	if (pointer) {
		wl_pointer_send_leave(pointer, 0, surface->wl_surface);
	}

	struct wl_resource *xdg_toplevel = surface->xdg_toplevel;
	if (xdg_toplevel) {
		struct wl_array array;
		wl_array_init(&array);
		xdg_toplevel_send_configure(xdg_toplevel, 0, 0, &array);
	}
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

/* wl_surface functions */
static void
wl_surface_attach(struct wl_client *client, struct wl_resource *resource,
	struct wl_resource *buffer, i32 x, i32 y)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	surface->wl_buffer = buffer;
}

static void
wl_surface_frame(struct wl_client *client, struct wl_resource *resource,
	u32 callback)
{
	struct surface *surface = wl_resource_get_user_data(resource);

	surface->wl_frame_callback =
		wl_resource_create(client, &wl_callback_interface, 1, callback);
}

static void
wl_surface_commit(struct wl_client *client, struct wl_resource *resource)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	struct compositor *compositor = surface->compositor;
	EGLDisplay *egl_display = compositor->egl_display;
	i32 texture_format;

	if (!surface->wl_buffer) {
		xdg_surface_send_configure(surface->xdg_surface, 0);
	} else if (eglQueryWaylandBufferWL(egl_display, surface->wl_buffer,
				EGL_TEXTURE_FORMAT, &texture_format)) {
		puts("egl texture");
		i32 width, height;
		eglQueryWaylandBufferWL(egl_display, surface->wl_buffer, EGL_WIDTH, &width);
		eglQueryWaylandBufferWL(egl_display, surface->wl_buffer, EGL_HEIGHT, &height);

		i64 attributes[] = { EGL_NONE };
		EGLImage image = eglCreateImage(
			egl_display, EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL,
			surface->wl_buffer, attributes);

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

		wl_buffer_send_release(surface->wl_buffer);
	} else {
		struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get(surface->wl_buffer);
		u32 width = wl_shm_buffer_get_width(shm_buffer);
		u32 height = wl_shm_buffer_get_height(shm_buffer);
		u32 format = wl_shm_buffer_get_format(shm_buffer);
		void *data = wl_shm_buffer_get_data(shm_buffer);
		assert(format == 0 || format == 1);

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

		wl_buffer_send_release(surface->wl_buffer);
	}
}

static const struct wl_surface_interface wl_surface_implementation = {
	.destroy              = do_nothing,
	.attach               = wl_surface_attach,
	.damage               = do_nothing,
	.frame                = wl_surface_frame ,
	.set_opaque_region    = do_nothing,
	.set_input_region     = do_nothing,
	.commit               = wl_surface_commit,
	.set_buffer_transform = do_nothing,
	.set_buffer_scale     = do_nothing,
	.damage_buffer        = do_nothing,
	.offset               = do_nothing,
};

static void
wl_surface_resource_destroy(struct wl_resource *resource)
{
	// TODO: fix the order of windows
	struct surface *surface = wl_resource_get_user_data(resource);
	struct compositor *compositor = surface->compositor;
	//struct client *client = surface->client;

	surface->texture = 0;
	surface->wl_frame_callback = 0;
	wl_list_remove(&surface->link);
	//wl_list_insert(&compositor->free_surfaces, &surface->link);

	//wl_list_remove(&client->link);
	//wl_list_insert(&compositor->free_clients, &client->link);

	if (surface == compositor->active_surface) {
		compositor->active_surface = 0;
	}
}

/* wl_compositor functions */
static void
wl_compositor_create_surface(struct wl_client *wl_client,
	struct wl_resource *resource, u32 id)
{
	struct compositor *compositor = wl_resource_get_user_data(resource);
	struct surface *surface = compositor_create_surface(compositor);
	struct client *client = compositor_get_client(compositor, wl_client);
	struct wl_resource *wl_surface = wl_resource_create(
		wl_client, &wl_surface_interface, WL_SURFACE_VERSION, id);
	wl_resource_set_implementation(wl_surface, &wl_surface_implementation,
		surface, &wl_surface_resource_destroy);

	surface->client = client;
	surface->wl_surface = wl_surface;
	client->wl_client = wl_client;
}

static void
wl_compositor_create_region(struct wl_client *client,
	struct wl_resource *resource, u32 id)
{
	struct wl_resource *region = wl_resource_create(
		client, &wl_region_interface, WL_REGION_VERSION, id);
	wl_resource_set_implementation(region, &wl_region_implementation, 0, 0);
}

static const struct wl_compositor_interface wl_compositor_implementation = {
	.create_surface = wl_compositor_create_surface,
	.create_region  = wl_compositor_create_region,
};

static void
wl_compositor_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *compositor = wl_resource_create(
		client, &wl_compositor_interface, WL_COMPOSITOR_VERSION, id);
	wl_resource_set_implementation(
		compositor, &wl_compositor_implementation, data, 0);
}

/* xdg toplevel functions */
static void
xdg_toplevel_destroy(struct wl_client *client, struct wl_resource *resource)
{
	puts("xdg_toplevel::destroy");
}

static const struct xdg_toplevel_interface xdg_toplevel_implementation = {
	.destroy          = xdg_toplevel_destroy,
	.set_parent       = do_nothing,
	.set_title        = do_nothing,
	.set_app_id       = do_nothing,
	.show_window_menu = do_nothing,
	.move             = do_nothing,
	.resize           = do_nothing,
	.set_max_size     = do_nothing,
	.set_min_size     = do_nothing,
	.set_maximized    = do_nothing,
	.unset_maximized  = do_nothing,
	.set_fullscreen   = do_nothing,
	.unset_fullscreen = do_nothing,
	.set_minimized    = do_nothing,
};

/* xdg surface functions */
static void
xdg_surface_destroy(struct wl_client *client, struct wl_resource *resource)
{
	puts("xdg_surface::destroy");
}

static void
xdg_surface_get_toplevel(struct wl_client *client,
	struct wl_resource *resource, u32 id)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	struct wl_resource *xdg_toplevel = wl_resource_create(
		client, &xdg_toplevel_interface, XDG_TOPLEVEL_VERSION, id);
	wl_resource_set_implementation(
		xdg_toplevel, &xdg_toplevel_implementation, surface, 0);

	surface->xdg_toplevel = xdg_toplevel;
}

static void
xdg_surface_set_window_geometry(struct wl_client *client,
	struct wl_resource *resource,
	i32 x, i32 y, i32 width, i32 height)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	surface->width = width;
	surface->height = height;
	surface->x = x;
	surface->y = y;
}

static const struct xdg_surface_interface xdg_surface_implementation = {
	.destroy             = xdg_surface_destroy,
	.get_toplevel        = xdg_surface_get_toplevel,
	.get_popup           = do_nothing,
	.set_window_geometry = xdg_surface_set_window_geometry,
	.ack_configure       = do_nothing,
};

/* xdg_wm_base functions */
static void
xdg_wm_base_get_xdg_surface(struct wl_client *client,
	struct wl_resource *resource,
	u32 id, struct wl_resource *wl_surface)
{
	struct surface *surface = wl_resource_get_user_data(wl_surface);
	struct wl_resource *xdg_surface = wl_resource_create(
		client, &xdg_surface_interface, XDG_SURFACE_VERSION, id);
	wl_resource_set_implementation(
		xdg_surface, &xdg_surface_implementation, surface, 0);

	surface->xdg_surface = xdg_surface;
}

static const struct xdg_wm_base_interface xdg_wm_base_implementation = {
	.destroy           = do_nothing,
	.create_positioner = do_nothing,
	.get_xdg_surface   = xdg_wm_base_get_xdg_surface,
	.pong              = do_nothing,
};

static void
xdg_wm_base_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *resource = wl_resource_create(
		client, &xdg_wm_base_interface, XDG_WM_BASE_VERSION, id);
	wl_resource_set_implementation(
		resource, &xdg_wm_base_implementation, data, 0);
}

/* TODO: wl_pointer functions */
static const struct wl_pointer_interface wl_pointer_implementation = {
	.set_cursor = do_nothing,
	.release    = do_nothing,
};

/* TODO: wl_keyboard functions */
static const struct wl_keyboard_interface wl_keyboard_implementation = {
	.release = do_nothing,
};

/* wl seat functions */
static void
wl_seat_get_pointer(struct wl_client *wl_client, struct wl_resource *resource,
	u32 id)
{
	struct compositor *compositor = wl_resource_get_user_data(resource);
	struct wl_resource *pointer = wl_resource_create(
		wl_client, &wl_pointer_interface, WL_POINTER_VERSION, id);

	wl_resource_set_implementation(pointer, &wl_pointer_implementation, 0, 0);
	struct client *client = compositor_get_client(compositor, wl_client);
	client->wl_pointer = pointer;
}

static void
wl_seat_get_keyboard(struct wl_client *wl_client, struct wl_resource *resource,
	u32 id)
{
	struct compositor *compositor = wl_resource_get_user_data(resource);
	struct wl_resource *keyboard = wl_resource_create(
		wl_client, &wl_keyboard_interface, WL_KEYBOARD_VERSION, id);
	wl_resource_set_implementation(keyboard, &wl_keyboard_implementation, 0, 0);

	struct client *client = compositor_get_client(compositor, wl_client);
	client->wl_keyboard = keyboard;

	wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
		compositor->keymap, compositor->keymap_size);
}

static const struct wl_seat_interface wl_seat_implementation = {
	.get_pointer  = wl_seat_get_pointer,
	.get_keyboard = wl_seat_get_keyboard,
	.get_touch    = do_nothing,
	.release      = do_nothing,
};

static void
wl_seat_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *seat =
		wl_resource_create(client, &wl_seat_interface, WL_SEAT_VERSION, id);
	wl_resource_set_implementation(seat, &wl_seat_implementation, data, 0);

	u32 caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
	wl_seat_send_capabilities(seat, caps);
}

// TODO: fill out the subsurface functions
static const struct wl_subsurface_interface wl_subsurface_imeplementation = {
	.destroy      = do_nothing,
	.set_position = do_nothing,
	.place_above  = do_nothing,
	.place_below  = do_nothing,
	.set_sync     = do_nothing,
	.set_desync   = do_nothing,
};

static void
wl_subcompositor_get_subsurface(
	struct wl_client *client, struct wl_resource *resource, u32 id,
	struct wl_resource *wl_surface, struct wl_resource *wl_parent)
{
	struct surface *surface = wl_resource_get_user_data(wl_surface);
	struct surface *parent = wl_resource_get_user_data(wl_parent);
	struct compositor *compositor = wl_resource_get_user_data(resource);
	struct subsurface *subsurface =
		arena_alloc(&compositor->arena, 1, struct subsurface);
	wl_list_insert(&surface->subsurfaces, &subsurface->link);

	struct wl_resource *wl_subsurface = wl_resource_create(
		client, &wl_subsurface_interface, WL_SUBSURFACE_VERSION, id);
	wl_resource_set_implementation(
		wl_subsurface, &wl_subsurface_imeplementation, subsurface, 0);

	subsurface->surface = surface;
	subsurface->parent = parent;
	subsurface->wl_subsurface = wl_subsurface;
}

// TODO: fill out the subcompositor functions
static const struct wl_subcompositor_interface wl_subcompositor_implementation = {
	.destroy        = do_nothing,
	.get_subsurface = wl_subcompositor_get_subsurface,
};

static void
wl_subcompositor_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *subcompositor = wl_resource_create(client,
		&wl_subcompositor_interface, WL_SUBCOMPOSITOR_VERSION, id);
	wl_resource_set_implementation(subcompositor,
		&wl_subcompositor_implementation, data, 0);
}

// TODO: fill out the data device manager functions
static const struct wl_data_device_manager_interface
wl_data_device_manager_implementation = {
	.create_data_source = do_nothing,
	.get_data_device = do_nothing,
};

static void
wl_data_device_manager_bind(struct wl_client *client, void *data,
	u32 version, u32 id)
{
	struct wl_resource *data_device_manager = wl_resource_create(client,
		&wl_data_device_manager_interface, WL_DATA_DEVICE_MANAGER_VERSION, id);
	wl_resource_set_implementation(data_device_manager,
		&wl_data_device_manager_implementation, data, 0);
}

static u32
compositor_time_msec(void)
{
	struct timespec ts = {0};

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void
compositor_update(struct backend_memory *memory, struct game_window_manager *wm)
{
	struct compositor *compositor = memory->data;
	compositor->wm = wm;

	wl_event_loop_dispatch(compositor->wl_event_loop, 0);
	wl_display_flush_clients(compositor->wl_display);

	u32 active_surface_index = wm->active_window - wm->windows;
	u32 surface_index = 0;
	u32 window_count = 0;

	struct surface *active_surface = compositor->active_surface;
	struct surface *surface;
	struct game_window *window = wm->windows;

	wl_list_for_each_reverse(surface, &compositor->surfaces, link) {
		window->texture = surface->texture;

		if (surface->wl_frame_callback) {
			wl_callback_send_done(surface->wl_frame_callback,
				compositor_time_msec());
			wl_resource_destroy(surface->wl_frame_callback);
			surface->wl_frame_callback = 0;
		}

		u32 is_active_surface = surface_index == active_surface_index;
		if (is_active_surface && surface != active_surface) {
			surface_deactivate(active_surface);
			surface_activate(surface);

			compositor->active_surface = surface;
		}

		window++;
		window_count++;
		surface_index++;
	}

	wm->window_count = window_count;
}

static void
compositor_send_key(struct backend_memory *memory, i32 key, i32 state)
{
	struct compositor *compositor = memory->data;

	struct surface *active_surface = compositor->active_surface;
	if (active_surface) {
		struct wl_resource *keyboard = active_surface->client->wl_keyboard;
		if (keyboard) {
			u32 time = compositor_time_msec();
			wl_keyboard_send_key(keyboard, 0, time, key, state);
		}
	}
}

static void
compositor_send_button(struct backend_memory *memory, i32 button, i32 state)
{
	struct compositor *compositor = memory->data;
	struct surface *active_surface = compositor->active_surface;
	if (active_surface) {
		struct wl_resource *pointer = active_surface->client->wl_pointer;
		if (pointer) {
			u32 time = compositor_time_msec();
			wl_pointer_send_button(pointer, 0, time, button, state);
		}
	}
}

static void
compositor_send_motion(struct backend_memory *memory, i32 x, i32 y)
{
	struct compositor *compositor = memory->data;
	struct surface *active_surface = compositor->active_surface;
	if (active_surface) {
		struct wl_resource *pointer = active_surface->client->wl_pointer;
		if (pointer) {
			v2 cursor_pos = compositor->wm->cursor_pos;
			f32 surface_width = active_surface->width;
			f32 surface_height = active_surface->height;
			f32 rel_cursor_x = 0.5f * (cursor_pos.x + 1.f) * surface_width;
			f32 rel_cursor_y = (1.f - 0.5f * (cursor_pos.y + 1.f)) * surface_height;
			wl_fixed_t surface_x = wl_fixed_from_double(rel_cursor_x);
			wl_fixed_t surface_y = wl_fixed_from_double(rel_cursor_y);
			u32 time = compositor_time_msec();
			wl_pointer_send_motion(pointer, time, surface_x, surface_y);
		}
	}
}

static void
compositor_send_modifiers(struct backend_memory *memory, u32 depressed,
	u32 latched, u32 locked, u32 group)
{
	struct compositor *compositor = memory->data;

	if (depressed == compositor->modifiers.depressed &&
			latched == compositor->modifiers.latched &&
			locked == compositor->modifiers.locked &&
			group == compositor->modifiers.group) {
		return;
	}

	compositor->modifiers.depressed = depressed;
	compositor->modifiers.latched = latched;
	compositor->modifiers.locked = locked;
	compositor->modifiers.group = group;

	struct surface *active_surface = compositor->active_surface;
	if (active_surface) {
		struct wl_resource *keyboard = active_surface->client->wl_keyboard;
		if (keyboard) {
			wl_keyboard_send_modifiers(keyboard, 0, depressed, latched, locked, group);
		}
	}
}

static void
compositor_finish(struct backend_memory *memory)
{
	struct compositor *compositor = memory->data;

#if ENABLE_WAYLAND
	xwayland_finish(&compositor->xwayland);
#endif
	wl_display_destroy(compositor->wl_display);
}

i32
compositor_init(struct backend_memory *memory, struct egl *egl, struct
	game_window_manager *wm, i32 keymap, i32 keymap_size)
{
	struct compositor *compositor = memory->data;
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
	wl_global_create(display, &wl_subcompositor_interface,
		WL_SUBCOMPOSITOR_VERSION, compositor, &wl_subcompositor_bind);
	wl_global_create(display, &wl_data_device_manager_interface,
		WL_DATA_DEVICE_MANAGER_VERSION, compositor,
		&wl_data_device_manager_bind);
	wl_display_init_shm(display);

	compositor->wl_display = display;
	compositor->wl_event_loop = wl_display_get_event_loop(display);
	compositor->egl_display = egl->display;

	arena_init(arena, compositor + 1, memory->size - sizeof(*compositor));
	wl_list_init(&compositor->surfaces);
	wl_list_init(&compositor->clients);
	wl_list_init(&compositor->free_surfaces);
	wl_list_init(&compositor->free_clients);

	compositor->keymap = keymap;
	compositor->keymap_size = keymap_size;

#if ENABLE_XWAYLAND
	if (xwayland_init(&compositor->xwayland, display) != 0) {
		fprintf(stderr, "Failed to initialize xwayland\n");
		return -1;
	}
#endif

	wm->windows = arena_alloc(&compositor->arena, MAX_WINDOW_COUNT,
		struct game_window);

	return 0;
}
