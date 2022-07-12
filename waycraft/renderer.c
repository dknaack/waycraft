#define VERTEX_BUFFER_SIZE MB(1)
#define INDEX_BUFFER_SIZE MB(2)
#define MESH_SIZE KB(16)
#define PUSH_BUFFER_SIZE MB(8)

static char *vert_shader_source =
	"#version 330 core\n"
	"layout (location = 0) in vec3 pos;\n"
	"layout (location = 1) in vec2 in_coords;\n"
	"out vec2 coords;\n"
	"out vec3 frag_pos;\n"
	"uniform mat4 model;\n"
	"uniform mat4 view;\n"
	"uniform mat4 projection;\n"
	"void main() {\n"
	"    gl_Position = projection * view * model * vec4(pos, 1.);\n"
	"    frag_pos = pos;\n"
	"    coords = in_coords;\n"
	"}\n";

static char *frag_shader_source =
	"#version 330 core\n"
	"in vec2 coords;\n"
	"in vec3 frag_pos;\n"
	"out vec4 frag_color;\n"
	"uniform int enable_fog;\n"
	"uniform sampler2D tex;\n"
	"uniform vec3 camera_pos;\n"
	"void main() {\n"
	"	frag_color = texture(tex, coords);\n"
	""
	"	if (enable_fog != 0) {\n"
	"		float dist = distance(camera_pos, frag_pos);\n"
	"		float alpha = clamp(0.1 * (256 - dist), 0, 1);\n"
	"		frag_color = mix(vec4(0.45, 0.65, 0.85, 1.0), frag_color, alpha);\n"
	"	}\n"
	"}\n";

static const u32 render_command_size[RENDER_COMMAND_COUNT] = {
	[RENDER_CLEAR] = sizeof(struct render_command_clear),
	[RENDER_QUADS] = sizeof(struct render_command_quads),
	[RENDER_MESH]  = sizeof(struct render_command_mesh),
};

static void
gl_uniform_m4x4(u32 uniform, m4x4 value)
{
	gl.UniformMatrix4fv(uniform, 1, GL_FALSE, (f32 *)value.e);
}

static void
gl_uniform_v3(u32 uniform, v3 value)
{
	gl.Uniform3f(uniform, value.x, value.y, value.z);
}

static void
gl_shader_error(u32 shader, char *buffer, u32 size)
{
	gl.GetShaderInfoLog(shader, size - 1, 0, buffer);
}

static u32
gl_shader_create(char *src, u32 type)
{
	u32 shader = gl.CreateShader(type);
	i32 success;

	gl.ShaderSource(shader, 1, (const char *const *)&src, 0);
	gl.CompileShader(shader);
	gl.GetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char error[1024] = {0};
		gl_shader_error(shader, error, sizeof(error));
		fprintf(stderr, "Failed to create shader: %s\n", error);
		assert(!"Failed to create shader");
	}

	return success ? shader : 0;
}

static u32
gl_program_create(char *vert_shader_source, char *frag_shader_source)
{
	u32 program = gl.CreateProgram();
	u32 vert_shader = gl_shader_create(vert_shader_source, GL_VERTEX_SHADER);
	u32 frag_shader = gl_shader_create(frag_shader_source, GL_FRAGMENT_SHADER);
	i32 success;

	if (vert_shader == 0 || frag_shader == 0) {
		return 0;
	}

	gl.AttachShader(program, vert_shader);
	gl.AttachShader(program, frag_shader);
	gl.LinkProgram(program);
	gl.DeleteShader(vert_shader);
	gl.DeleteShader(frag_shader);
	gl.GetProgramiv(program, GL_LINK_STATUS, &success);

	return success ? program : 0;
}

static void
gl_program_error(u32 program, char *buffer, u32 size)
{
	gl.GetProgramInfoLog(program, size - 1, 0, buffer);
}

