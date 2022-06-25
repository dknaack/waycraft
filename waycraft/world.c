static inline u32
block_index(u32 x, u32 y, u32 z)
{
	assert(x < BLOCK_COUNT_X);
	assert(y < BLOCK_COUNT_X);
	assert(z < BLOCK_COUNT_X);

	return (z * BLOCK_COUNT_X + y) * BLOCK_COUNT_X + x;
}

static inline u32
chunk_index(u32 x, u32 y, u32 z)
{
	assert(x < CHUNK_COUNT_X);
	assert(y < CHUNK_COUNT_Y);
	assert(z < CHUNK_COUNT_Z);

	return (z * CHUNK_COUNT_Y + y) * CHUNK_COUNT_X + x;
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
	v3 offset = V3(chunk->coord.x, chunk->coord.y, chunk->coord.z);
	v3 result = mulf(offset, BLOCK_COUNT_X);

	return result;
}

static inline f32
normalize_height(f32 value)
{
	f32 x = value + 0.8;

	if (x < 1.0) {
		x *= MIN(MAX(x, 0.6), 1.0);
	}

	return x;
}

static u32
chunk_at(const struct chunk *chunk, i32 x, i32 y, i32 z)
{
	u32 result = 0;
	u32 is_inside_chunk = (0 <= x && x < BLOCK_COUNT_X)
		&& (0 <= y && y < BLOCK_COUNT_X)
		&& (0 <= z && z < BLOCK_COUNT_X);
	if (is_inside_chunk) {
		u32 index = (z * BLOCK_COUNT_X + y) * BLOCK_COUNT_X + x;

		result = chunk->blocks[index];
	}

	return result;
}

static void
world_unload_chunk(struct world *world, struct chunk *chunk)
{
	struct chunk *first_chunk = world->chunks;
	struct chunk *last_chunk = first_chunk + CHUNK_COUNT;
	u32 is_valid_chunk = chunk && first_chunk <= chunk && chunk < last_chunk;

	if (is_valid_chunk) {
		chunk->state = CHUNK_UNLOADED;
	}
}

// NOTE: may generate chunk if it has not been initialized
static struct chunk *
world_get_chunk(struct world *world, f32 x, f32 y, f32 z)
{
	struct chunk *result = 0;

	v3i chunk_coord = {0};
	chunk_coord.x = floor(x / BLOCK_COUNT_X);
	chunk_coord.y = floor(y / BLOCK_COUNT_X);
	chunk_coord.z = floor(z / BLOCK_COUNT_X);

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

		v3 chunk_pos = chunk_get_pos(chunk);
		debug_set_color(1, 0, 1);
		debug_cube(chunk_pos, add(chunk_pos, V3(BLOCK_COUNT_X, BLOCK_COUNT_X, BLOCK_COUNT_X)));
	}

	return result;
}

static v3
world_get_block_pos(const struct world *world, f32 x, f32 y, f32 z)
{
	v3 block_pos = v3_modf(V3(x + 1e-3, y + 1e-3, z + 1e-3), BLOCK_COUNT_X);

	if (block_pos.x < 0) {
		block_pos.x += BLOCK_COUNT_X;
	}

	if (block_pos.y < 0) {
		block_pos.y += BLOCK_COUNT_X;
	}

	if (block_pos.z < 0) {
		block_pos.z += BLOCK_COUNT_X;
	}

	return block_pos;
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

static void
block_generate_mesh(enum block_type block, i32 x, i32 y, i32 z,
	struct world *world, struct render_command_buffer *mesh)
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

	u32 texture = TEXTURE_BLOCK_ATLAS;

	block_texcoords_bottom(block, uv);
	if (is_empty(world_at(world, x, y - 1, z))) {
		render_quad(mesh, pos[7], pos[6], pos[3], pos[2],
			uv[0], uv[1], uv[2], uv[3], texture);
	}

	block_texcoords_top(block, uv);
	if (is_empty(world_at(world, x, y + 1, z))) {
		render_quad(mesh, pos[4], pos[5], pos[0], pos[1],
			uv[0], uv[1], uv[2], uv[3], texture);
	}

	block_texcoords_right(block, uv);
	if (is_empty(world_at(world, x + 1, y, z))) {
		render_quad(mesh, pos[4], pos[0], pos[6], pos[2],
			uv[0], uv[1], uv[2], uv[3], texture);
	}

	block_texcoords_left(block, uv);
	if (is_empty(world_at(world, x - 1, y, z))) {
		render_quad(mesh, pos[1], pos[5], pos[3], pos[7],
			uv[0], uv[1], uv[2], uv[3], texture);
	}

	block_texcoords_front(block, uv);
	if (is_empty(world_at(world, x, y, z + 1))) {
		render_quad(mesh, pos[0], pos[1], pos[2], pos[3],
			uv[0], uv[1], uv[2], uv[3], texture);
	}

	block_texcoords_back(block, uv);
	if (is_empty(world_at(world, x, y, z - 1))) {
		render_quad(mesh, pos[5], pos[4], pos[7], pos[6],
			uv[0], uv[1], uv[2], uv[3], texture);
	}
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

	// NOTE: initialize the temporary command buffer for generating chunk meshes
	struct render_command_buffer *mesh = &world->tmp_cmd_buffer;
	u32 max_vertex_count = CHUNK_COUNT * 64;
	u32 max_index_count = CHUNK_COUNT * 128;
	render_command_buffer_init(mesh, arena, KB(1),
		max_vertex_count, max_index_count);

	return 0;
}

