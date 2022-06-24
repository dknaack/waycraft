// TODO: fix bug for top faces

static inline f32
normalize_height(f32 value)
{
	f32 x = value + 0.8;

	if (x < 1.0) {
		x *= MIN(MAX(x, 0.6), 1.0);
	}

	return x;
}

static void
chunk_init(struct chunk *chunk)
{
	f32 noise_size = 0.1f;

	if (chunk->flags & CHUNK_INITIALIZED) {
		return;
	}

	chunk->flags = CHUNK_INITIALIZED;
	u16 *blocks = chunk->blocks;
	v3 chunk_pos = chunk->position;
	assert(blocks);

	memset(blocks, 0, CHUNK_BLOCK_COUNT * sizeof(*blocks));
	for (i32 z = 0; z < CHUNK_SIZE; z++) {
		for (i32 x = 0; x < CHUNK_SIZE; x++) {
			f32 nx = noise_size * (chunk_pos.x + x + 0.5);
			f32 nz = noise_size * (chunk_pos.z + z + 0.5);
			i32 height = normalize_height(noise_layered_2d(nx, nz)) * CHUNK_SIZE - chunk_pos.y;

			i32 ymax = MAX(0, MIN(CHUNK_SIZE, height));
			for (i32 y = 0; y < ymax; y++) {
				u32 i = (z * CHUNK_SIZE + y) * CHUNK_SIZE + x;
				if (y + 1 == height && chunk_pos.y + y > 0) {
					blocks[i] = BLOCK_GRASS;
				} else if (y + 2 >= height && chunk_pos.y + ymax <= 2 &&
						chunk_pos.y + ymax >= -2) {
					blocks[i] = BLOCK_SAND;
				} else if (y + 2 >= height) {
					blocks[i] = BLOCK_DIRT;
				} else {
					blocks[i] = BLOCK_STONE;
				}
			}

			if (chunk_pos.y < 0) {
				for (i32 y = ymax; y < CHUNK_SIZE; y++) {
					u32 i = (z * CHUNK_SIZE + y) * CHUNK_SIZE + x;
					blocks[i] = BLOCK_WATER;
				}
			}
		}
	}
}

static u32
chunk_at(const struct chunk *chunk, i32 x, i32 y, i32 z)
{
	u32 result = 0;
	u32 is_inside_chunk = (0 <= x && x < CHUNK_SIZE)
		&& (0 <= y && y < CHUNK_SIZE)
		&& (0 <= z && z < CHUNK_SIZE);
	if (is_inside_chunk) {
		u32 index = (z * CHUNK_SIZE + y) * CHUNK_SIZE + x;

		result = chunk->blocks[index];
	}

	return result;
}

static void
world_unload_chunk(struct world *world, struct chunk *chunk)
{
	struct chunk *first_chunk = world->chunks;
	struct chunk *last_chunk = first_chunk + WORLD_CHUNK_COUNT;
	u32 is_valid_chunk = chunk && first_chunk <= chunk && chunk < last_chunk;

	if (is_valid_chunk) {
		u32 is_already_modified = chunk->flags & CHUNK_MODIFIED;
		if (!is_already_modified) {
			chunk->flags |= CHUNK_MODIFIED;

			u32 chunk_index = chunk - world->chunks;
			u32 index = world->unloaded_chunk_count++;
			assert(index < WORLD_CHUNK_COUNT);

			world->unloaded_chunks[index] = chunk_index;
		}
	}
}

// NOTE: may generate chunk if it has not been initialized
static struct chunk *
world_get_chunk(struct world *world, f32 x, f32 y, f32 z)
{
	struct chunk *chunk = world->chunks;
	for (u32 i = 0; i < WORLD_CHUNK_COUNT; i++) {
		v3 chunk_pos = chunk->position;

		if (chunk_pos.x <= x && x < chunk_pos.x + CHUNK_SIZE &&
				chunk_pos.y <= y && y < chunk_pos.y + CHUNK_SIZE &&
				chunk_pos.z <= z && z < chunk_pos.z + CHUNK_SIZE) {
			return chunk;
		}

		chunk++;
	}

	return 0;
}

static v3
world_get_block_position(const struct world *world, f32 x, f32 y, f32 z)
{
	v3 relative_pos = v3_sub(V3(x + 1e-3, y + 1e-3, z + 1e-3), world->position);
	v3 block_pos = v3_modf(relative_pos, CHUNK_SIZE);

	if (block_pos.x < 0) {
		block_pos.x += CHUNK_SIZE;
	}

	if (block_pos.y < 0) {
		block_pos.y += CHUNK_SIZE;
	}

	if (block_pos.z < 0) {
		block_pos.z += CHUNK_SIZE;
	}

	return block_pos;
}

