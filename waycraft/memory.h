struct memory_arena {
    u8 *data;
    usize size;
    usize used;
};

#define arena_alloc(arena, count, type) \
	((type *)arena_alloc_(arena, count * sizeof(type)))

void arena_init(struct memory_arena *arena, void *data, usize size);
void *arena_alloc_(struct memory_arena *arena, usize size);
