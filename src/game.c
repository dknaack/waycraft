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
#define WORLD_DEPTH 8
#define WORLD_HEIGHT 1
#define WORLD_WIDTH  8
#define WORLD_SIZE WORLD_WIDTH * WORLD_HEIGHT * WORLD_DEPTH
    static struct chunk chunks[WORLD_SIZE] = {0};
    game->world.chunks = chunks;
    game->world.width  = WORLD_WIDTH;
    game->world.height = WORLD_HEIGHT;
    game->world.depth  = WORLD_DEPTH;
    world_init(&game->world);
    camera_init(&game->camera, VEC3(0, 0, 0), 1.f, 45.f);

    gl.Enable(GL_DEPTH_TEST);
    gl.Enable(GL_CULL_FACE);
    gl.CullFace(GL_FRONT);

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

    return 0;
}

i32
game_update(struct game_state *game, struct game_input *input)
{
    f32 dt = input->dt;

    mat4 projection = game->camera.projection;
    mat4 view = game->camera.view;

    f32 haxis = input->controller.move_right - input->controller.move_left;
    f32 vaxis = input->controller.move_up - input->controller.move_down;
    f32 speed = game->camera.speed;
    if (haxis || vaxis) {
        vec3 direction = vec3_mulf(vec3_norm(vec3_add(
                vec3_mulf(game->camera.front, vaxis),
                vec3_mulf(game->camera.right, haxis))), dt * speed);
        game->camera.position = vec3_add(game->camera.position, direction);
    }

    camera_resize(&game->camera, input->width, input->height);
    camera_rotate(&game->camera, input->mouse.dx, input->mouse.dy);

    gl.ClearColor(0.15, 0.15, 0.25, 1.0);
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
    gl.DeleteProgram(game->shader.program);
}
