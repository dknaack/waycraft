#ifndef WAYCRAFT_RENDERER_H
#define WAYCRAFT_RENDERER_H

#include <waycraft/types.h>

struct vertex {
    v3 position;
    v2 texcoord;
};

struct mesh_data {
	struct vertex *vertices;
	u32 *indices;

	u32 vertex_count;
	u32 index_count;
};

enum render_command_type {
	RENDER_CLEAR,
	RENDER_QUADS,
	RENDER_MESH,
	RENDER_TRANSFORM,
	RENDER_COMMAND_COUNT
};

struct render_command {
	enum render_command_type type;
};

struct render_command_clear {
	struct render_command base;

	v4 color;
};

struct render_command_quads {
	struct render_command base;

	u32 index_offset;
	u32 quad_count;
	u32 texture;
};

struct render_command_mesh {
	struct render_command base;

	u32 mesh;
	u32 texture;
	m4x4 transform;
};

struct render_command_transform {
	struct render_command base;

	m4x4 view;
	m4x4 projection;
	v3 camera_pos;
};

struct render_command_buffer {
	u32 command_count;

	struct {
		m4x4 view;
		m4x4 projection;
		v3 camera_pos;
	} transform;

	u8 *push_buffer;
	u32 push_buffer_size;

	struct vertex *vertex_buffer;
	u32 *index_buffer;
	u32 vertex_count;
	u32 index_count;

	struct mesh *meshes;
	u32 mesh_count;

	struct render_command_quads *current_quads;
};

struct mesh {
	u32 vertex_array;
	u32 vertex_buffer;
	u32 index_buffer;

	u32 index_count;
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
		i32 camera_pos;
	} shader;
};

struct memory_arena;

static void renderer_init(struct renderer *renderer, struct memory_arena *arena);
static void renderer_finish(struct renderer *renderer);
static struct render_command_buffer *renderer_begin_frame(
	struct renderer *renderer);
static void renderer_end_frame(struct renderer *renderer,
	struct render_command_buffer *commands);

static void render_mesh(struct render_command_buffer *commands, u32 mesh,
	m4x4 transform, u32 texture);
static void render_clear(struct render_command_buffer *comands, v4 color);
static void render_textured_quad(struct render_command_buffer *commands,
	m4x4 transform, u32 texture);
static void render_quad(struct render_command_buffer *cmd_buffer,
	v3 pos0, v3 pos1, v3 pos2, v3 pos3, v2 uv0, v2 uv1, v2 uv2, v2 uv3, u32 texture);
static void render_set_transform(struct render_command_buffer *cmd_buffer,
	m4x4 view, m4x4 projection, v3 camera_pos);

static u32 mesh_create(struct render_command_buffer *cmd_buffer,
	struct mesh_data *data);
static void mesh_destroy(struct render_command_buffer *cmd_buffer);

#endif /* WAYCRAFT_RENDERER_H */
