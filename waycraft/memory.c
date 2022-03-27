#include <waycraft/memory.h>

void
arena_init(struct memory_arena *arena, void *data, u64 size)
{
    arena->data = data;
    arena->size = size;
    arena->used = 0;
}

void *
arena_alloc_(struct memory_arena *arena, u64 size)
{
    u64 used = arena->used;
    assert(used + size < arena->size);

    void *ptr = arena->data + used;
    arena->used += size;

    return ptr;
}

