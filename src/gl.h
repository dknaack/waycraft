#ifndef GL_H
#define GL_H

#define GL_ARRAY_BUFFER 0x8892
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
    GLuint (*CreateShader)(GLenum type);
    GLuint (*CreateProgram)(void);
    void (*AttachShader)(GLuint program, GLuint shader);
    void (*CompileShader)(GLuint shader);

    GLXContext context;
};

struct x11_window;

i32 gl_context_init(struct gl_context *context, 
        const struct x11_window *window);
void gl_context_finish(struct gl_context *context,
        const struct x11_window *window);

#endif /* GL_H */ 
