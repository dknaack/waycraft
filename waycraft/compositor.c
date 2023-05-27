#include "xdg-shell-protocol.c"
#include "waycraft/xwayland.c"

static void do_nothing() {}

static void
resource_remove(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));
}

static char *
surface_role_string(enum surface_role role)
{
	switch (role) {
	case SURFACE_ROLE_NONE:
		return "no";
	case SURFACE_ROLE_XDG_TOPLEVEL:
		return "xdg toplevel";
	case SURFACE_ROLE_XDG_POPUP:
		return "xdg popup";
	case SURFACE_ROLE_SUBSURFACE:
		return "subsurface";
	case SURFACE_ROLE_XWAYLAND:
		return "xwayland";
	case SURFACE_ROLE_CURSOR:
		return "cursor";
	default:
		return "invalid role";
	}
}

static bool
surface_set_role(struct surface *surface, u32 role, struct wl_resource *resource, u32 error)
{
	bool result = false;

	if (surface->role == SURFACE_ROLE_NONE || surface->role == role) {
		result = true;
		surface->role = role;
	} else if (resource) {
		wl_resource_post_error(resource, error,
			"Cannot assign %s role to surface@%d, as it already has %s role assigned.",
			surface_role_string(role), wl_resource_get_id(surface->resource),
			surface_role_string(surface->role));
	}

	return result;
}

static struct game_window *
window_manager_create_window(struct game_window_manager *wm)
{
	struct game_window *window = wm->windows;
	u32 window_count = wm->window_count;

	while (window_count-- > 0) {
		if (window->flags & WINDOW_DESTROYED) {
			return window;
		}

		window++;
	}

	wm->window_count++;
	return window;
}

/*
 * NOTE: surface implementation
 */

static void
surface_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	// TODO: remove the surface from the compositor
}

static void
surface_handle_attach(struct wl_client *client, struct wl_resource *resource,
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
surface_handle_damage(struct wl_client *client, struct wl_resource *resource,
		i32 x, i32 y, i32 width, i32 height)
{
	// TODO
}

static void
surface_handle_frame(struct wl_client *client,
		struct wl_resource *resource, u32 callback)
{
	struct wl_resource *frame_callback = wl_resource_create(client,
		&wl_callback_interface, wl_resource_get_version(resource), callback);

	struct surface *surface = wl_resource_get_user_data(resource);
	surface->pending.flags |= SURFACE_NEW_FRAME;
	surface->pending.frame_callback = frame_callback;
}

static void
surface_handle_set_opaque_region(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *region)
{
	// TODO
}

static void
surface_handle_set_input_region(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *region)
{
	// TODO
}

static void
surface_handle_commit(struct wl_client *client, struct wl_resource *resource)
{
	struct surface *surface = wl_resource_get_user_data(resource);

	if (!surface->current.buffer && !surface->pending.buffer &&
			surface->xdg_toplevel.surface) {
		xdg_surface_send_configure(surface->xdg_toplevel.surface, 0);
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

	if (surface->pending.flags & SURFACE_NEW_FRAME) {
		surface->current.frame_callback = surface->pending.frame_callback;
	}

	surface->pending.flags = 0;
}

static void
surface_handle_set_buffer_transform(struct wl_client *client,
		struct wl_resource *resource, i32 transform)
{
	if (transform != WL_OUTPUT_TRANSFORM_NORMAL) {
		wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_TRANSFORM,
			"Only normal buffer transform is supported");
	}
}

static void
surface_handle_set_buffer_scale(struct wl_client *client,
		struct wl_resource *resource, i32 scale)
{
	if (scale != 1) {
		wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_SCALE,
			"Scaling buffers is not supported");
	}
}

static void
surface_handle_damage_buffer(struct wl_client *client, struct wl_resource *resource,
		i32 x, i32 y, i32 width, i32 height)
{
}

