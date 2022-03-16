#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "gl.h"
#include "game.h"
#include "noise.h"
#include "world.h"
#include "timer.h"

static const u8 *vert_shader_source = (u8 *)
    "#version 330 core\n"
    "layout (location = 0) in v3 pos;"
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
    v3 min;
    v3 max;
};

i32
box_contains_point(struct box box, v3 point)
{
    return (box.min.x <= point.x && point.x <= box.max.x &&
            box.min.y <= point.y && point.y <= box.max.y &&
            box.min.z <= point.z && point.z <= box.max.z);
}

v3
player_direction_from_input(struct game_input *input, v3 front, v3 right, f32 speed)
{
    v3 direction = VEC3(0, 0, 0);
    f32 haxis = input->controller.move_right - input->controller.move_left;
    f32 vaxis = input->controller.move_up - input->controller.move_down;

    if (haxis || vaxis) {
        front = v3_mulf(front, vaxis);
        right = v3_mulf(right, haxis);
        front.y = right.y = 0;

        direction = v3_mulf(v3_norm(v3_add(front, right)), speed);
    }

    return direction;
}

// NOTE: assumes ray starts outside the box and intersects the box
f32
box_ray_intersection_point(struct box box, v3 start, v3 direction,
                           v3 *normal_min, v3 *normal_max, 
                           f32 *out_tmin, f32 *out_tmax)
{
    f32 tmin = 0.f;
    f32 tmax = INFINITY;
    v3 t0 = {0};
    v3 t1 = {0};

    for (u32 i = 0; i < 3; i++) {
        i32 sign = 0;
        if (direction.e[i] < 0) {
            t0.e[i] = (box.max.e[i] - start.e[i]) / direction.e[i];
            t1.e[i] = (box.min.e[i] - start.e[i]) / direction.e[i];
            sign = 1;
        } else if (direction.e[i] > 0) {
            t0.e[i] = (box.min.e[i] - start.e[i]) / direction.e[i];
            t1.e[i] = (box.max.e[i] - start.e[i]) / direction.e[i];
            sign = -1;
        }

        v3 normal = VEC3(sign * (i == 0), sign * (i == 1), sign * (i == 2));

        if (tmin < t0.e[i]) {
            tmin = t0.e[i];
            *normal_min = normal;
        }

        if (tmax > t1.e[i]) {
            tmax = t1.e[i];
            *normal_max = v3_neg(normal);
        }
    }

    *out_tmin = tmin;
    *out_tmax = tmax;
    return tmin < tmax;
}