static void
renderer_init(struct renderer *renderer, struct memory_arena *arena)
{
	gl.Enable(GL_DEPTH_TEST);
	gl.Enable(GL_CULL_FACE);
	gl.Enable(GL_BLEND);
	gl.CullFace(GL_FRONT);
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// NOTE: initialize the shader
	u32 program = gl_program_create(vert_shader_source, frag_shader_source);
	if (program == 0) {
		char error[1024] = {0};
		gl_program_error(program, error, sizeof(error));
		fprintf(stderr, "Failed to create program: %s\n", error);
		assert(!"Failed to create program");
	}

	renderer->shader.model = gl.GetUniformLocation(program, "model");
	renderer->shader.view = gl.GetUniformLocation(program, "view");
	renderer->shader.projection = gl.GetUniformLocation(program, "projection");
	renderer->shader.camera_pos = gl.GetUniformLocation(program, "camera_pos");
	renderer->shader.enable_fog = gl.GetUniformLocation(program, "enable_fog");
	renderer->shader.program = program;

	// NOTE: initialize the main vertex array
	gl.GenVertexArrays(1, &renderer->vertex_array);
	gl.GenBuffers(1, &renderer->vertex_buffer);
	gl.GenBuffers(1, &renderer->index_buffer);

	gl.BindVertexArray(renderer->vertex_array);
	gl.BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer);
	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->index_buffer);

	gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
		(const void *)offsetof(struct vertex, position));
	gl.EnableVertexAttribArray(0);
	gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
		(const void *)offsetof(struct vertex, texcoord));
	gl.EnableVertexAttribArray(1);

	gl.BindVertexArray(0);

	// NOTE: allocate memory for the meshes
	u32 max_mesh_count = 32 * 32 * 32;
	renderer->meshes = arena_alloc(arena, max_mesh_count, struct mesh);
	renderer->max_mesh_count = max_mesh_count;
	// NOTE: we ignore the first mesh
	renderer->mesh_count = 1;

	// NOTE: generate a white texture
	u8 white[4] = { 0xff, 0xff, 0xff, 0xff };
	gl.GenTextures(1, &renderer->white_texture);
	gl.BindTexture(GL_TEXTURE_2D, renderer->white_texture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

static void
renderer_finish(struct renderer *renderer)
{
	u32 mesh_count = renderer->mesh_count;
	struct mesh *mesh = renderer->meshes;
	while (mesh_count-- > 0) {
		gl.DeleteVertexArrays(1, &mesh->vertex_array);
		gl.DeleteBuffers(1, &mesh->vertex_buffer);
		gl.DeleteBuffers(1, &mesh->index_buffer);

		mesh++;
	}

	gl.DeleteProgram(renderer->shader.program);
}

static void
render_command_buffer_init(struct render_command_buffer *cmd_buffer,
		struct memory_arena *arena, u32 max_push_buffer_size,
		u32 max_vertex_count, u32 max_index_count)
{
	cmd_buffer->max_vertex_count = max_vertex_count;
	cmd_buffer->max_index_count = max_index_count;
	cmd_buffer->max_push_buffer_size = max_push_buffer_size;

	cmd_buffer->push_buffer = arena_alloc(arena, max_push_buffer_size, u8);
	cmd_buffer->vertex_buffer = arena_alloc(arena, max_vertex_count, struct vertex);
	cmd_buffer->index_buffer = arena_alloc(arena, max_index_count, u32);
}

static void
renderer_build_command_buffer(struct renderer *renderer,
		struct render_command_buffer *cmd_buffer, u32 *cmd_buffer_id)
{
	assert(renderer->mesh_count < renderer->max_mesh_count);

	u32 vertex_array, vertex_buffer, index_buffer;
	u32 index_count = cmd_buffer->index_count;

	gl.GenVertexArrays(1, &vertex_array);
	gl.GenBuffers(1, &vertex_buffer);
	gl.GenBuffers(1, &index_buffer);

	gl.BindVertexArray(vertex_array);
	gl.BindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	gl.BufferData(GL_ARRAY_BUFFER, cmd_buffer->vertex_count * sizeof(struct vertex),
		cmd_buffer->vertex_buffer, GL_STATIC_DRAW);
	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(u32),
		cmd_buffer->index_buffer, GL_STATIC_DRAW);

	gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
		(const void *)offsetof(struct vertex, position));
	gl.EnableVertexAttribArray(0);
	gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
		(const void *)offsetof(struct vertex, texcoord));
	gl.EnableVertexAttribArray(1);

	gl.BindVertexArray(0);

	if (*cmd_buffer_id == 0) {
		*cmd_buffer_id = renderer->mesh_count++;
	}

	struct mesh *mesh = &renderer->meshes[*cmd_buffer_id];
	mesh->vertex_array = vertex_array;
	mesh->vertex_buffer = vertex_buffer;
	mesh->index_buffer = index_buffer;
	mesh->index_count = index_count;
}

