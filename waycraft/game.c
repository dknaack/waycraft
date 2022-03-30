#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <waycraft/debug.h>
#include <waycraft/gl.h>
#include <waycraft/game.h>
#include <waycraft/math.h>
#include <waycraft/noise.h>
#include <waycraft/timer.h>
#include <waycraft/backend.h>

static void
camera_init(struct camera *camera, v3 position, f32 fov)
{
	camera->view = m4x4_id(1.f);
	camera->projection = m4x4_id(1.f);
	camera->position = position;
	camera->yaw = 0.f;
	camera->pitch = 0.f;
	camera->fov = fov;
}

static void
camera_resize(struct camera *camera, f32 width, f32 height)
{
	f32 fov = camera->fov;
	f32 znear = 0.1f;
	f32 zfar = 1000.f;
	f32 aspect_ratio = width / height;

	camera->projection = m4x4_perspective(fov, aspect_ratio, znear, zfar);
}

static void
camera_rotate(struct camera *camera, f32 dx, f32 dy)
{
	f32 sensitivity = 0.1f;

	f32 yaw   = camera->yaw + dx * sensitivity;
	f32 pitch = camera->pitch - dy * sensitivity;

	camera->yaw   = fmodf(yaw + 360.f, 360.f);
	camera->pitch = CLAMP(pitch, -89.f, 89.f);

	v3 front;
	front.x = cosf(DEG2RAD(camera->yaw)) * cosf(DEG2RAD(camera->pitch));
	front.y = sinf(DEG2RAD(camera->pitch));
	front.z = sinf(DEG2RAD(camera->yaw)) * cosf(DEG2RAD(camera->pitch));

	camera->front = v3_norm(front);
	camera->right = v3_norm(v3_cross(front, V3(0, 1, 0)));
	camera->up = v3_norm(v3_cross(camera->right, camera->front));

	v3 target = v3_add(camera->position, camera->front);
	camera->view = m4x4_look_at(camera->position, target, camera->up);
}

static void
player_init(struct player *player, struct camera *camera)
{
	f32 player_speed = 150.f;
	f32 camera_fov = 65.f;
	v3 player_position = V3(0, 20, 0);

	inventory_init(&player->inventory);
	camera_init(camera, player_position, camera_fov);
	player->position = player_position;
	player->speed = player_speed;

	struct inventory_item *hotbar = player->inventory.items;
	(*hotbar++).type = ITEM_NONE;
	(*hotbar++).type = ITEM_DIRT_BLOCK;
	(*hotbar++).type = ITEM_GRASS_BLOCK;
	(*hotbar++).type = ITEM_SAND_BLOCK;
	(*hotbar++).type = ITEM_PLANKS_BLOCK;
	(*hotbar++).type = ITEM_OAK_LOG_BLOCK;
	(*hotbar++).type = ITEM_OAK_LEAVES_BLOCK;
	(*hotbar++).type = ITEM_WATER_BLOCK;
}

static void
game_init(struct backend_memory *memory)
{
	struct game_state *game = memory->data;
	struct memory_arena *arena = &game->arena;
	arena_init(arena, game + 1, memory->size - sizeof(struct game_state));

	debug_init(arena);
	world_init(&game->world, arena);
	player_init(&game->player, &game->camera);
	renderer_init(&game->renderer, arena);
}

struct box {
	v3 min;
	v3 max;
};

static struct box
box_from_center(v3 center, v3 size)
{
	struct box box;

	box.min = v3_sub(center, size);
	box.max = v3_add(center, size);
	return box;
}

static i32
box_contains_point(struct box box, v3 point)
{
	return (box.min.x <= point.x && point.x <= box.max.x &&
		box.min.y <= point.y && point.y <= box.max.y &&
		box.min.z <= point.z && point.z <= box.max.z);
}

static v3
player_direction_from_input(struct game_input *input, v3 front, v3 right, f32 speed)
{
	v3 direction = V3(0, 0, 0);
	f32 haxis = button_is_down(input->controller.move_right) -
		button_is_down(input->controller.move_left);
	f32 vaxis = button_is_down(input->controller.move_up) -
		button_is_down(input->controller.move_down);

	if (haxis || vaxis) {
		front = v3_mulf(front, vaxis);
		right = v3_mulf(right, haxis);
		front.y = right.y = 0;

		direction = v3_mulf(v3_norm(v3_add(front, right)), speed);
	}

	return direction;
}

