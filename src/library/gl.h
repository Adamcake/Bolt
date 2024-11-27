#ifndef _BOLT_LIBRARY_GL_H_
#define _BOLT_LIBRARY_GL_H_

#include <stdint.h>

#include "../../modules/hashmap/hashmap.h"
#include "rwlock/rwlock.h"
struct hashmap;
struct SurfaceFunctions;

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

/// Struct representing all the OpenGL functions of interest to us that the game gets from GetProcAddress
struct GLProcFunctions {
    void (*ActiveTexture)(GLenum);
    void (*AttachShader)(GLuint, GLuint);
    void (*BindAttribLocation)(GLuint, GLuint, const GLchar*);
    void (*BindBuffer)(GLenum, GLuint);
    void (*BindFramebuffer)(GLenum, GLuint);
    void (*BindVertexArray)(GLuint);
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
    void (*DisableVertexAttribArray)(GLuint);
    void (*DrawElements)(GLenum, GLsizei, GLenum, const void*);
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
    void (*GetIntegerv)(GLenum, GLint*);
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
    void (*Uniform1i)(GLint, GLint);
    void (*Uniform2i)(GLint, GLint, GLint);
    void (*Uniform4i)(GLint, GLint, GLint, GLint, GLint);
    void (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
    GLboolean (*UnmapBuffer)(GLenum);
    void (*UseProgram)(GLuint);
    void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
};

/// Struct representing all the OpenGL functions of interest to us that the game gets from static or dynamic linkage
struct GLLibFunctions {
    void (*BindTexture)(GLenum, GLuint);
    void (*Clear)(GLbitfield);
    void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (*DeleteTextures)(GLsizei, const GLuint*);
    void (*DrawArrays)(GLenum, GLint, GLsizei);
    void (*DrawElements)(GLenum, GLsizei, GLenum, const void*);
    void (*Flush)(void);
    void (*GenTextures)(GLsizei, GLuint*);
    GLenum (*GetError)(void);
    void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
    void (*TexParameteri)(GLenum, GLenum, GLfloat);
    void (*TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
};

/* consts used from libgl */
#define GL_TEXTURE 5890
#define GL_TEXTURE_2D 3553
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

/* bolt re-implementation of some gl objects, storing only the things we need */

struct GLArrayBuffer {
    GLuint id;
    void* data;
    uint8_t* mapping;
    GLintptr mapping_offset;
    GLsizeiptr mapping_len;
    GLbitfield mapping_access_type;
};

struct GLTexture2D {
    GLuint id;
    uint8_t* data;
    GLsizei width;
    GLsizei height;
    double minimap_center_x;
    double minimap_center_y;
    uint8_t is_minimap_tex_big;
    uint8_t is_minimap_tex_small;
};

struct GLProgram {
    GLuint id;
    GLint loc_aVertexPosition2D;
    GLint loc_aVertexColour;
    GLint loc_aTextureUV;
    GLint loc_aTextureUVAtlasMin;
    GLint loc_aTextureUVAtlasExtents;
    GLint loc_aMaterialSettingsSlotXY_TilePositionXZ;
    GLint loc_aVertexPosition_BoneLabel;
    GLint loc_aVertexSkinBones;
    GLint loc_aVertexSkinWeights;
    GLint loc_uProjectionMatrix;
    GLint loc_uDiffuseMap;
    GLint loc_uTextureAtlas;
    GLint loc_uTextureAtlasSettings;
    GLint loc_uAtlasMeta;
    GLint loc_uModelMatrix;
    GLint loc_uBoneTransforms;
    GLint loc_uGridSize;
    GLint loc_uVertexScale;
    GLint loc_uSmoothSkinning;
    GLint loc_sSceneHDRTex;
    GLint loc_sSourceTex;
    GLint loc_sBlurFarTex;
    GLuint block_index_ViewTransforms;
    GLint offset_uCameraPosition;
    GLint offset_uViewMatrix;
    GLint offset_uProjectionMatrix;
    GLint offset_uViewProjMatrix;
    uint8_t is_minimap;
    uint8_t is_2d;
    uint8_t is_3d;
};

struct GLAttrBinding {
    struct GLArrayBuffer* buffer;
    uintptr_t offset;
    unsigned int stride;
    int size;
    uint32_t type;
    uint8_t normalise;
    uint8_t enabled;
};

struct GLVertexArray {
    GLuint id;
    struct GLAttrBinding* attributes;
};

struct HashMap {
    struct hashmap* map;
    RWLock rwlock;
};

/// Context-specific information - this is thread-specific on EGL, not sure about elsewhere
/// this method of context-sharing takes advantage of the fact that the game never chains shares together
/// with a depth greater than 1, and always deletes the non-owner before the owner. neither of those things
/// are actually safe assumptions in valid OpenGL usage.
struct GLContext {
    uintptr_t id;
    struct HashMap* programs;
    struct HashMap* buffers;
    struct HashMap* textures;
    struct HashMap* vaos;
    struct GLTexture2D** texture_units;
    struct GLProgram* bound_program;
    struct GLVertexArray* bound_vao;
    GLenum active_texture;
    GLuint current_draw_framebuffer;
    GLuint current_read_framebuffer;
    GLuint game_view_part_framebuffer;
    GLint game_view_sSourceTex;
    GLint game_view_sSceneHDRTex;
    GLint depth_of_field_sSourceTex;
    GLint target_3d_tex;
    GLint game_view_x;
    GLint game_view_y;
    GLint game_view_w;
    GLint game_view_h;
    uint8_t is_attached;
    uint8_t deferred_destroy;
    uint8_t is_shared_owner;
    uint8_t recalculate_sSceneHDRTex;
    uint8_t does_blit_3d_target;
    uint8_t depth_of_field_enabled;
    GLint viewport_x;
    GLint viewport_y;
    GLsizei viewport_w;
    GLsizei viewport_h;
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

/* plugin library interop stuff */

struct GLPluginDrawElementsVertex2DUserData {
    struct GLContext* c;
    unsigned short* indices;
    struct GLTexture2D* atlas;
    struct GLAttrBinding* position;
    struct GLAttrBinding* atlas_min;
    struct GLAttrBinding* atlas_size;
    struct GLAttrBinding* tex_uv;
    struct GLAttrBinding* colour;
    uint32_t screen_height;
};

struct GLPluginDrawElementsVertex3DUserData {
    struct GLContext* c;
    unsigned short* indices;
    int atlas_scale;
    struct GLTexture2D* atlas;
    struct GLTexture2D* settings_atlas;
    struct GLAttrBinding* xy_xz;
    struct GLAttrBinding* xyz_bone;
    struct GLAttrBinding* tex_uv;
    struct GLAttrBinding* colour;
    struct GLAttrBinding* skin_bones;
    struct GLAttrBinding* skin_weights;
};

struct GLPlugin3DMatrixUserData {
    float model_matrix[16];
    const struct GLProgram* program;
    const float* view_matrix;
    const float* proj_matrix;
    const float* viewproj_matrix;
};

struct GLPluginTextureUserData {
    struct GLTexture2D* tex;
};

#endif
