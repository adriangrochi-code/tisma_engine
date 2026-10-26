#pragma once

// The OpenGL 3.3 core functions and constants the interface uses (its 3D view
// and Dear ImGui's OpenGL 3 backend), loaded at run time through GLFW. Names,
// signatures and values are those of the OpenGL 3.3 core specification
// (Khronos). Own loader, so the program needs no OpenGL import library and no
// third-party loader; see PROVENANCE.md.

#include <cstddef>

#if defined(_WIN32) && !defined(_WIN64)
#define TISMA_GLAPI __stdcall
#else
#define TISMA_GLAPI
#endif

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLint = int;
using GLsizei = int;
using GLuint = unsigned int;
using GLfloat = float;
using GLchar = char;
using GLubyte = unsigned char;
using GLvoid = void;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;
using GLuint64 = unsigned long long;

// Versions this loader provides (Dear ImGui's backend checks for them).
#define GL_VERSION_3_1 1
#define GL_VERSION_3_2 1
#define GL_VERSION_3_3 1

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0
#define GL_ONE 1
#define GL_LINES 0x0001
#define GL_TRIANGLES 0x0004
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_TIME_ELAPSED 0x88BF
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_LESS 0x0201
#define GL_LEQUAL 0x0203
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_CCW 0x0901
#define GL_CULL_FACE 0x0B44
#define GL_POLYGON_MODE 0x0B40
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_VIEWPORT 0x0BA2
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_BOX 0x0C10
#define GL_SCISSOR_TEST 0x0C11
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_FILL 0x1B02
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_FUNC_ADD 0x8006
#define GL_BLEND_EQUATION_RGB 0x8009
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_MULTISAMPLE 0x809D
#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_ALPHA 0x80CA
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C
#define GL_NUM_EXTENSIONS 0x821D
#define GL_TEXTURE0 0x84C0
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE 0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE 0x8625
#define GL_VERTEX_ATTRIB_ARRAY_POINTER 0x8645
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED 0x886A
#define GL_BLEND_EQUATION_ALPHA 0x883D
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#define GL_SAMPLER_BINDING 0x8919
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_PRIMITIVE_RESTART 0x8F9D
#define GL_DEPTH_COMPONENT 0x1902
#define GL_RED 0x1903
#define GL_RGBA8 0x8058
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_RG 0x8227
#define GL_R8 0x8229
#define GL_RG16F 0x822F
#define GL_RGBA32F 0x8814
#define GL_RGBA16F 0x881A
#define GL_HALF_FLOAT 0x140B
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_MAX_SAMPLES 0x8D57
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x00000002
#define GL_CONTEXT_PROFILE_MASK 0x9126

