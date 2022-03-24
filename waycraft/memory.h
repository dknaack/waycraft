#ifndef MEMORY_H
#define MEMORY_H

struct memory_arena {
    u8 *data;
    u64 size;
    u64 used;
};

#define arena_alloc(arena, count, type) ((type *)arena_alloc_(arena, count * sizeof(type)))

void arena_init(struct memory_arena *arena, void *data, u64 size);
void *arena_alloc_(struct memory_arena *arena, u64 size);

#endif /* MEMORY_H */ 
