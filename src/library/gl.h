#ifndef _BOLT_LIBRARY_GL_H_
#define _BOLT_LIBRARY_GL_H_

#include <stdbool.h>
#include <stdint.h>

#if defined(VERBOSE)
#include <stdio.h>
void _bolt_gl_set_logfile(FILE* f);
#endif

/* types from gl.h */
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;

/* types from extensions, not actually defined in any header yet still referenced frequently in docs */
typedef char GLchar;
typedef intptr_t GLintptr;
typedef uintptr_t GLsizeiptr;

/* consts used from libgl */
#define GL_NONE 0
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_ALPHA 770
#define GL_ONE_MINUS_SRC_ALPHA 771
#define GL_TEXTURE 5890
#define GL_TEXTURE_2D 3553
#define GL_TEXTURE_2D_MULTISAMPLE 37120
#define GL_RGB 6407
#define GL_RGBA 6408
#define GL_BGRA 32993
#define GL_UNSIGNED_BYTE 5121
#define GL_UNSIGNED_SHORT 5123
#define GL_UNSIGNED_INT 5125
#define GL_BYTE 5120
#define GL_SHORT 5122
#define GL_INT 5124
#define GL_FLOAT 5126
#define GL_DOUBLE 5130
#define GL_HALF_FLOAT 5131
#define GL_TRIANGLES 4
#define GL_TRIANGLE_STRIP 5
#define GL_MAP_READ_BIT 1
#define GL_MAP_WRITE_BIT 2
#define GL_MAP_FLUSH_EXPLICIT_BIT 16
#define GL_TEXTURE_MAG_FILTER 10240
#define GL_TEXTURE_MIN_FILTER 10241
#define GL_TEXTURE_WRAP_S 10242
#define GL_TEXTURE_WRAP_T 10243
#define GL_CLAMP_TO_EDGE 33071
#define GL_DEPTH_TEST 2929
#define GL_SCISSOR_TEST 3089
#define GL_CULL_FACE 2884
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 33776
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 33777
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 33778
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 33779
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 35916
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 35917
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 35918
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 35919
#define GL_ACTIVE_TEXTURE 34016
#define GL_STATIC_DRAW 35044
#define GL_FRAGMENT_SHADER 35632
#define GL_VERTEX_SHADER 35633
#define GL_FRAMEBUFFER 36160
#define GL_READ_FRAMEBUFFER 36008
#define GL_DRAW_FRAMEBUFFER 36009
#define GL_ARRAY_BUFFER 34962
#define GL_ELEMENT_ARRAY_BUFFER 34963
#define GL_UNIFORM_BUFFER 35345
#define GL_VERTEX_ARRAY_BINDING 34229
#define GL_ARRAY_BUFFER_BINDING 34964
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 34965
#define GL_UNIFORM_BUFFER_BINDING 35368
#define GL_UNIFORM_OFFSET 35387
#define GL_UNIFORM_BLOCK_BINDING 35391
#define GL_TEXTURE0 33984
#define GL_COLOR_ATTACHMENT0 36064
#define GL_NEAREST 9728
#define GL_COLOR_BUFFER_BIT 16384
#define GL_RGBA8 32856
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE 36048
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME 36049
#define GL_MAX_VERTEX_ATTRIBS 34921
#define GL_TEXTURE_COMPARE_MODE 34892
#define GL_COMPILE_STATUS 35713
#define GL_LINK_STATUS 35714

/*
What's the difference between GLProcFunctions and GLLibFunctions? Well, I'm glad you asked.

On Windows, OpenGL functions prior to 2.0 are exposed in a microsoft-provided library called
"opengl32.dll" in System32, and functions after 2.0 are loaded from wgl (or egl) GetProcAddress.
GLLibFunctions represents the former category and GLProcFunctions the latter.

To this day, there remain to be two different methods of loading OpenGL functions. On platforms
other than Windows, there is no opengl32.dll and it doesn't seem to matter what method you load any
function by, and it also doesn't make any difference on Windows if using an AMD GPU, because AMD's
drivers allow GetProcAddress to work for any function.

However this is not the case on Windows with Nvidia drivers. Windows+Nvidia users will complain of
segmentation faults due to these functions failing to load if they're placed in the wrong category.

Additionally, the game still loads these functions by the two different methods even on Linux, even
though it doesn't need to. To hook functions, we need to know in advance which method the game will
use to load them. So if a function is expected to be hooked, but is placed in the wrong category,
then the hook won't get hit - on any platform.

So, to add a new OpenGL function, first run `dumpbin /exports C:\Windows\System32\opengl32.dll`. If
the output list contains your function, it needs to go in GLLibFunctions. If not, it needs to go in
GLProcFunctions.
*/