static void
world_load_chunk(struct world *world, struct chunk *chunk,
		struct render_command_buffer *mesh, struct renderer *renderer)
{
	v3 chunk_pos = chunk_get_pos(chunk);
	u16 *blocks = chunk->blocks;

	if (chunk->state == CHUNK_UNLOADED) {
		f32 noise_size = 0.1f;

		memset(blocks, 0, BLOCK_COUNT * sizeof(*blocks));
		for (i32 z = 0; z < BLOCK_COUNT_X; z++) {
			for (i32 x = 0; x < BLOCK_COUNT_X; x++) {
				f32 nx = noise_size * (chunk_pos.x + x + 0.5);
				f32 nz = noise_size * (chunk_pos.z + z + 0.5);
				i32 height = normalize_height(noise_layered_2d(nx, nz)) * BLOCK_COUNT_X - chunk_pos.y;

				i32 ymax = MAX(0, MIN(BLOCK_COUNT_X, height));
				for (i32 y = 0; y < ymax; y++) {
					u32 i = block_index(x, y, z);

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
					for (i32 y = ymax; y < BLOCK_COUNT_X; y++) {
						u32 i = (z * BLOCK_COUNT_X + y) * BLOCK_COUNT_X + x;
						blocks[i] = BLOCK_WATER;
					}
				}
			}
		}
	}

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

	u32 texture = TEXTURE_BLOCK_ATLAS;
	assert(blocks);

