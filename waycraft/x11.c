#include <EGL/eglext.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon-x11.h>

#include <waycraft/x11.h>

static const char *x11_atom_names[X11_ATOM_COUNT] = {
	[X11_NET_WM_NAME]      = "_NET_WM_NAME",
	[X11_WM_DELETE_WINDOW] = "WM_DELETE_WINDOW",
	[X11_WM_PROTOCOLS]     = "WM_PROTOCOLS",
	[X11_WM_NAME]          = "WM_NAME",
	[X11_UTF8_STRING]      = "UTF8_STRING",
};

static void
randname(char *buf)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	u64 r = ts.tv_nsec;
	for (u32 i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int
create_shm_file(void)
{
	i32 retries = 100;
	i32 fd = -1;

	do {
		char name[] = "/wl_shm-XXXXXX";
		randname(name + sizeof(name) - 7);
		retries--;
		fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			break;
		}
	} while (retries > 0 && errno == EEXIST);

	return fd;
}

static i32
allocate_shm_file(size_t size)
{
	i32 fd = create_shm_file();
	if (fd < 0)
		return -1;

	i32 ret = 0;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		close(fd);
		fd = -1;
	}

	return fd;
}

static u32
x11_get_key_state(u8 *key_vector, u8 key_code)
{
	u32 result = (key_vector[key_code / 8] & (1 << (key_code % 8))) != 0;
	return result;
}

static void
x11_poll_events(struct x11_state *state, struct game_input *input,
    struct platform_event_array *event_array)
{
	xcb_connection_t *connection = state->connection;
	union x11_event x11_event;

	struct xkb_state *xkb_state = state->xkb_state;

	memset(&input->mouse.buttons, 0, sizeof(input->mouse.buttons));
	input->dt = 0.01;
	input->width = state->width;
	input->height = state->height;
	input->mouse.dx = input->mouse.dy = 0;

	static const u32 wl_button[8] = {
		0,
		BTN_LEFT,
		BTN_MIDDLE,
		BTN_RIGHT,
	};

	xcb_atom_t wm_delete_state = state->atoms[X11_WM_DELETE_WINDOW];

	while ((x11_event.generic = xcb_poll_for_event(connection))) {
		switch (x11_event.generic->response_type & ~0x80) {
		case XCB_CONFIGURE_NOTIFY:
			input->width = state->width = x11_event.configure_notify->width;
			input->height = state->height = x11_event.configure_notify->height;
			break;
		case XCB_KEY_PRESS:
			{
				u32 keycode = x11_event.key_press->detail;
				u32 state = WL_KEYBOARD_KEY_STATE_PRESSED;
				xkb_state_update_key(xkb_state, keycode, XKB_KEY_DOWN);

				struct platform_event *event = push_event(event_array, PLATFORM_EVENT_KEY);
				if (event) {
					event->key.code = keycode - 8;
					event->key.state = state;
				}
			}
			break;
		case XCB_KEY_RELEASE:
			{
				u32 keycode = x11_event.key_press->detail;
				u32 state = WL_KEYBOARD_KEY_STATE_RELEASED;
				xkb_state_update_key(xkb_state, keycode, XKB_KEY_UP);

				struct platform_event *event = push_event(event_array, PLATFORM_EVENT_KEY);
				if (event) {
					event->key.code = keycode - 8;
					event->key.state = state;
				}
			}
			break;
		case XCB_MOTION_NOTIFY:
			{
				i32 x = x11_event.motion_notify->event_x;
				i32 y = x11_event.motion_notify->event_y;

				i32 mouse_was_warped = (x == state->width / 2 &&
				    y == state->height / 2);
				if (!mouse_was_warped) {
					input->mouse.dx = x - input->mouse.x;
					input->mouse.dy = y - input->mouse.y;

					struct platform_event *event = push_event(event_array, PLATFORM_EVENT_MOTION);
					if (event) {
						event->motion.x = x;
						event->motion.y = y;
					}
				}

				input->mouse.x = x;
				input->mouse.y = y;
			}
			break;
		case XCB_BUTTON_PRESS:
			{
				u32 button = x11_event.button_press->detail;
				if (button < 8) {
					input->mouse.buttons[button] |= 1;
				}

				u32 state = WL_POINTER_BUTTON_STATE_PRESSED;

				struct platform_event *event = push_event(event_array, PLATFORM_EVENT_BUTTON);
				if (event) {
					event->button.code = wl_button[button];
					event->button.state = state;
				}
			}
			break;
		case XCB_BUTTON_RELEASE:
			{
				u32 button = x11_event.button_release->detail;
				u32 state = WL_POINTER_BUTTON_STATE_RELEASED;

				struct platform_event *event = push_event(event_array, PLATFORM_EVENT_BUTTON);
				if (event) {
					event->button.code = wl_button[button];
					event->button.state = state;
				}
			}
			break;
		case XCB_CLIENT_MESSAGE:
			if (x11_event.client_message->data.data32[0] == wm_delete_state) {
				state->is_open = 0;
			}
			break;
		case XCB_FOCUS_IN:
			state->is_focused = 1;
			xcb_xfixes_hide_cursor(connection, state->window);
			break;
		case XCB_FOCUS_OUT:
			state->is_focused = 0;
			xcb_xfixes_show_cursor(connection, state->window);
			break;
		}

		free(x11_event.generic);
	}

