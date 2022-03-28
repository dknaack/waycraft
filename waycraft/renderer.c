#include <waycraft/gl.h>
#include <waycraft/renderer.h>

static void
renderer_init(struct renderer *renderer)
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
	struct render_command_buffer *command_buffer = &renderer->command_buffer;

	command_buffer->command_count = 0;
	command_buffer->push_buffer_size = 0;
	return command_buffer;
}

static void
renderer_end_frame(struct renderer *renderer,
	struct render_command_buffer *command_buffer)
{
	u32 command_count = command_buffer->command_count;
	u8 *push_buffer = command_buffer->push_buffer;

	m4x4 model = m4x4_id(1);
	m4x4 view = command_buffer->transform.view;
	m4x4 projection = command_buffer->transform.projection;

	gl.UseProgram(renderer->shader.program);
	gl.UniformMatrix4fv(renderer->shader.model, 1, GL_FALSE, model.e);
	gl.UniformMatrix4fv(renderer->shader.projection, 1, GL_FALSE, projection.e);
	gl.UniformMatrix4fv(renderer->shader.view, 1, GL_FALSE, view.e);

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
		}
	}
}

static void
render_clear(struct render_command_buffer *command_buffer, v4 color)
{
	struct render_command_clear *clear = (struct render_command_clear *)(
		command_buffer->push_buffer + command_buffer->push_buffer_size);

	clear->base.type = RENDER_CLEAR;
	clear->color = color;

	command_buffer->push_buffer_size += sizeof(*clear);
	command_buffer->command_count++;
}

static void
render_textured_quad(struct render_command_buffer *command_buffer,
	m4x4 transform, u32 texture)
{
	struct render_command_textured_quad *command = (struct render_command_textured_quad *)(
		command_buffer->push_buffer + command_buffer->push_buffer_size);

	command->base.type = RENDER_TEXTURED_QUAD;
	command->transform = transform;
	command->texture = texture;

	command_buffer->push_buffer_size += sizeof(*command);
	command_buffer->command_count++;
}
