#include <X11/Xlib.h>

#include "x11.h"
#include "game.h"

i32
x11_window_init(struct x11_window *window)
{
    char *title = "Hello, world!";
    XTextProperty prop;
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
    window->drawable = XCreateSimpleWindow(window->display, root, 0, 0,
            800, 600, 0, 0, 0);
    mask = ButtonPressMask | ButtonReleaseMask | KeyPressMask | 
        KeyReleaseMask | ExposureMask | PointerMotionMask |
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

    window->net_wm_name = XInternAtom(window->display, "_NET_WM_NAME", False);
    window->wm_delete_win = XInternAtom(window->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(window->display, window->drawable, &window->wm_delete_win, 1);

    if (Xutf8TextListToTextProperty(window->display, &title, 1, XUTF8StringStyle,
                &prop) != Success) {
        fprintf(stderr, "Failed to convert title\n");
        return -1;
    }

    XSetWMName(window->display, window->drawable, &prop);
    XSetTextProperty(window->display, window->drawable, &prop, window->net_wm_name);
    XFree(prop.value);

    window->is_open = 1;

    return 0;
}

void
x11_window_finish(struct x11_window *window)
{
    XDestroyWindow(window->display, window->drawable);
    XCloseDisplay(window->display);
}

void
x11_window_poll_events(struct x11_window *window, struct game_input *input)
{
    char keybuf[16] = {0};
    KeySym key;
    XEvent event;
    u32 is_pressed = 0;

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
            // TODO: convert to relative coordinates?
            input->mouse.dx = event.xmotion.x - input->mouse.x;
            input->mouse.dy = event.xmotion.y - input->mouse.y;
            input->mouse.x = event.xmotion.x;
            input->mouse.y = event.xmotion.y;
            break;
        case KeyPress:
            is_pressed = 1;
            /* fallthrough */
        case KeyRelease:
            XLookupString(&event.xkey, keybuf, sizeof(keybuf), &key, 0);

            switch (key) {
            case XK_w:
                input->controller.move_up = is_pressed;
                break;
            case XK_a:
                input->controller.move_left = is_pressed;
                break;
            case XK_s:
                input->controller.move_down = is_pressed;
                break;
            case XK_d:
                input->controller.move_right = is_pressed;
                break;
            }
            break;
        }
    }
}
