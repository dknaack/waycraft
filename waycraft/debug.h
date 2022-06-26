#define DEBUG_VERTEX_BUFFER_SIZE KB(8)

struct debug_vertex {
	v3 position;
	v3 color;
};

struct debug_state {
	struct debug_vertex *vertices;
	u32 vertex_count;
	u32 program;
	u32 model;
	u32 view;
	u32 projection;
	u32 vertex_array;
	u32 vertex_buffer;
	v3 color;
};
