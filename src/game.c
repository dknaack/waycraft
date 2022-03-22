#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "gl.h"
#include "game.h"
#include "noise.h"
#include "world.h"
#include "timer.h"
#include "backend.h"

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
camera_init(struct camera *c, v3 position, f32 speed, f32 fov)
{
    c->view = c->projection = m4x4_id(1.f);
    c->position = position;
    c->front = V3(0, 0, 1);
    c->up = V3(0, 1, 0);
    c->right = V3(1, 0, 0);
    c->yaw = c->pitch = 0.f;
    c->speed = speed;
    c->fov = fov;
}

void
camera_resize(struct camera *c, f32 width, f32 height)
{
    c->projection = m4x4_perspective(c->fov, width / height, 0.1f, 1000.f);
}

void
camera_rotate(struct camera *c, f32 dx, f32 dy)
{
    const f32 sensitivity = 0.1f;

    c->yaw   += dx * sensitivity;
    c->pitch -= dy * sensitivity;

    c->yaw   = fmodf(c->yaw + 360.f, 360.f);
    c->pitch = CLAMP(c->pitch, -89.f, 89.f);

	c->front.x = cosf(DEG2RAD(c->yaw)) * cosf(DEG2RAD(c->pitch));
	c->front.y = sinf(DEG2RAD(c->pitch));
	c->front.z = sinf(DEG2RAD(c->yaw)) * cosf(DEG2RAD(c->pitch));

	c->front = v3_norm(c->front);
    c->right = v3_norm(v3_cross(c->front, V3(0, 1, 0)));
    c->up    = v3_norm(v3_cross(c->right, c->front));
	c->view  = m4x4_look_at(c->position, v3_add(c->position, c->front), c->up);
}

void
player_init(struct player *player, struct camera *camera)
{
    f32 player_speed = 150.f;
    f32 camera_fov = 65.f;
    v3 player_position = V3(0, 20, 0);

    camera_init(camera, player_position, player_speed, camera_fov);
    player->position = player_position;

    u8 *hotbar = player->hotbar;
    *hotbar++ = BLOCK_WINDOW;
    *hotbar++ = BLOCK_STONE;
    *hotbar++ = BLOCK_DIRT;
    *hotbar++ = BLOCK_GRASS;
    *hotbar++ = BLOCK_SAND;
    *hotbar++ = BLOCK_PLANKS;
    *hotbar++ = BLOCK_OAK_LOG;
    *hotbar++ = BLOCK_OAK_LEAVES;
    *hotbar++ = BLOCK_WATER;
}

void
arena_init(struct memory_arena *arena, void *data, u64 size)
{
    arena->data = data;
    arena->size = size;
    arena->used = 0;
}

#define arena_alloc(arena, count, type) ((type *)arena_alloc_(arena, count * sizeof(type)))

void *
arena_alloc_(struct memory_arena *arena, u64 size)
{
    u64 used = arena->used;
    assert(used + size < arena->size);

    void *ptr = arena->data + used;
    arena->used += size;

    return ptr;
}

void
game_init(struct backend_memory *memory)
{
    struct game_state *game = memory->data;
    struct memory_arena *arena = &game->arena;
    arena_init(arena, game + 1, memory->size - sizeof(struct game_state));

    debug_init(arena);
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
        return;
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
u32
ray_box_intersection(struct box box, v3 start, v3 direction,
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

    v3 front = camera->front;
    v3 right = camera->right;
    f32 speed = camera->speed;
    v3 position = player->position;
    v3 velocity = player->velocity;
    v3 acceleration = player_direction_from_input(input, front, right, speed);
    acceleration = v3_sub(acceleration, v3_mulf(velocity, 15.f));
    acceleration.y = -100.f;

    if (input->controller.jump && player->frames_since_jump < 5) {
        acceleration.y = 300.f;
        player->frames_since_jump++;
        player->is_jumping = 1;
    }

    v3 position_delta = v3_add(
        v3_mulf(velocity, dt),
        v3_mulf(acceleration, dt * dt * 0.5f)
    );

    velocity = v3_add(velocity, v3_mulf(acceleration, dt));

    v3 old_player_pos = player->position;
    v3 new_player_pos = v3_add(old_player_pos, position_delta);

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
                    v3 relative_old_pos = v3_sub(position, block);

                    v3 new_position = v3_add(relative_old_pos, position_delta);
                    if (!block_is_empty(world_at(world, x, y, z)) &&
                        box_contains_point(block_bounds, new_position)) 
                    {
                        v3 normal_min = {0};
                        v3 normal_max;
                        f32 t, tmax;

                        ray_box_intersection(
                            block_bounds, 
                            relative_old_pos, 
                            position_delta, 
                            &normal_min, &normal_max, &t, &tmax);

                        t = MAX(t - 0.01f, 0);
                        if (t_min > t) {
                            t_min = t;
                            normal = normal_min;
                        }
                    }
                }
            }
        }

        if (normal.y == 1) {
            player->is_jumping = 0;
            player->frames_since_jump = 0;
        }

        position = v3_add(position, v3_mulf(position_delta, t_min));
        velocity = v3_sub(velocity, v3_mulf(normal, v3_dot(normal, velocity)));
        position_delta = v3_sub(
            position_delta, 
            v3_mulf(normal, v3_dot(normal, position_delta))
        );

        t_remaining -= t_min * t_remaining;
    }

    player->position = position;
    player->velocity = velocity;

    camera->position = v3_add(player->position, V3(0, 0.75, 0)); 
    camera_resize(&game->camera, input->width, input->height);
    camera_rotate(&game->camera, input->mouse.dx, input->mouse.dy);
}

