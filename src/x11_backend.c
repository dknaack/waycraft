#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <time.h>

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
};

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
    return 0;
}

void
x11_window_finish(struct x11_window *window)
{
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
x11_window_poll_events(struct x11_window *window, struct game_input *input)
{
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
            dx = event.xmotion.x - input->mouse.x;
            dy = event.xmotion.y - input->mouse.y;
            input->mouse.x = event.xmotion.x;
            input->mouse.y = event.xmotion.y;

            i32 mouse_was_warped = (input->mouse.x == window->width / 2 &&
                                    input->mouse.y == window->height / 2);
            if (!mouse_was_warped) {
                // TODO: convert to relative coordinates?
                input->mouse.dx = dx;
                input->mouse.dy = dy;
            }

            break;
        case ButtonPress:
            is_pressed = 1;
        case ButtonRelease:
            window->lock_cursor = 1;

            u32 button_index = event.xbutton.button;
            if (button_index < 8) {
                input->mouse.buttons[button_index] |= is_pressed;
            }
            break;
        case KeyPress:
            key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Escape) {
                window->lock_cursor = 0;
                XUngrabPointer(window->display, CurrentTime);
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
    struct compositor_state compositor_state = {0};
    struct x11_window window = {0};
    struct game_input input = {0};
    struct backend_memory game = {0};
    struct backend_memory compositor = {0};
    struct egl egl = {0};

    /* initialize the window */
    if (x11_window_init(&window) != 0) {
        fprintf(stderr, "Failed to initialize window\n");
        return 1;
    }

    /* initialize egl */
    x11_egl_init(&egl, &window);
    gl_init(&gl, (void (*(*)(const u8 *))(void))eglGetProcAddress);

    game.size = MB(256);
    game.data = calloc(game.size, 1);

    compositor.size = MB(64);
    compositor.data = calloc(compositor.size, 1);

    // TODO: fix timestep
    struct timespec wait_time = { 0, 1000000 };
    while (window.is_open) {
        x11_window_poll_events(&window, &input);

        gl.Viewport(0, 0, window.width, window.height);
        game_update(&game, &input, &compositor_state);
        compositor_update(&compositor, &egl, &compositor_state);

        eglSwapBuffers(egl.display, egl.surface);
        nanosleep(&wait_time, 0);
    }

    egl_finish(&egl);
    compositor_finish(&compositor);
    x11_window_finish(&window);
    return 0;
}