u32
world_at(struct world *world, f32 x, f32 y, f32 z)
{
	u32 result = 0;

	struct chunk *chunk = world_get_chunk(world, x, y, z);

	if (chunk) {
		v3 block_pos = world_get_block_position(world, x, y, z);

		assert(block_pos.x >= 0);
		assert(block_pos.y >= 0);
		assert(block_pos.z >= 0);

		result = chunk_at(chunk, block_pos.x, block_pos.y, block_pos.z);
	}

	return result;
}

static void
block_generate_mesh(enum block_type block, i32 x, i32 y, i32 z,
	struct world *world, struct mesh_data *mesh)
{
	v3 pos[8];
	v2 uv[4];

	pos[0] = V3(x + 0.5, y + 0.5, z + 0.5);
	pos[1] = V3(x - 0.5, y + 0.5, z + 0.5);
	pos[2] = V3(x + 0.5, y - 0.5, z + 0.5);
	pos[3] = V3(x - 0.5, y - 0.5, z + 0.5);

	pos[4] = V3(x + 0.5, y + 0.5, z - 0.5);
	pos[5] = V3(x - 0.5, y + 0.5, z - 0.5);
	pos[6] = V3(x + 0.5, y - 0.5, z - 0.5);
	pos[7] = V3(x - 0.5, y - 0.5, z - 0.5);

	u32 (*is_empty)(enum block_type block) = block_is_empty;

	if (block == BLOCK_WATER) {
		is_empty = block_is_not_water;

		if (world_at(world, x, y + 1, z) == BLOCK_AIR) {
			f32 offset = 0.1f;

			pos[0].y -= offset;
			pos[1].y -= offset;
			pos[4].y -= offset;
			pos[5].y -= offset;
		}
	}

	u32 texture = world->texture;

	block_texcoords_bottom(block, uv);
	if (is_empty(world_at(world, x, y - 1, z))) {
		mesh_push_quad(mesh, pos[7], pos[6], pos[3], pos[2],
			uv[0], uv[1], uv[2], uv[3], texture);
	}

	block_texcoords_top(block, uv);
	if (is_empty(world_at(world, x, y + 1, z))) {
		mesh_push_quad(mesh, pos[4], pos[5], pos[0], pos[1],
			uv[0], uv[1], uv[2], uv[3], texture);
	}

	block_texcoords_right(block, uv);
	if (is_empty(world_at(world, x + 1, y, z))) {
		mesh_push_quad(mesh, pos[4], pos[0], pos[6], pos[2],
			uv[0], uv[1], uv[2], uv[3], texture);
	}

	block_texcoords_left(block, uv);
	if (is_empty(world_at(world, x - 1, y, z))) {
		mesh_push_quad(mesh, pos[1], pos[5], pos[3], pos[7],
			uv[0], uv[1], uv[2], uv[3], texture);
	}

	block_texcoords_front(block, uv);
	if (is_empty(world_at(world, x, y, z + 1))) {
		mesh_push_quad(mesh, pos[0], pos[1], pos[2], pos[3],
			uv[0], uv[1], uv[2], uv[3], texture);
	}

	block_texcoords_back(block, uv);
	if (is_empty(world_at(world, x, y, z - 1))) {
		mesh_push_quad(mesh, pos[5], pos[4], pos[7], pos[6],
			uv[0], uv[1], uv[2], uv[3], texture);
	}
}

static inline u32
block_index(u32 x, u32 y, u32 z)
{
	return (z * CHUNK_SIZE + y) * CHUNK_SIZE + x;
}

static void
chunk_generate_mesh(struct chunk *chunk, struct world *world,
	struct mesh_data *mesh, struct render_command_buffer *cmd_buffer)
{
	chunk->flags &= ~CHUNK_MODIFIED;

	u16 blocks_empty[CHUNK_BLOCK_COUNT] = {0};
	v3 chunk_pos = chunk->position;

	struct chunk *chunk_first = world->chunks;
	struct chunk *chunk_last = world->chunks + WORLD_CHUNK_COUNT;