static void
surface_handle_offset(struct wl_client *client,
		struct wl_resource *resource, i32 x, i32 y)
{
	if (x != 0 || y != 0) {
		wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_OFFSET,
			"Buffer offset is not supported");
	}
}

static const struct wl_surface_interface surface_impl = {
	.destroy              = surface_handle_destroy,
	.attach               = surface_handle_attach,
	.damage               = surface_handle_damage,
	.frame                = surface_handle_frame,
	.set_opaque_region    = surface_handle_set_opaque_region,
	.set_input_region     = surface_handle_set_input_region,
	.commit               = surface_handle_commit,
	.set_buffer_transform = surface_handle_set_buffer_transform,
	.set_buffer_scale     = surface_handle_set_buffer_scale,
	.damage_buffer        = surface_handle_damage_buffer,
	.offset               = surface_handle_offset,
};

static void
surface_destroy_resource(struct wl_resource *resource)
{
	struct surface *surface = wl_resource_get_user_data(resource);

	surface->role = SURFACE_ROLE_NONE;
	if (surface->texture) {
		gl.DeleteTextures(1, &surface->texture);
		surface->texture = 0;
	}
}

/*
 * NOTE: region implementation
 */
#if 0
static bool
region_push(struct region *region, u32 mode, i32 x, i32 y, i32 width, i32 height)
{
	bool result = false;
	box2 rect = { x, y, width, height };

	if (region->count < MAX_RECT_COUNT) {
		region->entries[region->count].mode = mode;
		region->entries[region->count].rect = rect;
		region->count++;

		result = true;
	}

	return result;
}

static void
region_destroy()
{
	// TODO
}

static void
region_add(struct wl_client *client, struct wl_resource *resource,
		i32 x, i32 y, i32 width, i32 height)
{
	struct region *region = wl_resource_get_user_data(resource);

	if (!region_push(region, REGION_ADD, x, y, width, height)) {
		// TODO: error handling
	}
}

static void
region_subtract(struct wl_client *client, struct wl_resource *resource,
		i32 x, i32 y, i32 width, i32 height)
{
	struct region *region = wl_resource_get_user_data(resource);

	if (!region_push(region, REGION_SUBTRACT, x, y, width, height)) {
		// TODO: error handling
	}
}
#endif

static const struct wl_region_interface region_impl = {
	.destroy  = do_nothing,
	.add      = do_nothing,
	.subtract = do_nothing,
};

/*
 * NOTE: compositor implementation
 */

static void
compositor_handle_create_surface(struct wl_client *client,
		struct wl_resource *resource, u32 id)
{
	struct compositor *compositor = wl_resource_get_user_data(resource);
	struct surface *surface = compositor->surfaces +
		compositor->surface_count++;
	struct wl_resource *wl_surface = wl_resource_create(client,
		&wl_surface_interface, wl_resource_get_version(resource), id);
	wl_resource_set_implementation(wl_surface, &surface_impl, surface,
		surface_destroy_resource);

	surface->resource = wl_surface;

	wl_signal_emit(&compositor->new_surface, wl_surface);
}

static void
compositor_handle_create_region(struct wl_client *client,
		struct wl_resource *_resource, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_region_interface, wl_resource_get_version(_resource), id);
	wl_resource_set_implementation(resource, &region_impl, NULL, NULL);
}

static const struct wl_compositor_interface compositor_impl = {
	.create_surface = compositor_handle_create_surface,
	.create_region  = compositor_handle_create_region,
};

static void
compositor_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_compositor_interface, version, id);
	wl_resource_set_implementation(resource, &compositor_impl, data, NULL);
}

/*
 * NOTE: pointer implementation
 */

