#ifndef GL_H
#define GL_H

#define GL_FALSE 0
#define GL_TRUE 1

#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_TEXTURE4 0x84C4
#define GL_TEXTURE5 0x84C5
#define GL_TEXTURE6 0x84C6
#define GL_TEXTURE7 0x84C7
#define GL_TEXTURE8 0x84C8
#define GL_TEXTURE9 0x84C9
#define GL_TEXTURE10 0x84CA
#define GL_TEXTURE11 0x84CB
#define GL_TEXTURE12 0x84CC
#define GL_TEXTURE13 0x84CD
#define GL_TEXTURE14 0x84CE
#define GL_TEXTURE15 0x84CF
#define GL_TEXTURE16 0x84D0
#define GL_TEXTURE17 0x84D1
#define GL_TEXTURE18 0x84D2
#define GL_TEXTURE19 0x84D3
#define GL_TEXTURE20 0x84D4
#define GL_TEXTURE21 0x84D5
#define GL_TEXTURE22 0x84D6
#define GL_TEXTURE23 0x84D7
#define GL_TEXTURE24 0x84D8
#define GL_TEXTURE25 0x84D9
#define GL_TEXTURE26 0x84DA
#define GL_TEXTURE27 0x84DB
#define GL_TEXTURE28 0x84DC
#define GL_TEXTURE29 0x84DD
#define GL_TEXTURE30 0x84DE
#define GL_TEXTURE31 0x84DF
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82

struct gl_context {
    void (*Viewport)(GLint x, GLint y, GLsizei width, GLsizei height);
    void (*Clear)(GLbitfield mask);
    void (*ClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
    void (*GenBuffers)(GLsizei n, GLuint *buffers);
    void (*BindBuffer)(GLenum target, GLuint buffer);
    void (*DeleteBuffers)(GLsizei n, const GLuint *buffers);
    void (*BufferData)(GLenum target, GLsizeiptr size, const void *data, 
            GLenum usage);
    void (*GenVertexArrays)(GLsizei n, GLuint *arrays);
    void (*DeleteVertexArrays)(GLsizei n, const GLuint *arrays);
    void (*BindVertexArray)(GLuint array);
    void (*VertexAttribPointer)(GLuint index, GLint size, GLenum type, 
            GLboolean normalized, GLsizei stride, const void *pointer);
    void (*EnableVertexAttribArray)(GLuint index);
    GLuint (*CreateShader)(GLenum type);
    void (*ShaderSource)(GLuint shader, GLsizei count, 
            const GLchar *const *string, const GLint *length);
    void (*CompileShader)(GLuint shader);
    void (*GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
    void (*GetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length,
            GLchar *infoLog);
    void (*DeleteShader)(GLuint shader);
    GLuint (*CreateProgram)(void);
    void (*AttachShader)(GLuint program, GLuint shader);
    void (*LinkProgram)(GLuint program);
    void (*GetProgramiv)(GLuint program, GLenum pname, GLint *params);
    void (*GetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length,
            GLchar *infoLog);
    void (*UseProgram)(GLuint program);
    void (*DeleteProgram)(GLuint program);
    void (*DrawArrays)(GLenum mode, GLint first, GLsizei count);
    void (*DrawElements)(GLenum mode, GLsizei count, GLenum type, 
            const void *indices);
    void (*GenTextures)(GLsizei n, GLuint *textures);
    void (*TexImage2D)(GLenum target, GLint level, GLint internalformat,
            GLsizei width, GLsizei height, GLint border, GLenum format, 
            GLenum type, const void *pixels);
    void (*DeleteTextures)(GLsizei n, const GLuint *textures);
    void (*BindTexture)(GLenum target, GLuint texture);
    void (*ActiveTexture)(GLenum texture);
    void (*TexParameteri)(GLenum target, GLenum pname, GLint param);
    void (*GenerateMipmap)(GLenum target);
    GLint (*GetUniformLocation)(GLuint program, const GLchar *name);
    void (*Uniform1f)(GLint location, GLfloat v0);
    void (*Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
    void (*Uniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void (*Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2,
            GLfloat v3);

    GLXContext context;
};

struct x11_window;

static struct gl_context gl;

static i32 gl_context_init(struct gl_context *context, 
        const struct x11_window *window);
static void gl_context_finish(struct gl_context *context,
        const struct x11_window *window);

static u32 gl_shader_create(const u8 *src, u32 type);
static u32 gl_program_create(const u8 *vert_shader_source,
        const u8 *frag_shader_source);

#endif /* GL_H */ 
