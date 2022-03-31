#include <waycraft/gl.h>
#include <waycraft/renderer.h>

#define VERTEX_BUFFER_SIZE MB(1)
#define INDEX_BUFFER_SIZE MB(2)
#define MESH_SIZE KB(16)
#define PUSH_BUFFER_SIZE MB(8)

static const u8 *vert_shader_source = (u8 *)
	"#version 330 core\n"
	"layout (location = 0) in vec3 pos;"
	"layout (location = 1) in vec2 in_coords;"
	"out vec2 coords;"
	"out vec3 frag_pos;"
	"uniform mat4 model;"
	"uniform mat4 view;"
	"uniform mat4 projection;"
	"void main() {"
	"    gl_Position = projection * view * model * vec4(pos, 1.);"
	"    frag_pos = pos;"
	"    coords = in_coords;"
	"}";

static const u8 *frag_shader_source = (u8 *)
	"#version 330 core\n"
	"in vec2 coords;"
	"in vec3 frag_pos;"
	"out vec4 frag_color;"
	"uniform sampler2D tex;"
	"uniform vec3 camera_pos;"
	"void main() {"
	"	frag_color = texture(tex, coords);"
	"	frag_color = mix(vec4(0.45, 0.65, 0.85, 1.0), frag_color, clamp(16-0.1*distance(camera_pos, frag_pos), 0, 1));"
	"}";

static const u32 render_command_size[RENDER_COMMAND_COUNT] = {
	[RENDER_CLEAR] = sizeof(struct render_command_clear),
	[RENDER_QUADS] = sizeof(struct render_command_quads),
	[RENDER_MESH] = sizeof(struct render_command_mesh),
};

static void
gl_uniform_m4x4(u32 uniform, m4x4 value)
{
	gl.UniformMatrix4fv(uniform, 1, GL_FALSE, value.e);
}

static void
gl_uniform_v3(u32 uniform, v3 value)
{
	gl.Uniform3f(uniform, value.x, value.y, value.z);
}

static void
renderer_init(struct renderer *renderer, struct memory_arena *arena)
{
	gl.Enable(GL_DEPTH_TEST);
	gl.Enable(GL_CULL_FACE);
	gl.Enable(GL_BLEND);
	gl.CullFace(GL_FRONT);
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	u32 program = gl_program_create(vert_shader_source, frag_shader_source);
	if (program == 0) {
		u8 error[1024];
		gl_program_error(program, error, sizeof(error));
		fprintf(stderr, "Failed to create program: %s\n", error);
		return;
	}

	renderer->shader.model = gl.GetUniformLocation(program, "model");
	renderer->shader.view = gl.GetUniformLocation(program, "view");
	renderer->shader.projection = gl.GetUniformLocation(program, "projection");
	renderer->shader.camera_pos = gl.GetUniformLocation(program, "camera_pos");
	renderer->shader.program = program;

	renderer->command_buffer.push_buffer =
		arena_alloc(arena, PUSH_BUFFER_SIZE, u8);
	renderer->command_buffer.vertex_buffer =
		arena_alloc(arena, VERTEX_BUFFER_SIZE, struct vertex);
	renderer->command_buffer.index_buffer =
		arena_alloc(arena, VERTEX_BUFFER_SIZE, u32);
	renderer->command_buffer.meshes =
		arena_alloc(arena, MESH_SIZE, struct mesh);
	renderer->command_buffer.mesh_count = 0;

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
}

static void
renderer_finish(struct renderer *renderer)
{
	u32 mesh_count = renderer->command_buffer.mesh_count;
	struct mesh *mesh = renderer->command_buffer.meshes;
	while (mesh_count-- > 0) {
		gl.DeleteVertexArrays(1, &mesh->vertex_array);
		gl.DeleteBuffers(1, &mesh->vertex_buffer);
		gl.DeleteBuffers(1, &mesh->index_buffer);

		mesh++;
	}

	gl.DeleteProgram(renderer->shader.program);
}

static struct render_command_buffer *
renderer_begin_frame(struct renderer *renderer)
{
	struct render_command_buffer *cmd_buffer = &renderer->command_buffer;

	cmd_buffer->current_quads    = 0;
	cmd_buffer->command_count    = 0;
	cmd_buffer->push_buffer_size = 0;
	cmd_buffer->vertex_count     = 0;
	cmd_buffer->index_count      = 0;

	return cmd_buffer;
}

