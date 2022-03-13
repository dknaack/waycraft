#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include "x11.h"
#include "game.h"

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
        StructureNotifyMask;
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

    while (XPending(window->display)) {
        XNextEvent(window->display, &event);

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
            window->lock_cursor = 1;
        case KeyPress:
            key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Escape) {
                window->lock_cursor = 0;
                XUngrabPointer(window->display, CurrentTime);
                break;
            }
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

#if 0
i32
x11_window_init_gl_context(struct x11_window *window, struct gl_context *gl)
{
    int attributes[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    };

    Display *display = window->display;
    XVisualInfo *visual = glXChooseVisual(display, 0, attributes);
    gl->context = glXCreateContext(display, visual, 0, True);
    glXMakeCurrent(display, window->drawable, gl->context);

    return gl_context_init(gl, glXGetProcAddress);
}

void
x11_window_finish_gl_context(struct x11_window *window, struct gl_context *gl)
{
    glXDestroyContext(window->display, gl->context);
}
#endif
