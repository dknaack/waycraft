static char *debug_vertex_shader_source = "#version 330\n"
"layout (location = 0) in vec3 in_pos;"
"layout (location = 1) in vec3 in_color;"
"uniform mat4 model;"
"uniform mat4 view;"
"uniform mat4 projection;"
"out vec3 color;"
"void main() {"
"   gl_Position = projection * view * model * vec4(in_pos, 1.);"
"   color = in_color;"
"}";

static char *debug_fragment_shader_source = "#version 330\n"
"in vec3 color;\n"
"out vec4 frag_color;\n"
"void main() {"
"   frag_color = vec4(color, 1.);"
"}";

static struct debug_state *debug = 0;

static void
debug_init(struct debug_state *debug)
{
	gl.LineWidth(4.f);

	debug->program = gl_program_create(debug_vertex_shader_source,
		debug_fragment_shader_source);
	debug->model      = gl.GetUniformLocation(debug->program, "model");
	debug->view       = gl.GetUniformLocation(debug->program, "view");
	debug->projection = gl.GetUniformLocation(debug->program, "projection");

	gl.GenVertexArrays(1, &debug->vertex_array);
	gl.GenBuffers(1, &debug->vertex_buffer);
	gl.BindVertexArray(debug->vertex_array);
	gl.BindBuffer(GL_ARRAY_BUFFER, debug->vertex_buffer);
	gl.BufferData(GL_ARRAY_BUFFER, DEBUG_VERTEX_BUFFER_SIZE, 0, GL_STREAM_DRAW);

	gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
		sizeof(struct debug_vertex),
		(const void *)offsetof(struct debug_vertex, position));
	gl.EnableVertexAttribArray(0);
	gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
		sizeof(struct debug_vertex),
		(const void *)offsetof(struct debug_vertex, color));
	gl.EnableVertexAttribArray(1);
}

static void
debug_update(struct debug_state *debug_state, struct memory_arena *frame_arena)
{
	debug = debug_state;
	debug->vertices = arena_alloc(frame_arena, DEBUG_VERTEX_BUFFER_SIZE, struct debug_vertex);
	debug->vertex_count = 0;
}

static void
debug_set_color(f32 r, f32 g, f32 b)
{
	debug->color = V3(r, g, b);
}

static void
debug_line(v3 start, v3 end)
{
	struct debug_vertex *vertex = debug->vertices + debug->vertex_count;
	assert(debug->vertex_count *
		sizeof(struct debug_vertex) < DEBUG_VERTEX_BUFFER_SIZE);

	vertex->position = start;
	vertex->color    = debug->color;
	vertex++;

	vertex->position = end;
	vertex->color    = debug->color;
	vertex++;

	debug->vertex_count += 2;
}

static void
debug_cube(v3 min, v3 max)
{
	v3 pos0 = V3(min.x, min.y, min.z);
	v3 pos1 = V3(max.x, min.y, min.z);
	v3 pos2 = V3(min.x, max.y, min.z);
	v3 pos3 = V3(max.x, max.y, min.z);
	v3 pos4 = V3(min.x, min.y, max.z);
	v3 pos5 = V3(max.x, min.y, max.z);
	v3 pos6 = V3(min.x, max.y, max.z);
	v3 pos7 = V3(max.x, max.y, max.z);

	debug_line(pos0, pos1);
	debug_line(pos0, pos2);
	debug_line(pos1, pos3);
	debug_line(pos2, pos3);

	debug_line(pos4, pos5);
	debug_line(pos4, pos6);
	debug_line(pos5, pos7);
	debug_line(pos6, pos7);

	debug_line(pos0, pos4);
	debug_line(pos1, pos5);
	debug_line(pos2, pos6);
	debug_line(pos3, pos7);
}

static void
debug_render(m4x4 view, m4x4 projection)
{
	gl.Disable(GL_DEPTH_TEST);
	gl.PolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	gl.BindVertexArray(debug->vertex_array);
	gl.BindBuffer(GL_ARRAY_BUFFER, debug->vertex_buffer);
	gl.BufferSubData(GL_ARRAY_BUFFER, 0,
		debug->vertex_count * sizeof(struct debug_vertex), debug->vertices);

	gl.UseProgram(debug->program);
	gl.UniformMatrix4fv(debug->model, 1, GL_FALSE, (f32 *)m4x4_id(1).e);
	gl.UniformMatrix4fv(debug->projection, 1, GL_FALSE, (f32 *)projection.e);
	gl.UniformMatrix4fv(debug->view, 1, GL_FALSE, (f32 *)view.e);

	gl.DrawArrays(GL_LINES, 0, debug->vertex_count);

	gl.PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	gl.Enable(GL_DEPTH_TEST);

	debug->vertex_count = 0;
}