	struct chunk *chunk_right  = chunk + 1;
	struct chunk *chunk_left   = chunk - 1;
	struct chunk *chunk_top    = chunk + WORLD_WIDTH;
	struct chunk *chunk_bottom = chunk - WORLD_WIDTH;
	struct chunk *chunk_front  = chunk + WORLD_WIDTH * WORLD_HEIGHT;
	struct chunk *chunk_back   = chunk - WORLD_WIDTH * WORLD_HEIGHT;

	u16 *blocks_right  = blocks_empty;
	u16 *blocks_left   = blocks_empty;
	u16 *blocks_top    = blocks_empty;
	u16 *blocks_bottom = blocks_empty;
	u16 *blocks_front  = blocks_empty;
	u16 *blocks_back   = blocks_empty;

	if (chunk_right < chunk_last && chunk_right->position.x > chunk_pos.x) {
		chunk_init(chunk_right);
		blocks_right = chunk_right->blocks;
	}

	if (chunk_left >= chunk_first && chunk_left->position.x < chunk_pos.x) {
		chunk_init(chunk_left);
		blocks_left = chunk_left->blocks;
	}

	if (chunk_top < chunk_last && chunk_top->position.y > chunk_pos.y) {
		chunk_init(chunk_top);
		blocks_top = chunk_top->blocks;
	}

	if (chunk_bottom >= chunk_first && chunk_bottom->position.y < chunk_pos.y) {
		chunk_init(chunk_bottom);
		blocks_bottom = chunk_bottom->blocks;
	}

	if (chunk_front < chunk_last && chunk_front->position.z > chunk_pos.z) {
		chunk_init(chunk_front);
		blocks_front = chunk_front->blocks;
	}

	if (chunk_back >= chunk_first && chunk_back->position.z < chunk_pos.z) {
		chunk_init(chunk_back);
		blocks_back = chunk_back->blocks;
	}

	u32 texture = world->texture;
	u16 *blocks = chunk->blocks;
	assert(blocks);