void
player_move(struct game_state *game, struct game_input *input)
{
    struct player *player = &game->player;
    struct camera *camera = &game->camera;
    struct world *world = &game->world;

    f32 dt = input->dt;

    v3 velocity = player->velocity;
    v3 direction = player_direction_from_input(input, camera->front,
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

    v3 old_player_pos = player->position;
    v3 new_player_pos = v3_add(old_player_pos, velocity);

    v3 player_size = VEC3(0.25, 0.99f, 0.25f);
    v3 old_player_min = v3_sub(old_player_pos, player_size);
    v3 old_player_max = v3_add(old_player_pos, player_size);
    v3 new_player_min = v3_sub(new_player_pos, player_size);
    v3 new_player_max = v3_add(new_player_pos, player_size);

    i32 min_block_x = floorf(MIN(old_player_min.x, new_player_min.x) + 0.4);
    i32 min_block_y = floorf(MIN(old_player_min.y, new_player_min.y) + 0.4);
    i32 min_block_z = floorf(MIN(old_player_min.z, new_player_min.z) + 0.4);

    i32 max_block_x = floorf(MAX(old_player_max.x, new_player_max.x) + 1.5);
    i32 max_block_y = floorf(MAX(old_player_max.y, new_player_max.y) + 1.5);
    i32 max_block_z = floorf(MAX(old_player_max.z, new_player_max.z) + 1.5);

    assert(min_block_x <= max_block_x);
    assert(min_block_y <= max_block_y);
    assert(min_block_z <= max_block_z);

    v3 block_size = VEC3(0.5, 0.5, 0.5);
    v3 block_offset = v3_add(player_size, block_size);

    struct box block_bounds;
    block_bounds.min = v3_mulf(block_offset, -1.f);
    block_bounds.max = v3_mulf(block_offset,  1.f);

    f32 t_remaining = 1.f;
    for (u32 i = 0; i < 4 && t_remaining > 0.f; i++) {
        v3 normal = VEC3(0, 0, 0);
        f32 t_min = 1.f;

        for (i32 z = min_block_z; z <= max_block_z; z++) {
            for (i32 y = min_block_y; y <= max_block_y; y++) {
                for (i32 x = min_block_x; x <= max_block_x; x++) {
                    v3 block = VEC3(x, y, z);
                    v3 relative_old_pos = 
                        v3_sub(old_player_pos, block);
                    v3 relative_new_pos = 
                        v3_add(relative_old_pos, velocity);
                    if (!block_is_empty(world_at(world, x, y, z)) &&
                        box_contains_point(block_bounds, relative_new_pos)) 
                    {

                        v3 normal_min = {0};
                        v3 normal_max;
                        f32 t, tmax;
                        box_ray_intersection_point(
                            block_bounds, relative_old_pos, 
                            velocity, &normal_min, &normal_max, &t, &tmax);
                        t = MAX(t - 0.01f, 0);
                        if (t_min > t) {
                            t_min = t;
                            normal = normal_min;
                        }
                    }
                }
            }
        }

        v3 new_player_pos = v3_add(old_player_pos, 
                                       v3_mulf(velocity, t_min));
        old_player_pos = new_player_pos;
        velocity = v3_sub(velocity, 
                            v3_mulf(normal, v3_dot(normal, velocity)));
        t_remaining -= t_min * t_remaining;
    }

    if (velocity.y == 0) {
        player->is_jumping = 0;
        player->frames_since_jump = 0;
    }

    player->velocity = velocity;
    player->position = old_player_pos;
    camera->position = v3_add(player->position, VEC3(0, 0.75, 0)); 
    camera_resize(&game->camera, input->width, input->height);
    camera_rotate(&game->camera, input->mouse.dx, input->mouse.dy);
}

static void
player_update(struct player *player, struct camera *camera,
              struct world *world, struct game_input *input)
{
    static enum block_type blocks_in_inventory[8] = {
        BLOCK_STONE,
        BLOCK_DIRT,
        BLOCK_GRASS,
        BLOCK_SAND,
        BLOCK_PLANKS,
        BLOCK_OAK_LOG,
        BLOCK_OAK_LEAVES,
        BLOCK_WATER,
    };


    v3 block = v3_round(camera->position);
    v3 block_size = {{ 0.5, 0.5, 0.5 }};
    struct box selected_block;
    selected_block.min = v3_sub(block, block_size);
    selected_block.max = v3_add(block, block_size);

    v3 start = camera->position;
    v3 direction = camera->front;

    i32 has_selected_block = 0;
    v3 normal_min = {0};
    v3 normal_max = {0};
    for (u32 i = 0; i < 10; i++) {
        f32 tmin = 0.f;
        f32 tmax = INFINITY;

        box_ray_intersection_point(selected_block, start, direction,
                                   &normal_min, &normal_max, &tmin, &tmax);
        if (tmin > 5.f) {
            break;
        }

        if (block_is_empty(world_at(world, block.x, block.y, block.z))) {
            block = v3_add(block, normal_max);
            selected_block.min = v3_add(selected_block.min, normal_max);
            selected_block.max = v3_add(selected_block.max, normal_max);
        } else {
            has_selected_block = 1;
            debug_set_color(0, 0, 0);
            debug_cube(selected_block.min, selected_block.max);
            break;
        }
    }

    if (input->mouse.buttons[5]) {
        player->selected_block++;
        player->selected_block %= LENGTH(blocks_in_inventory);
    }
    
    if (input->mouse.buttons[4] && player->selected_block > 0) {
        if (player->selected_block == 0) {
            player->selected_block = LENGTH(blocks_in_inventory);
        } else {
            player->selected_block--;
        }
    }

    if (has_selected_block) {
        if (input->mouse.buttons[1]) {
            world_destroy_block(world, block.x, block.y, block.z);
        } 

        block = v3_add(block, normal_min);
        v3 player_size = VEC3(0.25, 0.99f, 0.25f);
        v3 player_pos = player->position;
        struct box block_bounds;
        v3 bounds_size = v3_add(block_size, player_size);
        block_bounds.min = v3_sub(block, bounds_size);
        block_bounds.max = v3_add(block, bounds_size);
        u32 can_place_block = !box_contains_point(block_bounds, player_pos);
        if (can_place_block && input->mouse.buttons[3]) {
            u32 selected_block = blocks_in_inventory[player->selected_block];
            world_place_block(world, block.x, block.y, block.z,
                              selected_block);
        }
    }
}

i32
game_update(struct game_state *game, struct game_input *input)
{
    mat4 projection = game->camera.projection;
    mat4 view = game->camera.view;
    mat4 model = mat4_id(1);

    player_move(game, input);
    player_update(&game->player, &game->camera, &game->world, input);
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
