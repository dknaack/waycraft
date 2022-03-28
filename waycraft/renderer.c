#include <waycraft/gl.h>
#include <waycraft/renderer.h>

#define VERTEX_BUFFER_SIZE KB(8)
#define INDEX_BUFFER_SIZE KB(12)

static const u32 render_command_size[RENDER_COMMAND_COUNT] = {
	[RENDER_CLEAR] = sizeof(struct render_command_clear),
	[RENDER_TEXTURED_QUAD] = sizeof(struct render_command_textured_quad),
	[RENDER_QUADS] = sizeof(struct render_command_quads),
};

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
	renderer->shader.program = program;

	renderer->command_buffer.vertex_buffer =
		arena_alloc(arena, VERTEX_BUFFER_SIZE, struct vertex);
	renderer->command_buffer.index_buffer =
		arena_alloc(arena, VERTEX_BUFFER_SIZE, u32);

	static const struct vertex vertices[] = {
		{ {{  1.,  1., 0.0f }}, {{ 1.0, 0.0 }} },
		{ {{  1., -1., 0.0f }}, {{ 1.0, 1.0 }} },
		{ {{ -1., -1., 0.0f }}, {{ 0.0, 1.0 }} },
		{ {{ -1.,  1., 0.0f }}, {{ 0.0, 0.0 }} },
	};

	static const u32 indices[] = { 0, 1, 3, 1, 2, 3, };

	gl.GenVertexArrays(1, &renderer->vertex_array);
	gl.BindVertexArray(renderer->vertex_array);

	gl.GenBuffers(1, &renderer->vertex_buffer);
	gl.BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer);
	gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
		GL_STATIC_DRAW);

	gl.GenBuffers(1, &renderer->index_buffer);
	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->index_buffer);
	gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
		GL_STATIC_DRAW);

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
	gl.DeleteProgram(renderer->shader.program);
}

static struct render_command_buffer *
renderer_begin_frame(struct renderer *renderer)
{
	struct render_command_buffer *cmd_buffer = &renderer->command_buffer;

	cmd_buffer->command_count = 0;
	cmd_buffer->push_buffer_size = 0;
	cmd_buffer->vertex_count = 0;
	cmd_buffer->index_count = 0;

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

	gl.UseProgram(renderer->shader.program);
	gl.UniformMatrix4fv(renderer->shader.model, 1, GL_FALSE, model.e);
	gl.UniformMatrix4fv(renderer->shader.projection, 1, GL_FALSE, projection.e);
	gl.UniformMatrix4fv(renderer->shader.view, 1, GL_FALSE, view.e);

	gl.BindVertexArray(renderer->vertex_array);
	gl.BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer);
	gl.BufferData(GL_ARRAY_BUFFER,
		cmd_buffer->vertex_count * sizeof(*cmd_buffer->vertex_buffer),
		cmd_buffer->vertex_buffer, GL_STREAM_DRAW);
	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->index_buffer);
	gl.BufferData(GL_ELEMENT_ARRAY_BUFFER,
		cmd_buffer->index_count * sizeof(*cmd_buffer->index_buffer),
		cmd_buffer->index_buffer, GL_STREAM_DRAW);

	while (command_count-- > 0){
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
		case RENDER_TEXTURED_QUAD:
			{
				struct render_command_textured_quad *command = CONTAINER_OF(
					base_command, struct render_command_textured_quad, base);
				m4x4 transform = command->transform;
				u32 texture = command->texture;

				gl.UniformMatrix4fv(renderer->shader.model, 1, GL_FALSE, transform.e);
				gl.BindVertexArray(renderer->vertex_array);
				gl.BindTexture(GL_TEXTURE_2D, texture);
				gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

				push_buffer += sizeof(*command);
			}
			break;

		case RENDER_QUADS:
			{
				struct render_command_quads *command = CONTAINER_OF(
					base_command, struct render_command_quads, base);

				gl.BindVertexArray(renderer->vertex_array);
				gl.BindTexture(GL_TEXTURE_2D, command->texture);
				gl.DrawElements(GL_TRIANGLES, command->quad_count * 6,
					GL_UNSIGNED_INT, (const void *)command->index_offset);

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
render_textured_quad(struct render_command_buffer *cmd_buffer,
	m4x4 transform, u32 texture)
{
	struct render_command_textured_quad *command =
		push_command(cmd_buffer, RENDER_TEXTURED_QUAD);

	command->transform = transform;
	command->texture = texture;
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

	cmd_buffer->index_count += 6;
	cmd_buffer->vertex_count += 4;
}
