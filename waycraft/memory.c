void
arena_init(struct memory_arena *arena, void *data, u64 size)
{
    arena->data = data;
    arena->size = size;
    arena->used = 0;
}

void *
arena_alloc_(struct memory_arena *arena, usize size)
{
    usize used = arena->used;
    assert(used + size < arena->size);

    void *ptr = arena->data + used;
    arena->used += size;

    return ptr;
}