	if (state->is_focused) {
		struct {
			xkb_keycode_t keycode;
			u8 *pressed;
		} keys_to_check[] = {
			{ 65, &input->controller.jump             },
			{ 25, &input->controller.move_up          },
			{ 26, &input->controller.toggle_inventory },
			{ 38, &input->controller.move_left        },
			{ 39, &input->controller.move_down        },
			{ 40, &input->controller.move_right       },
		};

		xcb_query_keymap_cookie_t cookie = xcb_query_keymap(connection);
		xcb_query_keymap_reply_t *reply = xcb_query_keymap_reply(
		    connection, cookie, 0);
		if (reply) {
			u8 *keys = reply->keys;

			for (u32 i = 0; i < LENGTH(keys_to_check); i++) {
				u32 keycode = keys_to_check[i].keycode;
				u8 *pressed = keys_to_check[i].pressed;
				u32 prev_state = *pressed & 1;
				u32 curr_state = x11_get_key_state(keys, keycode) != 0;
				*pressed = (prev_state << 1) | curr_state;
			}

			free(reply);
		}

		u32 state = XKB_STATE_MODS_DEPRESSED;
		input->shift_down = xkb_state_mod_name_is_active(xkb_state, XKB_MOD_NAME_SHIFT, state);
		input->ctrl_down = xkb_state_mod_name_is_active(xkb_state, XKB_MOD_NAME_CTRL, state);
		input->alt_down = xkb_state_mod_name_is_active(xkb_state, XKB_MOD_NAME_ALT, state);
	}

	// NOTE: update the modifiers
	struct platform_event *event = push_event(event_array, PLATFORM_EVENT_MODIFIERS);
	if (event) {
		event->modifiers.depressed = xkb_state_serialize_mods(xkb_state, XKB_STATE_MODS_DEPRESSED);
		event->modifiers.latched = xkb_state_serialize_mods(xkb_state, XKB_STATE_MODS_LATCHED);
		event->modifiers.locked = xkb_state_serialize_mods(xkb_state, XKB_STATE_MODS_LOCKED);
		event->modifiers.group = xkb_state_serialize_layout(xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	}

	// NOTE: lock the cursor
	if (state->is_focused) {
		xcb_warp_pointer(connection, 0, state->window, 0, 0, 0, 0,
		    state->width / 2, state->height / 2);
	}

	xcb_flush(connection);
}

static int
x11_main(struct game_code *game, struct platform_memory *compositor_memory,
    struct opengl_api *gl)
{
	/*
	 * NOTE: open the x11 window
	 */
	struct x11_state x11 = {0};
	x11.connection = xcb_connect(0, 0);
	if (xcb_connection_has_error(x11.connection)) {
		fprintf(stderr, "Failed to connect to X display\n");
		return -1;
	}

	x11.screen = xcb_setup_roots_iterator(
	    xcb_get_setup(x11.connection)).data;

	// create the window
	x11.window = xcb_generate_id(x11.connection);
	u32 values[2];
	u32 mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	values[0] = x11.screen->black_pixel;
	values[1] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
	    XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
	    XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
	    XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_EXPOSURE |
	    XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_FOCUS_CHANGE;
	xcb_create_window(x11.connection, x11.screen->root_depth, x11.window,
	    x11.screen->root, 0, 0, 640, 480, 1, XCB_WINDOW_CLASS_INPUT_OUTPUT,
	    x11.screen->root_visual, mask, values);
	xcb_map_window(x11.connection, x11.window);

