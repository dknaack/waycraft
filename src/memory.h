#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

#define MEMORY_BLOCK_SIZE MB(4)

struct memory_block {
    u8 *ptr;
    u64 used;
    u64 size;

    struct memory_block *prev;
};

struct memory_arena {
    struct memory_block *current_block;
};

#define arena_alloc(arena, count, type) \
    (type *)arena_alloc_(arena, count * sizeof(type))

void arena_finish(struct memory_arena *arena);
void *arena_alloc_(struct memory_arena *arena, u64 size);

#endif /* MEMORY_H */ 
