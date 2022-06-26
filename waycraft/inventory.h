enum item_type {
	ITEM_NONE,
	// NOTE: blocks should be synchronized with the block_type enum.
	ITEM_FIRST_BLOCK = ITEM_NONE,
    ITEM_AIR_BLOCK,
    ITEM_STONE_BLOCK,
    ITEM_DIRT_BLOCK,
    ITEM_GRASS_BLOCK,
    ITEM_GRASS_TOP_BLOCK,
    ITEM_PLANKS_BLOCK,
    ITEM_OAK_LOG_BLOCK,
    ITEM_OAK_LEAVES_BLOCK,
    ITEM_SAND_BLOCK,
    ITEM_WATER_BLOCK,
    ITEM_MONITOR_BLOCK,
	ITEM_LAST_BLOCK,
    ITEM_WINDOW = ITEM_LAST_BLOCK,
	ITEM_COUNT
};

struct inventory_item {
	enum item_type type;
	union {
		u32 id;
		u32 count;
	};
};

struct inventory {
	struct inventory_item items[36];
	u8 active_item;
	bool is_active;
};
