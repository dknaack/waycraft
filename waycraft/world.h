#define CHUNK_COUNT_X 36
#define CHUNK_COUNT_Y 8
#define CHUNK_COUNT_Z 36
#define CHUNK_COUNT (CHUNK_COUNT_X * CHUNK_COUNT_Y * CHUNK_COUNT_Z)
#define BLOCK_COUNT_X 16
#define BLOCK_COUNT (BLOCK_COUNT_X * BLOCK_COUNT_X * BLOCK_COUNT_X)

enum chunk_state {
	CHUNK_UNLOADED,
	// NOTE: a chunk is dirty if the mesh has not been generated for it yet
	CHUNK_DIRTY,
	CHUNK_READY,
};

struct chunk {
	u32 state;
	u32 mesh;
    u16 *blocks;
	v3i coord;
};

struct world {
    struct chunk *chunks;
};
