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

void
player_init(struct player *player, struct camera *camera)
{
    f32 player_speed = 10.f;
    f32 camera_fov = 65.f;
    v3 player_position = V3(0, 20, 0);

    camera_init(camera, player_position, player_speed, camera_fov);
    player->position = player_position;

    player->hotbar[0] = BLOCK_STONE;
    player->hotbar[1] = BLOCK_DIRT;
    player->hotbar[2] = BLOCK_GRASS;
    player->hotbar[3] = BLOCK_SAND;
    player->hotbar[4] = BLOCK_PLANKS;
    player->hotbar[5] = BLOCK_OAK_LOG;
    player->hotbar[6] = BLOCK_OAK_LEAVES;
    player->hotbar[7] = BLOCK_WATER;
    player->hotbar[8] = BLOCK_AIR;
}

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
    game->world.position = V3(offset, yoffset, offset);
    world_init(&game->world, arena);

    player_init(&game->player, &game->camera);

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

    game->window_count = 0;
    {
        static const struct vertex vertices[] = {
            { {{  1.,  1., 0.0f }}, {{ 1.0, 0.0 }} },
            { {{  1., -1., 0.0f }}, {{ 1.0, 1.0 }} },
            { {{ -1., -1., 0.0f }}, {{ 0.0, 1.0 }} },
            { {{ -1.,  1., 0.0f }}, {{ 0.0, 0.0 }} },
        };

        static const u32 indices[] = { 0, 1, 3, 1, 2, 3, };

        gl.GenVertexArrays(1, &game->window_vertex_array);
        gl.BindVertexArray(game->window_vertex_array);

        gl.GenBuffers(1, &game->window_vertex_buffer);
        gl.BindBuffer(GL_ARRAY_BUFFER, game->window_vertex_buffer);
        gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
                      GL_STATIC_DRAW);

        gl.GenBuffers(1, &game->window_index_buffer);
        gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, game->window_index_buffer);
        gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                      GL_STATIC_DRAW);

        gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
                               (const void *)offsetof(struct vertex, position));
        gl.EnableVertexAttribArray(0);
        gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
                               (const void *)offsetof(struct vertex, texcoord));
        gl.EnableVertexAttribArray(1);

        gl.BindVertexArray(0);
    }

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
    v3 direction = V3(0, 0, 0);
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

        v3 normal = V3(sign * (i == 0), sign * (i == 1), sign * (i == 2));

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

    v3 player_size = V3(0.25, 0.99f, 0.25f);
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

    v3 block_size = V3(0.5, 0.5, 0.5);
    v3 block_offset = v3_add(player_size, block_size);

    struct box block_bounds;
    block_bounds.min = v3_mulf(block_offset, -1.f);
    block_bounds.max = v3_mulf(block_offset,  1.f);

    f32 t_remaining = 1.f;
    for (u32 i = 0; i < 4 && t_remaining > 0.f; i++) {
        v3 normal = V3(0, 0, 0);
        f32 t_min = 1.f;

        for (i32 z = min_block_z; z <= max_block_z; z++) {
            for (i32 y = min_block_y; y <= max_block_y; y++) {
                for (i32 x = min_block_x; x <= max_block_x; x++) {
                    v3 block = V3(x, y, z);
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
    camera->position = v3_add(player->position, V3(0, 0.75, 0)); 
    camera_resize(&game->camera, input->width, input->height);
    camera_rotate(&game->camera, input->mouse.dx, input->mouse.dy);
}

static u32
player_select_block(struct game_state *game, struct game_input *input,
                    v3 *out_block_pos, v3 *out_normal_min)
{
    struct player *player = &game->player;
    struct world *world   = &game->world;
    struct camera *camera = &game->camera;

    v3 block_pos = v3_round(camera->position);
    v3 block_size = {{ 0.5, 0.5, 0.5 }};
    struct box selected_block;
    selected_block.min = v3_sub(block_pos, block_size);
    selected_block.max = v3_add(block_pos, block_size);

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

        if (block_is_empty(world_at(world, block_pos.x, block_pos.y, block_pos.z))) {
            block_pos = v3_add(block_pos, normal_max);
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
        player->hotbar_selection++;
        player->hotbar_selection %= LENGTH(player->hotbar);
    }
    
    if (input->mouse.buttons[4] && player->hotbar_selection > 0) {
        if (player->hotbar_selection == 0) {
            player->hotbar_selection = LENGTH(player->hotbar);
        } else {
            player->hotbar_selection--;
        }
    }

    *out_block_pos = block_pos;
    *out_normal_min = normal_min;
    return has_selected_block;
}

static void
window_move(struct window *window, v3 position, v3 normal, v3 up)
{
    v3 window_pos = v3_add(position, v3_mulf(normal, 0.6f));
    v3 window_forward = normal;
    v3 window_right = v3_cross(up, window_forward);
    v3 window_up = v3_cross(window_forward, window_right);
    f32 window_aspect_ratio = window->width / window->height;

    m4x4 window_transform = m4x4_mul(
        m4x4_to_coords(window_pos, window_right, window_up,
                       window_forward),
        m4x4_scale(window_aspect_ratio, 1, 1));

    window->transform = window_transform;
}

static void
window_render(struct window *window, u32 model_uniform)
{
    m4x4 window_transform = window->transform;
    u32 window_texture = window->texture;

    gl.UniformMatrix4fv(model_uniform, 1, GL_FALSE, window_transform.e);
    gl.BindTexture(GL_TEXTURE_2D, window_texture);
    gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

i32
game_update(struct game_state *game, struct game_input *input)
{
    struct world *world = &game->world;
    struct camera *camera = &game->camera;

    m4x4 projection = game->camera.projection;
    m4x4 view = game->camera.view;
    m4x4 model = m4x4_id(1);

    gl.ClearColor(0.45, 0.65, 0.85, 1.0);
    gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl.UseProgram(game->shader.program);
    gl.UniformMatrix4fv(game->shader.model, 1, GL_FALSE, model.e);
    gl.UniformMatrix4fv(game->shader.projection, 1, GL_FALSE, projection.e);
    gl.UniformMatrix4fv(game->shader.view, 1, GL_FALSE, view.e);

    player_move(game, input);
    v3 block_pos = {0};
    v3 block_normal = {0};
    u32 has_selected_block = 
        player_select_block(game, input, &block_pos, &block_normal);
    if (has_selected_block) {
        if (input->mouse.buttons[1]) {
            world_destroy_block(world, block_pos.x, block_pos.y, block_pos.z);
        } 

#if 0
        block_pos = v3_add(block_pos, block_normal);
        v3 player_size = V3(0.25, 0.99f, 0.25f);
        v3 player_pos = player->position;
        struct box block_bounds;
        v3 bounds_size = v3_add(block_size, player_size);
        block_bounds.min = v3_sub(block_pos, bounds_size);
        block_bounds.max = v3_add(block_pos, bounds_size);
        u32 can_place_block = !box_contains_point(block_bounds, player_pos);
        if (can_place_block && input->mouse.buttons[3]) {
            u32 selected_block = player->hotbar[player->hotbar_selection];
            world_place_block(world, block_pos.x, block_pos.y, block_pos.z,
                              selected_block);
        }
#endif
    }

    world_update(&game->world, game->camera.position);
    world_render(&game->world);

    if (has_selected_block) {
        u32 active_window = game->active_window;
        u32 window_count = game->window_count;
        if (input->mouse.buttons[3]) {
            active_window = (active_window + 1) % (window_count + 1);
            game->active_window = active_window;
        }

        struct window *window = game->windows + (active_window - 1);
        if (active_window && window->height && window->width) {
            v3 up = V3(0, 1, 0);
            if (block_normal.y > 0.5) {
                if (fabsf(camera->front.x) < fabsf(camera->front.z)) {
                    up = V3(0, 0, SIGN(camera->front.z));
                } else {
                    up = V3(SIGN(camera->front.x), 0, 0);
                }
            }

            window_move(window, block_pos, block_normal, up);
        }
    }

    u32 window_count = game->window_count;
    struct window *window = game->windows;
    gl.BindVertexArray(game->window_vertex_array);
    while (window_count-- > 0) {
        window_render(window++, game->shader.model);
    }

    debug_render(view, projection);
    return 0;
}

void
game_finish(struct game_state *game)
{
    gl.DeleteVertexArrays(1, &game->window_vertex_array);
    gl.DeleteBuffers(1, &game->window_vertex_buffer);
    gl.DeleteBuffers(1, &game->window_index_buffer);

    gl.DeleteProgram(game->shader.program);
    world_finish(&game->world);
    arena_finish(&game->arena);
}