static void
renderer_bind_texture(struct renderer *renderer, u32 texture_id)
{
	if (texture_id != 0) {
		gl.BindTexture(GL_TEXTURE_2D, texture_id);
	} else {
		gl.BindTexture(GL_TEXTURE_2D, renderer->white_texture);
	}
}

static void
renderer_submit(struct renderer *renderer, struct render_command_buffer *cmd_buffer)
{
	u32 command_count = cmd_buffer->command_count;
	u8 *push_buffer = cmd_buffer->push_buffer;

	m4x4 model = m4x4_id(1);
	m4x4 view = cmd_buffer->transform.view;
	m4x4 projection = cmd_buffer->transform.projection;
	v3 camera_pos = cmd_buffer->transform.camera_pos;
	v2 viewport = cmd_buffer->transform.viewport;

	gl.Viewport(0, 0, viewport.width, viewport.height);
	gl.UseProgram(renderer->shader.program);

	if (cmd_buffer->mode == RENDER_3D) {
		gl.Enable(GL_DEPTH_TEST);
		gl.Uniform1i(renderer->shader.enable_fog, true);
	} else if (cmd_buffer->mode == RENDER_2D) {
		gl.Disable(GL_DEPTH_TEST);
		gl.Uniform1i(renderer->shader.enable_fog, false);
	}

	gl_uniform_v3(renderer->shader.camera_pos, camera_pos);
	gl_uniform_m4x4(renderer->shader.model, model);
	gl_uniform_m4x4(renderer->shader.projection, projection);
	gl_uniform_m4x4(renderer->shader.view, view);

	gl.BindVertexArray(renderer->vertex_array);
	gl.BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer);
	gl.BufferData(GL_ARRAY_BUFFER,
		cmd_buffer->vertex_count * sizeof(*cmd_buffer->vertex_buffer),
		cmd_buffer->vertex_buffer, GL_STREAM_DRAW);
	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->index_buffer);
	gl.BufferData(GL_ELEMENT_ARRAY_BUFFER,
		cmd_buffer->index_count * sizeof(*cmd_buffer->index_buffer),
		cmd_buffer->index_buffer, GL_STREAM_DRAW);

	while (command_count-- > 0) {
		struct render_command *base_command = (struct render_command *)push_buffer;
		push_buffer += sizeof(*base_command);

		switch (base_command->type) {
		case RENDER_CLEAR:
			{
				struct render_command_clear *clear =
					(struct render_command_clear *)push_buffer;

				v4 color = clear->color;
				gl.ClearColor(color.r, color.g, color.b, color.a);
				gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				push_buffer += sizeof(*clear);
			}
			break;

		case RENDER_QUADS:
			{
				struct render_command_quads *command =
					(struct render_command_quads *)push_buffer;

				usize index_offset = sizeof(u32) * command->index_offset;

				gl.BindVertexArray(renderer->vertex_array);
				renderer_bind_texture(renderer, command->texture);
				gl.DrawElements(GL_TRIANGLES, command->quad_count * 6,
					GL_UNSIGNED_INT, (void *)index_offset);

				push_buffer += sizeof(*command);
			}
			break;

		case RENDER_MESH:
			{
				struct render_command_mesh *command =
					(struct render_command_mesh *)push_buffer;

				struct mesh *mesh = &renderer->meshes[command->mesh];

				gl.BindVertexArray(mesh->vertex_array);
				renderer_bind_texture(renderer, command->texture);
				gl_uniform_m4x4(renderer->shader.model, command->transform);
				gl.DrawElements(GL_TRIANGLES, mesh->index_count,
					GL_UNSIGNED_INT, 0);

				push_buffer += sizeof(*command);
			}
			break;

		default:
			assert(!"Invalid command type");
		}
	}
}

static void *
push_command(struct render_command_buffer *cmd_buffer, u32 type)
{
	struct render_command *command = (struct render_command *)
		(cmd_buffer->push_buffer + cmd_buffer->push_buffer_size);

	u32 command_size = render_command_size[type];

	assert(type < RENDER_COMMAND_COUNT);
	assert(command_size != 0);

	command->type = type;
	cmd_buffer->push_buffer_size += sizeof(*command);
	cmd_buffer->push_buffer_size += command_size;
	cmd_buffer->command_count++;

	assert(cmd_buffer->push_buffer_size < cmd_buffer->max_push_buffer_size);
	return command + 1;
}