// NOTE: assumes ray starts outside the box and intersects the box
static u32
ray_box_intersection(struct box box, v3 start, v3 direction,
	v3 *normal_min, v3 *normal_max, f32 *out_tmin, f32 *out_tmax)
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

static void
player_move(struct game_state *game, struct game_input *input)
{
	struct player *player = &game->player;
	struct camera *camera = &game->camera;
	struct world *world = &game->world;

	f32 dt = input->dt;

	v3 front = camera->front;
	v3 right = camera->right;
	f32 speed = player->speed;
	v3 position = player->position;
	v3 velocity = player->velocity;
	v3 acceleration = player_direction_from_input(input, front, right, speed);
	acceleration = v3_sub(acceleration, v3_mulf(velocity, 15.f));
	acceleration.y = -100.f;

	if (button_is_down(input->controller.jump) && player->frames_since_jump < 5) {
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
	struct box selected_block = box_from_center(block_pos, block_size);

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
		player->inventory.active_item++;
		player->inventory.active_item %= 9;
	}

	if (input->mouse.buttons[4]) {
		if (player->inventory.active_item == 0) {
			player->inventory.active_item = 8;
		} else {
			player->inventory.active_item--;
		}
	}

	*out_block_pos = block_pos;
	*out_normal_min = normal_min;
	*out_t = tmin;
	return has_selected_block;
}

static m4x4
window_transform(struct game_window *window)
{
	return m4x4_to_coords(window->position,
		window->x_axis, window->y_axis, window->z_axis);
}

static void
window_move(struct game_window *window, v3 window_pos, v3 normal, v3 up)
{
	window->position = window_pos;
	window->z_axis = normal;
	window->x_axis = v3_cross(up, window->z_axis);
	window->y_axis = v3_cross(window->z_axis, window->x_axis);
}

static void
window_render(struct game_window *window, u32 window_count, m4x4 view,
	m4x4 projection, struct render_command_buffer *cmd_buffer)
{
	while (window_count-- > 0) {
		m4x4 transform = m4x4_mul(m4x4_mul(projection, view),
			window_transform(window));

		render_textured_quad(cmd_buffer, transform, window->texture);
		window++;
	}
}

static u32
window_ray_intersection_point(struct game_window *window,
	v3 ray_start, v3 ray_direction, v2 *point_on_window)
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

static struct game_window *
window_find(struct game_window *window, u32 window_count,
	v3 ray_start, v3 ray_direction)
{
	for (u32 i = 0; i < window_count; i++) {
		u32 intersects_window = window_ray_intersection_point(
			window, ray_start, ray_direction, 0);
		if (intersects_window) {
			return window;
		}

		window++;
	}

	return 0;
}

