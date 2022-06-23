static void
gl_shader_error(u32 shader, u8 *buffer, u32 size)
{
	gl.GetShaderInfoLog(shader, size - 1, 0, (char *)buffer);
}

static u32
gl_shader_create(const u8 *src, u32 type)
{
	u32 shader = gl.CreateShader(type);
	i32 success;

	gl.ShaderSource(shader, 1, (const char *const *)&src, 0);
	gl.CompileShader(shader);
	gl.GetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		u8 error[1024];
		gl_shader_error(shader, error, sizeof(error));
		fprintf(stderr, "Failed to create shader: %s\n", error);
	}

	return success ? shader : 0;
}

static u32
gl_program_create(const u8 *vert_shader_source, const u8 *frag_shader_source)
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
gl_program_error(u32 program, u8 *buffer, u32 size)
{
	gl.GetProgramInfoLog(program, size - 1, 0, (char *)buffer);
}