	xcb_intern_atom_cookie_t cookies[X11_ATOM_COUNT] = {0};
	for (u32 i = 0; i < X11_ATOM_COUNT; i++) {
		cookies[i] = xcb_intern_atom(x11.connection,
		    0, strlen(x11_atom_names[i]), x11_atom_names[i]);
	}

	xcb_atom_t *atoms = x11.atoms;
	for (u32 i = 0; i < X11_ATOM_COUNT; i++) {
		xcb_intern_atom_reply_t *reply =
		    xcb_intern_atom_reply(x11.connection, cookies[i], 0);
		atoms[i] = reply->atom;
		free(reply);
	}

	// set the window title
	const char *title = "Waycraft";
	xcb_change_property(x11.connection, XCB_PROP_MODE_REPLACE, x11.window,
	    atoms[X11_WM_NAME], atoms[X11_UTF8_STRING], 8,
	    strlen(title), title);

	// enable closing the window
	xcb_change_property(x11.connection, XCB_PROP_MODE_REPLACE, x11.window,
	    atoms[X11_WM_PROTOCOLS], XCB_ATOM_ATOM, 32, 1,
	    &atoms[X11_WM_DELETE_WINDOW]);

	xcb_xfixes_query_version(x11.connection, 4, 0);
	xcb_xfixes_hide_cursor(x11.connection, x11.window);
	xcb_flush(x11.connection);

	/*
	 * NOTE: initialize xkb
	 */
	struct xkb_context *xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	xkb_x11_setup_xkb_extension(x11.connection, XKB_X11_MIN_MAJOR_XKB_VERSION,
	    XKB_X11_MIN_MINOR_XKB_VERSION, 0, 0, 0, 0, 0);
	i32 keyboard_device_id = xkb_x11_get_core_keyboard_device_id(x11.connection);
	struct xkb_keymap *keymap = xkb_x11_keymap_new_from_device(
	    xkb_context, x11.connection, keyboard_device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
	char *keymap_string = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
	i32 keymap_size = strlen(keymap_string) + 1;
	i32 keymap_file = allocate_shm_file(keymap_size);

	i32 prot = PROT_READ | PROT_WRITE;
	i32 flags = MAP_SHARED;
	char *contents = mmap(0, keymap_size, prot, flags, keymap_file, 0);
	memcpy(contents, keymap_string, keymap_size);
	munmap(contents, keymap_size);

	x11.xkb_state = xkb_x11_state_new_from_device(
	    keymap, x11.connection, keyboard_device_id);

	/*
	 * NOTE: initialize EGL
	 */
	struct egl_context egl = {0};
	if (egl_init(&egl, EGL_PLATFORM_XCB_EXT, x11.connection, x11.window) != 0) {
		fprintf(stderr, "Failed to initialize egl\n");
		return 1;
	}

#define X(name) gl->name = (gl##name##_t *)eglGetProcAddress("gl"#name);
	OPENGL_MAP_FUNCTIONS();
#undef X

	if (compositor_init(compositor_memory, egl.display, keymap_file, keymap_size) != 0) {
		fprintf(stderr, "Failed to initialize the compositor\n");
		return 1;
	}

	struct game_input input = {0};
	struct platform_event_array events = {0};
	events.max_count = 1024;
	events.at = calloc(events.max_count, sizeof(*events.at));

	x11.is_open = true;
	f64 target_frame_time = 1.0f / 60.0f;
	while (x11.is_open) {
		f64 start_time = get_time_sec();

		events.count = 0;
		x11_poll_events(&x11, &input, &events);
		if (!x11.is_open) {
			game->memory.is_done = true;
			compositor_memory->is_done = true;
		}

		struct game_window_manager *wm = compositor_update(
		    compositor_memory, events.at, events.count);

		game_load(game);
		if (game->update) {
			game->update(&game->memory, &input, wm);
		}

		eglSwapBuffers(egl.display, egl.surface);
		f64 end_time = get_time_sec();
		f64 elapsed_time = end_time - start_time;
		if (elapsed_time < target_frame_time) {
			u64 remaining_time = target_frame_time - elapsed_time;

			struct timespec sleep_time;
			sleep_time.tv_sec = remaining_time;
			sleep_time.tv_nsec = (remaining_time - sleep_time.tv_sec) * 1e9;

			nanosleep(&sleep_time, 0);
		}
	}

	// NOTE: cleanup
	close(keymap_file);
	egl_finish(&egl);
	xkb_state_unref(x11.xkb_state);
	xcb_destroy_window(x11.connection, x11.window);
	xcb_disconnect(x11.connection);
	return 0;
}
