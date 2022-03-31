#ifndef WAYCRAFT_INVENTORY_H
#define WAYCRAFT_INVENTORY_H

#include <waycraft/block.h>

struct texture {
	u32 handle;
	u32 width;
	u32 height;
};

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
	u8 is_active;
	struct texture inventory_texture;
	struct texture hotbar_texture;
	struct texture active_slot_texture;
	struct texture block_atlas_texture;
};

// NOTE: the direction determines the direction that the block faces after
// placing it in the world.
static enum block_type item_to_block(enum item_type item, v3 direction);

static void inventory_init(struct inventory *inventory);
static void inventory_render(struct inventory *inventory, f32 width, f32 height,
	struct render_command_buffer *cmd_buffer);
static void inventory_add_item(struct inventory *inventory,
	enum item_type item, u32 count);

#endif /* WAYCRAFT_INVENTORY_H */
