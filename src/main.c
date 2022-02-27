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
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "gl.c"
#include "x11.c"

static struct gl_context gl;

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

static const u8 *vert_shader_source = (u8 *)
    "#version 330 core\n"
    "layout (location = 0) in vec3 pos;"
    "layout (location = 1) in vec2 in_coords;"
    "out vec2 coords;"
    "void main() {"
        "gl_Position = vec4(pos, 1.);"
        "coords = in_coords;"
    "}";

static const u8 *frag_shader_source = (u8 *)
    "#version 330 core\n"
    "in vec2 coords;"
    "out vec4 frag_color;"
    "uniform sampler2D tex;"
    "uniform vec4 tint;"
    "void main() {"
        "frag_color = mix(tint, texture(tex, coords), 0.5);"
    "}";

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
    i32 tint_uniform = gl.GetUniformLocation(program, "tint");
    assert(program != 0);

    u32 vao, vbo, ebo;
    gl.GenBuffers(1, &vbo);
    gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    gl.GenBuffers(1, &ebo);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    gl.GenVertexArrays(1, &vao);
    gl.BindVertexArray(vao);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), 0);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), 
            (const void *)(3 * sizeof(f32)));
    gl.EnableVertexAttribArray(1);

    u32 texture;
    {
        i32 width, height, comp;
        u8 *data;

        if (!(data = stbi_load("texture.jpg", &width, &height, &comp, 0))) {
            fprintf(stderr, "Failed to load texture\n");
            return 1;
        }

        gl.GenTextures(1, &texture);
        gl.BindTexture(GL_TEXTURE_2D, texture);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        gl.GenerateMipmap(GL_TEXTURE_2D);
    }

    // TODO: fix timestep
    struct timespec wait_time = { 0, 1000000 };
    while (window.is_open) {
        x11_window_poll_events(&window);
        
        gl.Viewport(0, 0, window.width, window.height);
        gl.ClearColor(0.15, 0.15, 0.25, 1.0);
        gl.Clear(GL_COLOR_BUFFER_BIT);

        gl.UseProgram(program);
        gl.Uniform4f(tint_uniform, 1.f, 0.f, 0.f, 1.f);
        gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

        glXSwapBuffers(window.display, window.drawable);
        nanosleep(&wait_time, 0);
    }

    gl.DeleteTextures(1, &texture);
    gl.DeleteVertexArrays(1, &vao);
    gl.DeleteBuffers(1, &vbo);
    gl.DeleteBuffers(1, &ebo);
    gl.DeleteProgram(program);
    gl_context_finish(&gl, &window);
    x11_window_finish(&window);
    return 0;
}