// X(return type, name, parameters)
#define TISMA_GL_FUNCTIONS(X)                                                                                        \
    X(void, glActiveTexture, (GLenum texture))                                                                       \
    X(void, glBindFramebuffer, (GLenum target, GLuint framebuffer))                                                  \
    X(void, glBindRenderbuffer, (GLenum target, GLuint renderbuffer))                                                \
    X(void, glBlitFramebuffer, (GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0, GLint dx1,        \
                                GLint dy1, GLbitfield mask, GLenum filter))                                          \
    X(GLenum, glCheckFramebufferStatus, (GLenum target))                                                             \
    X(void, glDeleteFramebuffers, (GLsizei n, const GLuint* framebuffers))                                           \
    X(void, glDeleteRenderbuffers, (GLsizei n, const GLuint* renderbuffers))                                         \
    X(void, glFramebufferRenderbuffer, (GLenum target, GLenum attachment, GLenum renderbuffer_target,              \
                                        GLuint renderbuffer))                                                        \
    X(void, glFramebufferTexture2D, (GLenum target, GLenum attachment, GLenum texture_target, GLuint texture,       \
                                     GLint level))                                                                   \
    X(void, glGenFramebuffers, (GLsizei n, GLuint* framebuffers))                                                    \
    X(void, glGenRenderbuffers, (GLsizei n, GLuint* renderbuffers))                                                  \
    X(void, glRenderbufferStorageMultisample, (GLenum target, GLsizei samples, GLenum internal_format,              \
                                               GLsizei width, GLsizei height))                                       \
    X(void, glUniform2fv, (GLint location, GLsizei count, const GLfloat* value))                                      \
    X(void, glUniform2i, (GLint location, GLint v0, GLint v1))                                                       \
    X(void, glUniform4fv, (GLint location, GLsizei count, const GLfloat* value))                                     \
    X(void, glAttachShader, (GLuint program, GLuint shader))                                                         \
    X(void, glBindBuffer, (GLenum target, GLuint buffer))                                                            \
    X(void, glBindSampler, (GLuint unit, GLuint sampler))                                                            \
    X(void, glBindTexture, (GLenum target, GLuint texture))                                                          \
    X(void, glBindVertexArray, (GLuint array))                                                                       \
    X(void, glBlendEquation, (GLenum mode))                                                                          \
    X(void, glBlendEquationSeparate, (GLenum rgb, GLenum alpha))                                                     \
    X(void, glBlendFunc, (GLenum source, GLenum destination))                                                        \
    X(void, glBlendFuncSeparate, (GLenum source_rgb, GLenum destination_rgb, GLenum source_alpha,                   \
                                  GLenum destination_alpha))                                                         \
    X(void, glBufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage))                         \
    X(void, glBufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, const void* data))                   \
    X(void, glClear, (GLbitfield mask))                                                                              \
    X(void, glClearColor, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))                                \
    X(void, glCompileShader, (GLuint shader))                                                                        \
    X(GLuint, glCreateProgram, (void))                                                                               \
    X(GLuint, glCreateShader, (GLenum type))                                                                         \
    X(void, glDeleteBuffers, (GLsizei n, const GLuint* buffers))                                                     \
    X(void, glDeleteProgram, (GLuint program))                                                                       \
    X(void, glDeleteSamplers, (GLsizei n, const GLuint* samplers))                                                   \
    X(void, glDeleteShader, (GLuint shader))                                                                         \
    X(void, glDeleteTextures, (GLsizei n, const GLuint* textures))                                                   \
    X(void, glDeleteVertexArrays, (GLsizei n, const GLuint* arrays))                                                 \
    X(void, glDepthFunc, (GLenum function))                                                                          \
    X(void, glDepthMask, (GLboolean flag))                                                                           \
    X(void, glDetachShader, (GLuint program, GLuint shader))                                                         \
    X(void, glDisable, (GLenum capability))                                                                          \
    X(void, glDisableVertexAttribArray, (GLuint index))                                                              \
    X(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count))                                                 \
    X(void, glDrawArraysInstanced, (GLenum mode, GLint first, GLsizei count, GLsizei instances))                    \
    X(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const void* indices))                         \
    X(void, glDrawElementsBaseVertex, (GLenum mode, GLsizei count, GLenum type, const void* indices,                \
                                       GLint base_vertex))                                                           \
    X(void, glEnable, (GLenum capability))                                                                           \
    X(void, glEnableVertexAttribArray, (GLuint index))                                                               \
    X(void, glFinish, (void))                                                                                        \
    X(void, glGenQueries, (GLsizei n, GLuint* ids))                                                                  \
    X(void, glDeleteQueries, (GLsizei n, const GLuint* ids))                                                         \
    X(void, glBeginQuery, (GLenum target, GLuint id))                                                                \
    X(void, glEndQuery, (GLenum target))                                                                             \
    X(void, glGetQueryObjectiv, (GLuint id, GLenum name, GLint* value))                                              \
    X(void, glGetQueryObjectui64v, (GLuint id, GLenum name, GLuint64* value))                                        \
    X(void, glGenBuffers, (GLsizei n, GLuint* buffers))                                                              \
    X(void, glGenSamplers, (GLsizei n, GLuint* samplers))                                                            \
    X(void, glGenTextures, (GLsizei n, GLuint* textures))                                                            \
    X(void, glGenVertexArrays, (GLsizei n, GLuint* arrays))                                                          \
    X(GLint, glGetAttribLocation, (GLuint program, const GLchar* name))                                              \
    X(GLenum, glGetError, (void))                                                                                    \
    X(void, glGetIntegerv, (GLenum name, GLint* data))                                                               \
    X(void, glGetProgramInfoLog, (GLuint program, GLsizei size, GLsizei* length, GLchar* log))                      \
    X(void, glGetProgramiv, (GLuint program, GLenum name, GLint* value))                                            \
    X(void, glGetShaderInfoLog, (GLuint shader, GLsizei size, GLsizei* length, GLchar* log))                        \
    X(void, glGetShaderiv, (GLuint shader, GLenum name, GLint* value))                                              \
    X(const GLubyte*, glGetString, (GLenum name))                                                                    \
    X(const GLubyte*, glGetStringi, (GLenum name, GLuint index))                                                     \
    X(GLint, glGetUniformLocation, (GLuint program, const GLchar* name))                                             \
    X(GLboolean, glIsEnabled, (GLenum capability))                                                                   \
    X(GLboolean, glIsProgram, (GLuint program))                                                                      \
    X(void, glLinkProgram, (GLuint program))                                                                         \
    X(void, glPixelStorei, (GLenum name, GLint value))                                                               \
    X(void, glPolygonMode, (GLenum face, GLenum mode))                                                               \
    X(void, glPolygonOffset, (GLfloat factor, GLfloat units))                                                        \
    X(void, glReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,             \
                           void* data))                                                                              \
    X(void, glSamplerParameteri, (GLuint sampler, GLenum name, GLint value))                                         \
    X(void, glScissor, (GLint x, GLint y, GLsizei width, GLsizei height))                                           \
    X(void, glShaderSource, (GLuint shader, GLsizei count, const GLchar* const* text, const GLint* length))         \
    X(void, glTexImage2D, (GLenum target, GLint level, GLint internal_format, GLsizei width, GLsizei height,        \
                           GLint border, GLenum format, GLenum type, const void* pixels))                            \
    X(void, glTexParameteri, (GLenum target, GLenum name, GLint value))                                              \
    X(void, glTexSubImage2D, (GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,          \
                              GLenum format, GLenum type, const void* pixels))                                       \
    X(void, glUniform1f, (GLint location, GLfloat v0))                                                               \
    X(void, glUniform1i, (GLint location, GLint v0))                                                                 \
    X(void, glUniform1ui, (GLint location, GLuint v0))                                                               \
    X(void, glUniform2f, (GLint location, GLfloat v0, GLfloat v1))                                                   \
    X(void, glUniform2ui, (GLint location, GLuint v0, GLuint v1))                                                    \
    X(void, glUniform3f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2))                                       \
    X(void, glUniform3fv, (GLint location, GLsizei count, const GLfloat* value))                                     \
    X(void, glUniform4f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3))                           \
    X(void, glUniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value))         \
    X(void, glUseProgram, (GLuint program))                                                                          \
    X(void, glVertexAttribDivisor, (GLuint index, GLuint divisor))                                                   \
    X(void, glVertexAttribIPointer, (GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer))  \
    X(void, glVertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,    \
                                    const void* pointer))                                                            \
    X(void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height))