static void
pointer_handle_set_cursor(struct wl_client *client, struct wl_resource *resource,
		u32 serial, struct wl_resource *_surface, i32 hotspot_x, i32 hotspot_y)
{
	if (_surface) {
		struct surface *surface = wl_resource_get_user_data(_surface);
		struct compositor *compositor = wl_resource_get_user_data(resource);

		if (surface_set_role(surface, SURFACE_ROLE_CURSOR,
				resource, WL_POINTER_ERROR_ROLE)) {
			compositor->window_manager.cursor.texture = surface->texture;
			compositor->window_manager.cursor.scale.x = surface->width;
			compositor->window_manager.cursor.scale.y = surface->height;
			compositor->window_manager.cursor.offset.x = -hotspot_x;
			compositor->window_manager.cursor.offset.x = -hotspot_y;
		}
	}
}

static const struct wl_pointer_interface pointer_impl = {
	.set_cursor = pointer_handle_set_cursor,
	.release    = do_nothing,
};

static const struct wl_keyboard_interface keyboard_impl = {
	.release = do_nothing,
};

/*
 * NOTE: seat implementation
 */

static void
seat_handle_get_pointer(struct wl_client *client, struct wl_resource *resource, u32 id)
{
	struct compositor *compositor = wl_resource_get_user_data(resource);

	u32 version = wl_resource_get_version(resource);
	struct wl_resource *pointer = wl_resource_create(
		client, &wl_pointer_interface, version, id);
	wl_resource_set_implementation(pointer, &pointer_impl, compositor, resource_remove);
	wl_list_insert(&compositor->pointers, wl_resource_get_link(pointer));
}

static void
seat_handle_get_keyboard(struct wl_client *client, struct wl_resource *resource, u32 id)
{
	struct compositor *compositor = wl_resource_get_user_data(resource);
	struct wl_resource *keyboard = wl_resource_create(client,
		&wl_keyboard_interface, wl_resource_get_version(resource), id);
	wl_resource_set_implementation(keyboard, &keyboard_impl, NULL, resource_remove);
	wl_list_insert(&compositor->keyboards, wl_resource_get_link(keyboard));
	wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
		compositor->keymap, compositor->keymap_size);
}

static void
seat_handle_get_touch(struct wl_client *client, struct wl_resource *resource, u32 id)
{
	wl_resource_post_error(resource, WL_SEAT_ERROR_MISSING_CAPABILITY,
		"missing touch capability");
}

static const struct wl_seat_interface seat_impl = {
	.get_pointer  = seat_handle_get_pointer,
	.get_keyboard = seat_handle_get_keyboard,
	.get_touch    = seat_handle_get_touch,
	.release      = do_nothing,
};

static void
seat_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_seat_interface, version, id);
	wl_resource_set_implementation(resource, &seat_impl, data, NULL);
	wl_resource_set_user_data(resource, data);

	u32 caps = WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER;
	const char *name = "seat0";

	wl_seat_send_name(resource, name);
	wl_seat_send_capabilities(resource, caps);
}

/*
 * NOTE: output implementation
 */

static void
output_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_output_interface, WL_OUTPUT_VERSION, id);
	wl_resource_set_implementation(resource, NULL, NULL, NULL);

	wl_output_send_geometry(resource, 0, 0, 0, 0, WL_OUTPUT_SUBPIXEL_NONE,
		"waycraft", "virtual-output0", WL_OUTPUT_TRANSFORM_NORMAL);
	wl_output_send_done(resource);
}

/*
 * NOTE: xdg toplevel implementation
 */

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

	surface->xdg_toplevel.toplevel = NULL;
}

/*
 * NOTE: xdg surface implementation
 */

static void
xdg_surface_handle_get_toplevel(struct wl_client *client, struct wl_resource *resource,
		u32 id)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	struct wl_resource *xdg_toplevel = wl_resource_create(client,
		&xdg_toplevel_interface, wl_resource_get_version(resource), id);
	wl_resource_set_implementation(xdg_toplevel, &xdg_toplevel_impl, surface,
		xdg_toplevel_unbind);

	surface->role = SURFACE_ROLE_XDG_TOPLEVEL;
	if (surface_set_role(surface, SURFACE_ROLE_XDG_TOPLEVEL,
			resource, XDG_WM_BASE_ERROR_ROLE)) {
		surface->xdg_toplevel.toplevel = xdg_toplevel;
		assert(xdg_toplevel);
	}
}

