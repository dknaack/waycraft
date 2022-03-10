#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "gl.h"
#include "game.h"
#include "noise.h"
#include "world.h"

static const u8 *vert_shader_source = (u8 *)
    "#version 330 core\n"
    "layout (location = 0) in vec3 pos;"
    "layout (location = 1) in vec2 in_coords;"
    "out vec2 coords;"
    "uniform mat4 model;"
    "uniform mat4 view;"
    "uniform mat4 projection;"
    "void main() {"
        "gl_Position = projection * view * model * vec4(pos, 1.);"
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
    debug_init();

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
    camera_init(&game->camera, VEC3(0, 20, 0), 10.f, 65.f);
    game->player.position = VEC3(0, 20, 0);

    gl.Enable(GL_DEPTH_TEST);
    gl.Enable(GL_CULL_FACE);
    gl.Enable(GL_BLEND);
    gl.CullFace(GL_FRONT);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    u32 program = gl_program_create(vert_shader_source, frag_shader_source);
    if (program == 0) {
        u8 error[1024];
        gl_program_error(program, error, sizeof(error));
        fprintf(stderr, "Failed to create program: %s\n", error);
        return -1;
    }

    game->shader.model = gl.GetUniformLocation(program, "model");
    game->shader.view = gl.GetUniformLocation(program, "view");
    game->shader.projection = gl.GetUniformLocation(program, "projection");
    game->shader.program = program;

    return 0;
}

struct aabb {
    vec3 min;
    vec3 max;
};

i32
aabb_intersects_aabb(struct aabb a, struct aabb b)
{
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

vec3
aabb_distance_to_aabb(struct aabb a, struct aabb b)
{
    vec3 result = {0};

    if (a.min.x < b.min.x) {
        result.x = b.min.x - a.max.x;
    } else if (a.min.x > b.min.x) {
        result.x = a.min.x - b.max.x;
    }

    if (a.min.y < b.min.y) {
        result.y = b.min.y - a.max.y;
    } else if (a.min.y > b.min.y) {
        result.y = a.min.y - b.max.y;
    }

    if (a.min.z < b.min.z) {
        result.z = b.min.z - a.max.z;
    } else if (a.min.z > b.min.z) {
        result.z = a.min.z - b.max.z;
    }

    return result;
}

vec3
player_direction_from_input(struct game_input *input, vec3 front, vec3 right, f32 speed)
{
    vec3 direction = VEC3(0, 0, 0);
    f32 haxis = input->controller.move_right - input->controller.move_left;
    f32 vaxis = input->controller.move_up - input->controller.move_down;

    if (haxis || vaxis) {
        front = vec3_mulf(front, vaxis);
        right = vec3_mulf(right, haxis);
        front.y = right.y = 0;

        direction = vec3_mulf(vec3_norm(vec3_add(front, right)), speed);
    }

    return direction;
}

void
player_move(struct game_state *game, struct game_input *input)
{
    struct player *player = &game->player;
    struct camera *camera = &game->camera;

    f32 dt = input->dt;

    vec3 position = player->position;
    vec3 velocity = player->velocity;
    vec3 direction = player_direction_from_input(
            input, camera->front, camera->right, camera->speed);
    velocity.x = direction.x * dt;
    velocity.z = direction.z * dt;

    if (input->controller.jump && game->player.frames_since_jump < 5) {
        velocity.y += 3.2f * dt;
        player->frames_since_jump++;
        player->is_jumping = 1;
    }

    vec3 new_position = vec3_add(position, velocity);

    player->velocity = velocity;
    player->position = new_position;
    camera->position = vec3_add(new_position, VEC3(0, 1.75, 0)); 
    camera_resize(&game->camera, input->width, input->height);
    camera_rotate(&game->camera, input->mouse.dx, input->mouse.dy);
}

i32
game_update(struct game_state *game, struct game_input *input)
{
    mat4 projection = game->camera.projection;
    mat4 view = game->camera.view;
    mat4 model = mat4_id(1);

    player_move(game, input);
    world_update(&game->world, game->camera.position);

    gl.ClearColor(0.45, 0.65, 0.85, 1.0);
    gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl.UseProgram(game->shader.program);
    gl.UniformMatrix4fv(game->shader.model, 1, GL_FALSE, model.e);
    gl.UniformMatrix4fv(game->shader.projection, 1, GL_FALSE, projection.e);
    gl.UniformMatrix4fv(game->shader.view, 1, GL_FALSE, view.e);
    world_render(&game->world);
    debug_render(view, projection);

    return 0;
}

void
game_finish(struct game_state *game)
{
    gl.DeleteProgram(game->shader.program);
    world_finish(&game->world);
    arena_finish(&game->arena);
}
