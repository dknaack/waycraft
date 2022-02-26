#include <GL/gl.h>
#include <GL/glx.h>

#include "x11.h"
#include "gl.h"

static const char *gl_function_names[] = {
    "glClear",
    "glClearColor",
    "glGenBuffers",
    "glBindBuffer",
    "glDeleteBuffers",
    "glBufferData",
    "glGenVertexArrays",
    "glDeleteVertexArrays",
    "glBindVertexArrays",
    "glCreateShader",
    "glCreateProgram",
    "glAttachShader",
    "glCompileShader",
};

i32
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

void
gl_context_finish(struct gl_context *gl, const struct x11_window *window)
{
    glXDestroyContext(window->display, gl->context);
}