static void
xdg_surface_handle_get_popup(struct wl_client *client, struct wl_resource *resource,
		u32 id, struct wl_resource *parent, struct wl_resource *positioner)
{
	struct surface *surface = wl_resource_get_user_data(resource);

	if (surface_set_role(surface, SURFACE_ROLE_XDG_POPUP,
			resource, XDG_WM_BASE_ERROR_ROLE)) {
		surface->xdg_popup.parent = parent;
		surface->xdg_popup.positioner = positioner;
	}
}

static void
xdg_surface_handle_set_window_geometry(struct wl_client *client,
		struct wl_resource *resource, i32 x, i32 y, i32 width, i32 height)
{
	// TODO
}

static const struct xdg_surface_interface xdg_surface_impl = {
	.destroy             = do_nothing,
	.get_toplevel        = xdg_surface_handle_get_toplevel,
	.get_popup           = xdg_surface_handle_get_popup,
	.set_window_geometry = xdg_surface_handle_set_window_geometry,
	.ack_configure       = do_nothing,
};

/*
 * NOTE: xdg wm base implementation
 */

static void
xdg_wm_base_handle_get_xdg_surface(struct wl_client *client,
		struct wl_resource *resource, u32 id,
		struct wl_resource *wl_surface)
{
	struct surface *surface = wl_resource_get_user_data(wl_surface);
	struct wl_resource *xdg_surface = wl_resource_create(client,
		&xdg_surface_interface, wl_resource_get_version(resource), id);
	wl_resource_set_implementation(xdg_surface, &xdg_surface_impl, surface, NULL);

	surface->xdg_toplevel.surface = xdg_surface;
}

static const struct xdg_wm_base_interface xdg_wm_base_impl = {
	.destroy           = do_nothing,
	.create_positioner = do_nothing,
	.get_xdg_surface   = xdg_wm_base_handle_get_xdg_surface,
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

/*
 * NOTE: subcompositor implementation
 */
static void
subcompositor_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
subcompositor_handle_get_subsurface(struct wl_client *client,
		struct wl_resource *resource, u32 id, struct wl_resource *_surface,
		struct wl_resource *parent)
{
	struct surface *surface = wl_resource_get_user_data(_surface);
	struct wl_resource *subsurface = wl_resource_create(client,
		&wl_subsurface_interface, wl_resource_get_version(resource), id);
	wl_resource_set_implementation(subsurface, &subsurface_impl, surface, 0);

	if (surface_set_role(surface, SURFACE_ROLE_SUBSURFACE, resource,
			WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE)) {
		surface->subsurface.parent = wl_resource_get_user_data(parent);
	}
}

static const struct wl_subcompositor_interface subcompositor_impl = {
	.destroy        = subcompositor_handle_destroy,
	.get_subsurface = subcompositor_handle_get_subsurface,
};

static void
subcompositor_bind(struct wl_client *client, void *data, u32 version, u32 id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&wl_subcompositor_interface, WL_SUBCOMPOSITOR_VERSION, id);
	wl_resource_set_implementation(resource, &subcompositor_impl, data, 0);
}

/*
 * NOTE: data source implementation
 */
static void
data_source_handle_offer(struct wl_client *client,
	struct wl_resource *resource, const char *mime_type)
{
	// TODO
}

static void
data_source_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	// TODO
}

static void
data_source_handle_set_actions(struct wl_client *client,
		struct wl_resource *resource, u32 dnd_actions)
{
	// TODO
}