void
game_update(struct backend_memory *memory, struct game_input *input,
	struct game_window_manager *wm)
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

	v3 camera_pos = camera->position;
	v3 camera_front = camera->front;

	struct render_command_buffer *render_commands =
		renderer_begin_frame(&game->renderer);
	render_commands->transform.view = view;
	render_commands->transform.projection = projection;
	render_clear(render_commands, V4(0.45, 0.65, 0.85, 1.0));

	u32 window_count = wm->window_count;
	struct game_window *windows = wm->windows;
	struct game_window *active_window = wm->active_window;

	struct player *player = &game->player;
	u32 inventory_is_active = player->inventory.is_active;

	for (u32 i = 0; i < window_count; i++) {
		if (!windows[i].is_initialized) {
			inventory_add_item(&player->inventory, ITEM_WINDOW, i);
			windows[i].is_initialized = 1;
		}
	}

	if (!active_window && !inventory_is_active) {
		player_move(game, input);
		v3 block_pos = {0};
		v3 block_normal = {0};
		f32 t = 0.f;
		u32 has_selected_block = player_select_block(game, input,
			&block_pos, &block_normal, &t);

		struct game_window *hot_window = wm->hot_window;
		if (hot_window && has_selected_block) {
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
			window_move(hot_window, window_pos, block_normal, relative_up);
		}

		if (input->mouse.buttons[1]) {
			u32 is_pressing_alt = input->controller.modifiers & MOD_ALT;
			struct game_window *window = 0;

			if (is_pressing_alt) {
				if (hot_window) {
					hot_window = 0;
				} else {
					hot_window = window_find(windows, window_count, camera_pos,
						camera_front);
				}
			} else if ((window = window_find(windows, window_count, camera_pos,
					camera_front))) {
				// TODO: destroy the window
			} else if (has_selected_block) {
				world_destroy_block(world, block_pos.x, block_pos.y, block_pos.z);
			}
		}

		if (input->mouse.buttons[3]) {
			u32 is_pressing_alt = input->controller.modifiers & MOD_ALT;
			if (is_pressing_alt) {
				if (hot_window) {
					// TODO: resize window
				}
			} else {
				u32 hotbar_selection = player->inventory.active_item;
				struct inventory_item selected_item =
					player->inventory.items[hotbar_selection];

				u32 selected_block = item_to_block(selected_item.type, camera_front);
				v3 new_block_pos = v3_add(block_pos, block_normal);
				v3 player_size = V3(0.25, 0.99f, 0.25f);
				v3 player_pos = player->position;
				v3 block_size = V3(0.5, 0.5, 0.5);
				struct box block_bounds = box_from_center(new_block_pos,
					v3_add(block_size, player_size));

				struct game_window *window = window_find(windows, window_count,
					camera_pos, camera_front);
				if (window) {
					wm->is_active = 1;
					wm->active_window = window;
				} else if (selected_item.type == ITEM_WINDOW) {
					hot_window = &wm->windows[selected_item.count];
				} else if (has_selected_block &&
						!box_contains_point(block_bounds, player_pos)) {
					world_place_block(world, new_block_pos.x, new_block_pos.y,
						new_block_pos.z, selected_block);
				}
			}
		}

		if (button_was_pressed(input->controller.toggle_inventory)) {
			player->inventory.is_active = 1;
		}

		wm->hot_window = hot_window;
	} else if (inventory_is_active) {

		if (button_was_pressed(input->controller.toggle_inventory)) {
			player->inventory.is_active = 0;
		}
	} else {
		v3 mouse_dx = v3_mulf(camera->right, input->mouse.dx * 0.001f);
		v3 mouse_dy = v3_mulf(camera->up, -input->mouse.dy * 0.001f);
		v3 mouse_pos = v3_add(game->mouse_pos, v3_add(mouse_dx, mouse_dy));
		v3 camera_pos = v3_add(camera->position, mouse_pos);
		v3 camera_front = camera->front;
		v3 window_pos = active_window->position;

		v2 cursor_pos = {0};
		u32 is_inside_window = window_ray_intersection_point(active_window,
			camera_pos, camera_front, &cursor_pos);
		if (is_inside_window) {
			v3 x = v3_mulf(active_window->x_axis, cursor_pos.x);
			v3 y = v3_mulf(active_window->y_axis, cursor_pos.y);
			debug_line(window_pos, v3_add(v3_add(window_pos, y), x));
		}

		debug_set_color(0, 1, 0);
		debug_line(window_pos, v3_add(window_pos, active_window->x_axis));

		u32 is_pressing_alt = input->controller.modifiers & MOD_ALT;
		if (input->mouse.buttons[3] && is_pressing_alt) {
			// TODO: change this to resizing the window, choose a different
			// keybind for deselecting the active window.
			wm->active_window = 0;
			wm->is_active = 0;
		}

		game->mouse_pos = mouse_pos;
		wm->cursor_pos = cursor_pos;
	}

	world_update(&game->world, game->camera.position, render_commands);

	world_render(&game->world, render_commands);
	window_render(windows, window_count, view, projection, render_commands);
	inventory_render(&player->inventory, input->width, input->height, render_commands);
	renderer_end_frame(&game->renderer, render_commands);

	gl.UseProgram(game->renderer.shader.program);
	gl.Uniform3f(gl.GetUniformLocation(game->renderer.shader.program, "camera_pos"), camera_pos.x, camera_pos.y, camera_pos.z);
	gl.UniformMatrix4fv(game->renderer.shader.model, 1, GL_FALSE, m4x4_id(1).e);
	gl.UniformMatrix4fv(game->renderer.shader.view, 1, GL_FALSE, view.e);
	gl.UniformMatrix4fv(game->renderer.shader.projection, 1, GL_FALSE, projection.e);

	debug_render(view, projection);
}

void
game_finish(struct game_state *game)
{
	gl.DeleteVertexArrays(1, &game->window_vertex_array);
	gl.DeleteBuffers(1, &game->window_vertex_buffer);
	gl.DeleteBuffers(1, &game->window_index_buffer);

	renderer_finish(&game->renderer);
	world_finish(&game->world);
}
