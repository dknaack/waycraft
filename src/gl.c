#include <GL/gl.h>
#include <GL/glx.h>

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
    "glBindVertexArrays",
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
};

static i32
gl_context_init(struct gl_context *gl, const struct x11_window *window)
{
    int attributes[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    };

    Display *display = window->display;
    XVisualInfo *visual = glXChooseVisual(display, 0, attributes);
    gl->context = glXCreateContext(display, visual, 0, True);
    glXMakeCurrent(display, window->drawable, gl->context);

    void (**gl_functions)(void);
    *(void **)&gl_functions = gl;

    for (i32 i = 0; i < LENGTH(gl_function_names); i++) {
        gl_functions[i] = glXGetProcAddress((u8 *)gl_function_names[i]);
        if (!gl_functions[i]) {
            return -1;
        }
    }

    return 0;
}

static void
gl_context_finish(struct gl_context *gl, const struct x11_window *window)
{
    glXDestroyContext(window->display, gl->context);
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
