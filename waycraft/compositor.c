#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>
#include <time.h>

#include <waycraft/backend.h>
#include <waycraft/compositor.h>
#include <waycraft/game.h>
#include <waycraft/gl.h>
#include <waycraft/log.h>
#include <waycraft/xdg-shell-protocol.h>
#include <waycraft/xwayland.h>

#define WL_CALLBACK_VERSION 1
#define WL_COMPOSITOR_VERSION 5
#define WL_DATA_DEVICE_MANAGER_VERSION 3
#define WL_DATA_SOURCE_VERSION 3
#define WL_KEYBOARD_VERSION 7
#define WL_OUTPUT_VERSION 4
#define WL_POINTER_VERSION 7
#define WL_REGION_VERSION 1
#define WL_SEAT_VERSION 7
#define WL_SUBCOMPOSITOR_VERSION 1
#define WL_SUBSURFACE_VERSION 1
#define WL_SURFACE_VERSION 5
#define XDG_SURFACE_VERSION 4
#define XDG_TOPLEVEL_VERSION 4
#define XDG_WM_BASE_VERSION 4

#define MAX_SURFACE_COUNT 256
#define MAX_WINDOW_COUNT 256

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
			struct wl_resource *xdg_surface;
			struct wl_resource *xdg_toplevel;
		};

		struct wl_resource *parent_surface;
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

static void
do_nothing()
{

}

static void
resource_remove(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));
}

static void
surface_attach(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *buffer, i32 x, i32 y)
{
	if (x != 0 || y != 0) {
		wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_OFFSET,
			"invalid offset: (%d, %d)\n", x, y);
		return;
	}

	struct surface *surface = wl_resource_get_user_data(resource);
	surface->pending.flags |= SURFACE_NEW_BUFFER;
	surface->pending.buffer = buffer;
}

static void
surface_frame(struct wl_client *client, struct wl_resource *resource,
		u32 callback)
{
	struct wl_resource *frame_callback = wl_resource_create(client,
		&wl_callback_interface, WL_CALLBACK_VERSION, callback);

	struct surface *surface = wl_resource_get_user_data(resource);
	surface->pending.flags |= SURFACE_NEW_FRAME;
	surface->pending.frame_callback = frame_callback;
}

static void
surface_commit(struct wl_client *client, struct wl_resource *resource)
{
	struct surface *surface = wl_resource_get_user_data(resource);

	if (!surface->current.buffer && !surface->pending.buffer &&
			surface->xdg_surface) {
		xdg_surface_send_configure(surface->xdg_surface, 0);
	}

	if (surface->pending.flags & SURFACE_NEW_BUFFER) {
		// NOTE: buffer may be null
		struct wl_resource *buffer = surface->pending.buffer;
		surface->current.buffer = buffer;

		if (surface->texture) {
			gl.DeleteTextures(1, &surface->texture);
		}

		if (buffer) {
			struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get(buffer);
			u32 width = wl_shm_buffer_get_width(shm_buffer);
			u32 height = wl_shm_buffer_get_height(shm_buffer);
			u32 format = wl_shm_buffer_get_format(shm_buffer);
			void *data = wl_shm_buffer_get_data(shm_buffer);
			assert(format == 0 || format == 1);

			gl.GenTextures(1, &surface->texture);
			gl.BindTexture(GL_TEXTURE_2D, surface->texture);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
				GL_BGRA, GL_UNSIGNED_BYTE, data);
			gl.BindTexture(GL_TEXTURE_2D, 0);

			wl_buffer_send_release(buffer);

			surface->width = width;
			surface->height = height;
		}

		struct game_window *window = surface->window;
		if (window) {
			window->texture = surface->texture;
			window->scale.x = surface->width;
			window->scale.y = surface->height;
		}
	}

	if (surface->pending.flags & SURFACE_NEW_ROLE) {
		struct compositor *compositor = surface->compositor;
		struct game_window_manager *wm = &compositor->window_manager;

		if (surface->pending.role == SURFACE_ROLE_TOPLEVEL) {
			// TODO: reuse destroyed windows instead of allocating a new one each time.
			struct game_window *window = wm->windows + wm->window_count++;
			window->flags = 0;
			window->texture = surface->texture;
			surface->window = window;

			surface->current.role = surface->pending.role;
		} else if (surface->pending.role == SURFACE_ROLE_SUBSURFACE) {
			if (surface->current.role == SURFACE_ROLE_SUBSURFACE) {
				wl_resource_post_error(resource, WL_SUBSURFACE_ERROR_BAD_SURFACE,
					"surface is already a subsurface");
			} else if (surface->current.role != SURFACE_ROLE_NONE) {
				wl_resource_post_error(resource, WL_SUBSURFACE_ERROR_BAD_SURFACE,
					"surface already has a role assigned");
			} else {
				surface->current.role = surface->pending.role;
			}
		} else if (surface->pending.role == SURFACE_ROLE_XWAYLAND) {
			log_info("xwayland buffer = %p\n", (void *)surface->current.buffer);
			struct game_window *window = wm->windows + wm->window_count++;
			window->flags = 0;
			window->texture = surface->texture;
			surface->window = window;

			surface->current.role = surface->pending.role;
		}
	}

	if (surface->pending.flags & SURFACE_NEW_FRAME) {
		surface->current.frame_callback = surface->pending.frame_callback;
	}

	surface->pending.flags = 0;
}