	for (u32 i = 0; i < CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE; i++) {
		u32 block = blocks[i];
		if (block == BLOCK_AIR) {
			continue;
		}

		i32 x = i % CHUNK_SIZE;
		i32 y = i / CHUNK_SIZE % CHUNK_SIZE;
		i32 z = i / CHUNK_SIZE / CHUNK_SIZE % CHUNK_SIZE;

		v3 pos[8];
		v2 uv[4];

		pos[0] = v3_add(chunk_pos, V3(x + 0.5, y + 0.5, z + 0.5));
		pos[1] = v3_add(chunk_pos, V3(x - 0.5, y + 0.5, z + 0.5));
		pos[2] = v3_add(chunk_pos, V3(x + 0.5, y - 0.5, z + 0.5));
		pos[3] = v3_add(chunk_pos, V3(x - 0.5, y - 0.5, z + 0.5));

		pos[4] = v3_add(chunk_pos, V3(x + 0.5, y + 0.5, z - 0.5));
		pos[5] = v3_add(chunk_pos, V3(x - 0.5, y + 0.5, z - 0.5));
		pos[6] = v3_add(chunk_pos, V3(x + 0.5, y - 0.5, z - 0.5));
		pos[7] = v3_add(chunk_pos, V3(x - 0.5, y - 0.5, z - 0.5));

		u32 (*is_empty)(enum block_type block) = block_is_empty;

		u16 block_right = x + 1 >= CHUNK_SIZE ?
			blocks_right[block_index(x + 1 - CHUNK_SIZE, y, z)] :
			blocks[block_index(x + 1, y, z)];

		u16 block_left = x - 1 < 0 ?
			blocks_left[block_index(x - 1 + CHUNK_SIZE, y, z)] :
			blocks[block_index(x - 1, y, z)];

		u16 block_top = y + 1 >= CHUNK_SIZE ?
			blocks_top[block_index(x, y + 1 - CHUNK_SIZE, z)] :
			blocks[block_index(x, y + 1, z)];

		u16 block_bottom = y - 1 < 0 ?
			blocks_bottom[block_index(x, y - 1 + CHUNK_SIZE, z)] :
			blocks[block_index(x, y - 1, z)];

		u16 block_front = z + 1 >= CHUNK_SIZE ?
			blocks_front[block_index(x, y, z + 1 - CHUNK_SIZE)] :
			blocks[block_index(x, y, z + 1)];

		u16 block_back = z - 1 < 0 ?
			blocks_back[block_index(x, y, z - 1 + CHUNK_SIZE)] :
			blocks[block_index(x, y, z - 1)];

		if (block == BLOCK_WATER) {
			is_empty = block_is_not_water;

			if (block_top == BLOCK_AIR) {
				f32 offset = 0.1f;

				pos[0].y -= offset;
				pos[1].y -= offset;
				pos[4].y -= offset;
				pos[5].y -= offset;
			}
		}

		if (is_empty(block_right)) {
			block_texcoords_right(block, uv);
			mesh_push_quad(mesh, pos[4], pos[0], pos[6], pos[2],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (is_empty(block_left)) {
			block_texcoords_left(block, uv);
			mesh_push_quad(mesh, pos[1], pos[5], pos[3], pos[7],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (is_empty(block_top)) {
			block_texcoords_top(block, uv);
			mesh_push_quad(mesh, pos[4], pos[5], pos[0], pos[1],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (is_empty(block_bottom)) {
			block_texcoords_bottom(block, uv);
			mesh_push_quad(mesh, pos[7], pos[6], pos[3], pos[2],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (is_empty(block_front)) {
			block_texcoords_front(block, uv);
			mesh_push_quad(mesh, pos[0], pos[1], pos[2], pos[3],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (is_empty(block_back)) {
			block_texcoords_back(block, uv);
			mesh_push_quad(mesh, pos[5], pos[4], pos[7], pos[6],
				uv[0], uv[1], uv[2], uv[3], texture);
		}
	}

	chunk->mesh = mesh_create(cmd_buffer, mesh);
}

i32
world_init(struct world *world, struct memory_arena *arena)
{
	world->chunks = arena_alloc(arena, WORLD_CHUNK_COUNT, struct chunk);

	f32 xoffset = -0.5f * WORLD_WIDTH  * CHUNK_SIZE;
	f32 yoffset = -0.5f * WORLD_HEIGHT * CHUNK_SIZE;
	f32 zoffset = -0.5f * WORLD_DEPTH  * CHUNK_SIZE;
	world->position = V3(xoffset, yoffset, zoffset);

	struct mesh_data *mesh = &world->mesh;
	u32 size = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
	mesh->vertices = arena_alloc(arena, size * 64, struct vertex);
	mesh->indices = arena_alloc(arena, size * 36, u32);
	if (!mesh->vertices || !mesh->indices) {
		return -1;
	}

	i32 width, height, comp;
	u8 *data;

	// TODO: load this separate from the game
	if (!(data = stbi_load("res/textures.png", &width, &height, &comp, 3))) {
		fprintf(stderr, "Failed to load texture atlas\n");
		return -1;
	}

	u32 texture;
	gl.GenTextures(1, &texture);
	gl.BindTexture(GL_TEXTURE_2D, texture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.GenerateMipmap(GL_TEXTURE_2D);
	world->texture = texture;

	free(data);

	usize block_size = WORLD_CHUNK_COUNT * CHUNK_BLOCK_COUNT;
	world->blocks = arena_alloc(arena, block_size, u16);
	world->unloaded_chunks = arena_alloc(arena, WORLD_CHUNK_COUNT, u32);
	world->unloaded_chunk_count = 0;

	v3 world_size = V3(WORLD_WIDTH, WORLD_HEIGHT, WORLD_DEPTH);
	v3 world_min = world->position;
	v3 world_max = v3_add(world_min, v3_mulf(world_size, CHUNK_SIZE));

	u16 *blocks = world->blocks;
	assert(blocks);

	struct chunk *chunk = world->chunks;
	for (f32 z = world_min.z; z < world_max.z; z += CHUNK_SIZE) {
		for (f32 y = world_min.y; y < world_max.y; y += CHUNK_SIZE) {
			for (f32 x = world_min.x; x < world_max.x; x += CHUNK_SIZE) {
				chunk->blocks = blocks;
				chunk->position = V3(x, y, z);
				blocks += CHUNK_BLOCK_COUNT;

				world_unload_chunk(world, chunk++);
			}
		}
	}

	assert(chunk - world->chunks == WORLD_CHUNK_COUNT);

	return 0;
}

void
world_update(struct world *world, v3 player_pos,
		struct render_command_buffer *cmd_buffer)
{
	struct mesh_data *mesh = &world->mesh;
	struct chunk *chunks = world->chunks;

	v3 world_size = V3(WORLD_WIDTH, WORLD_HEIGHT, WORLD_DEPTH);
	v3 player_min = v3_sub(player_pos, v3_mulf(world_size, 0.5 * CHUNK_SIZE));
	v3 player_max = v3_add(player_pos, v3_mulf(world_size, 0.5 * CHUNK_SIZE));

	struct chunk *chunk = world->chunks;
	u32 chunk_count = WORLD_CHUNK_COUNT;
	while (chunk_count-- > 0) {
		u32 is_modified = 0;
		v3 chunk_pos = chunk->position;

		if (chunk_pos.x + CHUNK_SIZE < player_min.x) {
			chunk_pos.x += WORLD_WIDTH * CHUNK_SIZE;
			is_modified = 1;
		} else if (chunk_pos.x > player_max.x) {
			chunk_pos.x -= WORLD_WIDTH * CHUNK_SIZE;
			is_modified = 1;
		}

		if (chunk_pos.y + CHUNK_SIZE < player_min.y) {
			chunk_pos.y += WORLD_WIDTH * CHUNK_SIZE;
			is_modified = 1;
		} else if (chunk_pos.y > player_max.y) {
			chunk_pos.y -= WORLD_WIDTH * CHUNK_SIZE;
			is_modified = 1;
		}

		if (chunk_pos.z + CHUNK_SIZE < player_min.z) {
			chunk_pos.z += WORLD_WIDTH * CHUNK_SIZE;
			is_modified = 1;
		} else if (chunk_pos.z > player_max.z) {
			chunk_pos.z -= WORLD_WIDTH * CHUNK_SIZE;
			is_modified = 1;
		}

		if (is_modified) {
			chunk->position = chunk_pos;
			world_unload_chunk(world, chunk);
			world_unload_chunk(world, chunk + 1);
			world_unload_chunk(world, chunk - 1);
			world_unload_chunk(world, chunk + WORLD_WIDTH);
			world_unload_chunk(world, chunk - WORLD_WIDTH);
			world_unload_chunk(world, chunk + WORLD_WIDTH * WORLD_HEIGHT);
			world_unload_chunk(world, chunk - WORLD_WIDTH * WORLD_HEIGHT);
			chunk->flags &= ~CHUNK_INITIALIZED;
		}

		chunk++;
	}

	u32 max_load = 8;
	u32 unloaded_chunk_count = world->unloaded_chunk_count;
	u32 batch_count = MIN(unloaded_chunk_count, max_load);
	u32 *unloaded_chunks = world->unloaded_chunks + unloaded_chunk_count;

	for (u32 i = 0; i < batch_count; i++) {
		unloaded_chunks--;
		struct chunk *chunk = &chunks[*unloaded_chunks];

		mesh->index_count = 0;
		mesh->vertex_count = 0;

		chunk_init(chunk);
		chunk_generate_mesh(chunk, world, mesh, cmd_buffer);
	}

	world->unloaded_chunk_count -= batch_count;
}

void
world_render(const struct world *world, struct render_command_buffer *cmd_buffer)
{
#if 1
	m4x4 transform = m4x4_id(1);
	u32 texture = world->texture;

	u32 chunk_count = WORLD_CHUNK_COUNT;
	struct chunk *chunk = world->chunks;
	while (chunk_count-- > 0) {
		u32 is_initialized = chunk->flags & CHUNK_INITIALIZED;
		if (is_initialized) {
			render_mesh(cmd_buffer, chunk->mesh, transform, texture);
		}

		chunk++;
	}
#endif
}

void
world_finish(struct world *world)
{
	gl.DeleteTextures(1, &world->texture);
}

void
world_place_block(struct world *world, f32 x, f32 y, f32 z,
	enum block_type block_type)
{
	v3 point = {{ x, y, z }};
	struct chunk *chunk = world_get_chunk(world, x, y, z);
	if (chunk) {
		v3 block_pos = world_get_block_position(world, x, y, z);
		v3i block = v3i_vec3(v3_floor(block_pos));

		u32 i = (block.z * CHUNK_SIZE + block.y) * CHUNK_SIZE + block.x;
		chunk->blocks[i] = block_type;
		world_unload_chunk(world, chunk);

		struct chunk *next_chunk = 0;
		v3 offset, next;
		for (u32 i = 0; i < 3; i++) {
			offset = V3(i == 0, i == 1, i == 2);
			next = v3_add(point, offset);
			next_chunk = world_get_chunk(world, next.x, next.y, next.z);
			world_unload_chunk(world, next_chunk);

			next = v3_sub(point, offset);
			next_chunk = world_get_chunk(world, next.x, next.y, next.z);
			world_unload_chunk(world, next_chunk);
		}
	}
}

void
world_destroy_block(struct world *world, f32 x, f32 y, f32 z)
{
	world_place_block(world, x, y, z, BLOCK_AIR);
}
