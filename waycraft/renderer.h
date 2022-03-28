#ifndef WAYCRAFT_RENDERER_H
#define WAYCRAFT_RENDERER_H

#include <waycraft/types.h>

struct renderer {
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
static void renderer_begin_frame(struct renderer *renderer, m4x4 view,
	m4x4 projection);

static void render_textured_quad(struct renderer *renderer, m4x4 transform,
	u32 texture);

#endif /* WAYCRAFT_RENDERER_H */
