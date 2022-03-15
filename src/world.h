#ifndef WORLD_H
#define WORLD_H

#include "mesh.h"

#define WORLD_SIZE 16
#define WORLD_HEIGHT 8
#define CHUNK_SIZE 16

enum block_type {
    BLOCK_AIR,
    BLOCK_STONE,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_GRASS_TOP,
    BLOCK_PLANKS,
    BLOCK_OAK_LOG,
    BLOCK_OAK_LEAVES,
    BLOCK_SAND,
    BLOCK_WATER,
};

enum chunk_state {
    CHUNK_INITIALIZED = 0x1,
    CHUNK_MODIFIED    = 0x2,
};

/* 
 * NOTE: world and chunks should be initialized to zero 
 */
struct world {
    struct chunk *chunks;
    u8 *blocks;

    struct mesh mesh;
    u32 *unloaded_chunks;
    u32 unloaded_chunk_count;
    u32 unloaded_chunk_start;
    u32 chunk_count;
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
    u8 flags;
};

struct memory_arena;

i32 world_init(struct world *world, struct memory_arena *arena);
u32 world_at(struct world *world, f32 x, f32 y, f32 z);
void world_update(struct world *world, vec3 player_position);
void world_render(const struct world *world);
void world_finish(struct world *world);
void world_destroy_block(struct world *world, f32 x, f32 y, f32 z);
void world_place_block(struct world *world, f32 x, f32 y, f32 z, 
                       enum block_type block);

u32 block_is_empty(enum block_type block);

#endif /* WORLD_H */ 
