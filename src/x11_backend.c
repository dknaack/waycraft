#include <errno.h>
#include <fcntl.h>
#include <X11/XKBlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/mman.h>
#include <time.h>
#include <wayland-server.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "backend.h"
#include "compositor.h"
#include "game.h"
#include "gl.h"
#include "x11_backend.h"

struct x11_window {
    Display *display;
    Drawable drawable;
    Visual *visual;
    XIM xim;
    XIC xic;
    GC gc;
    Atom net_wm_name;
    Atom wm_delete_win;
    u32 width, height;
    uint is_open;
    uint lock_cursor;
    uint is_active;
    i32 keymap;
    i32 keymap_size;
    struct xkb_state *xkb_state;
};

static void
randname(char *buf)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int
create_shm_file(void)
{
	int retries = 100;
	do {
		char name[] = "/wl_shm-XXXXXX";
		randname(name + sizeof(name) - 7);
		--retries;
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);
	return -1;
}

int
allocate_shm_file(size_t size)
{
	int fd = create_shm_file();
	if (fd < 0)
		return -1;
	int ret;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static void
x11_hide_cursor(struct x11_window *window)
{
    Cursor cursor;
    Pixmap empty_pixmap;
    XColor black = {0};
    static u8 empty_data[8] = {0};

    empty_pixmap = XCreateBitmapFromData(window->display, window->drawable,
                                         (const char *)empty_data, 8, 8);
    cursor = XCreatePixmapCursor(window->display, empty_pixmap, empty_pixmap,
                                 &black, &black, 0, 0);
    XDefineCursor(window->display, window->drawable, cursor);
    XFreeCursor(window->display, cursor);
    XFreePixmap(window->display, empty_pixmap);
}

static i32
x11_window_set_title(struct x11_window *window, char *title)
{
    XTextProperty prop;

    window->net_wm_name = XInternAtom(window->display, "_NET_WM_NAME", False);
    if (Xutf8TextListToTextProperty(window->display, &title, 1,
                                    XUTF8StringStyle, &prop) != Success) {
        fprintf(stderr, "Failed to convert title\n");
        return -1;
    }

    XSetWMName(window->display, window->drawable, &prop);
    XSetTextProperty(window->display, window->drawable, &prop,
                     window->net_wm_name);
    XFree(prop.value);

    return 0;
}

i32
x11_window_init(struct x11_window *window)
{
    char *title = "Waycraft";
    Window root;
    u32 mask, screen;

    if (!(window->display = XOpenDisplay(0))) {
        fprintf(stderr, "Failed to connect to X display\n");
        return -1;
    }

    screen = DefaultScreen(window->display);
    root = DefaultRootWindow(window->display);

    window->visual = DefaultVisual(window->display, screen);
    window->gc = DefaultGC(window->display, screen);
    window->width = 800;
    window->height = 600;
    window->drawable = XCreateSimpleWindow(window->display, root,
                                           0, 0, 800, 600, 0, 0, 0);
    mask = ButtonPressMask | ButtonReleaseMask | KeyPressMask |
        KeymapStateMask | ExposureMask | PointerMotionMask |
        StructureNotifyMask | EnterWindowMask | LeaveWindowMask;
    XSelectInput(window->display, window->drawable, mask);
    XMapWindow(window->display, window->drawable);

#if 0
    /* TODO: read keys as utf8 input */
    if (!(window->xim = XOpenIM(window->display, 0, 0, 0))) {
        fprintf(stderr, "Failed to open input method\n");
        return -1;
    }

    window->xic = XCreateIC(window->xim, XNInputStyle,
                            XIMPreeditNothing | XIMStatusNothing,
                            XNClientWindow, &window->drawable, NULL);
#endif

    window->wm_delete_win = XInternAtom(window->display,
                                        "WM_DELETE_WINDOW", False);
    XSetWMProtocols(window->display, window->drawable,
                    &window->wm_delete_win, 1);

    x11_window_set_title(window, title);
    x11_hide_cursor(window);

    window->is_open = 1;
    window->lock_cursor = 1;

    {
        xcb_connection_t *connection = XGetXCBConnection(window->display);
        struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        xkb_x11_setup_xkb_extension(connection, XKB_X11_MIN_MAJOR_XKB_VERSION,
                                    XKB_X11_MIN_MINOR_XKB_VERSION, 0, 0, 0, 0, 0);
        i32 keyboard_device_id = xkb_x11_get_core_keyboard_device_id(connection);
        struct xkb_keymap *keymap = xkb_x11_keymap_new_from_device(
            context, connection, keyboard_device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
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
        window->xkb_state = xkb_x11_state_new_from_device(
            keymap, connection, keyboard_device_id);
    }

    return 0;
}

void
x11_window_finish(struct x11_window *window)
{
    close(window->keymap);
    XDestroyWindow(window->display, window->drawable);
    XCloseDisplay(window->display);
}

u32
x11_get_key_state(u8 *key_vector, u8 key_code)
{
    u32 result = (key_vector[key_code / 8] & (1 << (key_code % 8))) != 0;
    return result;
}

void
x11_window_update_modifiers(struct x11_window *window,
                            struct compositor *compositor)
{
    struct xkb_state *state = window->xkb_state;

	u32 depressed = xkb_state_serialize_mods(state, XKB_STATE_MODS_DEPRESSED);
	u32 latched = xkb_state_serialize_mods(state, XKB_STATE_MODS_LATCHED);
	u32 locked = xkb_state_serialize_mods(state, XKB_STATE_MODS_LOCKED);
	u32 group = xkb_state_serialize_layout(state, XKB_STATE_LAYOUT_EFFECTIVE);

    compositor->send_modifiers(compositor, depressed, latched, locked, group);
}

void
x11_window_poll_events(struct x11_window *window, struct game_input *input,
                       struct compositor *compositor)
{
    struct xkb_state *xkb_state = window->xkb_state;
    KeySym key;
    XEvent event;
    i32 dx, dy;

    for (u32 i = 0; i < LENGTH(input->mouse.buttons); i++) {
        input->mouse.buttons[i] = 0;
    }

    input->dt = 0.01;
    input->width = window->width;
    input->height = window->height;
    input->mouse.dx = input->mouse.dy = 0;
    memset(&input->controller, 0, sizeof(input->controller));

    while (XPending(window->display)) {
        XNextEvent(window->display, &event);

        u32 is_pressed = 0;
        switch (event.type) {
        case ClientMessage:
            if (event.xclient.data.l[0] == window->wm_delete_win) {
                window->is_open = 0;
            }
            break;
        case ConfigureNotify:
            window->width = event.xconfigure.width;
            window->height = event.xconfigure.height;
            break;
        case MotionNotify:
            {
                i32 x = event.xmotion.x;
                i32 y = event.xmotion.y;

                dx = event.xmotion.x - input->mouse.x;
                dy = event.xmotion.y - input->mouse.y;
                input->mouse.x = x;
                input->mouse.y = y;

                i32 mouse_was_warped = (x == window->width / 2 &&
                                        y == window->height / 2);
                if (!mouse_was_warped) {
                    // TODO: convert to relative coordinates?
                    input->mouse.dx = dx;
                    input->mouse.dy = dy;

                    compositor->send_motion(compositor, x, y);
                }
            }
            break;
        case ButtonPress:
            is_pressed = 1;
        case ButtonRelease:
            window->lock_cursor = 1;

            u32 button_index = event.xbutton.button;
            if (button_index < 8) {
                input->mouse.buttons[button_index] |= is_pressed;
                u32 state = is_pressed ? 
                    WL_POINTER_BUTTON_STATE_PRESSED : 
                    WL_POINTER_BUTTON_STATE_RELEASED;
                compositor->send_button(compositor, button_index, state);
            }
            break;
        case KeyRelease:
            {
                key = XLookupKeysym(&event.xkey, 0);
                u32 keycode = event.xkey.keycode - 8;
                u32 state = WL_KEYBOARD_KEY_STATE_RELEASED;
                compositor->send_key(compositor, keycode, state);
                xkb_state_update_key(xkb_state, event.xkey.keycode, XKB_KEY_UP);
                x11_window_update_modifiers(window, compositor);
            }
            break;
        case KeyPress:
            {
                key = XLookupKeysym(&event.xkey, 0);
                u32 keycode = event.xkey.keycode - 8;
                u32 state = WL_KEYBOARD_KEY_STATE_PRESSED;
                compositor->send_key(compositor, keycode, state);
                xkb_state_update_key(xkb_state, event.xkey.keycode, XKB_KEY_DOWN);
                if (key == XK_Escape) {
                    window->lock_cursor = 0;
                    XUngrabPointer(window->display, CurrentTime);
                }

                x11_window_update_modifiers(window, compositor);
            }
            break;
        case EnterNotify:
            window->is_active = 1;
            break;
        case LeaveNotify:
            window->is_active = 0;
            break;
        }
    }

    if (window->is_active) {
        struct {
            KeySym key_sym;
            u8 *pressed;
        } keys_to_check[] = {
            { XK_space, &input->controller.jump   },
            { XK_w, &input->controller.move_up    },
            { XK_a, &input->controller.move_left  },
            { XK_s, &input->controller.move_down  },
            { XK_d, &input->controller.move_right },
        };

        u8 key_vector[32] = {0};
        XQueryKeymap(window->display, (char *)key_vector);
        for (u32 i = 0; i < LENGTH(keys_to_check); i++) {
            u8 key_code = XKeysymToKeycode(window->display,
                                           keys_to_check[i].key_sym);
            if (x11_get_key_state(key_vector, key_code)) {
                *keys_to_check[i].pressed = 1;
            }
        }

        u8 shift_keycode = XKeysymToKeycode(window->display, XK_Shift_L); 
        if (x11_get_key_state(key_vector, shift_keycode)) {
            input->controller.modifiers |= MOD_SHIFT;
        }

        u8 ctrl_keycode = XKeysymToKeycode(window->display, XK_Control_L); 
        if (x11_get_key_state(key_vector, ctrl_keycode)) {
            input->controller.modifiers |= MOD_CTRL;
        }

        u8 alt_keycode = XKeysymToKeycode(window->display, XK_Alt_L); 
        if (x11_get_key_state(key_vector, alt_keycode)) {
            input->controller.modifiers |= MOD_ALT;
        }
    }

    if (window->lock_cursor) {
        XWarpPointer(window->display, 0, window->drawable, 0, 0, 0, 0,
                     window->width / 2, window->height / 2);
        while (XPending(window->display)) {
            XNextEvent(window->display, &event);
        }
    }
}

static i32
x11_egl_init(struct egl *egl, struct x11_window *window)
{
    egl->display = eglGetDisplay(window->display);
    if (egl->display == EGL_NO_DISPLAY) {
        goto error_get_display;
    }

    i32 major, minor;
    if (!eglInitialize(egl->display, &major, &minor)) {
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
    if (!eglChooseConfig(egl->display, config_attributes, &config, 1, 
            &config_count)) {
        goto error_choose_config;
    }

    if (config_count != 1) {
        goto error_choose_config;
    }

    egl->surface = eglCreateWindowSurface(egl->display,
            config, window->drawable, 0);

    if (!eglBindAPI(EGL_OPENGL_API)) {
        goto error_bind_api;
    }

    EGLint context_attributes[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_NONE
    };

    egl->context = eglCreateContext(egl->display, config, 
            EGL_NO_CONTEXT, context_attributes);
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

int
x11_main(void)
{
    struct compositor *compositor;
    struct x11_window window = {0};
    struct game_input input = {0};
    struct backend_memory game_memory = {0};
    struct backend_memory compositor_memory = {0};
    struct egl egl = {0};

    /* initialize the window */
    if (x11_window_init(&window) != 0) {
        fprintf(stderr, "Failed to initialize window\n");
        return 1;
    }

    /* initialize egl */
    x11_egl_init(&egl, &window);
    gl_init(&gl, (gl_get_proc_address_t *)eglGetProcAddress);

    game_memory.size = MB(256);
    game_memory.data = calloc(game_memory.size, 1);

    compositor_memory.size = MB(64);
    compositor_memory.data = calloc(compositor_memory.size, 1);

    i32 keymap = window.keymap;
    i32 keymap_size = window.keymap_size;
    compositor = compositor_init(&compositor_memory, &egl, keymap, keymap_size);
    if (!compositor) {
        fprintf(stderr, "Failed to initialize the compositor\n");
        return 1;
    }

    // TODO: fix timestep
    struct timespec wait_time = { 0, 1000000 };
    while (window.is_open) {
        x11_window_poll_events(&window, &input, compositor);

        gl.Viewport(0, 0, window.width, window.height);
        game_update(&game_memory, &input, compositor);
        compositor->update(compositor);

        eglSwapBuffers(egl.display, egl.surface);
        nanosleep(&wait_time, 0);
    }

    egl_finish(&egl);
    compositor->finish(compositor);
    free(compositor_memory.data);
    free(game_memory.data);
    x11_window_finish(&window);
    return 0;
}
