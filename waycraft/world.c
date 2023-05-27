static inline u32
block_index(u32 x, u32 y, u32 z)
{
	assert(x < BLOCK_COUNT_X);
	assert(y < BLOCK_COUNT_Y);
	assert(z < BLOCK_COUNT_Z);

	return (z * BLOCK_COUNT_Y + y) * BLOCK_COUNT_X + x;
}

static inline u32
chunk_index_pack(v3i coord)
{
	u32 index = (coord.z * CHUNK_COUNT_Y + coord.y) * CHUNK_COUNT_X + coord.x;

	assert(0 <= coord.x && coord.x < CHUNK_COUNT_X);
	assert(0 <= coord.y && coord.y < CHUNK_COUNT_Y);
	assert(0 <= coord.z && coord.z < CHUNK_COUNT_Z);

	return index;
}

static inline v3i
chunk_index_unpack(u32 index)
{
	assert(index < CHUNK_COUNT);

	v3i result;
	result.x = index % CHUNK_COUNT_X;
	result.y = index / CHUNK_COUNT_X % CHUNK_COUNT_Y;
	result.z = index / CHUNK_COUNT_X / CHUNK_COUNT_Y;

	return result;
}

static inline v3
chunk_get_pos(struct chunk *chunk)
{
	v3 offset = v3(chunk->coord.x, chunk->coord.y, chunk->coord.z);
	v3 block_count = v3(BLOCK_COUNT_X, BLOCK_COUNT_Y, BLOCK_COUNT_Z);
	v3 result = mul(offset, block_count);

	return result;
}

static inline f32
normalize_height(f32 value)
{
	return 2.f * value;
}

static u32
chunk_at(const struct chunk *chunk, i32 x, i32 y, i32 z)
{
	u32 result = 0;
	u32 is_inside_chunk = (0 <= x && x < BLOCK_COUNT_X)
		&& (0 <= y && y < BLOCK_COUNT_Y)
		&& (0 <= z && z < BLOCK_COUNT_Z);
	if (is_inside_chunk) {
		u32 index = (z * BLOCK_COUNT_Y + y) * BLOCK_COUNT_X + x;

		result = chunk->blocks[index];
	}

	return result;
}

// NOTE: may generate chunk if it has not been initialized
static struct chunk *
world_get_chunk(struct world *world, f32 x, f32 y, f32 z)
{
	struct chunk *result = 0;

	v3i chunk_coord = {0};
	chunk_coord.x = floor(x / BLOCK_COUNT_X);
	chunk_coord.y = floor(y / BLOCK_COUNT_Y);
	chunk_coord.z = floor(z / BLOCK_COUNT_Z);

	v3i chunk_rel_coord = chunk_coord;
	chunk_rel_coord.x %= CHUNK_COUNT_X;
	if (chunk_rel_coord.x < 0) {
		chunk_rel_coord.x += CHUNK_COUNT_X;
	}

	chunk_rel_coord.y %= CHUNK_COUNT_Y;
	if (chunk_rel_coord.y < 0) {
		chunk_rel_coord.y += CHUNK_COUNT_Y;
	}

	chunk_rel_coord.z %= CHUNK_COUNT_Z;
	if (chunk_rel_coord.z < 0) {
		chunk_rel_coord.z += CHUNK_COUNT_Z;
	}

	assert(0 <= chunk_rel_coord.x && chunk_rel_coord.x < CHUNK_COUNT_X);
	assert(0 <= chunk_rel_coord.y && chunk_rel_coord.y < CHUNK_COUNT_Y);
	assert(0 <= chunk_rel_coord.z && chunk_rel_coord.z < CHUNK_COUNT_Z);

	u32 chunk_index = chunk_index_pack(chunk_rel_coord);
	struct chunk *chunk = &world->chunks[chunk_index];
	if (v3i_equals(chunk->coord, chunk_coord)) {
		result = chunk;
	}

	return result;
}

static v3
world_get_block_pos(const struct world *world, f32 x, f32 y, f32 z)
{
	v3 result = {0};

	result.x = fmodf(x, BLOCK_COUNT_X);
	if (result.x < 0) {
		result.x += BLOCK_COUNT_X;
	}

	result.y = fmodf(y, BLOCK_COUNT_Y);
	if (result.y < 0) {
		result.y += BLOCK_COUNT_Y;
	}

	result.z = fmodf(z, BLOCK_COUNT_Z);
	if (result.z < 0) {
		result.z += BLOCK_COUNT_Z;
	}

	return result;
}

