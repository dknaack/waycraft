struct memory_arena {
    u8 *data;
    usize size;
    usize used;
};

#define arena_alloc(arena, count, type) \
	((type *)arena_alloc_(arena, count * sizeof(type)))
