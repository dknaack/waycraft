struct vertex {
    v3 position;
    v2 texcoord;
};

enum render_command_type {
	RENDER_CLEAR,
	RENDER_QUADS,
	RENDER_MESH,
	RENDER_COMMAND_COUNT
};

struct render_command {
	enum render_command_type type;
};

struct render_command_clear {
	v4 color;
};

struct render_command_quads {
	u32 index_offset;
	u32 quad_count;
	u32 texture;
};

struct render_command_mesh {
	u32 mesh;
	u32 texture;
	m4x4 transform;
};

struct render_transform {
	m4x4 view;
	m4x4 projection;
	v3 camera_pos;
};

struct render_command_buffer {
	struct render_transform transform;

	u32 command_count;
	u8 *push_buffer;
	u32 push_buffer_size;
	u32 max_push_buffer_size;

	struct vertex *vertex_buffer;
	u32 *index_buffer;
	u32 vertex_count;
	u32 index_count;
	u32 max_vertex_count;
	u32 max_index_count;

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

	struct mesh *meshes;
	u32 max_mesh_count;
	u32 mesh_count;
};

struct memory_arena;
