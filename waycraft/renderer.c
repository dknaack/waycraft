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

static void
renderer_begin_frame(struct renderer *renderer, m4x4 view, m4x4 projection)
{
	m4x4 model = m4x4_id(1);

	gl.ClearColor(0.45, 0.65, 0.85, 1.0);
	gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gl.UseProgram(renderer->shader.program);
	gl.UniformMatrix4fv(renderer->shader.model, 1, GL_FALSE, model.e);
	gl.UniformMatrix4fv(renderer->shader.projection, 1, GL_FALSE, projection.e);
	gl.UniformMatrix4fv(renderer->shader.view, 1, GL_FALSE, view.e);
}

static void
render_textured_quad(struct renderer *renderer, m4x4 transform, u32 texture)
{
	gl.UniformMatrix4fv(renderer->shader.model, 1, GL_FALSE, transform.e);
	gl.BindTexture(GL_TEXTURE_2D, texture);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
