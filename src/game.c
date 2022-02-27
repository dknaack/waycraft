#include <stdio.h>
#include <stdlib.h>

#include "gl.h"
#include "game.h"

struct mesh {
    struct vertex *vertices;
    u32 *indices;

    u32 vertex_count;
    u32 index_count;
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
                *block++ = rand() > RAND_MAX / 2;
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
mesh_push_quad(struct mesh *mesh, vec3 pos0, vec3 pos1, vec3 pos2, vec3 pos3)
{
    u32 vertex_count = mesh->vertex_count;
    struct vertex *out_vertex = mesh->vertices + vertex_count;
    u32 *out_index = mesh->indices + mesh->index_count;

    out_vertex->position = pos0;
    out_vertex->texcoord = VEC2(1, 1);
    out_vertex++;

    out_vertex->position = pos1;
    out_vertex->texcoord = VEC2(1, 0);
    out_vertex++;

    out_vertex->position = pos2;
    out_vertex->texcoord = VEC2(0, 1);
    out_vertex++;

    out_vertex->position = pos3;
    out_vertex->texcoord = VEC2(0, 0);
    out_vertex++;

    *out_index++ = vertex_count;
    *out_index++ = vertex_count + 2;
    *out_index++ = vertex_count + 1;
    *out_index++ = vertex_count + 2;
    *out_index++ = vertex_count + 3;
    *out_index++ = vertex_count + 1;

    mesh->index_count += 6;
    mesh->vertex_count += 4;
}

static void
world_generate_mesh(struct world *world, struct mesh *mesh)
{
    u32 depth  = world->depth;
    u32 height = world->height;
    u32 width  = world->width;

    f32 size = 0.2;
    for (u32 z = 0; z < depth; z++) {
        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                if (world_at(world, x, y) != 0) {
                    vec3 pos0 = VEC3(
                            size * (x + 0.5 - width / 2.), 
                            size * (y + 0.5 - height / 2.), 
                            size * (z + 0.5 - depth / 2.));
                    vec3 pos1 = VEC3(
                            size * (x - 0.5 - width / 2.),
                            size * (y + 0.5 - height / 2.),
                            size * (z + 0.5 - depth / 2.));
                    vec3 pos2 = VEC3(
                            size * (x + 0.5 - width / 2.),
                            size * (y - 0.5 - height / 2.),
                            size * (z + 0.5 - depth / 2.));
                    vec3 pos3 = VEC3(
                            size * (x - 0.5 - width / 2.),
                            size * (y - 0.5 - height / 2.),
                            size * (z + 0.5 - depth / 2.));

                    vec3 pos4 = VEC3(
                            size * (x + 0.5 - width / 2.), 
                            size * (y + 0.5 - height / 2.), 
                            size * (z - 0.5 - depth / 2.));
                    vec3 pos5 = VEC3(
                            size * (x - 0.5 - width / 2.),
                            size * (y + 0.5 - height / 2.),
                            size * (z - 0.5 - depth / 2.));
                    vec3 pos6 = VEC3(
                            size * (x + 0.5 - width / 2.),
                            size * (y - 0.5 - height / 2.),
                            size * (z - 0.5 - depth / 2.));
                    vec3 pos7 = VEC3(
                            size * (x - 0.5 - width / 2.),
                            size * (y - 0.5 - height / 2.),
                            size * (z - 0.5 - depth / 2.));

                    mesh_push_quad(mesh, pos6, pos7, pos2, pos3);
                    mesh_push_quad(mesh, pos4, pos5, pos0, pos1);
                    mesh_push_quad(mesh, pos4, pos0, pos6, pos2);
                    mesh_push_quad(mesh, pos1, pos5, pos3, pos7);
                    mesh_push_quad(mesh, pos0, pos1, pos2, pos3);
                    mesh_push_quad(mesh, pos5, pos4, pos7, pos6);
                }
            }
        }
    }
}

