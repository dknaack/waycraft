#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "types.h"

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
};

/* NOTE: order must match the function names */
struct x11_function_table {
    Display *(*XOpenDisplay)(const char *display_name);
    Window (*XCreateSimpleWindow)(Display *display, Window parent, 
            int x, int y, uint width, uint height, uint border_width, 
            ulong border, ulong background);
    int (*XSelectInput)(Display *display, Window window, long event_mask);
    int (*XMapWindow)(Display *display, Window window);
    Atom (*XInternAtom)(Display *display, 
            const char *atom_name, Bool only_if_exists);
    Status (*XSetWMProtocols)(Display *display, Window window, 
            Atom *protocols, int count);
    int (*Xutf8TextListToTextProperty)(Display *display, char **list, 
            int count, XICCEncodingStyle style, XTextProperty *text_property);
    void (*XSetWMName)(Display *display, Window window, 
            XTextProperty *text_property);
    void (*XSetTextProperty)(Display *display, Window window,
                             XTextProperty *text_property, Atom property);
    int (*XFree)(void *data);
    XImage *(*XCreateImage)(Display *display, Visual *visual,
                            uint depth, int format, int offset,
                            char *data, uint width, uint height,
                            int bitmap_pad, int bytes_per_line);
    int (*XDestroyImage)(XImage *ximage);
    int (*XDestroyWindow)(Display *display, Window window);
    int (*XCloseDisplay)(Display *display);
    int (*XPutImage)(Display *display, Drawable d, GC gc, XImage *image,
            int src_x, int src_y, int dst_x, int dst_y, 
            uint width, uint height);
    int (*XPending)(Display *display);
    int (*XNextEvent)(Display *display, XEvent *event);
    int (*XLookupString)(XKeyEvent *event, char *buffer, int count, 
            KeySym *keysym, XComposeStatus *status);
};

static const char *x11_function_names[] = {
    "XOpenDisplay",
    "XCreateSimpleWindow",
    "XSelectInput",
    "XMapWindow",
    "XInternAtom",
    "XSetWMProtocols",
    "Xutf8TextListToTextProperty",
    "XSetWMName",
    "XSetTextProperty",
    "XFree",
    "XCreateImage",
    "XDestroyImage",
    "XDestroyWindow",
    "XCloseDisplay",
    "XPutImage",
    "XPending",
    "XNextEvent",
    "XLookupString",
};

static struct x11_function_table x11;

static int
x11_window_init(struct x11_window *window)
{
    char *title = "Hello, world!";
    XTextProperty prop;
    Window root;
    u32 mask, depth, screen;

    if (!(window->display = x11.XOpenDisplay(0))) {
        fprintf(stderr, "Failed to connect to X display\n");
        return -1;
    }

    screen = DefaultScreen(window->display);
    depth = DefaultDepth(window->display, screen);
    root = DefaultRootWindow(window->display);

    window->visual = DefaultVisual(window->display, screen);
    window->gc = DefaultGC(window->display, screen);
    window->width = 800;
    window->height = 600;
    window->drawable = x11.XCreateSimpleWindow(window->display, root, 0, 0,
            800, 600, 0, 0, 0);
    mask = ButtonPressMask | ButtonReleaseMask | KeyPressMask | 
        KeyReleaseMask | ExposureMask | PointerMotionMask |
        StructureNotifyMask;
    x11.XSelectInput(window->display, window->drawable, mask);
    x11.XMapWindow(window->display, window->drawable);

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

    window->net_wm_name = x11.XInternAtom(window->display, "_NET_WM_NAME", False);
    window->wm_delete_win = x11.XInternAtom(window->display, "WM_DELETE_WINDOW", False);
    x11.XSetWMProtocols(window->display, window->drawable, &window->wm_delete_win, 1);

    if (x11.Xutf8TextListToTextProperty(window->display, &title, 1, XUTF8StringStyle,
                &prop) != Success) {
        fprintf(stderr, "Failed to convert title\n");
        return -1;
    }

    x11.XSetWMName(window->display, window->drawable, &prop);
    x11.XSetTextProperty(window->display, window->drawable, &prop, window->net_wm_name);
    x11.XFree(prop.value);

    window->is_open = 1;

    return 0;
}

static void
x11_window_finish(struct x11_window *window)
{
    x11.XDestroyWindow(window->display, window->drawable);
    x11.XCloseDisplay(window->display);
}

static int
load_code(void *module, u32 count, const char **names, void **functions)
{
    for (u32 i = 0; i < count; i++) {
        if (!(functions[i] = dlsym(module, names[i]))) {
            return -1;
        }
    }

    return 0;
}

static void
x11_window_poll_events(struct x11_window *window)
{
    XEvent event;

    while (x11.XPending(window->display)) {
        x11.XNextEvent(window->display, &event);

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
        }
    }
}

int
main(void)
{
    void *x11_module = dlopen("libX11.so", RTLD_LAZY);
    if (!x11_module) {
        fprintf(stderr, "Failed to load libX11.so: %s\n", dlerror());
        return 1;
    }

    if (load_code(x11_module, LENGTH(x11_function_names), 
                x11_function_names, (void **)&x11) != 0) {
        fprintf(stderr, "Failed to load the functions for X11: %s\n",
                dlerror());
        return 1;
    }

    struct x11_window window = {0};
    if (x11_window_init(&window) != 0) {
        fprintf(stderr, "Failed to initialize window\n");
        return 1;
    }

    struct timespec wait_time = { 0, 1000000 };
    while (window.is_open) {
        x11_window_poll_events(&window);
        nanosleep(&wait_time, 0);
    }

    x11_window_finish(&window);
    dlclose(x11_module);
    return 0;
}