static u32
world_at(struct world *world, f32 x, f32 y, f32 z)
{
	u32 result = 0;

	struct chunk *chunk = world_get_chunk(world, x, y, z);

	if (chunk) {
		v3 block_pos = world_get_block_pos(world, x, y, z);

		assert(block_pos.x >= 0);
		assert(block_pos.y >= 0);
		assert(block_pos.z >= 0);

		result = chunk_at(chunk, block_pos.x, block_pos.y, block_pos.z);
	}

	return result;
}

static i32
world_init(struct world *world, struct memory_arena *arena)
{
	// NOTE: allocate memory for the chunks and blocks
	world->chunks = arena_alloc(arena, CHUNK_COUNT, struct chunk);
	usize block_count = CHUNK_COUNT * BLOCK_COUNT;
	u16 *blocks = arena_alloc(arena, block_count, u16);

	for (u32 i = 0; i < CHUNK_COUNT; i++) {
		world->chunks[i].blocks = blocks + i * BLOCK_COUNT;
	}

	return 0;
}

static void
world_load_chunk(struct world *world, struct chunk *chunk,
		struct render_cmdbuf *mesh, struct renderer *renderer)
{
	struct game_assets *assets = mesh->assets;

	v3 chunk_pos = chunk_get_pos(chunk);
	u16 *blocks = chunk->blocks;
	if (chunk->state == CHUNK_UNLOADED) {
		/*
		 * NOTE: terrain generation
		 */

		f32 noise_size = 0.02f;
		f32 low_noise_size = 0.0005f;

		i32 height[BLOCK_COUNT_X][BLOCK_COUNT_Z];
		for (i32 z = 0; z < BLOCK_COUNT_Z; z++) {
			for (i32 x = 0; x < BLOCK_COUNT_X; x++) {
				v3 point = {0};
				point.x = chunk_pos.x + x;
				point.z = chunk_pos.z + z;

				v3 high_point = mulf(point, noise_size);
				v3 low_point = mulf(point, low_noise_size);
				f32 value = 2.0f * perlin_noise_layered(high_point, 8, 0.5f) - 1.0f;
				f32 low_value = 2.0f * perlin_noise_layered(low_point, 8, 0.8f) - 1.0f;
				height[x][z] = 8.0f * (value + 0.2f) * (2.0f * low_value + 0.3f) * BLOCK_COUNT_X - chunk_pos.y;

				i32 stone_max = CLAMP(height[x][z] - 3, 0, BLOCK_COUNT_Y);
				i32 dirt_max = CLAMP(height[x][z] - 1, 0, BLOCK_COUNT_Y);
				i32 grass_max = CLAMP(height[x][z], 0, BLOCK_COUNT_Y);

				i32 y = 0;
				while (y < stone_max) {
					u32 i = block_index(x, y++, z);
					blocks[i] = BLOCK_STONE;
				}

				while (y < dirt_max) {
					f32 world_y = chunk_pos.y + y;
					u32 i = block_index(x, y++, z);
					blocks[i] = world_y < 2 ? BLOCK_SAND : BLOCK_DIRT;
				}

				while (y < grass_max) {
					f32 world_y = chunk_pos.y + y;
					u32 i = block_index(x, y++, z);
					blocks[i] = world_y < 2 ? BLOCK_SAND : BLOCK_GRASS;
				}

				enum block_type filler = BLOCK_AIR;
				if (chunk_pos.y < 0) {
					filler = BLOCK_WATER;
				}

				while (y < BLOCK_COUNT_Y) {
					u32 i = block_index(x, y++, z);
					blocks[i] = filler;
				}
			}
		}

		/*
		 * NOTE: tree generation
		 */

		v3i coord = chunk->coord;
		coord.y = 0;
#if 1
		u32 seed = djb2(&coord, sizeof(coord));
#else
		coord.x *= 0x328401efa;
		coord.z ^= coord.x << 16 | coord.x >> 16;
		coord.z *= 0x3820afb8d;
		coord.x ^= coord.z << 16 | coord.z >> 16;
		coord.x *= 0x328401efa;
		u32 seed = coord.x;
#endif

		f32 tree_noise_size = 100.f;
		v3 density_point = mulf(chunk_pos, tree_noise_size);
		density_point.y = 0;

		f32 density = 9.f * perlin_noise(density_point) - 2.f;
		u32 tree_count = CLAMP(density, 0, 7);
		for (u32 i = 0; i < tree_count; i++) {
			u32 x = xorshift32(&seed) % BLOCK_COUNT_X;
			u32 z = xorshift32(&seed) % BLOCK_COUNT_Z;
			u32 tree_height = (xorshift32(&seed) & 7) + 2;

			if (height[x][z] + chunk_pos.y > 2) {
				for (i32 y = height[x][z]; y < BLOCK_COUNT_Y && 2 <= y &&
						y < height[x][z] + tree_height; y++) {
					u32 i = block_index(x, y, z);
					blocks[i] = BLOCK_OAK_LOG;
				}
			}
		}
	}