static void
renderer_end_frame(struct renderer *renderer,
	struct render_command_buffer *cmd_buffer)
{
	u32 command_count = cmd_buffer->command_count;
	u8 *push_buffer = cmd_buffer->push_buffer;

	m4x4 model = m4x4_id(1);
	m4x4 view = cmd_buffer->transform.view;
	m4x4 projection = cmd_buffer->transform.projection;
	v3 camera_pos = cmd_buffer->transform.camera_pos;

	gl.UseProgram(renderer->shader.program);
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

		switch (base_command->type) {
		case RENDER_CLEAR:
			{
				struct render_command_clear *clear = CONTAINER_OF(base_command,
					struct render_command_clear, base);

				v4 color = clear->color;
				gl.ClearColor(color.r, color.g, color.b, color.a);
				gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				push_buffer += sizeof(*clear);
			}
			break;

		case RENDER_QUADS:
			{
				struct render_command_quads *command = CONTAINER_OF(
					base_command, struct render_command_quads, base);

				usize index_offset = sizeof(u32) * command->index_offset;

				gl_uniform_v3(renderer->shader.camera_pos, V3(0, 0, 0));
				gl_uniform_m4x4(renderer->shader.projection, m4x4_id(1));
				gl_uniform_m4x4(renderer->shader.view, m4x4_id(1));

				gl.BindVertexArray(renderer->vertex_array);
				gl.BindTexture(GL_TEXTURE_2D, command->texture);
				gl.DrawElements(GL_TRIANGLES, command->quad_count * 6,
					GL_UNSIGNED_INT, (void *)index_offset);

				push_buffer += sizeof(*command);
			}
			break;

		case RENDER_MESH:
			{
				struct render_command_mesh *command = CONTAINER_OF(
					base_command, struct render_command_mesh, base);

				struct mesh *mesh = &cmd_buffer->meshes[command->mesh];

				gl.BindVertexArray(mesh->vertex_array);
				gl.BindTexture(GL_TEXTURE_2D, command->texture);
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
	cmd_buffer->push_buffer_size += command_size;
	cmd_buffer->command_count++;

	assert(cmd_buffer->push_buffer_size < PUSH_BUFFER_SIZE);
	return command;
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
	v2 uv0, v2 uv1, v2 uv2, v2 uv3, u32 texture)
{
	struct render_command_quads *command = cmd_buffer->current_quads;

	if (!command || command->texture != texture) {
		command = push_command(cmd_buffer, RENDER_QUADS);
		command->texture = texture;
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
render_textured_quad(struct render_command_buffer *cmd_buffer,
	m4x4 transform, u32 texture)
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

static void
render_mesh(struct render_command_buffer *cmd_buffer, u32 mesh, m4x4 transform,
	u32 texture)
{
	struct render_command_mesh *command = push_command(cmd_buffer, RENDER_MESH);

	command->mesh = mesh;
	command->texture = texture;
	command->transform = transform;
}

static u32
mesh_create(struct render_command_buffer *cmd_buffer, struct mesh_data *data)
{
	u32 vertex_array, vertex_buffer, index_buffer;
	u32 index_count = data->index_count;

	gl.GenVertexArrays(1, &vertex_array);
	gl.GenBuffers(1, &vertex_buffer);
	gl.GenBuffers(1, &index_buffer);

	gl.BindVertexArray(vertex_array);
	gl.BindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	gl.BufferData(GL_ARRAY_BUFFER, data->vertex_count * sizeof(struct vertex),
		data->vertices, GL_STATIC_DRAW);
	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(u32),
		data->indices, GL_STATIC_DRAW);

	gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
		(const void *)offsetof(struct vertex, position));
	gl.EnableVertexAttribArray(0);
	gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
		(const void *)offsetof(struct vertex, texcoord));
	gl.EnableVertexAttribArray(1);

	gl.BindVertexArray(0);

	u32 mesh_index = cmd_buffer->mesh_count++;
	struct mesh *mesh = &cmd_buffer->meshes[mesh_index];

	mesh->vertex_array = vertex_array;
	mesh->vertex_buffer = vertex_buffer;
	mesh->index_buffer = index_buffer;
	mesh->index_count = index_count;

	return mesh_index;
}