static const struct wl_surface_interface surface_impl = {
	.destroy              = do_nothing,
	.attach               = surface_attach,
	.damage               = do_nothing,
	.frame                = surface_frame,
	.set_opaque_region    = do_nothing,
	.set_input_region     = do_nothing,
	.commit               = surface_commit,
	.set_buffer_transform = do_nothing,
	.set_buffer_scale     = do_nothing,
	.damage_buffer        = do_nothing,
	.offset               = do_nothing,
};

static const struct wl_region_interface region_impl = {
	do_nothing,
	do_nothing,
	do_nothing,
};

static void
compositor_create_surface(struct wl_client *client,
		struct wl_resource *resource, u32 id)
{
	struct compositor *compositor = wl_resource_get_user_data(resource);
	struct surface *surface = compositor->surfaces +
		compositor->surface_count++;
	struct wl_resource *wl_surface = wl_resource_create(client,
		&wl_surface_interface, WL_SURFACE_VERSION, id);
	wl_resource_set_implementation(wl_surface, &surface_impl, surface, 0);

	surface->resource = wl_surface;
	surface->compositor = compositor;

	wl_signal_emit(&compositor->new_surface, wl_surface);
}

static void
compositor_create_region(struct wl_client *client,
		struct wl_resource *_resource, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_region_interface, WL_REGION_VERSION, id);
	wl_resource_set_implementation(resource, &region_impl, 0, 0);
}

static const struct wl_compositor_interface compositor_impl = {
	.create_surface = compositor_create_surface,
	.create_region = compositor_create_region,
};

static void
compositor_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_compositor_interface, version, id);
	wl_resource_set_implementation(resource, &compositor_impl, data, 0);
}

static void
pointer_set_cursor(struct wl_client *client, struct wl_resource *resource,
		u32 serial, struct wl_resource *_surface, i32 hotspot_x, i32 hotspot_y)
{
	if (_surface) {
		struct surface *surface = wl_resource_get_user_data(_surface);

		surface->pending.flags |= SURFACE_NEW_ROLE;
		surface->pending.role = SURFACE_ROLE_CURSOR;
	}
}

static const struct wl_pointer_interface pointer_impl = {
	.set_cursor = pointer_set_cursor,
	.release    = do_nothing,
};

static const struct wl_keyboard_interface keyboard_impl = {
	.release = do_nothing,
};

static void
seat_get_pointer(struct wl_client *client, struct wl_resource *resource, u32 id)
{
	struct compositor *compositor = wl_resource_get_user_data(resource);
	struct wl_resource *pointer = wl_resource_create(client,
		&wl_pointer_interface, WL_POINTER_VERSION, id);
	wl_resource_set_implementation(pointer, &pointer_impl, 0, resource_remove);
	wl_list_insert(&compositor->pointers, wl_resource_get_link(pointer));
}

