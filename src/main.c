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
    0.5f,  0.5f, 0.0f,
    0.0f, -0.5f, 0.0f,
    1.0f, -0.5f, 0.0f,
};

static const u8 *vert_shader_source = (u8 *)
    "#version 330 core\n"
    "layout (location = 0) in vec3 pos;"
    "void main() {"
        "gl_Position = vec4(pos, 1.);"
    "}";

static const u8 *frag_shader_source = (u8 *)
    "#version 330 core\n"
    "out vec4 frag_color;"
    "void main() {"
        "frag_color = vec4(1.);"
    "}";

static u32
gl_shader_create(const u8 *src, u32 type)
{
    u32 shader = gl.CreateShader(type);

    gl.ShaderSource(shader, 1, (const char *const *)&src, 0);
    gl.CompileShader(shader);

    return shader;
}

static u32
gl_program_create(const u8 *vert_shader_source, const u8 *frag_shader_source)
{
    u32 program = gl.CreateProgram();
    u32 vert_shader = gl_shader_create(vert_shader_source, GL_VERTEX_SHADER);
    u32 frag_shader = gl_shader_create(frag_shader_source, GL_FRAGMENT_SHADER);

    gl.AttachShader(program, vert_shader);
    gl.AttachShader(program, frag_shader);
    gl.LinkProgram(program);
    gl.DeleteShader(vert_shader);
    gl.DeleteShader(frag_shader);

    return program;
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

    u32 program = gl_program_create(vert_shader_source, frag_shader_source);
    assert(program != 0);

    u32 vao, vbo;
    gl.GenBuffers(1, &vbo);
    gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    gl.GenVertexArrays(1, &vao);
    gl.BindVertexArray(vao);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), 0);
    gl.EnableVertexAttribArray(0);

    struct timespec wait_time = { 0, 1000000 };
    while (window.is_open) {
        x11_window_poll_events(&window);
        
        gl.ClearColor(0.15, 0.15, 0.25, 1.0);
        gl.Clear(GL_COLOR_BUFFER_BIT);

        gl.UseProgram(program);
        gl.BindVertexArray(vao);
        gl.DrawArrays(GL_TRIANGLES, 0, 3);

        glXSwapBuffers(window.display, window.drawable);
        nanosleep(&wait_time, 0);
    }

    gl.DeleteVertexArrays(1, &vao);
    gl.DeleteBuffers(1, &vbo);
    gl_context_finish(&gl, &window);
    x11_window_finish(&window);
    return 0;
}