// Each function is a pointer named tisma_<name>, and <name> is a macro for it,
// so nothing clashes with a system OpenGL header.
#define TISMA_GL_DECLARE(ret, name, params)                                                                          \
    using PFN_##name = ret(TISMA_GLAPI*) params;                                                                     \
    extern PFN_##name tisma_##name;
TISMA_GL_FUNCTIONS(TISMA_GL_DECLARE)
#undef TISMA_GL_DECLARE

#define glActiveTexture tisma_glActiveTexture
#define glBindFramebuffer tisma_glBindFramebuffer
#define glBindRenderbuffer tisma_glBindRenderbuffer
#define glBlitFramebuffer tisma_glBlitFramebuffer
#define glCheckFramebufferStatus tisma_glCheckFramebufferStatus
#define glDeleteFramebuffers tisma_glDeleteFramebuffers
#define glDeleteRenderbuffers tisma_glDeleteRenderbuffers
#define glFramebufferRenderbuffer tisma_glFramebufferRenderbuffer
#define glFramebufferTexture2D tisma_glFramebufferTexture2D
#define glGenFramebuffers tisma_glGenFramebuffers
#define glGenRenderbuffers tisma_glGenRenderbuffers
#define glRenderbufferStorageMultisample tisma_glRenderbufferStorageMultisample
#define glUniform2fv tisma_glUniform2fv
#define glUniform2i tisma_glUniform2i
#define glUniform4fv tisma_glUniform4fv
#define glAttachShader tisma_glAttachShader
#define glBindBuffer tisma_glBindBuffer
#define glBindSampler tisma_glBindSampler
#define glBindTexture tisma_glBindTexture
#define glBindVertexArray tisma_glBindVertexArray
#define glBlendEquation tisma_glBlendEquation
#define glBlendEquationSeparate tisma_glBlendEquationSeparate
#define glBlendFunc tisma_glBlendFunc
#define glBlendFuncSeparate tisma_glBlendFuncSeparate
#define glBufferData tisma_glBufferData
#define glBufferSubData tisma_glBufferSubData
#define glClear tisma_glClear
#define glClearColor tisma_glClearColor
#define glCompileShader tisma_glCompileShader
#define glCreateProgram tisma_glCreateProgram
#define glCreateShader tisma_glCreateShader
#define glDeleteBuffers tisma_glDeleteBuffers
#define glDeleteProgram tisma_glDeleteProgram
#define glDeleteSamplers tisma_glDeleteSamplers
#define glDeleteShader tisma_glDeleteShader
#define glDeleteTextures tisma_glDeleteTextures
#define glDeleteVertexArrays tisma_glDeleteVertexArrays
#define glDepthFunc tisma_glDepthFunc
#define glDepthMask tisma_glDepthMask
#define glDetachShader tisma_glDetachShader
#define glDisable tisma_glDisable
#define glDisableVertexAttribArray tisma_glDisableVertexAttribArray
#define glDrawArrays tisma_glDrawArrays
#define glDrawArraysInstanced tisma_glDrawArraysInstanced
#define glDrawElements tisma_glDrawElements
#define glDrawElementsBaseVertex tisma_glDrawElementsBaseVertex
#define glEnable tisma_glEnable
#define glEnableVertexAttribArray tisma_glEnableVertexAttribArray
#define glFinish tisma_glFinish
#define glGenQueries tisma_glGenQueries
#define glDeleteQueries tisma_glDeleteQueries
#define glBeginQuery tisma_glBeginQuery
#define glEndQuery tisma_glEndQuery
#define glGetQueryObjectiv tisma_glGetQueryObjectiv
#define glGetQueryObjectui64v tisma_glGetQueryObjectui64v
#define glGenBuffers tisma_glGenBuffers
#define glGenSamplers tisma_glGenSamplers
#define glGenTextures tisma_glGenTextures
#define glGenVertexArrays tisma_glGenVertexArrays
#define glGetAttribLocation tisma_glGetAttribLocation
#define glGetError tisma_glGetError
#define glGetIntegerv tisma_glGetIntegerv
#define glGetProgramInfoLog tisma_glGetProgramInfoLog
#define glGetProgramiv tisma_glGetProgramiv
#define glGetShaderInfoLog tisma_glGetShaderInfoLog
#define glGetShaderiv tisma_glGetShaderiv
#define glGetString tisma_glGetString
#define glGetStringi tisma_glGetStringi
#define glGetUniformLocation tisma_glGetUniformLocation
#define glIsEnabled tisma_glIsEnabled
#define glIsProgram tisma_glIsProgram
#define glLinkProgram tisma_glLinkProgram
#define glPixelStorei tisma_glPixelStorei
#define glPolygonMode tisma_glPolygonMode
#define glPolygonOffset tisma_glPolygonOffset
#define glReadPixels tisma_glReadPixels
#define glSamplerParameteri tisma_glSamplerParameteri
#define glScissor tisma_glScissor
#define glShaderSource tisma_glShaderSource
#define glTexImage2D tisma_glTexImage2D
#define glTexParameteri tisma_glTexParameteri
#define glTexSubImage2D tisma_glTexSubImage2D
#define glUniform1f tisma_glUniform1f
#define glUniform1i tisma_glUniform1i
#define glUniform1ui tisma_glUniform1ui
#define glUniform2f tisma_glUniform2f
#define glUniform2ui tisma_glUniform2ui
#define glUniform3f tisma_glUniform3f
#define glUniform3fv tisma_glUniform3fv
#define glUniform4f tisma_glUniform4f
#define glUniformMatrix4fv tisma_glUniformMatrix4fv
#define glUseProgram tisma_glUseProgram
#define glVertexAttribDivisor tisma_glVertexAttribDivisor
#define glVertexAttribIPointer tisma_glVertexAttribIPointer
#define glVertexAttribPointer tisma_glVertexAttribPointer
#define glViewport tisma_glViewport

namespace tisma::ui {

// Loads every function with `get` (glfwGetProcAddress). Returns the name of
// the first one missing, or null when all are there.
using GlGetProc = void (*(*)(const char*))();
const char* load_gl(GlGetProc get);

} // namespace tisma::ui