	for (u32 i = 0; i < BLOCK_COUNT_X * BLOCK_COUNT_X * BLOCK_COUNT_X; i++) {
		u32 block = blocks[i];
		if (block == BLOCK_AIR) {
			continue;
		}

		i32 x = i % BLOCK_COUNT_X;
		i32 y = i / BLOCK_COUNT_X % BLOCK_COUNT_X;
		i32 z = i / BLOCK_COUNT_X / BLOCK_COUNT_X % BLOCK_COUNT_X;

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

		u16 block_right = x + 1 >= BLOCK_COUNT_X ?
			blocks_right[block_index(x + 1 - BLOCK_COUNT_X, y, z)] :
			blocks[block_index(x + 1, y, z)];

		u16 block_left = x - 1 < 0 ?
			blocks_left[block_index(x - 1 + BLOCK_COUNT_X, y, z)] :
			blocks[block_index(x - 1, y, z)];

		u16 block_top = y + 1 >= BLOCK_COUNT_X ?
			blocks_top[block_index(x, y + 1 - BLOCK_COUNT_X, z)] :
			blocks[block_index(x, y + 1, z)];

		u16 block_bottom = y - 1 < 0 ?
			blocks_bottom[block_index(x, y - 1 + BLOCK_COUNT_X, z)] :
			blocks[block_index(x, y - 1, z)];

		u16 block_front = z + 1 >= BLOCK_COUNT_X ?
			blocks_front[block_index(x, y, z + 1 - BLOCK_COUNT_X)] :
			blocks[block_index(x, y, z + 1)];

		u16 block_back = z - 1 < 0 ?
			blocks_back[block_index(x, y, z - 1 + BLOCK_COUNT_X)] :
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
			render_quad(mesh, pos[4], pos[0], pos[6], pos[2],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (is_empty(block_left)) {
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

		if (is_empty(block_front)) {
			block_texcoords_front(block, uv);
			render_quad(mesh, pos[0], pos[1], pos[2], pos[3],
				uv[0], uv[1], uv[2], uv[3], texture);
		}

		if (is_empty(block_back)) {
			block_texcoords_back(block, uv);
			render_quad(mesh, pos[5], pos[4], pos[7], pos[6],
				uv[0], uv[1], uv[2], uv[3], texture);
		}
	}

	renderer_build_command_buffer(renderer, mesh, &chunk->mesh);
	chunk->state = CHUNK_READY;
}

static void
world_update(struct world *world, v3 player_pos, struct renderer *renderer,
		struct render_command_buffer *cmd_buffer)
{
	struct render_command_buffer *tmp_buffer = &world->tmp_cmd_buffer;
	struct box player_bounds;

	v3 world_size = V3(CHUNK_COUNT_X, CHUNK_COUNT_Y, CHUNK_COUNT_Z);
	player_bounds.min = sub(player_pos, mulf(world_size, 0.5 * BLOCK_COUNT_X));
	player_bounds.max = add(player_pos, mulf(world_size, 0.5 * BLOCK_COUNT_X));

	// NOTE: unload any chunks that are outside the player bounds
	struct chunk *chunk = world->chunks;
	for (u32 i = 0; i < CHUNK_COUNT; i++) {
		v3 chunk_pos = chunk_get_pos(&world->chunks[i]);
		if (!box_contains_point(player_bounds, chunk_pos)) {
			chunk->state = CHUNK_UNLOADED;
		}
	}

	// NOTE: load any chunks that are unloaded or dirty
	u32 max_load_count = 8;
	v3i player_offset;
	player_offset.x = floor(player_pos.x / BLOCK_COUNT_X - CHUNK_COUNT_X / 2.0f);
	player_offset.y = floor(player_pos.y / BLOCK_COUNT_X - CHUNK_COUNT_Y / 2.0f);
	player_offset.z = floor(player_pos.z / BLOCK_COUNT_X - CHUNK_COUNT_Z / 2.0f);

	for (u32 i = 0; max_load_count > 0 && i < CHUNK_COUNT; i++) {
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

		if (!v3i_equals(chunk->coord, chunk_coord)) {
			chunk->state = CHUNK_UNLOADED;
			chunk->coord = chunk_coord;
		}

		if (chunk->state != CHUNK_READY) {
			tmp_buffer->push_buffer_size = 0;
			tmp_buffer->index_count = 0;
			tmp_buffer->vertex_count = 0;

			chunk->coord = chunk_coord;
			world_load_chunk(world, chunk, tmp_buffer, renderer);
			max_load_count--;
		}

		assert(v3i_equals(chunk->coord, chunk_coord));
	}

	// NOTE: draw the loaded chunks
	m4x4 transform = m4x4_id(1);
	u32 texture = TEXTURE_BLOCK_ATLAS;
	for (u32 i = 0; i < CHUNK_COUNT; i++) {
		if (world->chunks[i].state == CHUNK_READY) {
			render_mesh(cmd_buffer, world->chunks[i].mesh, transform, texture);
		}

		chunk++;
	}
}

static void
world_place_block(struct world *world, f32 x, f32 y, f32 z,
		enum block_type block_type)
{
	v3 point = V3(x, y, z);

	struct chunk *chunk = world_get_chunk(world, x, y, z);
	if (chunk) {
		v3 block_pos = world_get_block_pos(world, x, y, z);
		v3i block = v3i_vec3(v3_floor(block_pos));

		u32 i = block_index(block.x, block.y, block.z);
		chunk->blocks[i] = block_type;
		chunk->state = CHUNK_DIRTY;

		for (u32 i = 0; i < 3; i++) {
			v3 offset = V3(i == 0, i == 1, i == 2);
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
