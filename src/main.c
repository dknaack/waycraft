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
#include "x11.h"
#include "gl.h"

#include "gl.c"
#include "x11.c"

static struct gl_context gl;

static const f32 vertices[] = {
    0.5f,  0.5f,
    0.0f, -0.5f,
    1.0f, -0.5f,
};

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

    u32 vao, vbo;
    gl.GenVertexArrays(1, &vao);
    gl.GenBuffers(1, &vbo);

    gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    struct timespec wait_time = { 0, 1000000 };
    while (window.is_open) {
        x11_window_poll_events(&window);
        
        gl.ClearColor(0.15, 0.15, 0.15, 1.0);
        gl.Clear(GL_COLOR_BUFFER_BIT);

        glXSwapBuffers(window.display, window.drawable);
        nanosleep(&wait_time, 0);
    }

    gl.DeleteVertexArrays(1, &vao);
    gl.DeleteBuffers(1, &vbo);
    glXDestroyContext(window.display, gl.context);
    x11_window_finish(&window);
    return 0;
}