/// Struct representing all the OpenGL functions of interest to us that the game gets from GetProcAddress
struct GLProcFunctions {
    void (*ActiveTexture)(GLenum);
    void (*AttachShader)(GLuint, GLuint);
    void (*BindAttribLocation)(GLuint, GLuint, const GLchar*);
    void (*BindBuffer)(GLenum, GLuint);
    void (*BindFramebuffer)(GLenum, GLuint);
    void (*BindVertexArray)(GLuint);
    void (*BlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum);
    void (*BlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
    void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
    void (*BufferStorage)(GLenum, GLsizeiptr, const void*, GLbitfield);
    void (*BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*);
    void (*CompileShader)(GLuint);
    void (*CompressedTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void*);
    void (*CopyImageSubData)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei);
    GLuint (*CreateProgram)(void);
    GLuint (*CreateShader)(GLenum);
    void (*DeleteBuffers)(GLsizei, const GLuint*);
    void (*DeleteFramebuffers)(GLsizei, const GLuint*);
    void (*DeleteProgram)(GLuint);
    void (*DeleteShader)(GLuint);
    void (*DeleteVertexArrays)(GLsizei, const GLuint*);
    void (*DetachShader)(GLuint, GLuint);
    void (*DisableVertexAttribArray)(GLuint);
    void (*EnableVertexAttribArray)(GLuint);
    void (*FlushMappedBufferRange)(GLenum, GLintptr, GLsizeiptr);
    void (*FramebufferTexture)(GLenum, GLenum, GLuint, GLint);
    void (*FramebufferTextureLayer)(GLenum, GLenum, GLuint, GLint, GLint);
    void (*GenBuffers)(GLsizei, GLuint*);
    void (*GenFramebuffers)(GLsizei, GLuint*);
    void (*GenVertexArrays)(GLsizei, GLuint*);
    void (*GetActiveUniformBlockiv)(GLuint, GLuint, GLenum, GLint*);
    void (*GetActiveUniformsiv)(GLuint, GLsizei, const GLuint*, GLenum, GLint*);
    void (*GetFramebufferAttachmentParameteriv)(GLenum, GLenum, GLenum, GLint*);
    void (*GetIntegeri_v)(GLenum, GLuint, GLint*);
    void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    void (*GetProgramiv)(GLuint, GLenum, GLint*);
    void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    void (*GetShaderiv)(GLuint,GLenum, GLint*);
    GLuint (*GetUniformBlockIndex)(GLuint, const GLchar*);
    void (*GetUniformfv)(GLuint, GLint, GLfloat*);
    void (*GetUniformIndices)(GLuint, GLsizei, const GLchar**, GLuint*);
    void (*GetUniformiv)(GLuint, GLint, GLint*);
    GLint (*GetUniformLocation)(GLuint, const GLchar*);
    void (*LinkProgram)(GLuint);
    void* (*MapBufferRange)(GLenum, GLintptr, GLsizeiptr, GLbitfield);
    void (*MultiDrawElements)(GLenum, const GLsizei*, GLenum, const void* const*, GLsizei);
    void (*ShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*);
    void (*TexStorage2D)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
    void (*TexStorage2DMultisample)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean);
    void (*Uniform1f)(GLint, GLfloat);
    void (*Uniform2f)(GLint, GLfloat, GLfloat);
    void (*Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
    void (*Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (*Uniform1fv)(GLint, GLsizei, const GLfloat*);
    void (*Uniform2fv)(GLint, GLsizei, const GLfloat*);
    void (*Uniform3fv)(GLint, GLsizei, const GLfloat*);
    void (*Uniform4fv)(GLint, GLsizei, const GLfloat*);
    void (*Uniform1i)(GLint, GLint);
    void (*Uniform2i)(GLint, GLint, GLint);
    void (*Uniform3i)(GLint, GLint, GLint, GLint);
    void (*Uniform4i)(GLint, GLint, GLint, GLint, GLint);
    void (*Uniform1iv)(GLint, GLsizei, const GLint*);
    void (*Uniform2iv)(GLint, GLsizei, const GLint*);
    void (*Uniform3iv)(GLint, GLsizei, const GLint*);
    void (*Uniform4iv)(GLint, GLsizei, const GLint*);
    void (*UniformMatrix2fv)(GLint, GLsizei, GLboolean, const GLfloat*);
    void (*UniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat*);
    void (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
    GLboolean (*UnmapBuffer)(GLenum);
    void (*UseProgram)(GLuint);
    void (*VertexAttribLPointer)(GLuint, GLint, GLenum, GLsizei, const void*);
    void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
};

/// Struct representing all the OpenGL functions of interest to us that the game gets from static or dynamic linkage
struct GLLibFunctions {
    void (*BindTexture)(GLenum, GLuint);
    void (*BlendFunc)(GLenum, GLenum);
    void (*Clear)(GLbitfield);
    void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (*DeleteTextures)(GLsizei, const GLuint*);
    void (*Disable)(GLenum);
    void (*DrawArrays)(GLenum, GLint, GLsizei);
    void (*DrawElements)(GLenum, GLsizei, GLenum, const void*);
    void (*Enable)(GLenum);
    void (*Flush)(void);
    void (*GenTextures)(GLsizei, GLuint*);
    void (*GetBooleanv)(GLenum, GLboolean*);
    GLenum (*GetError)(void);
    void (*GetIntegerv)(GLenum, GLint*);
    void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
    void (*TexParameteri)(GLenum, GLenum, GLint);
    void (*TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
};

void _bolt_gl_close();

/* os-level interop */
/// Call this in response to eglGetProcAddress or equivalent. If this function returns a non-NULL
/// value, return it instead of allowing GetProcAddress to run as normal.
void* _bolt_gl_GetProcAddress(const char*);

/// Call this in response to eglSwapBuffers or equivalent, before allowing the real function to run.
/// Provide the current size of the drawable area of the window.
void _bolt_gl_onSwapBuffers(uint32_t window_width, uint32_t window_height);

/// Call this in response to eglCreateContext or equivalent, if the call is successful (returns nonzero).
/// Provide a pointer returned by the OS, which will be used to identify this context. Provide the shared
/// context if any. Protect onCreateContext, onMakeCurrent and onDestroyContext with a mutex.
/// Also provide your libgl functions struct, which you should have already populated by now.
/// Finally, provide the generic GetProcAddress function.
/// The "is_important" boolean indicates whether or not this could be the main context.
void _bolt_gl_onCreateContext(void*, void*, const struct GLLibFunctions*, void* (*)(const char*), bool is_important);

/// Call this in response to eglMakeCurrent or equivalent, if the call is successful (returns nonzero).
/// Protect onCreateContext, onMakeCurrent and onDestroyContext with a mutex.
void _bolt_gl_onMakeCurrent(void*);

/// Call this in response to eglDestroyContext or equivalent, if the call is successful (returns non-zero).
/// If this function itself returns non-zero, the returned value is a context, which you should call
/// MakeCurrent on, then call _bolt_gl_close(), then unbind the context and call eglTerminate or equivalent.
/// Don't allow the same Terminate function to be used normally by the game.
void* _bolt_gl_onDestroyContext(void*);

/// Call this in response to glGenTextures, which needs to be hooked from libgl.
void _bolt_gl_onGenTextures(GLsizei, GLuint*);

/// Call this in response to glDrawElements, which needs to be hooked from libgl.
void _bolt_gl_onDrawElements(GLenum, GLsizei, GLenum, const void*);

/// Call this in response to glDrawArrays, which needs to be hooked from libgl.
void _bolt_gl_onDrawArrays(GLenum, GLint, GLsizei);

/// Call this in response to glBindTexture, which needs to be hooked from libgl.
void _bolt_gl_onBindTexture(GLenum, GLuint);

/// Call this in response to glTexSubImage2D, which needs to be hooked from libgl.
void _bolt_gl_onTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);

// Call this in response to glDeleteTextures, which needs to be hooked from libgl.
void _bolt_gl_onDeleteTextures(GLsizei, const GLuint*);

/// Call this in response to glClear, which needs to be hooked from libgl.
void _bolt_gl_onClear(GLbitfield);

/// Call this in response to glViewport, which needs to be hooked from libgl.
void _bolt_gl_onViewport(GLint, GLint, GLsizei, GLsizei);

/// Call this in response to glTexParameteri, which needs to be hooked from libgl.
void _bolt_gl_onTexParameteri(GLenum, GLenum, GLint);

/// Call this in response to glBlendFunc, which needs to be hooked from libgl.
void _bolt_gl_onBlendFunc(GLenum, GLenum);

/// Implemented in os-specific code and referred to by gl.c
void _bolt_flash_window();

#endif