	/*
	 * NOTE: mesh generation
	 */

	// TODO: fix bug for top faces
	u16 blocks_empty[BLOCK_COUNT] = {0};

#if 0
	struct chunk *chunk_first = world->chunks;
	struct chunk *chunk_last = world->chunks + CHUNK_COUNT;

	struct chunk *chunk_right  = chunk + 1;
	struct chunk *chunk_left   = chunk - 1;
	struct chunk *chunk_top    = chunk + CHUNK_COUNT_X;
	struct chunk *chunk_bottom = chunk - CHUNK_COUNT_X;
	struct chunk *chunk_front  = chunk + CHUNK_COUNT_X * CHUNK_COUNT_Y;
	struct chunk *chunk_back   = chunk - CHUNK_COUNT_X * CHUNK_COUNT_Y;
#endif

	u16 *blocks_right  = blocks_empty;
	u16 *blocks_left   = blocks_empty;
	u16 *blocks_top    = blocks_empty;
	u16 *blocks_bottom = blocks_empty;
	u16 *blocks_front  = blocks_empty;
	u16 *blocks_back   = blocks_empty;

#if 0
	if (chunk_right < chunk_last && chunk_right->coord.x > chunk->coord.x) {
		blocks_right = chunk_right->blocks;
	}

	if (chunk_left >= chunk_first && chunk_left->coord.x < chunk->coord.x) {
		blocks_left = chunk_left->blocks;
	}

	if (chunk_top < chunk_last && chunk_top->coord.y > chunk->coord.y) {
		blocks_top = chunk_top->blocks;
	}

	if (chunk_bottom >= chunk_first && chunk_bottom->coord.y < chunk->coord.y) {
		blocks_bottom = chunk_bottom->blocks;
	}

	if (chunk_front < chunk_last && chunk_front->coord.z > chunk->coord.z) {
		blocks_front = chunk_front->blocks;
	}

	if (chunk_back >= chunk_first && chunk_back->coord.z < chunk->coord.z) {
		blocks_back = chunk_back->blocks;
	}
#endif

	struct texture_id texture = get_texture(assets, TEXTURE_BLOCK_ATLAS).id;
	assert(blocks);

