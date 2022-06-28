#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <waycraft/game.h>
#include <waycraft/stb_image.h>

#include "waycraft/math.c"
#include "waycraft/memory.c"
#include "waycraft/log.c"
#include "waycraft/renderer.c"
#include "waycraft/block.c"
#include "waycraft/debug.c"
#include "waycraft/inventory.c"
#include "waycraft/noise.c"
#include "waycraft/timer.c"
#include "waycraft/world.c"

#define VIRTUAL_SCREEN_SIZE 400

static void
texture_init(struct texture *texture, char *path)
{
	i32 width, height, comp;
	u8 *data = stbi_load(path, &width, &height, &comp, 3);

	if (data) {
		u32 texture_handle = 0;
		gl.GenTextures(1, &texture_handle);
		gl.BindTexture(GL_TEXTURE_2D, texture_handle);
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl.GenerateMipmap(GL_TEXTURE_2D);

		texture->id.value = texture_handle;
		texture->width  = width;
		texture->height = height;

		free(data);
	}
}

static struct texture
get_texture(struct game_assets *assets, u32 texture_id)
{
	static char *texture_filenames[TEXTURE_COUNT] = {
		[TEXTURE_NONE]        = "",
		[TEXTURE_BLOCK_ATLAS] = "res/block_atlas.png",
		[TEXTURE_INVENTORY]   = "res/inventory.png",
		[TEXTURE_HOTBAR]      = "res/hotbar.png",
		[TEXTURE_ACTIVE_SLOT] = "res/active_slot.png",
	};

	assert(texture_id < TEXTURE_COUNT);

	if (assets->textures[texture_id].id.value == 0) {
		texture_init(&assets->textures[texture_id], texture_filenames[texture_id]);
	}

	return assets->textures[texture_id];
}

static void
camera_init(struct camera *camera, v3 position, f32 fov)
{
	camera->position = position;
	camera->yaw = 0.f;
	camera->pitch = 0.f;
	camera->fov = fov;
}

static void
camera_resize(struct camera *camera, f32 width, f32 height)
{
	camera->viewport.width = width;
	camera->viewport.height = height;
}

static void
camera_axes(struct camera *camera, v3 *out_right, v3 *out_up, v3 *out_forward)
{
	v3 forward, right, up;
	forward.x = cosf(radians(camera->yaw)) * cosf(radians(camera->pitch));
	forward.y = sinf(radians(camera->pitch));
	forward.z = sinf(radians(camera->yaw)) * cosf(radians(camera->pitch));

	forward = normalize(forward);
	right = normalize(cross(forward, V3(0, 1, 0)));
	up = normalize(cross(right, forward));

	*out_right   = right;
	*out_up      = up;
	*out_forward = forward;
}

static void
camera_rotate(struct camera *camera, f32 dx, f32 dy)
{
	f32 sensitivity = 0.1f;

	f32 yaw   = camera->yaw + dx * sensitivity;
	f32 pitch = camera->pitch - dy * sensitivity;

	camera->yaw   = fmodf(yaw + 360.f, 360.f);
	camera->pitch = CLAMP(pitch, -89.f, 89.f);

	v3 direction;
	direction.x = cosf(radians(camera->yaw)) * cosf(radians(camera->pitch));
	direction.y = sinf(radians(camera->pitch));
	direction.z = sinf(radians(camera->yaw)) * cosf(radians(camera->pitch));

	camera->direction = normalize(direction);
}

static m4x4
camera_get_view(struct camera *camera)
{
	v3 target = v3_add(camera->position, camera->direction);
	m4x4 result = m4x4_look_at(camera->position, target, V3(0, 1, 0));

	return result;
}

static m4x4
camera_get_projection(struct camera *camera)
{
	f32 fov = camera->fov;
	f32 znear = 0.1f;
	f32 zfar = 1000.f;
	f32 aspect_ratio = camera->viewport.width / camera->viewport.height;
	m4x4 result = m4x4_perspective(fov, aspect_ratio, znear, zfar);

	return result;
}

