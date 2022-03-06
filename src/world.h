#ifndef WORLD_H
#define WORLD_H

#define CHUNK_SIZE 16
#define BLOCK_SIZE 0.1

/* 
 * NOTE: world and chunks should be initialized to zero 
 */
struct world {
    struct chunk *chunks;
    u8 *blocks;

    struct mesh mesh;
    u32 loaded_chunk_count;
    u32 width;
    u32 height;
    u32 depth;
    u32 texture;
    // NOTE: bottom left corner of the world, such that (0, 0, 0) relative to
    // world is the first chunk (0, 0, 0)
    vec3 position;
};

struct chunk {
    u8 *blocks;
    u32 vao, vbo, ebo;
    u32 index_count;
};

struct memory_arena;

i32 world_init(struct world *world, struct memory_arena *arena);
u32 world_at(const struct world *world, f32 x, f32 y, f32 z);
void world_update(struct world *world, vec3 player_position);
void world_render(const struct world *world);
void world_finish(struct world *world);

#endif /* WORLD_H */ 
