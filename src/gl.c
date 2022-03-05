#include "x11.h"
#include "gl.h"

static const char *gl_function_names[] = {
    "glViewport",
    "glClear",
    "glClearColor",
    "glGenBuffers",
    "glBindBuffer",
    "glDeleteBuffers",
    "glBufferData",
    "glGenVertexArrays",
    "glDeleteVertexArrays",
    "glBindVertexArray",
    "glVertexAttribPointer",
    "glEnableVertexAttribArray",
    "glCreateShader",
    "glShaderSource",
    "glCompileShader",
    "glGetShaderiv",
    "glGetShaderInfoLog",
    "glDeleteShader",
    "glCreateProgram",
    "glAttachShader",
    "glLinkProgram",
    "glGetProgramiv",
    "glGetProgramInfoLog",
    "glUseProgram",
    "glDeleteProgram",
    "glDrawArrays",
    "glDrawElements",
    "glGenTextures",
    "glTexImage2D",
    "glDeleteTextures",
    "glBindTexture",
    "glActiveTexture",
    "glTexParameteri",
    "glGenerateMipmap",
    "glGetUniformLocation",
    "glUniform1f",
    "glUniform2f",
    "glUniform3f",
    "glUniform4f",
    "glUniformMatrix4fv",
    "glEnable",
    "glDisable",
    "glCullFace",
};

typedef void gl_proc_t(void);

static i32
gl_context_init(struct gl_context *gl, 
        gl_proc_t *(*get_proc_address)(const u8 *proc_name))
{

    gl_proc_t **gl_functions;
    *(void **)&gl_functions = gl;

    for (i32 i = 0; i < LENGTH(gl_function_names); i++) {
        gl_functions[i] = get_proc_address((const u8 *)gl_function_names[i]);
        if (!gl_functions[i]) {
            return -1;
        }
    }

    return 0;
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

static void
gl_shader_error(u32 shader, u8 *buffer, u32 size)
{
    gl.GetShaderInfoLog(shader, size - 1, 0, (char *)buffer);
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
