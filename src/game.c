#include <stdio.h>
#include <stdlib.h>

#include "gl.h"
#include "game.h"
#include "noise.h"
#include "world.h"

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

i32
game_init(struct game_state *game)
{
    struct memory_arena *arena = &game->arena;

    u32 size = WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE;
    game->world.chunks = arena_alloc(arena, size, struct chunk);
    game->world.width  = WORLD_SIZE;
    game->world.height = WORLD_HEIGHT;
    game->world.depth  = WORLD_SIZE;
    f32 offset = -WORLD_SIZE * CHUNK_SIZE / 2.f;
    f32 yoffset = -WORLD_HEIGHT * CHUNK_SIZE / 2.f;
    game->world.position = VEC3(offset, yoffset, offset);
    world_init(&game->world, arena);
    camera_init(&game->camera, VEC3(0, 0, 0), 4.f, 45.f);

    gl.Enable(GL_DEPTH_TEST);
    gl.Enable(GL_CULL_FACE);
    gl.Enable(GL_BLEND);
    gl.CullFace(GL_FRONT);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    u32 program = gl_program_create(vert_shader_source, frag_shader_source);
    game->shader.camera_position = 
        gl.GetUniformLocation(program, "camera_position");
    game->shader.view = gl.GetUniformLocation(program, "view");
    game->shader.projection = gl.GetUniformLocation(program, "projection");
    game->shader.program = program;

    if (program == 0) {
        u8 error[1024];
        gl_program_error(program, error, sizeof(error));
        fprintf(stderr, "Failed to create program: %s\n", error);
        return -1;
    }

    return 0;
}

void
player_update(struct game_state *game, struct game_input *input)
{
    f32 dt = input->dt;

    vec3 position = game->camera.position;

    f32 haxis = input->controller.move_right - input->controller.move_left;
    f32 vaxis = input->controller.move_up - input->controller.move_down;
    f32 speed = game->camera.speed;
    if (haxis || vaxis) {
        vec3 front = game->camera.front;
        vec3 right = game->camera.right;
        vec3 direction = vec3_mulf(vec3_norm(vec3_add(
                vec3_mulf(front, vaxis),
                vec3_mulf(right, haxis))), dt * speed);
        position = vec3_add(game->camera.position, direction);
    }

    game->camera.position = position;
    camera_resize(&game->camera, input->width, input->height);
    camera_rotate(&game->camera, input->mouse.dx, input->mouse.dy);
}

i32
game_update(struct game_state *game, struct game_input *input)
{
    mat4 projection = game->camera.projection;
    mat4 view = game->camera.view;

    player_update(game, input);
    world_update(&game->world, game->camera.position);

    gl.ClearColor(0.45, 0.65, 0.85, 1.0);
    gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl.UseProgram(game->shader.program);
    gl.UniformMatrix4fv(game->shader.projection, 1, GL_FALSE, projection.e);
    gl.UniformMatrix4fv(game->shader.view, 1, GL_FALSE, view.e);
    world_render(&game->world);

    return 0;
}

void
game_finish(struct game_state *game)
{
    world_finish(&game->world);
    arena_finish(&game->arena);
    gl.DeleteProgram(game->shader.program);
}
