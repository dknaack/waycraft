#ifndef WORLD_H
#define WORLD_H

#include <waycraft/renderer.h>

#define WORLD_WIDTH 32
#define WORLD_DEPTH 32
#define WORLD_HEIGHT 8
#define WORLD_CHUNK_COUNT (WORLD_WIDTH * WORLD_HEIGHT * WORLD_DEPTH)
#define CHUNK_SIZE 16
#define CHUNK_BLOCK_COUNT (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)

// NOTE: block with different directions should have the block facing to the
// right first, then the block facing left next, and then the block facing up
// and then down. The last two block should be the blocks facing forward and
// backward.
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
    BLOCK_MONITOR_RIGHT,
    BLOCK_MONITOR_LEFT,
    BLOCK_MONITOR_UP,
    BLOCK_MONITOR_DOWN,
    BLOCK_MONITOR_FORWARD,
    BLOCK_MONITOR_BACKWARD,
    BLOCK_WINDOW,

    BLOCK_MONITOR      = BLOCK_MONITOR_RIGHT,
    BLOCK_MONITOR_SIDE = BLOCK_MONITOR_RIGHT,
    BLOCK_MONITOR_BACK,
    BLOCK_MONITOR_FRONT,
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
    u16 *blocks;

    struct mesh_data mesh;
    u32 *unloaded_chunks;
    u32 unloaded_chunk_count;
    u32 unloaded_chunk_start;
    u32 texture;
    // NOTE: bottom left corner of the world, such that (0, 0, 0) relative to
    // world is the first chunk (0, 0, 0)
    v3 position;
};

struct chunk {
    u16 *blocks;
	u32 mesh;
    u8 flags;
};

struct memory_arena;
struct render_command_buffer;

i32 world_init(struct world *world, struct memory_arena *arena);
u32 world_at(struct world *world, f32 x, f32 y, f32 z);
void world_update(struct world *world, v3 player_position,
	struct render_command_buffer *cmd_buffer);
void world_render(const struct world *world,
	struct render_command_buffer *cmd_buffer);
void world_finish(struct world *world);
void world_destroy_block(struct world *world, f32 x, f32 y, f32 z);
void world_place_block(struct world *world, f32 x, f32 y, f32 z,
                       enum block_type block);

u32 block_is_empty(enum block_type block);

#endif /* WORLD_H */