	for (u32 i = 0; i < BLOCK_COUNT; i++) {
		u32 block = blocks[i];
		if (block == BLOCK_AIR) {
			continue;
		}

		i32 x = i % BLOCK_COUNT_X;
		i32 y = i / BLOCK_COUNT_X % BLOCK_COUNT_Y;
		i32 z = i / BLOCK_COUNT_X / BLOCK_COUNT_Y % BLOCK_COUNT_Z;

		v3 pos[8];
		v2 uv[4];

		pos[0] = v3_add(chunk_pos, v3(x + 0.5, y + 0.5, z + 0.5));
		pos[1] = v3_add(chunk_pos, v3(x - 0.5, y + 0.5, z + 0.5));
		pos[2] = v3_add(chunk_pos, v3(x + 0.5, y - 0.5, z + 0.5));
		pos[3] = v3_add(chunk_pos, v3(x - 0.5, y - 0.5, z + 0.5));

		pos[4] = v3_add(chunk_pos, v3(x + 0.5, y + 0.5, z - 0.5));
		pos[5] = v3_add(chunk_pos, v3(x - 0.5, y + 0.5, z - 0.5));
		pos[6] = v3_add(chunk_pos, v3(x + 0.5, y - 0.5, z - 0.5));
		pos[7] = v3_add(chunk_pos, v3(x - 0.5, y - 0.5, z - 0.5));

		u32 (*is_empty)(enum block_type block) = block_is_empty;

		u16 block_right = x + 1 >= BLOCK_COUNT_X ?
			blocks_right[block_index(x + 1 - BLOCK_COUNT_X, y, z)] :
			blocks[block_index(x + 1, y, z)];

		u16 block_left = x - 1 < 0 ?
			blocks_left[block_index(x - 1 + BLOCK_COUNT_X, y, z)] :
			blocks[block_index(x - 1, y, z)];

		u16 block_top = y + 1 >= BLOCK_COUNT_Y ?
			blocks_top[block_index(x, y + 1 - BLOCK_COUNT_Y, z)] :
			blocks[block_index(x, y + 1, z)];

		u16 block_bottom = y - 1 < 0 ?
			blocks_bottom[block_index(x, y - 1 + BLOCK_COUNT_Y, z)] :
			blocks[block_index(x, y - 1, z)];

		u16 block_front = z + 1 >= BLOCK_COUNT_Z ?
			blocks_front[block_index(x, y, z + 1 - BLOCK_COUNT_Z)] :
			blocks[block_index(x, y, z + 1)];

		u16 block_back = z - 1 < 0 ?
			blocks_back[block_index(x, y, z - 1 + BLOCK_COUNT_Z)] :
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

		if (block != BLOCK_WATER && is_empty(block_right)) {
			block_texcoords_right(block, uv);
			render_quad(mesh, pos[4], pos[0], pos[6], pos[2],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (block != BLOCK_WATER && is_empty(block_left)) {
			block_texcoords_left(block, uv);
			render_quad(mesh, pos[1], pos[5], pos[3], pos[7],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (is_empty(block_top)) {
			block_texcoords_top(block, uv);
			render_quad(mesh, pos[4], pos[5], pos[0], pos[1],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (is_empty(block_bottom)) {
			block_texcoords_bottom(block, uv);
			render_quad(mesh, pos[7], pos[6], pos[3], pos[2],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (block != BLOCK_WATER && is_empty(block_front)) {
			block_texcoords_front(block, uv);
			render_quad(mesh, pos[0], pos[1], pos[2], pos[3],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (block != BLOCK_WATER && is_empty(block_back)) {
			block_texcoords_back(block, uv);
			render_quad(mesh, pos[5], pos[4], pos[7], pos[6],
				uv[0], uv[1], uv[2], uv[3], texture);
		}
	}

	renderer_build_command_buffer(renderer, mesh, &chunk->mesh);
	chunk->state = CHUNK_READY;
}

static void
world_update(struct world *world, v3 player_pos, v3 player_dir,
		struct renderer *renderer, struct render_cmdbuf *cmd_buffer,
		struct memory_arena *frame_arena, struct game_assets *assets)
{
	u32 max_vertex_count = BLOCK_COUNT * 4 * 6;
	u32 max_index_count = BLOCK_COUNT * 6 * 6;
	struct render_cmdbuf tmp_buffer = render_cmdbuf_init(frame_arena,
	    KB(1), max_vertex_count, max_index_count);
	tmp_buffer.assets = cmd_buffer->assets;

	struct box player_bounds = {0};
	v3 target = add(player_pos, mulf(player_dir, 3.0f * BLOCK_COUNT_X));
	v3 world_size = v3(CHUNK_COUNT_X, CHUNK_COUNT_Y, CHUNK_COUNT_Z);
	player_bounds.min = sub(target, mulf(world_size, 0.6 * BLOCK_COUNT_X));
	player_bounds.max = add(target, mulf(world_size, 0.6 * BLOCK_COUNT_X));

	// NOTE: unload any chunks that are outside the player bounds
	for (u32 i = 0; i < CHUNK_COUNT; i++) {
		v3 chunk_pos = chunk_get_pos(&world->chunks[i]);
		if (!box_contains_point(player_bounds, chunk_pos)) {
			world->chunks[i].state = CHUNK_UNLOADED;
		}
	}

	// NOTE: load any chunks that are unloaded or dirty
	v3i player_offset;
	player_offset.x = floor(target.x / BLOCK_COUNT_X - CHUNK_COUNT_X / 2.0f);
	player_offset.y = floor(target.y / BLOCK_COUNT_X - CHUNK_COUNT_Y / 2.0f);
	player_offset.z = floor(target.z / BLOCK_COUNT_X - CHUNK_COUNT_Z / 2.0f);

	struct chunk *chunks_to_load[MAX_LOAD_PER_FRAME] = {0};
	f32 distances[MAX_LOAD_PER_FRAME];
	for (u32 i = 0; i < MAX_LOAD_PER_FRAME; i++) {
		distances[i] = F32_INF;
	}

	for (u32 i = 0; i < CHUNK_COUNT; i++) {
		v3i chunk_offset = chunk_index_unpack(i);
		v3i chunk_coord = add(player_offset, chunk_offset);
		v3i chunk_rel_coord = chunk_coord;

		chunk_rel_coord.x %= CHUNK_COUNT_X;
		if (chunk_rel_coord.x < 0) {
			chunk_rel_coord.x += CHUNK_COUNT_X;
		}

		chunk_rel_coord.y %= CHUNK_COUNT_Y;
		if (chunk_rel_coord.y < 0) {
			chunk_rel_coord.y += CHUNK_COUNT_Y;
		}

		chunk_rel_coord.z %= CHUNK_COUNT_Z;
		if (chunk_rel_coord.z < 0) {
			chunk_rel_coord.z += CHUNK_COUNT_Z;
		}

		u32 chunk_index = chunk_index_pack(chunk_rel_coord);
		struct chunk *chunk = &world->chunks[chunk_index];

		if (chunk->state != CHUNK_READY) {
			chunk->coord = chunk_coord;
			v3 chunk_pos = chunk_get_pos(chunk);
			v3 chunk_half_dim = mulf(v3(BLOCK_COUNT_X, BLOCK_COUNT_Y, BLOCK_COUNT_Z), 0.5f);
			chunk_pos = add(chunk_pos, chunk_half_dim);

			// NOTE: find the farthest chunk and if it the current chunk is
			// closer load the current chunk instead
			f32 distance = length_sq(sub(target, chunk_pos));
			f32 max_distance = distance;
			u32 farthest_chunk_index = LENGTH(chunks_to_load);
			for (u32 i = 0; i < LENGTH(chunks_to_load); i++) {
				if (!chunks_to_load[i]) {
					farthest_chunk_index = i;
					break;
				}

				if (max_distance < distances[i]) {
					max_distance = distances[i];
					farthest_chunk_index = i;
				}
			}

			if (farthest_chunk_index < LENGTH(chunks_to_load)) {
				chunks_to_load[farthest_chunk_index] = chunk;
				distances[farthest_chunk_index] = distance;
			}
		}
	}

	for (u32 i = 0; i < LENGTH(chunks_to_load) && chunks_to_load[i]; i++) {
		assert(chunks_to_load[i]->state != CHUNK_READY);

		tmp_buffer.push_buffer_size = 0;
		tmp_buffer.index_count = 0;
		tmp_buffer.vertex_count = 0;
		tmp_buffer.current_quads = 0;
		world_load_chunk(world, chunks_to_load[i], &tmp_buffer, renderer);
	}

	// NOTE: draw the loaded chunks
	m4x4 transform = m4x4_id(1);
	struct texture_id texture = get_texture(assets, TEXTURE_BLOCK_ATLAS).id;
	for (u32 i = 0; i < CHUNK_COUNT; i++) {
		if (world->chunks[i].mesh != 0) {
			render_mesh(cmd_buffer, world->chunks[i].mesh, transform, texture);
		}
	}
}

static void
world_place_block(struct world *world, f32 x, f32 y, f32 z,
		enum block_type block_type)
{
	v3 point = v3(x, y, z);

	struct chunk *chunk = world_get_chunk(world, x, y, z);
	if (chunk) {
		v3 block_pos = world_get_block_pos(world, x, y, z);
		v3i block = v3i_vec3(v3_floor(block_pos));

		u32 i = block_index(block.x, block.y, block.z);
		chunk->blocks[i] = block_type;
		chunk->state = CHUNK_DIRTY;

		for (u32 i = 0; i < 3; i++) {
			v3 offset = v3(i == 0, i == 1, i == 2);
			v3 next = add(point, offset);
			struct chunk *next_chunk = world_get_chunk(world, next.x, next.y, next.z);
			next_chunk->state = CHUNK_DIRTY;

			next = v3_sub(point, offset);
			next_chunk = world_get_chunk(world, next.x, next.y, next.z);
			next_chunk->state = CHUNK_DIRTY;
		}
	}
}

static void
world_destroy_block(struct world *world, f32 x, f32 y, f32 z)
{
	world_place_block(world, x, y, z, BLOCK_AIR);
}
