struct vertex {
	v3 position;
	v2 texcoord;
};

enum render_mode {
	RENDER_3D,
	RENDER_2D,
};

enum render_cmd_type {
	RENDER_CLEAR,
	RENDER_QUADS,
	RENDER_MESH,
	RENDER_COMMAND_COUNT
};

struct render_cmd {
	enum render_cmd_type type;
};

struct render_cmd_clear {
	v4 color;
};

struct render_cmd_quads {
	u32 index_offset;
	u32 quad_count;
	u32 texture;
};

struct render_cmd_mesh {
	u32 mesh;
	u32 texture;
	m4x4 transform;
};

struct render_transform {
	m4x4 view;
	m4x4 projection;
	v3 camera_pos;
	v2 viewport;
};

struct render_cmdbuf {
	struct render_transform transform;
	enum render_mode mode;

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

	struct render_cmd_quads *current_quads;
	struct game_assets *assets;
};

struct mesh {
	u32 vertex_array;
	u32 vertex_buffer;
	u32 index_buffer;

	u32 index_count;
};

struct renderer {
	struct render_cmdbuf command_buffer;

	u32 white_texture;
	u32 vertex_array;
	u32 vertex_buffer;
	u32 index_buffer;

	struct {
		u32 program;
		i32 model;
		i32 view;
		i32 projection;
		i32 camera_pos;
		i32 enable_fog;
	} shader;

	struct mesh *meshes;
	u32 max_mesh_count;
	u32 mesh_count;
};

struct arena;