static const struct wl_data_source_interface data_source_impl = {
	.offer       = data_source_handle_offer,
	.destroy     = data_source_handle_destroy,
	.set_actions = data_source_handle_set_actions,
};

/*
 * NOTE: data device implementation
 */

static void
data_device_start_drag(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *source, struct wl_resource *origin,
		struct wl_resource *icon, u32 serial)
{
	if (icon) {
		struct surface *icon_surface = wl_resource_get_user_data(icon);
		if (surface_set_role(icon_surface, SURFACE_ROLE_DRAG_AND_DROP_ICON,
				resource, WL_DATA_DEVICE_ERROR_ROLE)) {
			// TODO
		}
	} else {
		// TODO
	}
}

static void
data_device_set_selection(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *source, u32 serial)
{
	// TODO
}

static void
data_device_release(struct wl_client *client, struct wl_resource *resource)
{
	// TODO
}

static const struct wl_data_device_interface data_device_impl = {
	.start_drag    = data_device_start_drag,
	.set_selection = data_device_set_selection,
	.release       = data_device_release,
};

/*
 * NOTE: data device manager implementation
 */

static void
data_device_manager_create_data_source(struct wl_client *client,
		struct wl_resource *resource, u32 id)
{
	struct wl_resource *data_source = wl_resource_create(client,
		&wl_data_source_interface, wl_resource_get_version(resource), id);
	wl_resource_set_implementation(data_source, &data_source_impl, NULL, NULL);
}

static void
data_device_manager_get_data_device(struct wl_client *client,
		struct wl_resource *resource, u32 id, struct wl_resource *seat)
{
	struct wl_resource *data_device = wl_resource_create(client,
		&wl_data_device_interface, wl_resource_get_version(resource), id);
	wl_resource_set_implementation(data_device, &data_device_impl, NULL, NULL);
}

