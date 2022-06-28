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

#include <waycraft/backend_x11.h>

static const char *x11_atom_names[X11_ATOM_COUNT] = {
	[X11_NET_WM_NAME]      = "_NET_WM_NAME",
	[X11_WM_DELETE_WINDOW] = "WM_DELETE_WINDOW",
	[X11_WM_PROTOCOLS]     = "WM_PROTOCOLS",
	[X11_WM_NAME]          = "WM_NAME",
	[X11_UTF8_STRING]      = "UTF8_STRING",
};

struct compositor_event_array {
	struct compositor_event *at;
	u32 max_count;
	u32 count;
};

static struct compositor_event *
push_event(struct compositor_event_array *events, u32 type)
{
	struct compositor_event *result = 0;

	if (events->count < events->max_count) {
		result = &events->at[events->count++];
	}

	return result;
}

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

static void
x11_window_hide_cursor(xcb_connection_t *connection, xcb_window_t window)
{
	xcb_xfixes_hide_cursor(connection, window);
	xcb_xfixes_show_cursor(connection, window);
}

static i32
x11_window_init(struct x11_window *window)
{
	xcb_connection_t *connection = xcb_connect(0, 0);
	if (xcb_connection_has_error(connection)) {
		fprintf(stderr, "Failed to connect to X display\n");
		return -1;
	}

	xcb_screen_t *screen =
		xcb_setup_roots_iterator(xcb_get_setup(connection)).data;

	xcb_window_t xcb_window = xcb_generate_id(connection);
	u32 values[2];
	u32 mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	values[0] = screen->black_pixel;
	values[1] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
		XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
		XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
		XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_EXPOSURE |
		XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_FOCUS_CHANGE;

	xcb_create_window(connection, screen->root_depth, xcb_window, screen->root,
		0, 0, 640, 480, 1, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
		mask, values);
	xcb_map_window(connection, xcb_window);

	xcb_intern_atom_cookie_t cookies[X11_ATOM_COUNT] = {0};
	for (u32 i = 0; i < X11_ATOM_COUNT; i++) {
		cookies[i] = xcb_intern_atom(connection, 0, strlen(x11_atom_names[i]),
			x11_atom_names[i]);
	}

	xcb_atom_t *atoms = window->atoms;
	for (u32 i = 0; i < X11_ATOM_COUNT; i++) {
		xcb_intern_atom_reply_t *reply =
			xcb_intern_atom_reply(connection, cookies[i], 0);
		atoms[i] = reply->atom;
		free(reply);
	}

	const char *title = "Waycraft";
	xcb_change_property(connection, XCB_PROP_MODE_REPLACE, xcb_window,
		atoms[X11_WM_NAME], atoms[X11_UTF8_STRING], 8,
		strlen(title), title);

	xcb_change_property(connection, XCB_PROP_MODE_REPLACE, xcb_window,
		atoms[X11_WM_PROTOCOLS], XCB_ATOM_ATOM, 32, 1,
		&atoms[X11_WM_DELETE_WINDOW]);

	xcb_xfixes_query_version(connection, 4, 0);
	xcb_xfixes_hide_cursor(connection, xcb_window);

	xcb_flush(connection);

	/* initialize xkb */
	struct xkb_context *xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	xkb_x11_setup_xkb_extension(connection, XKB_X11_MIN_MAJOR_XKB_VERSION,
		XKB_X11_MIN_MINOR_XKB_VERSION, 0, 0, 0, 0, 0);
	i32 keyboard_device_id = xkb_x11_get_core_keyboard_device_id(connection);
	struct xkb_keymap *keymap = xkb_x11_keymap_new_from_device(
		xkb_context, connection, keyboard_device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
	char *keymap_string = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
	i32 keymap_size = strlen(keymap_string) + 1;
	i32 keymap_file = allocate_shm_file(keymap_size);

	i32 prot = PROT_READ | PROT_WRITE;
	i32 flags = MAP_SHARED;
	char *contents = mmap(0, keymap_size, prot, flags, keymap_file, 0);
	memcpy(contents, keymap_string, keymap_size);
	munmap(contents, keymap_size);

	window->keymap = keymap_file;
	window->keymap_size = keymap_size;
	window->xkb_state = xkb_x11_state_new_from_device(keymap, connection,
		keyboard_device_id);

	window->connection = connection;
	window->screen = screen;
	window->window = xcb_window;
	window->is_open = 1;

	return 0;
}

static u32
x11_get_key_state(u8 *key_vector, u8 key_code)
{
	u32 result = (key_vector[key_code / 8] & (1 << (key_code % 8))) != 0;
	return result;
}

static void
x11_window_update_modifiers(struct x11_window *window,
	struct compositor_event_array *event_array)
{
}

static void
x11_window_poll_events(struct x11_window *window, struct game_input *input,
	struct compositor_event_array *event_array)
{
	xcb_connection_t *connection = window->connection;
	union x11_event x11_event;

	struct xkb_state *xkb_state = window->xkb_state;

	memset(&input->mouse.buttons, 0, sizeof(input->mouse.buttons));
	input->dt = 0.01;
	input->width = window->width;
	input->height = window->height;
	input->mouse.dx = input->mouse.dy = 0;

	static const u32 wl_button[8] = {
		0,
		BTN_LEFT,
		BTN_MIDDLE,
		BTN_RIGHT,
	};

	xcb_atom_t wm_delete_window = window->atoms[X11_WM_DELETE_WINDOW];

	while ((x11_event.generic = xcb_poll_for_event(connection))) {
		switch (x11_event.generic->response_type & ~0x80) {
		case XCB_CONFIGURE_NOTIFY:
			input->width = window->width = x11_event.configure_notify->width;
			input->height = window->height = x11_event.configure_notify->height;
			break;
		case XCB_KEY_PRESS:
			{
				u32 keycode = x11_event.key_press->detail;
				u32 state = WL_KEYBOARD_KEY_STATE_PRESSED;
				xkb_state_update_key(xkb_state, keycode, XKB_KEY_DOWN);

				struct compositor_event *event = push_event(event_array, COMPOSITOR_KEY);
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

				struct compositor_event *event = push_event(event_array, COMPOSITOR_KEY);
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

				i32 mouse_was_warped = (x == window->width / 2 &&
					y == window->height / 2);
				if (!mouse_was_warped) {
					input->mouse.dx = x - input->mouse.x;
					input->mouse.dy = y - input->mouse.y;

					struct compositor_event *event = push_event(event_array, COMPOSITOR_MOTION);
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

				struct compositor_event *event = push_event(event_array, COMPOSITOR_BUTTON);
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

				struct compositor_event *event = push_event(event_array, COMPOSITOR_BUTTON);
				if (event) {
					event->button.code = wl_button[button];
					event->button.state = state;
				}
			}
			break;
		case XCB_CLIENT_MESSAGE:
			if (x11_event.client_message->data.data32[0] == wm_delete_window) {
				window->is_open = 0;
			}
			break;
		case XCB_FOCUS_IN:
			window->is_focused = 1;
			xcb_xfixes_hide_cursor(connection, window->window);
			break;
		case XCB_FOCUS_OUT:
			window->is_focused = 0;
			xcb_xfixes_show_cursor(connection, window->window);
			break;
		}

		free(x11_event.generic);
	}

	if (window->is_focused) {
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
	struct xkb_state *state = window->xkb_state;

	struct compositor_event *event = push_event(event_array, COMPOSITOR_MODIFIERS);
	if (event) {
		event->modifiers.depressed = xkb_state_serialize_mods(state, XKB_STATE_MODS_DEPRESSED);
		event->modifiers.latched = xkb_state_serialize_mods(state, XKB_STATE_MODS_LATCHED);
		event->modifiers.locked = xkb_state_serialize_mods(state, XKB_STATE_MODS_LOCKED);
		event->modifiers.group = xkb_state_serialize_layout(state, XKB_STATE_LAYOUT_EFFECTIVE);
	}

	// NOTE: lock the cursor
	if (window->is_focused) {
		xcb_warp_pointer(connection, 0, window->window, 0, 0, 0, 0,
			window->width / 2, window->height / 2);
	}

	xcb_flush(connection);
}

static void
x11_window_finish(struct x11_window *window)
{
	xkb_state_unref(window->xkb_state);
	xcb_destroy_window(window->connection, window->window);
	xcb_disconnect(window->connection);
}

static i32
x11_egl_init(struct egl *egl, struct x11_window *window)
{
	egl->display = eglGetDisplay(0);
	if (egl->display == EGL_NO_DISPLAY) {
		fprintf(stderr, "Failed to get the egl display\n");
		goto error_get_display;
	}

	i32 major, minor;
	if (!eglInitialize(egl->display, &major, &minor)) {
		fprintf(stderr, "Failed to initialize egl\n");
		goto error_initialize;
	}

	EGLint config_attributes[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_NONE
	};

	EGLConfig config;
	EGLint config_count;
	if (!eglChooseConfig(egl->display, config_attributes, &config, 1, &config_count)) {
		fprintf(stderr, "Failed to choose config\n");
		goto error_choose_config;
	}

	if (config_count != 1) {
		goto error_choose_config;
	}

	egl->surface = eglCreateWindowSurface(egl->display,
		config, window->window, 0);

	if (!eglBindAPI(EGL_OPENGL_API)) {
		goto error_bind_api;
	}

	EGLint context_attributes[] = {
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 3,
		EGL_NONE
	};

	egl->context = eglCreateContext(egl->display, config, EGL_NO_CONTEXT,
		context_attributes);
	if (egl->context == EGL_NO_CONTEXT) {
		goto error_context;
	}

	eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context);

	return 0;
error_context:
	eglDestroySurface(egl->display, egl->surface);
error_bind_api:
error_choose_config:
error_initialize:
	eglTerminate(egl->display);
error_get_display:
	return -1;
}

void
egl_finish(struct egl *egl)
{
	eglDestroyContext(egl->display, egl->context);
	eglDestroySurface(egl->display, egl->surface);
	eglTerminate(egl->display);
}

static f64
get_time_sec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int
x11_main(struct game_code game)
{
	struct game_window_manager window_manager = {0};
	struct x11_window window = {0};
	struct game_input input = {0};
	struct platform_memory game_memory = {0};
	struct platform_memory compositor_memory = {0};
	struct egl egl = {0};

	/* initialize the window */
	if (x11_window_init(&window) != 0) {
		fprintf(stderr, "Failed to initialize window\n");
		return 1;
	}

	/* initialize egl */
	if (x11_egl_init(&egl, &window) != 0) {
		fprintf(stderr, "Failed to initialize egl\n");
		return 1;
	}

#define X(name) gl.name = (gl##name##_t *)eglGetProcAddress("gl"#name);
	OPENGL_MAP_FUNCTIONS();
#undef X

	game.memory.gl = &gl;

	// NOTE: initialize the compositor
	compositor_memory.size = MB(64);
	compositor_memory.data = calloc(compositor_memory.size, 1);

	i32 keymap = window.keymap;
	i32 keymap_size = window.keymap_size;
	if (compositor_init(&compositor_memory, &egl, &window_manager, keymap,
			keymap_size) != 0) {
		fprintf(stderr, "Failed to initialize the compositor\n");
		return 1;
	}

	struct compositor_event_array events = {0};
	events.max_count = 1024;
	events.at = calloc(events.max_count, sizeof(*events.at));

	f64 target_frame_time = 1.0f / 60.0f;
	while (window.is_open) {
		f64 start_time = get_time_sec();

		events.count = 0;
		x11_window_poll_events(&window, &input, &events);
		struct game_window_manager *wm = compositor_update(&compositor_memory, events.at, events.count);

		gl.Viewport(0, 0, window.width, window.height);
		game_load(&game);
		if (game.update) {
			game.update(&game.memory, &input, wm);
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

	compositor_finish(&compositor_memory);
	free(compositor_memory.data);
	free(game_memory.data);
	egl_finish(&egl);
	x11_window_finish(&window);
	return 0;
}