static void
seat_get_keyboard(struct wl_client *client, struct wl_resource *resource, u32 id)
{
	struct compositor *compositor = wl_resource_get_user_data(resource);
	struct wl_resource *keyboard = wl_resource_create(client,
		&wl_keyboard_interface, WL_KEYBOARD_VERSION, id);
	wl_resource_set_implementation(keyboard, &keyboard_impl, 0, resource_remove);
	wl_list_insert(&compositor->keyboards, wl_resource_get_link(keyboard));
	wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
		compositor->keymap, compositor->keymap_size);
}

static void
seat_get_touch(struct wl_client *client, struct wl_resource *resource, u32 id)
{
	wl_resource_post_error(resource, WL_SEAT_ERROR_MISSING_CAPABILITY,
		"missing touch capability");
}

static const struct wl_seat_interface seat_impl = {
	.get_pointer  = seat_get_pointer,
	.get_keyboard = seat_get_keyboard,
	.get_touch    = seat_get_touch,
	.release      = do_nothing,
};

static void
seat_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_seat_interface, version, id);
	wl_resource_set_implementation(resource, &seat_impl, data, 0);
	wl_resource_set_user_data(resource, data);

	u32 caps = WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER;
	const char *name = "seat0";

	wl_seat_send_name(resource, name);
	wl_seat_send_capabilities(resource, caps);
}

static void
output_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_output_interface, WL_OUTPUT_VERSION, id);
	wl_resource_set_implementation(resource, 0, 0, 0);

	wl_output_send_geometry(resource, 0, 0, 0, 0, WL_OUTPUT_SUBPIXEL_NONE,
		"waycraft", "virtual-output0", WL_OUTPUT_TRANSFORM_NORMAL);
	wl_output_send_done(resource);
}

static const struct xdg_toplevel_interface xdg_toplevel_impl = {
	.destroy          = do_nothing,
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

static void
xdg_toplevel_unbind(struct wl_resource *resource)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	struct compositor *compositor = surface->compositor;
	struct game_window_manager *wm = &compositor->window_manager;

	struct surface *focused_surface = compositor->surfaces +
		compositor->focused_surface;
	if (focused_surface == surface) {
		struct game_window *focused_window = window_manager_get_window(wm,
			wm->focused_window);
		focused_window->flags |= WINDOW_DESTROYED;

		compositor->focused_surface = 0;
		compositor->window_manager.focused_window = 0;
	}

	surface->pending.flags |= SURFACE_NEW_ROLE;
	surface->pending.role = SURFACE_ROLE_NONE;
	surface->xdg_toplevel = 0;
}

static void
xdg_surface_get_toplevel(struct wl_client *client, struct wl_resource *resource,
		u32 id)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	struct wl_resource *xdg_toplevel = wl_resource_create(client,
		&xdg_toplevel_interface, XDG_TOPLEVEL_VERSION, id);
	wl_resource_set_implementation(xdg_toplevel, &xdg_toplevel_impl, surface,
		xdg_toplevel_unbind);

	surface->pending.flags |= SURFACE_NEW_ROLE;
	surface->pending.role = SURFACE_ROLE_TOPLEVEL;
	surface->xdg_toplevel = xdg_toplevel;
}


static void
xdg_surface_set_window_geometry(struct wl_client *client,
		struct wl_resource *resource, i32 x, i32 y, i32 width, i32 height)
{
	// TODO
}

static const struct xdg_surface_interface xdg_surface_impl = {
	.destroy             = do_nothing,
	.get_toplevel        = xdg_surface_get_toplevel,
	.get_popup           = do_nothing,
	.set_window_geometry = xdg_surface_set_window_geometry,
	.ack_configure       = do_nothing,
};

