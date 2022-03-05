#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server.h>

#include "types.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "game.h"
#include "math.h"
#include "gl.h"
#include "x11.h"

#include "camera.c"
#include "game.c"
#include "gl.c"
#include "math.c"
#include "memory.c"
#include "noise.c"
#include "world.c"
#include "x11.c"

int
main(void)
{
    struct x11_window window = {0};
    struct game_state game = {0};
    struct game_input input = {0};
    struct wl_display *display;
    const char *socket;

    if (x11_window_init(&window) != 0) {
        fprintf(stderr, "Failed to initialize window\n");
        return 1;
    }

    if (!(display = wl_display_create())) {
        fprintf(stderr, "Failed to initialize display\n");
        return 1;
    }

    if (!(socket = wl_display_add_socket_auto(display))) {
        fprintf(stderr, "Failed to add a socket to the display\n");
        return 1;
    }

    struct wl_event_loop *event_loop = 
        wl_display_get_event_loop(display);

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
        wl_event_loop_dispatch(event_loop, 0);

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
    wl_display_destroy(display);
    x11_window_finish(&window);
    return 0;
}
