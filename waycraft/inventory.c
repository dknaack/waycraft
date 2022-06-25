static void
inventory_init(struct inventory *inventory)
{
	texture_init(&inventory->inventory_texture, "res/inventory.png");
	texture_init(&inventory->hotbar_texture, "res/hotbar.png");
	texture_init(&inventory->active_slot_texture, "res/active_slot.png");
	texture_init(&inventory->block_atlas_texture, "res/textures.png");
}

// NOTE: the direction determines the direction that the block faces after
// placing it in the world.
static enum block_type
item_to_block(enum item_type item, v3 direction)
{
	if (ITEM_FIRST_BLOCK < item && item < ITEM_LAST_BLOCK) {
		enum block_type block = item - (ITEM_FIRST_BLOCK + 1);

		u32 can_rotate = item == ITEM_MONITOR_BLOCK;
		if (can_rotate) {
			v3 abs_direction = v3_abs(direction);

			u32 is_facing_x_axis = abs_direction.x > abs_direction.y &&
				abs_direction.x > abs_direction.z;
			u32 is_facing_y_axis = abs_direction.y > abs_direction.x &&
				abs_direction.y > abs_direction.z;

			u32 is_facing_in_negative_direction = 0;
			if (is_facing_x_axis) {
				is_facing_in_negative_direction = direction.x < 0;
			} else if (is_facing_y_axis) {
				is_facing_in_negative_direction = direction.y < 0;
				block += 2;
			} else {
				is_facing_in_negative_direction = direction.z < 0;
				block += 4;
			}

			block += is_facing_in_negative_direction;
		}

		return block;
	} else {
		return 0;
	}
}

static void
item_render(struct inventory_item *item, struct rectangle rect,
		struct render_command_buffer *cmd_buffer)
{
	if (item->type == ITEM_WINDOW) {
		v3 pos0 = V3(rect.x + 1 * rect.width, rect.y + 1 * rect.height, 0);
		v3 pos1 = V3(rect.x + 0 * rect.width, rect.y + 1 * rect.height, 0);
		v3 pos2 = V3(rect.x + 1 * rect.width, rect.y + 0 * rect.height, 0);
		v3 pos3 = V3(rect.x + 0 * rect.width, rect.y + 0 * rect.height, 0);

		v2 uv[4] = {0};
		block_texcoords_right(BLOCK_AIR, uv);
		render_quad(cmd_buffer, pos0, pos1, pos2, pos3,
			uv[0], uv[1], uv[2], uv[3], TEXTURE_BLOCK_ATLAS);
	} else {
		enum block_type block = item_to_block(item->type, V3(1, 0, 0));
		if (block != BLOCK_AIR) {
			v3 pos0 = V3(rect.x + 1 * rect.width, rect.y + 1 * rect.height, 0);
			v3 pos1 = V3(rect.x + 0 * rect.width, rect.y + 1 * rect.height, 0);
			v3 pos2 = V3(rect.x + 1 * rect.width, rect.y + 0 * rect.height, 0);
			v3 pos3 = V3(rect.x + 0 * rect.width, rect.y + 0 * rect.height, 0);

			v2 uv[4] = {0};
			block_texcoords_right(block, uv);
			render_quad(cmd_buffer, pos0, pos1, pos2, pos3,
				uv[0], uv[1], uv[2], uv[3], TEXTURE_BLOCK_ATLAS);
		}
	}
}

static void
inventory_render(struct inventory *inventory,
		f32 screen_width, f32 screen_height,
		struct render_command_buffer *cmd_buffer)
{
	f32 hotbar_aspect = inventory->hotbar_texture.width / inventory->hotbar_texture.height;

	struct rectangle hotbar = {0};
	hotbar.width = screen_width * 0.5f;
	hotbar.height = hotbar.width / hotbar_aspect;
	hotbar.x = (screen_width - hotbar.width) / 2.0f;
	hotbar.y = 0;

	render_sprite(cmd_buffer, hotbar, TEXTURE_HOTBAR);

	if (inventory->is_active) {
		m4x4 transform = m4x4_id(0.75);
		render_textured_quad(cmd_buffer, transform, TEXTURE_INVENTORY);

		//struct inventory_item *item = inventory->items;
		u32 y = LENGTH(inventory->items) / 9;
		while (y-- > 0) {
			for (u32 x = 0; x < 9; x++) {
				//item_render(item++, x, y, cmd_buffer);
			}
		}
	} else {
		f32 slot_size = hotbar.height / 20.0 * 14.0;
		f32 slot_advance = hotbar.height / 20.0f * 17.5f;

		struct rectangle active_slot = {0};
		active_slot.x = hotbar.x + slot_advance * inventory->active_item;
		active_slot.y = hotbar.y;
		active_slot.width = hotbar.height;
		active_slot.height = hotbar.height;

		render_sprite(cmd_buffer, active_slot, TEXTURE_ACTIVE_SLOT);

		struct inventory_item *item = inventory->items;
		for (u32 i = 0; i < 9; i++) {
			struct rectangle item_rect = {0};
			item_rect.x = hotbar.x + 0.5f * (hotbar.height - slot_size) + slot_advance * i;
			item_rect.y = hotbar.y + 0.5f * (hotbar.height - slot_size);
			item_rect.width = slot_size;
			item_rect.height = slot_size;

			item_render(item++, item_rect, cmd_buffer);
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

static void
inventory_add_item(struct inventory *inventory, enum item_type item, u32 count)
{
	struct inventory_item *items = inventory->items;

	u32 max_stack_size = item_stack_size(item);
	for (u32 i = 0; i < LENGTH(inventory->items); i++) {
		enum item_type item_type = items[i].type;
		if (item_type == 0 || item_type == item) {
			u32 item_count = items[i].count;
			assert(item_type == 0 ? item_count == 0 : 1);

			bool is_empty = item_count == 0;
			bool is_non_stackable = max_stack_size == 0;
			if (is_non_stackable && is_empty) {
				items[i].type = item;
				items[i].count = count;
				break;
			} else if (is_empty || item_count < max_stack_size) {
				items[i].type = item;
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
