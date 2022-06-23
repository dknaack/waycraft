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
	BLOCK_COUNT,

    BLOCK_MONITOR      = BLOCK_MONITOR_RIGHT,
    BLOCK_MONITOR_SIDE = BLOCK_MONITOR_RIGHT,
    BLOCK_MONITOR_BACK,
    BLOCK_MONITOR_FRONT,
};

static inline u32
block_is_empty(enum block_type block)
{
	return block == BLOCK_AIR || block == BLOCK_WATER;
}

static inline u32
block_is_not_water(enum block_type block)
{
	return block != BLOCK_WATER;
}
