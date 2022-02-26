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
#include <GL/glx.h>

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

struct gl_context {
    void (*Clear)(GLbitfield mask);
    void (*ClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

    GLXContext context;
};

static const char *gl_function_names[] = {
    "glClear",
    "glClearColor",
};

static struct gl_context gl;

static i32
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

static void
x11_window_finish(struct x11_window *window)
{
    XDestroyWindow(window->display, window->drawable);
    XCloseDisplay(window->display);
}

static void
x11_window_poll_events(struct x11_window *window)
{
    XEvent event;

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
        }
    }
}

static i32
gl_context_init(struct gl_context *gl, const struct x11_window *window)
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

    void (**gl_functions)(void);
    *(void **)&gl_functions = gl;

    for (i32 i = 0; i < LENGTH(gl_function_names); i++) {
        gl_functions[i] = glXGetProcAddress((u8 *)gl_function_names[i]);
        if (!gl_functions[i]) {
            return -1;
        }
    }

    return 0;
}

int
main(void)
{
    struct x11_window window = {0};
    if (x11_window_init(&window) != 0) {
        fprintf(stderr, "Failed to initialize window\n");
        return 1;
    }

    if (gl_context_init(&gl, &window) != 0) {
        fprintf(stderr, "Failed to load the functions for OpenGL\n");
        return 1;
    }

    struct timespec wait_time = { 0, 1000000 };
    while (window.is_open) {
        x11_window_poll_events(&window);
        
        gl.ClearColor(0.15, 0.15, 0.15, 1.0);
        gl.Clear(GL_COLOR_BUFFER_BIT);

        glXSwapBuffers(window.display, window.drawable);
        nanosleep(&wait_time, 0);
    }

    glXDestroyContext(window.display, gl.context);
    x11_window_finish(&window);
    return 0;
}