static void
player_init(struct player *player, struct camera *camera)
{
	f32 player_speed = 150.f;
	f32 camera_fov = 75.f;
	v3 player_position = V3(0, 20, 0);

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
	arena_suballoc(arena, MB(64), &game->frame_arena);

	debug_init(&game->debug_state);
	world_init(&game->world, arena);
	player_init(&game->player, &game->camera);
	renderer_init(&game->renderer, arena);
}

static struct box
box_from_center(v3 center, v3 size)
{
	struct box box;

	box.min = v3_sub(center, size);
	box.max = v3_add(center, size);
	return box;
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

	v3 player_pos = player->position;
	struct chunk *chunk = world_get_chunk(world, player_pos.x, player_pos.y,
		player_pos.z);
	if (!chunk || chunk->state != CHUNK_READY) {
		return;
	}

	v3 right, up, forward;
	camera_axes(camera, &right, &up, &forward);
	f32 speed = player->speed;
	v3 position = player->position;
	v3 velocity = player->velocity;
	v3 acceleration = player_direction_from_input(input, forward, right, speed);
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
	v3 direction = camera->direction;

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

		u32 block = world_at(world, block_pos.x, block_pos.y, block_pos.z);
		if (block_is_empty(block)) {
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

static v3
window_get_global_position(struct game_window *window,
		struct game_window_manager *wm)
{
	v3 position = window->position;
	u32 parent = window->parent;
	if (parent) {
		struct game_window *parent_window =
			window_manager_get_window(wm, parent);
		position = v3_add(position,
			window_get_global_position(parent_window, wm));
	}

	return position;
}

static void
window_move(struct game_window *window, v3 new_position, v3 normal, v3 up)
{
	v3 right = v3_cross(up, normal);

	m3x3 transform = M3X3(
		right.x,  right.y,  right.z,
		up.x,     up.y,     up.z,
		normal.x, normal.y, normal.z);

	window->position = new_position;
	window->rotation = transform;
}

static void
window_axes(struct game_window *window, v3 *x_axis, v3 *y_axis, v3 *z_axis)
{
	m3x3 rotation = window->rotation;

	f32 window_width = window->scale.x / VIRTUAL_SCREEN_SIZE;
	f32 window_height = window->scale.y / VIRTUAL_SCREEN_SIZE;

	*x_axis = m3x3_mulv(rotation, V3(window_width, 0, 0));
	*y_axis = m3x3_mulv(rotation, V3(0, -window_height, 0));
	*z_axis = m3x3_mulv(rotation, V3(0, 0, -1));
}

static void
window_manager_render(struct game_window_manager *wm, m4x4 view,
		m4x4 projection, struct render_command_buffer *cmd_buffer)
{
	u32 window_count = wm->window_count;
	struct game_window *window = wm->windows;

	v3 pos[4] = {0};
	v2 uv[4] = {
		V2(0, 0),
		V2(0, 1),
		V2(1, 0),
		V2(1, 1),
	};

	while (window_count-- > 0) {
		u32 is_window_visible = window->flags & WINDOW_VISIBLE;
		u32 is_window_destroyed = window->flags & WINDOW_DESTROYED;
		if (is_window_visible && !is_window_destroyed) {
			v3 window_pos = window_get_global_position(window, wm);
			v3 window_x, window_y, window_z;
			window_axes(window, &window_x, &window_y, &window_z);

			pos[0] = window_pos;
			pos[1] = v3_add(window_pos, window_y);
			pos[2] = v3_add(window_pos, window_x);
			pos[3] = v3_add(pos[2], window_y);

			struct texture_id window_texture = {window->texture};
			render_quad(cmd_buffer, pos[0], pos[1], pos[2], pos[3],
				uv[0], uv[1], uv[2], uv[3], window_texture);
			window++;
		}
	}
}

static u32
window_ray_intersection_point(struct game_window *window,
		v3 ray_start, v3 ray_direction, v2 *point_on_window)
{
	u32 hit = 0;
	v3 window_pos = window->position;
	v3 normal, window_x, window_y;
	window_axes(window, &window_x, &window_y, &normal);

	f32 d_dot_n = v3_dot(ray_direction, normal);
	if (fabsf(d_dot_n) > 0.0001f) {
		f32 p_dot_n = v3_dot(v3_sub(window_pos, ray_start), normal);
		f32 t = p_dot_n / d_dot_n;
		if (t > 0.f) {
			v3 point = v3_add(ray_start, v3_mulf(ray_direction, t));
			v3 relative_point = v3_sub(point, window_pos);

			f32 tx = v3_dot(window_x, relative_point) / v3_dot(window_x, window_x);
			f32 ty = v3_dot(window_y, relative_point) / v3_dot(window_y, window_y);
			u32 is_inside_window = (0 <= tx && tx <= 1) && (0 <= ty && ty <= 1);

			if (point_on_window) {
				point_on_window->x = tx * window->scale.x;
				point_on_window->y = ty * window->scale.y;
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
		if (intersects_window && window->flags & WINDOW_VISIBLE) {
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

	assert(memory->gl);
	gl = *memory->gl;

	if (!memory->is_initialized) {
		game_init(memory);
		memory->is_initialized = 1;
	}

	// NOTE: reset the frame arena
	game->frame_arena.used = 0;
	debug_update(&game->debug_state, &game->frame_arena);

	m4x4 projection = camera_get_projection(camera);
	m4x4 view = camera_get_view(camera);

	v3 camera_pos = camera->position;
	v3 camera_front = camera->direction;

	u32 max_quad_count = 1024;
	struct render_command_buffer cmd_buffer = {0};
	render_command_buffer_init(&cmd_buffer, &game->frame_arena,
		MB(2), 4 * max_quad_count, 6 * max_quad_count);
	cmd_buffer.mode = RENDER_3D;
	cmd_buffer.transform.view = view;
	cmd_buffer.transform.projection = projection;
	cmd_buffer.transform.camera_pos = camera_pos;

	struct render_command_buffer ui_cmd_buffer = {0};
	render_command_buffer_init(&ui_cmd_buffer, &game->frame_arena,
		KB(64), 4 * 128, 6 * 128);
	ui_cmd_buffer.mode = RENDER_2D;
	ui_cmd_buffer.transform.view = m4x4_id(1);
	ui_cmd_buffer.transform.projection = m4x4_ortho(0, input->height, 0, input->width, -1, 1);
	ui_cmd_buffer.transform.camera_pos = V3(0, 0, 0);

	render_clear(&cmd_buffer, V4(0.45, 0.65, 0.85, 1.0));

	u32 window_count = wm->window_count;
	struct game_window *windows = wm->windows;
	struct game_window *focused_window = window_manager_get_focused_window(wm);

	struct player *player = &game->player;
	u32 inventory_is_active = player->inventory.is_active;

	for (u32 i = 0; i < window_count; i++) {
		if (!(windows[i].flags & WINDOW_INITIALIZED)) {
			inventory_add_item(&player->inventory, ITEM_WINDOW, i);
			windows[i].flags |= WINDOW_INITIALIZED;
		}
	}

	if (!focused_window && !inventory_is_active) {
		player_move(game, input);
		camera->position = v3_add(player->position, V3(0, 0.75, 0));
		camera_resize(&game->camera, input->width, input->height);
		camera_rotate(&game->camera, input->mouse.dx, input->mouse.dy);

		v3 block_pos = {0};
		v3 block_normal = {0};
		f32 t = 0.f;
		u32 has_selected_block = player_select_block(game, input,
			&block_pos, &block_normal, &t);

		struct game_window *hot_window = game->hot_window;
		if (hot_window) {
			if (has_selected_block) {
				v3 relative_up = V3(0, 1, 0);
				if (block_normal.y > 0.5) {
					if (fabsf(camera_front.x) < fabsf(camera_front.z)) {
						relative_up = V3(0, 0, SIGN(camera_front.z));
					} else {
						relative_up = V3(SIGN(camera_front.x), 0, 0);
					}
				}

				v3 offset = v3_mulf(camera_front, t - 0.01f);
				v3 window_pos = v3_add(camera_pos, offset);
				hot_window->flags |= WINDOW_VISIBLE;
				window_move(hot_window, window_pos, block_normal, relative_up);
			} else {
				hot_window->flags &= ~WINDOW_VISIBLE;
			}
		}

		if (input->mouse.buttons[1]) {
			u32 is_pressing_alt = input->alt_down;
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
			u32 is_pressing_alt = input->alt_down;
			if (is_pressing_alt) {
				if (hot_window) {
					// TODO: resize window
				}
			} else {
				u32 hotbar_selection = player->inventory.active_item;
				struct inventory_item *selected_item =
					&player->inventory.items[hotbar_selection];

				u32 selected_block = item_to_block(selected_item->type, camera_front);
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
					wm->focused_window = window_manager_get_window_id(wm, window);
				} else if (selected_item->type == ITEM_WINDOW) {
					hot_window = &wm->windows[selected_item->count];
					selected_item->type = ITEM_NONE;
					selected_item->count = 0;
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

		game->hot_window = hot_window;
	} else if (inventory_is_active) {
		if (button_was_pressed(input->controller.toggle_inventory)) {
			player->inventory.is_active = 0;
		}
	}

	world_update(&game->world, game->camera.position, game->camera.direction,
		&game->renderer, &cmd_buffer, &game->frame_arena, &game->assets);
	window_manager_render(wm, view, projection, &cmd_buffer);

	if (focused_window) {
		v3 camera_right, camera_up, camera_forward;
		camera_axes(camera, &camera_right, &camera_up, &camera_forward);
		v3 mouse_dx = v3_mulf(camera_right, input->mouse.dx * 0.001f);
		v3 mouse_dy = v3_mulf(camera_up, -input->mouse.dy * 0.001f);
		v3 mouse_pos = v3_add(game->mouse_pos, v3_add(mouse_dx, mouse_dy));
		v3 camera_pos = v3_add(camera->position, mouse_pos);

		// draw the cursor
		v3 window_x, window_y, window_z;
		window_axes(focused_window, &window_x, &window_y, &window_z);

		v2 cursor_pos = {0};
		window_ray_intersection_point(focused_window,
			camera_pos, camera->direction, &cursor_pos);

		v3 window_pos = focused_window->position;
		v2 window_scale = focused_window->scale;
		v2 cursor_offset = v2_mulf(wm->cursor.offset, 1./VIRTUAL_SCREEN_SIZE);
		v3 cursor_world_pos = v3_add(window_pos, v3_add(v3_add(
			v3_mulf(window_x, cursor_pos.x / window_scale.x + cursor_offset.x),
			v3_mulf(window_y, cursor_pos.y / window_scale.y + cursor_offset.y)),
			v3_mulf(window_z, -0.01f)));

		struct texture_id cursor_texture = {wm->cursor.texture};
		v2 uv[4];
		uv[0] = V2(0, 0);
		uv[1] = V2(0, 1);
		uv[2] = V2(1, 0);
		uv[3] = V2(1, 1);

		v2 cursor_scale = v2_mulf(wm->cursor.scale, 1./VIRTUAL_SCREEN_SIZE);
		v3 cursor_x = v3_mulf(v3_norm(window_x), cursor_scale.x);
		v3 cursor_y = v3_mulf(v3_norm(window_y), cursor_scale.y);

		v3 pos[4];
		pos[0] = cursor_world_pos;
		pos[1] = v3_add(cursor_world_pos, cursor_y);
		pos[2] = v3_add(cursor_world_pos, cursor_x);
		pos[3] = v3_add(pos[2], cursor_y);

		render_quad(&cmd_buffer, pos[0], pos[1], pos[2], pos[3],
			uv[0], uv[1], uv[2], uv[3], cursor_texture);

		u32 is_pressing_alt = input->alt_down;
		if (input->mouse.buttons[3] && is_pressing_alt) {
			// TODO: change this to resizing the window, choose a different
			// keybind for deselecting the active window.
			wm->focused_window = 0;
			wm->is_active = 0;
		}

		wm->cursor.position = cursor_pos;
		game->mouse_pos = mouse_pos;

		if (focused_window->flags & WINDOW_DESTROYED) {
			wm->focused_window = 0;
		}
	}

	inventory_render(&player->inventory, input->width, input->height, &ui_cmd_buffer, &game->assets);

	renderer_submit(&game->renderer, &cmd_buffer);
	renderer_submit(&game->renderer, &ui_cmd_buffer);

	debug_render(view, projection);
}

static void
game_finish(struct game_state *game)
{
	renderer_finish(&game->renderer);
}