static void
xdg_wm_base_get_xdg_surface(struct wl_client *client,
		struct wl_resource *resource, u32 id,
		struct wl_resource *wl_surface)
{
	struct surface *surface = wl_resource_get_user_data(wl_surface);
	struct wl_resource *xdg_surface = wl_resource_create(client,
		&xdg_surface_interface, XDG_SURFACE_VERSION, id);
	wl_resource_set_implementation(xdg_surface, &xdg_surface_impl, surface, 0);

	surface->xdg_surface = xdg_surface;
}

static const struct xdg_wm_base_interface xdg_wm_base_impl = {
	.destroy           = do_nothing,
	.create_positioner = do_nothing,
	.get_xdg_surface   = xdg_wm_base_get_xdg_surface,
	.pong              = do_nothing,
};

static void
xdg_wm_base_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&xdg_wm_base_interface, XDG_WM_BASE_VERSION, id);
	wl_resource_set_implementation(resource, &xdg_wm_base_impl, data, 0);
}

static const struct wl_subsurface_interface subsurface_impl = {
	.destroy      = do_nothing,
	.set_position = do_nothing,
	.place_above  = do_nothing,
	.place_below  = do_nothing,
	.set_sync     = do_nothing,
	.set_desync   = do_nothing,
};

static void
subcompositor_get_subsurface(struct wl_client *client,
		struct wl_resource *resource, u32 id, struct wl_resource *_surface,
		struct wl_resource *parent)
{
	struct surface *surface = wl_resource_get_user_data(_surface);
	struct wl_resource *subsurface = wl_resource_create(client,
		&wl_subsurface_interface, WL_SUBSURFACE_VERSION, id);
	wl_resource_set_implementation(subsurface, &subsurface_impl, surface, 0);

	surface->pending.flags |= SURFACE_NEW_ROLE;
	surface->pending.role = SURFACE_ROLE_SUBSURFACE;
	surface->parent_surface = wl_resource_get_user_data(parent);
}

static const struct wl_subcompositor_interface subcompositor_impl = {
	.destroy        = do_nothing,
	.get_subsurface = subcompositor_get_subsurface,
};

static void
subcompositor_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_subcompositor_interface, WL_SUBCOMPOSITOR_VERSION, id);
	wl_resource_set_implementation(resource, &subcompositor_impl, data, 0);
}

static const struct wl_data_source_interface data_source_impl = {
	.offer       = do_nothing,
	.destroy     = do_nothing,
	.set_actions = do_nothing,
};

static void
data_device_manager_create_data_source(struct wl_client *client,
		struct wl_resource *resource, u32 id)
{
	struct wl_resource *data_source = wl_resource_create(client,
		&wl_data_source_interface, WL_DATA_SOURCE_VERSION, id);
	wl_resource_set_implementation(data_source, &data_source_impl, 0, 0);
}

static const struct wl_data_device_manager_interface data_device_manager_impl = {
	.create_data_source = data_device_manager_create_data_source,
	.get_data_device = do_nothing,
};

static void
data_device_manager_bind(struct wl_client *client, void *data, u32 version,
		u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_data_device_manager_interface, WL_DATA_DEVICE_MANAGER_VERSION, id);
	wl_resource_set_implementation(resource, &data_device_manager_impl, data, 0);
}

static i32
compositor_init(struct backend_memory *memory, struct egl *egl,
		struct game_window_manager *wm, i32 keymap, i32 keymap_size)
{
	struct compositor *compositor = memory->data;
	struct memory_arena *arena = &compositor->arena;
	struct wl_display *display = 0;

	if (!(display = wl_display_create())) {
		log_err("Failed to create wayland display");
		goto error_display;
	}

	const char *socket = wl_display_add_socket_auto(display);
	if (!socket) {
		log_err("Failed to add socket to the display");
		goto error_socket;
	}

	arena_init(arena, compositor + 1, memory->size - sizeof(*compositor));

	compositor->display = display;
	compositor->egl_display = egl->display;
	compositor->surfaces = arena_alloc(arena, MAX_SURFACE_COUNT, struct surface);
	compositor->window_manager.windows = arena_alloc(arena, MAX_WINDOW_COUNT,
		struct game_window);
	compositor->surface_count = 1;