static void
render_clear(struct render_command_buffer *cmd_buffer, v4 color)
{
	struct render_command_clear *clear =
		push_command(cmd_buffer, RENDER_CLEAR);

	clear->color = color;
}

static void
render_quad(struct render_command_buffer *cmd_buffer,
		v3 pos0, v3 pos1, v3 pos2, v3 pos3,
		v2 uv0, v2 uv1, v2 uv2, v2 uv3, struct texture_id texture)
{
	struct render_command_quads *command = cmd_buffer->current_quads;

	if (!command || command->texture != texture.value) {
		command = push_command(cmd_buffer, RENDER_QUADS);
		command->texture = texture.value;
		command->index_offset = cmd_buffer->index_count;
		command->quad_count = 0;
		cmd_buffer->current_quads = command;
	}

	u32 vertex_count = cmd_buffer->vertex_count;
	u32 index_count = cmd_buffer->index_count;
	struct vertex *out_vertex = cmd_buffer->vertex_buffer + vertex_count;
	u32 *out_index = cmd_buffer->index_buffer + index_count;

	out_vertex->position = pos0;
	out_vertex->texcoord = uv0;
	out_vertex++;

	out_vertex->position = pos1;
	out_vertex->texcoord = uv1;
	out_vertex++;

	out_vertex->position = pos2;
	out_vertex->texcoord = uv2;
	out_vertex++;

	out_vertex->position = pos3;
	out_vertex->texcoord = uv3;
	out_vertex++;

	*out_index++ = vertex_count;
	*out_index++ = vertex_count + 2;
	*out_index++ = vertex_count + 1;
	*out_index++ = vertex_count + 2;
	*out_index++ = vertex_count + 3;
	*out_index++ = vertex_count + 1;

	command->quad_count++;
	cmd_buffer->index_count += 6;
	cmd_buffer->vertex_count += 4;

	assert(cmd_buffer->index_count < INDEX_BUFFER_SIZE);
	assert(cmd_buffer->vertex_count < VERTEX_BUFFER_SIZE);
}

static void
render_sprite(struct render_command_buffer *cmd_buffer,
		struct rectangle rect, struct texture_id texture)
{
	v3 pos0 = V3(rect.x + 0 * rect.width, rect.y + 0 * rect.height, 0);
	v3 pos1 = V3(rect.x + 1 * rect.width, rect.y + 0 * rect.height, 0);
	v3 pos2 = V3(rect.x + 0 * rect.width, rect.y + 1 * rect.height, 0);
	v3 pos3 = V3(rect.x + 1 * rect.width, rect.y + 1 * rect.height, 0);

	v2 uv0 = V2(0, 1);
	v2 uv1 = V2(1, 1);
	v2 uv2 = V2(0, 0);
	v2 uv3 = V2(1, 0);

	render_quad(cmd_buffer, pos0, pos1, pos2, pos3, uv0, uv1, uv2, uv3, texture);
}

static void
render_rect(struct render_command_buffer *cmd_buffer, struct rectangle rect)
{
	struct texture_id texture_id = {0};

	render_sprite(cmd_buffer, rect, texture_id);
}

#if 0
static void
render_textured_quad(struct render_command_buffer *cmd_buffer,
		m4x4 transform, struct texture_id texture)
{
	v3 pos0 = m4x4_mulv(transform, V4(+1, +1, 0, 1)).xyz;
	v3 pos1 = m4x4_mulv(transform, V4(-1, +1, 0, 1)).xyz;
	v3 pos2 = m4x4_mulv(transform, V4(+1, -1, 0, 1)).xyz;
	v3 pos3 = m4x4_mulv(transform, V4(-1, -1, 0, 1)).xyz;

	v2 uv0 = V2(1, 0);
	v2 uv1 = V2(0, 0);
	v2 uv2 = V2(1, 1);
	v2 uv3 = V2(0, 1);

	render_quad(cmd_buffer, pos0, pos1, pos2, pos3, uv0, uv1, uv2, uv3, texture);
}
#endif

static void
render_mesh(struct render_command_buffer *cmd_buffer,
		u32 mesh, m4x4 transform, struct texture_id texture)
{
	struct render_command_mesh *command = push_command(cmd_buffer, RENDER_MESH);

	command->mesh = mesh;
	command->texture = texture.value;
	command->transform = transform;
}
