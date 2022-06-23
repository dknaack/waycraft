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