	wl_list_init(&compositor->keyboards);
	wl_list_init(&compositor->pointers);
	wl_signal_init(&compositor->new_surface);

	compositor->compositor = wl_global_create(display,
		&wl_compositor_interface, WL_COMPOSITOR_VERSION, compositor,
		compositor_bind);
	compositor->output = wl_global_create(display, &wl_output_interface,
		WL_OUTPUT_VERSION, compositor, output_bind);
	compositor->xdg_wm_base = wl_global_create(display, &xdg_wm_base_interface,
		XDG_WM_BASE_VERSION, compositor, xdg_wm_base_bind);
	compositor->subcompositor = wl_global_create(display,
		&wl_subcompositor_interface, WL_SUBCOMPOSITOR_VERSION, compositor,
		subcompositor_bind);
	compositor->data_device_manager = wl_global_create(display,
		&wl_data_device_manager_interface, WL_DATA_DEVICE_MANAGER_VERSION,
		compositor, &data_device_manager_bind);
	compositor->seat = wl_global_create(display,
		&wl_seat_interface, WL_SEAT_VERSION, compositor, seat_bind);
	wl_display_init_shm(display);

	compositor->keymap = keymap;
	compositor->keymap_size = keymap_size;

	if (xwayland_init(&compositor->xwayland, compositor) != 0) {
		log_err("Failed to initialize xwayland");
		goto error_xwayland;
	}

	return 0;
error_xwayland:
	wl_global_destroy(compositor->compositor);
	wl_global_destroy(compositor->output);
	wl_global_destroy(compositor->xdg_wm_base);
	wl_global_destroy(compositor->subcompositor);
	wl_global_destroy(compositor->data_device_manager);
	wl_global_destroy(compositor->seat);
error_socket:
	wl_display_destroy(display);
error_display:
	return -1;
}

static void
compositor_finish(struct backend_memory *memory)
{
	struct compositor *compositor = memory->data;

	xwayland_finish(&compositor->xwayland);

	wl_global_destroy(compositor->compositor);
	wl_global_destroy(compositor->output);
	wl_global_destroy(compositor->xdg_wm_base);
	wl_global_destroy(compositor->subcompositor);
	wl_global_destroy(compositor->data_device_manager);
	wl_global_destroy(compositor->seat);
	wl_display_destroy(compositor->display);
}

