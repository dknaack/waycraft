#ifndef WAYCRAFT_RENDERER_H
#define WAYCRAFT_RENDERER_H

#include <waycraft/types.h>

enum render_command_type {
	RENDER_CLEAR,
	RENDER_TEXTURED_QUAD,
};

struct render_command {
	enum render_command_type type;
};

struct render_command_clear {
	struct render_command base;

	v4 color;
};

struct render_command_textured_quad {
	struct render_command base;

	m4x4 transform;
	u32 texture;
};

struct render_command_buffer {
	u32 command_count;

	struct {
		m4x4 view;
		m4x4 projection;
	} transform;

	u8 push_buffer[8192];
	u32 push_buffer_size;
};

struct renderer {
	struct render_command_buffer command_buffer;

	u32 vertex_array;
	u32 vertex_buffer;
	u32 index_buffer;

	struct {
		u32 program;
		i32 model;
		i32 view;
		i32 projection;
	} shader;
};

static void renderer_init(struct renderer *renderer);
static void renderer_finish(struct renderer *renderer);
static struct render_command_buffer *renderer_begin_frame(
	struct renderer *renderer);
static void renderer_end_frame(struct renderer *renderer,
	struct render_command_buffer *commands);

static void render_clear(struct render_command_buffer *comands, v4 color);
static void render_textured_quad(struct render_command_buffer *commands,
	m4x4 transform, u32 texture);

#endif /* WAYCRAFT_RENDERER_H */
