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
#include <waycraft/log.h>
#include <waycraft/xdg-shell-protocol.h>
#include <waycraft/xwayland.h>

#define WL_COMPOSITOR_VERSION 5
#define WL_KEYBOARD_VERSION 7
#define WL_OUTPUT_VERSION 4
#define WL_POINTER_VERSION 7
#define WL_REGION_VERSION 1
#define WL_SEAT_VERSION 7
#define WL_SURFACE_VERSION 5

struct compositor {
	struct game_window_manager window_manager;
	struct memory_arena arena;
	struct xwayland xwayland;
	struct wl_display *display;
	EGLDisplay *egl_display;

	struct wl_global *compositor;
	struct wl_global *output;
	struct seat {
		struct wl_global *global;
		struct wl_list keyboards;
		struct wl_list pointers;
	} seat;

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

	// TODO: attach the buffer to the pending state
}

static void
surface_frame(struct wl_client *client, struct wl_resource *resource,
		u32 callback)
{
	// TODO: create a frame callback resource
}

static void
surface_commit(struct wl_client *client, struct wl_resource *resource)
{
	// TODO: change the current state to the pending state
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
		struct wl_resource *_resource, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_surface_interface, WL_SURFACE_VERSION, id);
	wl_resource_set_implementation(resource, &surface_impl, 0, 0);
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
	wl_resource_set_implementation(resource, &compositor_impl, 0, 0);
}

static const struct wl_pointer_interface pointer_impl = {
	.set_cursor = do_nothing,
	.release    = do_nothing,
};

static const struct wl_keyboard_interface keyboard_impl = {
	.release = do_nothing,
};

static void
seat_get_pointer(struct wl_client *client, struct wl_resource *resource, u32 id)
{
	struct seat *seat = wl_resource_get_user_data(resource);
	struct wl_resource *pointer = wl_resource_create(client,
		&wl_pointer_interface, WL_POINTER_VERSION, id);
	wl_resource_set_implementation(pointer, &pointer_impl, 0, resource_remove);
	wl_list_insert(&seat->pointers, wl_resource_get_link(pointer));
}

static void
seat_get_keyboard(struct wl_client *client, struct wl_resource *resource, u32 id)
{
	struct seat *seat = wl_resource_get_user_data(resource);
	struct wl_resource *keyboard = wl_resource_create(client,
		&wl_pointer_interface, WL_KEYBOARD_VERSION, id);
	wl_resource_set_implementation(keyboard, &keyboard_impl, 0, resource_remove);
	wl_list_insert(&seat->keyboards, wl_resource_get_link(keyboard));
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
seat_init(struct seat *seat, struct wl_display *display)
{
	seat->global = wl_global_create(display, &wl_seat_interface,
		WL_SEAT_VERSION, seat, seat_bind);

	wl_list_init(&seat->keyboards);
	wl_list_init(&seat->pointers);
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

	arena_init(arena, compositor + 1, memory->size - sizeof(*compositor));

	compositor->display = display;
	compositor->egl_display = egl->display;

	compositor->compositor = wl_global_create(display,
		&wl_compositor_interface, WL_COMPOSITOR_VERSION, 0, compositor_bind);
	compositor->output = wl_global_create(display,
		&wl_output_interface, WL_OUTPUT_VERSION, 0, output_bind);
	seat_init(&compositor->seat, display);
	wl_display_init_shm(display);

	return 0;
error_display:
	return -1;
}

static void
compositor_finish(struct backend_memory *memory)
{
	struct compositor *compositor = memory->data;

	wl_global_destroy(compositor->compositor);
	wl_global_destroy(compositor->output);
	wl_global_destroy(compositor->seat.global);
	wl_display_destroy(compositor->display);
}

static struct game_window_manager *
compositor_update(struct backend_memory *memory)
{
	struct compositor *compositor = memory->data;
	struct wl_display *display = compositor->display;
	struct wl_event_loop *event_loop = wl_display_get_event_loop(display);

	wl_event_loop_dispatch(event_loop, 0);
	wl_display_flush_clients(display);

	return &compositor->window_manager;
}

static void
compositor_send_key(struct backend_memory *memory, i32 key, i32 state)
{
	// TODO
}

static void
compositor_send_modifiers(struct backend_memory *memory, u32 depressed,
		u32 latched, u32 locked, u32 group)
{
	// TODO
}

static void
compositor_send_button(struct backend_memory *memory, i32 button, i32 state)
{
	// TODO
}

static void
compositor_send_motion(struct backend_memory *memory, i32 x, i32 y)
{
	// TODO
}