static u32
get_time_msec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static struct game_window_manager *
compositor_update(struct backend_memory *memory)
{
	struct compositor *compositor = memory->data;
	struct game_window_manager *wm = &compositor->window_manager;
	struct wl_display *display = compositor->display;
	struct wl_event_loop *event_loop = wl_display_get_event_loop(display);

	wl_event_loop_dispatch(event_loop, 0);
	wl_display_flush_clients(display);

	u32 focused_surface = 0;
	u32 surface_count = compositor->surface_count;
	struct surface *surface = compositor->surfaces + 1;
	for (u32 i = 1; i < surface_count; i++) {
		struct wl_resource *frame_callback = surface->current.frame_callback;
		if (frame_callback) {
			wl_callback_send_done(frame_callback, get_time_msec());
			wl_resource_destroy(frame_callback);
			surface->current.frame_callback = 0;
		}

		if (surface->current.role == SURFACE_ROLE_CURSOR) {
			compositor->window_manager.cursor.texture = surface->texture;
		}

		u32 window_id = window_manager_get_window_id(wm, surface->window);
		if (window_id && window_id == wm->focused_window) {
			focused_surface = i;
		}

		surface++;
	}

	if (focused_surface != compositor->focused_surface) {
		if (compositor->focused_surface) {
			assert(focused_surface < compositor->surface_count);
			struct surface *surface =
				&compositor->surfaces[compositor->focused_surface];
			assert(surface->current.role != SURFACE_ROLE_NONE);

			struct wl_resource *keyboard;
			wl_resource_for_each(keyboard, &compositor->keyboards) {
				wl_keyboard_send_leave(keyboard, 0, surface->resource);
			}

			struct wl_resource *pointer;
			wl_resource_for_each(pointer, &compositor->pointers) {
				wl_pointer_send_leave(pointer, 0, surface->resource);
			}

			if (surface->current.role == SURFACE_ROLE_TOPLEVEL) {
				struct wl_resource *xdg_toplevel = surface->xdg_toplevel;
				struct wl_array array;
				wl_array_init(&array);
				xdg_toplevel_send_configure(xdg_toplevel, 0, 0, &array);
			}
		}

		if (focused_surface) {
			assert(focused_surface < compositor->surface_count);
			struct surface *surface = &compositor->surfaces[focused_surface];
			assert(surface->current.role != SURFACE_ROLE_NONE);

			struct wl_array array;
			wl_array_init(&array);

			struct wl_resource *keyboard;
			wl_resource_for_each(keyboard, &compositor->keyboards) {
				wl_keyboard_send_enter(keyboard, 0, surface->resource, &array);
				wl_keyboard_send_modifiers(keyboard, 0, 0, 0, 0, 0);
			}

			struct wl_resource *pointer;
			wl_resource_for_each(pointer, &compositor->pointers) {
				// TODO: compute the current position of the cursor on the
				// window itself.
				wl_fixed_t surface_x = wl_fixed_from_double(0.f);
				wl_fixed_t surface_y = wl_fixed_from_double(0.f);
				// TODO: generate a serial for the event
				wl_pointer_send_enter(pointer, 0, surface->resource,
					surface_x, surface_y);
			}

			if (surface->current.role == SURFACE_ROLE_TOPLEVEL) {
				struct wl_resource *xdg_toplevel = surface->xdg_toplevel;
				i32 *state = wl_array_add(&array, sizeof(*state));
				*state = XDG_TOPLEVEL_STATE_ACTIVATED;
				xdg_toplevel_send_configure(xdg_toplevel, 0, 0, &array);
			}
		}

		compositor->focused_surface = focused_surface;
	}

	return &compositor->window_manager;
}

static void
compositor_send_key(struct backend_memory *memory, i32 key, i32 state)
{
	struct compositor *compositor = memory->data;

	if (compositor->focused_surface) {
		u32 time = get_time_msec();

		struct wl_resource *keyboard;
		wl_resource_for_each(keyboard, &compositor->keyboards) {
			wl_keyboard_send_key(keyboard, 0, time, key, state);
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

	u32 serial = 0;

	// TODO: only send event to the focused window
	if (compositor->focused_surface) {
		struct wl_resource *keyboard;
		wl_resource_for_each(keyboard, &compositor->keyboards) {
			wl_keyboard_send_modifiers(keyboard, serial, depressed, latched,
				locked, group);
		}
	}
}

static void
compositor_send_button(struct backend_memory *memory, i32 button, i32 state)
{
	struct compositor *compositor = memory->data;

	if (compositor->focused_surface) {
		u32 time = get_time_msec();

		struct wl_resource *pointer;
		wl_resource_for_each(pointer, &compositor->pointers) {
			// TODO: generate a serial for the event
			wl_pointer_send_button(pointer, 0, time, button, state);
		}
	}
}

static void
compositor_send_motion(struct backend_memory *memory, i32 x, i32 y)
{
	struct compositor *compositor = memory->data;
	struct surface *focused_surface =
		&compositor->surfaces[compositor->focused_surface];
	if (focused_surface) {
		u32 time = get_time_msec();

		struct wl_resource *pointer;
		wl_resource_for_each(pointer, &compositor->pointers) {
			v2 cursor_pos = compositor->window_manager.cursor.position;
			wl_fixed_t surface_x = wl_fixed_from_double(cursor_pos.x);
			wl_fixed_t surface_y = wl_fixed_from_double(cursor_pos.y);
			wl_pointer_send_motion(pointer, time, surface_x, surface_y);
		}
	}
}
