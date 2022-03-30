#include <waycraft/stb_image.h>
#include <waycraft/inventory.h>

static i32
texture_init(struct texture *texture, const char *path)
{
	i32 width, height, comp;
	u8 *data = 0;

	if (!(data = stbi_load(path, &width, &height, &comp, 3))) {
		fprintf(stderr, "Failed to load texture\n");
		return -1;
	}

	u32 texture_handle;
	gl.GenTextures(1, &texture_handle);
	gl.BindTexture(GL_TEXTURE_2D, texture_handle);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.GenerateMipmap(GL_TEXTURE_2D);

	texture->handle = texture_handle;
	texture->width  = width;
	texture->height = height;

	free(data);
	return 0;
}

void
inventory_init(struct inventory *inventory)
{
	texture_init(&inventory->inventory_texture, "res/inventory.png");
	texture_init(&inventory->hotbar_texture, "res/hotbar.png");
}

static void
item_render(struct inventory_item *item, u32 y, u32 x)
{
	// TODO: render the item
}

void
inventory_render(struct inventory *inventory, f32 width, f32 height,
	struct render_command_buffer *cmd_buffer)
{
	if (inventory->is_active) {
		m4x4 transform = m4x4_id(0.75);
		u32 texture = inventory->inventory_texture.handle;
		render_textured_quad(cmd_buffer, transform, texture);

		struct inventory_item *item = inventory->items;
		u32 y = LENGTH(inventory->items) / 9;
		while (y-- > 0) {
			for (u32 x = 0; x < 9; x++) {
				item_render(item++, x, y);
			}
		}
	} else {
		f32 hotbar_width = inventory->hotbar_texture.width;
		f32 hotbar_height = inventory->hotbar_texture.height;
		f32 hotbar_aspect_ratio = hotbar_width / hotbar_height;
		f32 screen_aspect_ratio = width / height;
		f32 scale = 0.6;
		f32 h = screen_aspect_ratio / hotbar_aspect_ratio;

		m4x4 transform = m4x4_mul(m4x4_translate(0, -1 + h * scale, 0),
			m4x4_scale(scale, h * scale, scale));

		u32 texture = inventory->hotbar_texture.handle;
		render_textured_quad(cmd_buffer, transform, texture);
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

// NOTE: direction determines the block faces after placement
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

void
inventory_add_item(struct inventory *inventory, enum item_type item, u32 count)
{
	struct inventory_item *items = inventory->items;

	u32 max_stack_size = item_stack_size(item);
	for (u32 i = 0; i < LENGTH(inventory->items); i++) {
		enum item_type item_type = items[i].type;
		if (item_type == 0 || item_type == item) {
			u32 item_count = items[i].count;
			assert(item_type == 0 ? item_count == 0 : 1);

			u32 is_empty = item_count == 0;
			u32 is_non_stackable = max_stack_size == 0;
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
