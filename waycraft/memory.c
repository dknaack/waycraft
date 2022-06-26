static void
arena_init(struct memory_arena *arena, void *data, u64 size)
{
    arena->data = data;
    arena->size = size;
    arena->used = 0;
}

static void *
arena_alloc_(struct memory_arena *arena, usize size)
{
    assert(arena->used + size < arena->size);

    void *ptr = arena->data + arena->used;
    arena->used += size;

    return ptr;
}

static void
arena_suballoc(struct memory_arena *arena, usize size, struct memory_arena *result)
{
	result->data = arena_alloc_(arena, size);
	result->size = size;
}