i32
game_init(struct game_state *game)
{
    world_init(&game->world);
    camera_init(&game->camera, VEC3(0, 0, -5), 1.f, 45.f);

    gl.Enable(GL_DEPTH_TEST);

    u32 program = gl_program_create(vert_shader_source, frag_shader_source);
    game->shader.camera_position = 
        gl.GetUniformLocation(program, "camera_position");
    game->shader.view =
        gl.GetUniformLocation(program, "view");
    game->shader.projection =
        gl.GetUniformLocation(program, "projection");
    game->shader.program = program;

    if (program == 0) {
        u8 error[1024];
        gl_program_error(program, error, sizeof(error));
        fprintf(stderr, "Failed to create program: %s\n", error);
        return -1;
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

    u32 size = WORLD_WIDTH * WORLD_HEIGHT * WORLD_DEPTH;
    struct mesh mesh = {0};
    mesh.vertices = calloc(size * 64, sizeof(struct vertex));
    mesh.indices = calloc(size * 36, sizeof(u32));
    world_generate_mesh(&world, &mesh);
    game->world.index_count = mesh.index_count;
    game->world.vertex_count = mesh.vertex_count;

    u32 vao, vbo, ebo;
    gl.GenBuffers(1, &vbo);
    gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.BufferData(GL_ARRAY_BUFFER, mesh.vertex_count * sizeof(*mesh.vertices), 
            mesh.vertices, GL_STATIC_DRAW);

    gl.GenBuffers(1, &ebo);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.index_count * 
            sizeof(*mesh.indices), mesh.indices, GL_STATIC_DRAW);

    free(mesh.vertices);
    free(mesh.indices);

    gl.GenVertexArrays(1, &vao);
    gl.BindVertexArray(vao);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), 
            (const void *)offsetof(struct vertex, position));
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), 
            (const void *)offsetof(struct vertex, texcoord));
    gl.EnableVertexAttribArray(1);

    game->world.vao = vao;
    game->world.vbo = vbo;
    game->world.ebo = ebo;

    u32 texture;
    i32 width, height, comp;
    u8 *data;

    if (!(data = stbi_load("texture.jpg", &width, &height, &comp, 0))) {
        fprintf(stderr, "Failed to load texture\n");
        return -1;
    }

    gl.GenTextures(1, &texture);
    gl.BindTexture(GL_TEXTURE_2D, texture);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    gl.GenerateMipmap(GL_TEXTURE_2D);
    game->world.texture = texture;

    return 0;
}

i32
game_update(struct game_state *game, struct game_input *input)
{
    f32 dt = input->dt;

    gl.ClearColor(0.15, 0.15, 0.25, 1.0);
    gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera_resize(&game->camera, input->width, input->height);

    mat4 projection = game->camera.projection;
    mat4 view = game->camera.view;

    gl.UseProgram(game->shader.program);

    /* update the camera */
    f32 haxis = input->controller.move_right - input->controller.move_left;
    f32 vaxis = input->controller.move_up - input->controller.move_down;
    f32 speed = game->camera.speed;
    if (haxis || vaxis) {
        vec3 direction = vec3_mulf(vec3_norm(vec3_add(
                vec3_mulf(game->camera.front, vaxis),
                vec3_mulf(game->camera.right, haxis))), dt * speed);
        game->camera.position = vec3_add(game->camera.position, direction);
    }

    camera_rotate(&game->camera, input->mouse.dx, input->mouse.dy);

    gl.UniformMatrix4fv(game->shader.projection, 1, GL_FALSE, projection.e);
    gl.UniformMatrix4fv(game->shader.view, 1, GL_FALSE, view.e);

    gl.DrawElements(GL_TRIANGLES, game->world.index_count, GL_UNSIGNED_INT, 0);

    return 0;
}

void
game_finish(struct game_state *game)
{
    gl.DeleteTextures(1, &game->world.texture);
    gl.DeleteVertexArrays(1, &game->world.vao);
    gl.DeleteBuffers(1, &game->world.vbo);
    gl.DeleteBuffers(1, &game->world.ebo);
    gl.DeleteProgram(game->shader.program);
}
