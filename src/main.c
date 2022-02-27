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
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "game.h"
#include "math.h"
#include "gl.h"
#include "x11.h"

#include "gl.c"
#include "x11.c"
#include "math.c"
#include "camera.c"
#include "game.c"

static struct gl_context gl;

#if 0
static const u16 indices[] = {
    0, 1, 3,
    1, 2, 3,
};

static const f32 vertices[] = {
     0.5f,  0.5f, 0.0f,     1.f, 1.f,
     0.5f, -0.5f, 0.0f,     1.f, 0.f,
    -0.5f, -0.5f, 0.0f,     0.f, 0.f,
    -0.5f,  0.5f, 0.0f,     0.f, 1.f,
};
#endif

int
main(void)
{
    struct game_state game = {0};
    struct game_input input = {0};
    struct x11_window window = {0};
    if (x11_window_init(&window) != 0) {
        fprintf(stderr, "Failed to initialize window\n");
        return 1;
    }

    if (gl_context_init(&gl, &window) != 0) {
        fprintf(stderr, "Failed to load the functions for OpenGL\n");
        return 1;
    }

    if (game_init(&game) != 0) {
        fprintf(stderr, "Failed to initialize the game\n");
        return 1;
    }

    // TODO: fix timestep
    struct timespec wait_time = { 0, 1000000 };
    while (window.is_open) {
        input.mouse.dx = input.mouse.dy = 0;
        memset(&input.controller, 0, sizeof(input.controller));
        x11_window_poll_events(&window, &input);

        input.dt = 0.01;
        input.width = window.width;
        input.height = window.height;
        gl.Viewport(0, 0, window.width, window.height);
        if (game_update(&game, &input) != 0) {
            window.is_open = 0;
        }

        glXSwapBuffers(window.display, window.drawable);
        nanosleep(&wait_time, 0);
    }

    gl_context_finish(&gl, &window);
    x11_window_finish(&window);
    return 0;
}
