#define WORLD_WIDTH 8
#define WORLD_DEPTH 8
#define WORLD_HEIGHT 8
#define WORLD_CHUNK_COUNT (WORLD_WIDTH * WORLD_HEIGHT * WORLD_DEPTH)
#define CHUNK_SIZE 16
#define CHUNK_BLOCK_COUNT (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)

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
	v3i offset;
};

struct chunk {
    u16 *blocks;
	u32 mesh;
    u8 flags;
	v3 position;
};

struct memory_arena;
struct render_command_buffer;

static i32 world_init(struct world *world, struct memory_arena *arena);
static u32 world_at(struct world *world, f32 x, f32 y, f32 z);
static void world_update(struct world *world, v3 player_position,
	struct render_command_buffer *cmd_buffer);
static void world_render(const struct world *world,
	struct render_command_buffer *cmd_buffer);
static void world_finish(struct world *world);
static void world_destroy_block(struct world *world, f32 x, f32 y, f32 z);
static void world_place_block(struct world *world, f32 x, f32 y, f32 z,
	enum block_type block);
static struct chunk *world_get_chunk(struct world *world, f32 x, f32 y, f32 z);
