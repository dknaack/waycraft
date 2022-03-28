#include <waycraft/inventory.h>

void
inventory_init(struct inventory *inventory)
{
	// TODO: create a mesh for the inventory
}

static void
item_render(struct inventory_item *item, u32 y, u32 x)
{
	// TODO: render the item
}

void
inventory_render(struct inventory *inventory)
{
	// TODO: draw a grid for the items

	struct inventory_item *item = inventory->items;
	u32 y = LENGTH(inventory->items) / 9;
	while (y-- > 0) {
		for (u32 x = 0; x < 9; x++) {
			item_render(item++, x, y);
		}
	}
}

static u32
item_stack_size(enum item_type item)
{
	switch (item) {
	case ITEM_WINDOW:
		return 0;
	default:
		return 64;
	}
}

static enum block_type
item_to_block(enum item_type item)
{
	if (ITEM_FIRST_BLOCK < item && item < ITEM_LAST_BLOCK) {
		return item - (ITEM_FIRST_BLOCK + 1);
	} else {
		return 0;
	}
}

void
inventory_add_item(struct inventory *inventory, enum item_type item, u32 count)
{
	struct inventory_item *items = inventory->items;

	for (u32 i = 0; i < LENGTH(inventory->items); i++) {
		enum item_type item_type = items[i].type;
		u32 max_stack_size = item_stack_size(items[i].type);
		if (item_type == 0 || item_type == item) {
			u32 item_count = items[i].count;
			assert(item_type == 0 ? item_count == 0 : 1);

			if (item_count < max_stack_size) {
				items[i].count = MAX(item_count + count, max_stack_size);
				if (item_count + count < max_stack_size) {
					break;
				} else {
					count -= max_stack_size - item_count;
				}
			}
		}

		item++;
	}
}
