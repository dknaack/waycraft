#include <stdlib.h>

#include "memory.h"

static struct memory_block *
block_create(u64 size)
{
    size += sizeof(struct memory_block);
    struct memory_block *block = calloc(size, 1);
    block->ptr = (u8 *)(block + 1);
    block->size = size;
    block->used = 0;
    return block;
}

void *
arena_alloc_(struct memory_arena *arena, u64 size)
{
    struct memory_block *block = arena->current_block;

    if (!block || block->used + size >= block->size) {
        struct memory_block *next = block_create(MAX(size, MEMORY_BLOCK_SIZE));
        next->prev = block;
        arena->current_block = block = next;
    }

    u8 *ptr = block->ptr + block->used;
    block->used += size;
    return ptr;
}

void
arena_finish(struct memory_arena *arena)
{
    struct memory_block *block = arena->current_block;

    while (block) {
        struct memory_block *prev = block->prev;
        free(block);
        block = prev;
    }
}