static u32
player_select_block(struct game_state *game, struct game_input *input,
                    v3 *out_block_pos, v3 *out_normal_min, f32 *out_t)
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
    f32 tmin, tmax;
    for (u32 i = 0; i < 10; i++) {
        tmin = 0.f;
        tmax = INFINITY;

        ray_box_intersection(selected_block, start, direction,
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
    *out_t = tmin;
    return has_selected_block;
}

static m4x4
window_transform(struct window *window)
{
    return m4x4_to_coords(window->position, 
                          window->x_axis, window->y_axis, window->z_axis);
}

static void
window_move(struct window *window, v3 window_pos, v3 normal, v3 up)
{
    window->position = window_pos;
    window->z_axis = normal;
    window->x_axis = v3_cross(up, window->z_axis);
    window->y_axis = v3_cross(window->z_axis, window->x_axis);
}

static void
window_render(struct window *window, u32 window_count, u32 model_uniform)
{
    while (window_count-- > 0) {
        m4x4 transform = window_transform(window);
        gl.UniformMatrix4fv(model_uniform, 1, GL_FALSE, transform.e);
        gl.BindTexture(GL_TEXTURE_2D, window->texture);
        gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        window++;
    }
}

#if 0
static enum block_type
block_from_direction(enum block_type base_block, v3 direction)
{
    enum block_type block = base_block;
    v3 abs_direction = v3_abs(direction);

    u32 is_facing_x_axis = (abs_direction.x > abs_direction.y &&
                            abs_direction.x > abs_direction.z);
    u32 is_facing_y_axis = (abs_direction.y > abs_direction.x &&
                            abs_direction.y > abs_direction.z);

    u32 is_facing_in_negative_direction = 0;
    if (is_facing_x_axis) {
        is_facing_in_negative_direction = direction.x < 0;
    } else if (is_facing_y_axis) {
        is_facing_in_negative_direction = direction.y < 0;
        block += 2;
    } else {
        is_facing_in_negative_direction = direction.z < 0;
        block += 4;
    }

    block += is_facing_in_negative_direction;
    return block;
}
#endif

static u32
window_ray_intersection_point(struct window *window, 
                              v3 ray_start, v3 ray_direction,
                              v2 *point_on_window)
{
    u32 hit = 0;
    v3 normal = window->z_axis;
    v3 window_pos = window->position;

    f32 d_dot_n = v3_dot(ray_direction, normal);
    if (fabsf(d_dot_n) > 0.0001f) {
        f32 p_dot_n = v3_dot(v3_sub(window_pos, ray_start), normal);
        f32 t = p_dot_n / d_dot_n;
        if (t > 0.f) {
            v3 point = v3_add(ray_start, v3_mulf(ray_direction, t));
            v3 relative_point = v3_sub(point, window_pos);

            f32 tx = v3_dot(window->x_axis, relative_point);
            f32 ty = v3_dot(window->y_axis, relative_point);
            u32 is_inside_window = ((-1.f <= tx && tx < 1.f) &&
                                    (-1.f <= ty && ty < 1.f));

            if (point_on_window) {
                point_on_window->x = tx;
                point_on_window->y = ty;
            }
            hit = is_inside_window;
        }
    }

    return hit;
}

// NOTE: returns index + 1 instead of just index
static i32
window_find(struct window *window, u32 window_count,
            v3 ray_start, v3 ray_direction)
{
    for (u32 i = 0; i < window_count; i++) {
        u32 intersects_window = window_ray_intersection_point(
            window, ray_start, ray_direction, 0);
        if (intersects_window) {
            return i + 1;
        }

        window++;
    }

    return 0;
}

void
game_update(struct backend_memory *memory, struct game_input *input)
{
    struct game_state *game = memory->data;
    struct world *world = &game->world;
    struct camera *camera = &game->camera;

    if (!memory->is_initialized) {
        game_init(memory);
        memory->is_initialized = 1;
    }

    m4x4 projection = game->camera.projection;
    m4x4 view = game->camera.view;
    m4x4 model = m4x4_id(1);

    v3 camera_pos = camera->position;
    v3 camera_front = camera->front;

    gl.ClearColor(0.45, 0.65, 0.85, 1.0);
    gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl.UseProgram(game->shader.program);
    gl.UniformMatrix4fv(game->shader.model, 1, GL_FALSE, model.e);
    gl.UniformMatrix4fv(game->shader.projection, 1, GL_FALSE, projection.e);
    gl.UniformMatrix4fv(game->shader.view, 1, GL_FALSE, view.e);

    struct window *windows = game->windows;
    u32 window_count = game->window_count;
    u32 active_window = game->active_window;
    if (!active_window) {
        player_move(game, input);
        v3 block_pos = {0};
        v3 block_normal = {0};
        f32 t = 0.f;
        u32 has_selected_block = 
            player_select_block(game, input, &block_pos, &block_normal, &t);

        u32 hot_window = game->hot_window;
        if (hot_window && has_selected_block) {
            struct window *window = game->windows + (hot_window - 1);

            v3 relative_up = V3(0, 1, 0);
            if (block_normal.y > 0.5) {
                if (fabsf(camera_front.x) < fabsf(camera_front.z)) {
                    relative_up = V3(0, 0, SIGN(camera_front.z));
                } else {
                    relative_up = V3(SIGN(camera_front.x), 0, 0);
                }
            }

            v3 offset = v3_mulf(camera_front, t - 0.001f);
            v3 window_pos = v3_add(camera_pos, offset);
            window_move(window, window_pos, block_normal, relative_up);
        } else if (!hot_window) {
            hot_window = window_find(windows, window_count, 
                                     camera_pos, camera_front);
        }


        if (input->mouse.buttons[1]) {
            u32 is_pressing_alt = input->controller.modifiers & MOD_ALT;
            if (hot_window && is_pressing_alt) {
                game->hot_window = game->hot_window ? 0 : hot_window;
            } else if (hot_window && !is_pressing_alt) {
                // TODO: destroy the active window
            } else if (has_selected_block && !is_pressing_alt) {
                world_destroy_block(world, block_pos.x, block_pos.y, block_pos.z);
            }
        }

        if (input->mouse.buttons[3]) {
            // TODO: resize the window
            if (hot_window) {
                game->active_window = hot_window;
            } else if (has_selected_block) {
                struct player *player = &game->player;
                v3 new_block_pos = v3_add(block_pos, block_normal);
                v3 player_size = V3(0.25, 0.99f, 0.25f);
                v3 player_pos = player->position;
                struct box block_bounds;
                v3 block_size = V3(0.5, 0.5, 0.5);
                v3 bounds_size = v3_add(block_size, player_size);
                block_bounds.min = v3_sub(new_block_pos, bounds_size);
                block_bounds.max = v3_add(new_block_pos, bounds_size);
                u32 can_place_block = !box_contains_point(block_bounds, player_pos);
                if (can_place_block) {
                    u32 selected_block = player->hotbar[player->hotbar_selection];
                    if (selected_block == BLOCK_WINDOW) {
                        game->hot_window = hot_window = game->window_count;
                    } else {
                        world_place_block(
                            world, 
                            new_block_pos.x, new_block_pos.y, new_block_pos.z,
                            selected_block);
                    }
                }
            }
        }
    } else {
        struct window *window = &game->windows[active_window - 1];
        v3 mouse_dx = v3_mulf(camera->right, input->mouse.dx * 0.001f);
        v3 mouse_dy = v3_mulf(camera->up, -input->mouse.dy * 0.001f);
        v3 mouse_pos = v3_add(game->mouse_pos, v3_add(mouse_dx, mouse_dy));
        v3 camera_pos = v3_add(camera->position, mouse_pos);
        v3 camera_front = camera->front;
        v3 window_pos = window->position;

        v2 cursor_pos = {0};
        u32 is_inside_window = 
            window_ray_intersection_point(window, camera_pos, camera_front,
                                          &cursor_pos);
        if (is_inside_window) {
            v3 x = v3_mulf(window->x_axis, cursor_pos.x);
            v3 y = v3_mulf(window->y_axis, cursor_pos.y);
            debug_line(window_pos, v3_add(v3_add(window_pos, y), x));
        }

        debug_set_color(0, 1, 0);
        debug_line(window_pos, v3_add(window_pos, window->x_axis));

        u32 is_pressing_alt = input->controller.modifiers & MOD_ALT;
        if (input->mouse.buttons[3] && is_pressing_alt) {
            game->active_window = 0;
        }

        game->mouse_pos = mouse_pos;
        game->cursor_pos = cursor_pos;
    }

    world_update(&game->world, game->camera.position);
    world_render(&game->world);

    gl.BindVertexArray(game->window_vertex_array);
    window_render(windows, window_count, game->shader.model);

    debug_render(view, projection);
}

void
game_finish(struct game_state *game)
{
    gl.DeleteVertexArrays(1, &game->window_vertex_array);
    gl.DeleteBuffers(1, &game->window_vertex_buffer);
    gl.DeleteBuffers(1, &game->window_index_buffer);

    gl.DeleteProgram(game->shader.program);
    world_finish(&game->world);
}