static struct wl_data_device_manager_interface data_device_manager_impl = {
	.create_data_source = data_device_manager_create_data_source,
	.get_data_device    = data_device_manager_get_data_device,
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
compositor_init(struct platform_memory *memory,
		EGLDisplay *egl_display, i32 keymap, i32 keymap_size)
{
	struct compositor *compositor = memory->data;
	struct arena *arena = &compositor->arena;
	struct wl_display *display = NULL;

	if (!(display = wl_display_create())) {
		log_err("Failed to create wayland display");
		goto error_display;
	}

	const char *socket = wl_display_add_socket_auto(display);
	if (!socket) {
		log_err("Failed to add socket to the display");
		goto error_socket;
	}

	*arena = arena_init(compositor + 1, memory->size - sizeof(*compositor));

	compositor->display = display;
	compositor->egl_display = egl_display;
	compositor->surfaces = ALLOC(arena, MAX_SURFACE_COUNT, struct surface);
	compositor->window_manager.windows = ALLOC(
		arena, MAX_WINDOW_COUNT, struct game_window);
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

	compositor->xwayland_surface_destroy.notify = xwayland_surface_destroy;
	wl_signal_add(&compositor->xwayland.xwm.destroy_notify,
		&compositor->xwayland_surface_destroy);

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
compositor_finish(struct platform_memory *memory)
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
compositor_update(struct platform_memory *memory,
		struct platform_event *event, u32 event_count)
{
	gl = *memory->gl;

	struct compositor *compositor = memory->data;
	struct game_window_manager *wm = &compositor->window_manager;
	struct wl_display *display = compositor->display;
	struct wl_event_loop *event_loop = wl_display_get_event_loop(display);

	wl_event_loop_dispatch(event_loop, 0);
	wl_display_flush_clients(display);

	struct surface *focused_surface = NULL;
	struct wl_client *focused_client = NULL;
	if (compositor->focused_surface) {
		focused_surface = &compositor->surfaces[compositor->focused_surface];
		focused_client = wl_resource_get_client(focused_surface->resource);
	}

	while (event_count-- > 0) {
		switch (event->type) {
		case PLATFORM_EVENT_BUTTON:
			{
				i32 button = event->button.code;
				i32 state = event->button.state;
				if (focused_surface) {
					u32 time = get_time_msec();
					u32 serial = wl_display_next_serial(compositor->display);

					struct wl_resource *pointer;
					wl_resource_for_each(pointer, &compositor->pointers) {
						if (wl_resource_get_client(pointer) == focused_client) {
							wl_pointer_send_button(pointer, serial, time, button, state);
						}
					}
				}
			}
			break;
		case PLATFORM_EVENT_KEY:
			{
				i32 key = event->key.code;
				i32 state = event->key.state;

				if (focused_surface) {
					u32 time = get_time_msec();
					u32 serial = wl_display_next_serial(compositor->display);

					struct wl_resource *keyboard;
					wl_resource_for_each(keyboard, &compositor->keyboards) {
						if (wl_resource_get_client(keyboard) == focused_client) {
							wl_keyboard_send_key(keyboard, serial, time, key, state);
						}
					}
				}
			}
			break;
		case PLATFORM_EVENT_MODIFIERS:
			{
				u32 depressed = event->modifiers.depressed;
				u32 latched = event->modifiers.latched;
				u32 locked = event->modifiers.locked;
				u32 group = event->modifiers.group;

				if (depressed != compositor->modifiers.depressed ||
						latched != compositor->modifiers.latched ||
						locked != compositor->modifiers.locked ||
						group != compositor->modifiers.group) {
					compositor->modifiers.depressed = depressed;
					compositor->modifiers.latched = latched;
					compositor->modifiers.locked = locked;
					compositor->modifiers.group = group;

					u32 serial = 0;

					if (focused_surface) {
						struct wl_resource *keyboard;
						wl_resource_for_each(keyboard, &compositor->keyboards) {
							if (wl_resource_get_client(keyboard) == focused_client) {
								wl_keyboard_send_modifiers(keyboard, serial,
									depressed, latched, locked, group);
							}
						}
					}
				}
			}
			break;
		case PLATFORM_EVENT_MOTION:
			{
				if (focused_surface) {
					u32 time = get_time_msec();

					struct wl_resource *pointer;
					wl_resource_for_each(pointer, &compositor->pointers) {
						if (wl_resource_get_client(pointer) == focused_client) {
							v2 cursor_pos = compositor->window_manager.cursor.position;
							wl_fixed_t surface_x = wl_fixed_from_double(cursor_pos.x);
							wl_fixed_t surface_y = wl_fixed_from_double(cursor_pos.y);
							wl_pointer_send_motion(pointer, time, surface_x, surface_y);
						}
					}
				}
			}
			break;
		default:
			assert(!"Invalid event received");
		}

		event++;
	}

	u32 focused_surface_index = 0;
	u32 surface_count = compositor->surface_count;
	for (u32 i = 1; i < surface_count; i++) {
		struct surface *surface = &compositor->surfaces[i];
		struct wl_resource *frame_callback = surface->current.frame_callback;
		if (frame_callback) {
			wl_callback_send_done(frame_callback, get_time_msec());
			wl_resource_destroy(frame_callback);
			surface->current.frame_callback = 0;
		}

		if (&compositor->surfaces[compositor->focused_surface] == surface &&
				surface->role == SURFACE_ROLE_NONE) {
			struct game_window *focused_window = window_manager_get_window(wm,
				wm->focused_window);
			focused_window->flags |= WINDOW_DESTROYED;

			compositor->focused_surface = 0;
			wm->focused_window = 0;
		}

		if (surface->role == SURFACE_ROLE_XDG_TOPLEVEL && !surface->window) {
			struct game_window *window = window_manager_create_window(wm);
			window->flags = 0;
			window->texture = surface->texture;
			surface->window = window;
		} else if (surface->role == SURFACE_ROLE_XWAYLAND && !surface->window) {
			struct game_window *window = window_manager_create_window(wm);
			window->flags = 0;
			window->texture = surface->texture;
			surface->window = window;
		} else if (surface->role == SURFACE_ROLE_CURSOR) {
			assert(surface->texture);
			wm->cursor.texture = surface->texture;
		}

		u32 window_id = window_manager_get_window_id(wm, surface->window);
		if (window_id && window_id == wm->focused_window) {
			focused_surface_index = i;
		}
	}

	if (focused_surface_index != compositor->focused_surface) {
		if (compositor->focused_surface) {
			assert(focused_surface->role != SURFACE_ROLE_NONE);

			struct wl_resource *keyboard;
			wl_resource_for_each(keyboard, &compositor->keyboards) {
				if (wl_resource_get_client(keyboard) == focused_client) {
					wl_keyboard_send_leave(keyboard, 0, focused_surface->resource);
				}
			}

			struct wl_resource *pointer;
			wl_resource_for_each(pointer, &compositor->pointers) {
				if (wl_resource_get_client(pointer) == focused_client) {
					wl_pointer_send_leave(pointer, 0, focused_surface->resource);
				}
			}

			if (focused_surface->role == SURFACE_ROLE_XDG_TOPLEVEL) {
				struct wl_resource *xdg_toplevel = focused_surface->xdg_toplevel.toplevel;
				struct wl_array array;
				wl_array_init(&array);
				xdg_toplevel_send_configure(xdg_toplevel, 0, 0, &array);
			} else if (focused_surface->role == SURFACE_ROLE_XWAYLAND) {
				xcb_connection_t *connection = compositor->xwayland.xwm.connection;
				xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE, XCB_NONE, XCB_CURRENT_TIME);
				xcb_flush(connection);
			}
		}

		if (focused_surface_index) {
			assert(focused_surface_index < compositor->surface_count);
			focused_surface = &compositor->surfaces[focused_surface_index];
			focused_client = wl_resource_get_client(focused_surface->resource);
			assert(focused_surface->role != SURFACE_ROLE_NONE);

			struct wl_array array;
			wl_array_init(&array);

			u32 serial = wl_display_next_serial(compositor->display);

			struct wl_resource *keyboard = NULL;
			wl_resource_for_each(keyboard, &compositor->keyboards) {
				if (wl_resource_get_client(keyboard) == focused_client) {
					wl_keyboard_send_enter(keyboard, serial, focused_surface->resource, &array);
					wl_keyboard_send_modifiers(keyboard, serial, 0, 0, 0, 0);
				}
			}

			struct wl_resource *pointer = NULL;
			wl_resource_for_each(pointer, &compositor->pointers) {
				if (wl_resource_get_client(pointer) == focused_client) {
					// TODO: compute the current position of the cursor on the
					// window itself.
					wl_fixed_t surface_x = wl_fixed_from_double(0.f);
					wl_fixed_t surface_y = wl_fixed_from_double(0.f);
					// TODO: generate a serial for the event
					wl_pointer_send_enter(pointer, serial, focused_surface->resource,
						surface_x, surface_y);
				}
			}

			if (focused_surface->role == SURFACE_ROLE_XDG_TOPLEVEL) {
				struct wl_resource *xdg_toplevel =
					focused_surface->xdg_toplevel.toplevel;

				i32 *state = wl_array_add(&array, sizeof(*state));
				*state = XDG_TOPLEVEL_STATE_ACTIVATED;
				xdg_toplevel_send_configure(xdg_toplevel, 0, 0, &array);
			} else if (focused_surface->role == SURFACE_ROLE_XWAYLAND) {
				xwm_focus(&compositor->xwayland.xwm,
					&focused_surface->xwayland_surface);
			}
		}

		compositor->focused_surface = focused_surface_index;
	}

	if (memory->is_done) {
		compositor_finish(memory);
	}

	return &compositor->window_manager;
}
