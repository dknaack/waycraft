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
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "game.h"
#include "math.h"
#include "gl.h"
#include "x11.h"

#include "gl.c"
#include "x11.c"
#include "math.c"

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

struct vertex {
    vec3 position;
    vec2 texcoord;
};

static const u8 *vert_shader_source = (u8 *)
    "#version 330 core\n"
    "layout (location = 0) in vec3 pos;"
    "layout (location = 1) in vec2 in_coords;"
    "out vec2 coords;"
    "uniform vec3 camera_position;"
    "uniform mat4 view;"
    "uniform mat4 projection;"
    "void main() {"
        "gl_Position = projection * view * vec4(pos - camera_position, 1.);"
        "coords = in_coords;"
    "}";

static const u8 *frag_shader_source = (u8 *)
    "#version 330 core\n"
    "in vec2 coords;"
    "out vec4 frag_color;"
    "uniform sampler2D tex;"
    "void main() {"
        "frag_color = texture(tex, coords);"
    "}";

struct world {
    u8 *blocks;
    u32 width;
    u32 height;
    u32 depth;
};

static void
world_init(struct world *world)
{
    u32 height = world->height;
    u32 width = world->width;
    u32 depth = world->depth;

    u8 *block = world->blocks;
    for (u32 z = 0; z < depth; z++) {
        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                *block++ = (x + y + z) & 1;
            }
        }
    }
}

static u32
world_at(const struct world *world, u32 x, u32 y)
{
    if ((0 <= x && x < world->width) && (0 <= y && y < world->height)) {
        return world->blocks[y * world->width + x];
    } else {
        return 0;
    }
}

static void
push_quad(struct vertex *out_vertex, u16 *out_index, u16 vertex_index,
        vec3 pos0, vec3 pos1, vec3 pos2, vec3 pos3)
{
    out_vertex->position = pos0;
    out_vertex->texcoord = VEC2(1, 1);
    out_vertex++;

    out_vertex->position = pos1;
    out_vertex->texcoord = VEC2(1, 0);
    out_vertex++;

    out_vertex->position = pos2;
    out_vertex->texcoord = VEC2(0, 0);
    out_vertex++;

    out_vertex->position = pos3;
    out_vertex->texcoord = VEC2(0, 1);
    out_vertex++;

    *out_index++ = vertex_index;
    *out_index++ = vertex_index + 1;
    *out_index++ = vertex_index + 3;
    *out_index++ = vertex_index + 1;
    *out_index++ = vertex_index + 2;
    *out_index++ = vertex_index + 3;
}

static void
world_generate_mesh(struct world *world, struct vertex *vertices, u32 *vertex_count, u16 *indices, u32 *index_count)
{
    struct vertex *out_vertex = vertices;
    u16 *out_index  = indices;
    u32 depth  = world->depth;
    u32 height = world->height;
    u32 width  = world->width;

    f32 size = 0.2;
    for (u32 z = 0; z < depth; z++) {
        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                if (world_at(world, x, y) != 0) {
                    u16 vertex_index = *vertex_count;

                    vec3 pos0 = VEC3(size * (x + 0.5), size * (y + 0.5), size * (z + 0.5));
                    vec3 pos1 = VEC3(size * (x + 0.5), size * (y - 0.5), size * (z + 0.5));
                    vec3 pos2 = VEC3(size * (x - 0.5), size * (y - 0.5), size * (z + 0.5));
                    vec3 pos3 = VEC3(size * (x - 0.5), size * (y + 0.5), size * (z + 0.5));

                    push_quad(out_vertex, out_index, vertex_index,
                            pos0, pos1, pos2, pos3);
                    out_vertex += 4;
                    out_index += 6;
                    *vertex_count += 4;
                    *index_count += 6;
                }
            }
        }
    }
}

int
main(void)
{
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

    u32 program = gl_program_create(vert_shader_source, frag_shader_source);
    i32 camera_uniform = gl.GetUniformLocation(program, "camera_position");
    i32 view_uniform = gl.GetUniformLocation(program, "view");
    i32 projection_uniform = gl.GetUniformLocation(program, "projection");

    if (program == 0) {
        u8 error[1024];
        gl_program_error(program, error, sizeof(error));
        fprintf(stderr, "Failed to create program: %s\n", error);
        return 1;
    }

    /* world mesh generation */
#define WORLD_DEPTH 32
#define WORLD_HEIGHT 32
#define WORLD_WIDTH  32
    static u8 world_blocks[WORLD_WIDTH * WORLD_HEIGHT * WORLD_DEPTH] = {0};
    struct world world = { 
        world_blocks,
        WORLD_WIDTH,
        WORLD_HEIGHT,
        WORLD_DEPTH
    };

    world_init(&world);

    struct vertex vertices[WORLD_WIDTH * WORLD_HEIGHT * WORLD_DEPTH * 4] = {0};
    u16 indices[WORLD_WIDTH * WORLD_HEIGHT * WORLD_DEPTH * 6] = {0};
    u32 index_count, vertex_count;
    world_generate_mesh(&world, vertices, &vertex_count, indices, &index_count);

    u32 vao, vbo, ebo;
    gl.GenBuffers(1, &vbo);
    gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.BufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(*vertices), 
            vertices, GL_STATIC_DRAW);

    gl.GenBuffers(1, &ebo);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(*indices),
            indices, GL_STATIC_DRAW);

    gl.GenVertexArrays(1, &vao);
    gl.BindVertexArray(vao);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), 
            (const void *)offsetof(struct vertex, position));
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), 
            (const void *)offsetof(struct vertex, texcoord));
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

    vec3 camera_position = {0};

    // TODO: fix timestep
    struct timespec wait_time = { 0, 1000000 };
    while (window.is_open) {
        f32 dt = 0.01;
        x11_window_poll_events(&window, &input);
        
        gl.Viewport(0, 0, window.width, window.height);
        gl.ClearColor(0.15, 0.15, 0.25, 1.0);
        gl.Clear(GL_COLOR_BUFFER_BIT);

        mat4 projection = mat4_perspective(45.f, 
                (f32)window.width / window.height, 0.1f, 100.f);
        mat4 view = mat4_look_at(VEC3(0, 0, 5), VEC3(0, 0, -1), VEC3(0, 1, 0));

        gl.UseProgram(program);

        f32 horizontal_axis = input.controller.move_right - 
            input.controller.move_left;
        f32 vertical_axis = input.controller.move_up - 
            input.controller.move_down;
        vec3 direction = VEC3(dt * horizontal_axis, dt * vertical_axis, 0);
        camera_position = vec3_add(camera_position, direction);
        gl.UniformMatrix4fv(projection_uniform, 1, GL_FALSE, projection.e);
        gl.UniformMatrix4fv(view_uniform, 1, GL_FALSE, view.e);
        gl.Uniform3f(camera_uniform, camera_position.x, camera_position.y,
                camera_position.z);
        gl.DrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, 0);

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
