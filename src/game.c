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
    "    gl_Position = projection * view * model * vec4(pos, 1.);"
    "    coords = in_coords;"
    "}";

static const u8 *frag_shader_source = (u8 *)
    "#version 330 core\n"
    "in vec2 coords;"
    "out vec4 frag_color;"
    "uniform sampler2D tex;"
    "void main() {"
    "    frag_color = texture(tex, coords);"
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

struct box {
    vec3 min;
    vec3 max;
};

i32
box_contains_point(struct box box, vec3 point)
{
    return (box.min.x <= point.x && point.x <= box.max.x &&
            box.min.y <= point.y && point.y <= box.max.y &&
            box.min.z <= point.z && point.z <= box.max.z);
}

vec3
box_distance_to_box(struct box a, struct box b)
{
    vec3 result = {{ INFINITY, INFINITY, INFINITY }};

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

// NOTE: assumes ray starts outside the box and intersects the box
f32
box_ray_intersection_point(struct box box, vec3 start, vec3 direction,
                            vec3 *normal)
{
    f32 tx = -0.1f;
    f32 ty = -0.1f;
    f32 tz = -0.1f; 
    f32 tmin = 0.f;

    if (direction.x < 0) {
        tx = (box.max.x - start.x) / direction.x;
    } else if (direction.x > 0) {
        tx = (box.min.x - start.x) / direction.x;
    }

    if (tmin < tx) {
        tmin = tx;
        *normal = VEC3(SIGN(direction.x), 0, 0);
    }

    if (direction.y < 0) {
        ty = (box.max.y - start.y) / direction.y;
    } else if (direction.y > 0) {
        ty = (box.min.y - start.y) / direction.y;
    }

    if (tmin < ty) {
        tmin = ty;
        *normal = VEC3(0, SIGN(direction.y), 0);
    }

    if (direction.z < 0) {
        tz = (box.max.z - start.z) / direction.z;
    } else if (direction.z > 0) {
        tz = (box.min.z - start.z) / direction.z;
    }

    if (tmin < tz) {
        tmin = tz;
        *normal = VEC3(0, 0, SIGN(direction.z));
    }

    return MAX(tmin - 0.01f, 0.f);
}

void
player_move(struct game_state *game, struct game_input *input)
{
    struct player *player = &game->player;
    struct camera *camera = &game->camera;
    struct world *world = &game->world;

    f32 dt = input->dt;

    vec3 velocity = player->velocity;
    vec3 direction = player_direction_from_input(input, camera->front,
                                                 camera->right, camera->speed);
    velocity.x = direction.x * dt;
    velocity.z = direction.z * dt;

    if (input->controller.jump && game->player.frames_since_jump < 5) {
        velocity.y += 3.9f * dt;
        player->frames_since_jump++;
        player->is_jumping = 1;
    }


    if (velocity.y < 10) {
        velocity.y -= 0.9f * dt;
    }

    vec3 old_player_pos = player->position;
    vec3 new_player_pos = vec3_add(old_player_pos, velocity);

    vec3 player_size = VEC3(0.25, 1.f, 0.25f);
    vec3 old_player_min = vec3_sub(old_player_pos, player_size);
    vec3 old_player_max = vec3_add(old_player_pos, player_size);
    vec3 new_player_min = vec3_sub(new_player_pos, player_size);
    vec3 new_player_max = vec3_add(new_player_pos, player_size);

    i32 min_block_x = floorf(MIN(old_player_min.x, new_player_min.x) + 0.4);
    i32 min_block_y = floorf(MIN(old_player_min.y, new_player_min.y) + 0.4);
    i32 min_block_z = floorf(MIN(old_player_min.z, new_player_min.z) + 0.4);

    i32 max_block_x = floorf(MAX(old_player_max.x, new_player_max.x) + 1.5);
    i32 max_block_y = floorf(MAX(old_player_max.y, new_player_max.y) + 1.5);
    i32 max_block_z = floorf(MAX(old_player_max.z, new_player_max.z) + 1.5);

    assert(min_block_x <= max_block_x);
    assert(min_block_y <= max_block_y);
    assert(min_block_z <= max_block_z);

    vec3 block_size = VEC3(0.5, 0.5, 0.5);
    vec3 block_offset = vec3_add(player_size, block_size);

    struct box block_bounds;
    block_bounds.min = vec3_mulf(block_offset, -1.f);
    block_bounds.max = vec3_mulf(block_offset,  1.f);

    f32 t_remaining = 1.f;
    for (u32 i = 0; i < 4 && t_remaining > 0.f; i++) {
        vec3 normal = VEC3(0, 0, 0);
        f32 t_min = 1.f;

        for (i32 z = min_block_z; z <= max_block_z; z++) {
            for (i32 y = min_block_y; y <= max_block_y; y++) {
                for (i32 x = min_block_x; x <= max_block_x; x++) {
                    vec3 block = VEC3(x, y, z);
                    vec3 relative_old_pos = 
                        vec3_sub(old_player_pos, block);
                    vec3 relative_new_pos = 
                        vec3_add(relative_old_pos, velocity);
                    if (world_at(world, x, y, z) != 0 &&
                        box_contains_point(block_bounds, relative_new_pos)) 
                    {

                        vec3 current_normal = VEC3(0, 0, 0);
                        f32 t = box_ray_intersection_point(
                            block_bounds, relative_old_pos, 
                            velocity, &current_normal);
                        if (t_min > t) {
                            t_min = t;
                            normal = current_normal;
                        }
                    }
                }
            }
        }

        vec3 new_player_pos = vec3_add(old_player_pos, 
                                       vec3_mulf(velocity, t_min));
        old_player_pos = new_player_pos;
        velocity = vec3_sub(velocity, 
                            vec3_mulf(normal, vec3_dot(normal, velocity)));
        t_remaining -= t_min * t_remaining;
    }

    if (velocity.y == 0) {
        player->is_jumping = 0;
        player->frames_since_jump = 0;
    }

    player->velocity = velocity;
    player->position = old_player_pos;
    camera->position = vec3_add(player->position, VEC3(0, 0.75, 0)); 
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
