#include <EGL/egl.h>
#include <GL/gl.h>

typedef void glViewport_t(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void glClear_t(GLbitfield mask);
typedef void glClearColor_t(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
typedef void glGenBuffers_t(GLsizei n, GLuint *buffers);
typedef void glBindBuffer_t(GLenum target, GLuint buffer);
typedef void glDeleteBuffers_t(GLsizei n, const GLuint *buffers);
typedef void glBufferData_t(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void glBufferSubData_t(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef void glGenVertexArrays_t(GLsizei n, GLuint *arrays);
typedef void glDeleteVertexArrays_t(GLsizei n, const GLuint *arrays);
typedef void glBindVertexArray_t(GLuint array);
typedef void glVertexAttribPointer_t(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void glEnableVertexAttribArray_t(GLuint index);
typedef GLuint glCreateShader_t(GLenum type);
typedef void glShaderSource_t(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
typedef void glCompileShader_t(GLuint shader);
typedef void glGetShaderiv_t(GLuint shader, GLenum pname, GLint *params);
typedef void glGetShaderInfoLog_t(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void glDeleteShader_t(GLuint shader);
typedef GLuint glCreateProgram_t(void);
typedef void glAttachShader_t(GLuint program, GLuint shader);
typedef void glLinkProgram_t(GLuint program);
typedef void glGetProgramiv_t(GLuint program, GLenum pname, GLint *params);
typedef void glGetProgramInfoLog_t(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void glUseProgram_t(GLuint program);
typedef void glDeleteProgram_t(GLuint program);
typedef void glDrawArrays_t(GLenum mode, GLint first, GLsizei count);
typedef void glDrawElements_t(GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void glGenTextures_t(GLsizei n, GLuint *textures);
typedef void glTexImage2D_t(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void glDeleteTextures_t(GLsizei n, const GLuint *textures);
typedef void glBindTexture_t(GLenum target, GLuint texture);
typedef void glActiveTexture_t(GLenum texture);
typedef void glTexParameteri_t(GLenum target, GLenum pname, GLint param);
typedef void glGenerateMipmap_t(GLenum target);
typedef GLint glGetUniformLocation_t(GLuint program, const GLchar *name);
typedef void glUniform1i_t(GLint location, GLint v0);
typedef void glUniform1f_t(GLint location, GLfloat v0);
typedef void glUniform2f_t(GLint location, GLfloat v0, GLfloat v1);
typedef void glUniform3f_t(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void glUniform4f_t(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void glUniformMatrix4fv_t(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void glEnable_t(GLenum cap);
typedef void glDisable_t(GLenum cap);
typedef void glCullFace_t(GLenum mode);
typedef void glEGLImageTargetTexture2DOES_t(GLenum target, EGLImage image);
typedef void glBlendFunc_t(GLenum sfactor, GLenum dfactor);
typedef void glPolygonMode_t(GLenum face, GLenum mode);
typedef void glLineWidth_t(GLfloat width);
typedef GLenum glGetError_t(void);

#define OPENGL_MAP_FUNCTIONS() \
	X(Viewport) \
	X(Clear) \
	X(ClearColor) \
	X(GenBuffers) \
	X(BindBuffer) \
	X(DeleteBuffers) \
	X(BufferData) \
	X(BufferSubData) \
	X(GenVertexArrays) \
	X(DeleteVertexArrays) \
	X(BindVertexArray) \
	X(VertexAttribPointer) \
	X(EnableVertexAttribArray) \
	X(CreateShader) \
	X(ShaderSource) \
	X(CompileShader) \
	X(GetShaderiv) \
	X(GetShaderInfoLog) \
	X(DeleteShader) \
	X(CreateProgram) \
	X(AttachShader) \
	X(LinkProgram) \
	X(GetProgramiv) \
	X(GetProgramInfoLog) \
	X(UseProgram) \
	X(DeleteProgram) \
	X(DrawArrays) \
	X(DrawElements) \
	X(GenTextures) \
	X(TexImage2D) \
	X(DeleteTextures) \
	X(BindTexture) \
	X(ActiveTexture) \
	X(TexParameteri) \
	X(GenerateMipmap) \
	X(GetUniformLocation) \
	X(Uniform1i) \
	X(Uniform1f) \
	X(Uniform2f) \
	X(Uniform3f) \
	X(Uniform4f) \
	X(UniformMatrix4fv) \
	X(Enable) \
	X(Disable) \
	X(CullFace) \
	X(EGLImageTargetTexture2DOES) \
	X(BlendFunc) \
	X(PolygonMode) \
	X(LineWidth) \
	X(GetError)

struct opengl_api {
#define X(name) gl##name##_t *name;
	OPENGL_MAP_FUNCTIONS()
#undef X
};

struct x11_window;

typedef void gl_proc_t(void);
typedef gl_proc_t *gl_get_proc_address_t(const u8 *proc_name);

static struct opengl_api gl;
