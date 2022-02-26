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
    void (*DeleteShader)(GLuint shader);
    GLuint (*CreateProgram)(void);
    void (*AttachShader)(GLuint program, GLuint shader);
    void (*LinkProgram)(GLuint program);
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

    GLXContext context;
};

struct x11_window;

i32 gl_context_init(struct gl_context *context, 
        const struct x11_window *window);
void gl_context_finish(struct gl_context *context,
        const struct x11_window *window);

#endif /* GL_H */ 
