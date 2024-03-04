#ifndef _BOLT_LIBRARY_GL_H_
#define _BOLT_LIBRARY_GL_H_

#include <stdint.h>
#include <stdlib.h>

#include "../../modules/hashmap/hashmap.h"
#include "rwlock.h"
struct hashmap;
struct SurfaceFunctions;

/// Struct representing all the OpenGL functions of interest to us that the game gets from GetProcAddress
struct GLProcFunctions {
    void (*ActiveTexture)(uint32_t);
    void (*AttachShader)(unsigned int, unsigned int);
    void (*BindAttribLocation)(unsigned int, unsigned int, const char*);
    void (*BindBuffer)(uint32_t, unsigned int);
    void (*BindFramebuffer)(uint32_t, unsigned int);
    void (*BindVertexArray)(uint32_t);
    void (*BlitFramebuffer)(int, int, int, int, int, int, int, int, uint32_t, uint32_t);
    void (*BufferData)(uint32_t, uintptr_t, const void*, uint32_t);
    void (*BufferStorage)(unsigned int, uintptr_t, const void*, uintptr_t);
    void (*BufferSubData)(uint32_t, intptr_t, uintptr_t, const void*);
    void (*CompileShader)(unsigned int);
    void (*CompressedTexSubImage2D)(uint32_t, int, int, int, unsigned int, unsigned int, uint32_t, unsigned int, const void*);
    void (*CopyImageSubData)(unsigned int, uint32_t, int, int, int, int, unsigned int, uint32_t, int, int, int, int, unsigned int, unsigned int, unsigned int);
    unsigned int (*CreateProgram)();
    unsigned int (*CreateShader)(uint32_t);
    void (*DeleteBuffers)(unsigned int, const unsigned int*);
    void (*DeleteFramebuffers)(uint32_t, unsigned int*);
    void (*DeleteProgram)(unsigned int);
    void (*DeleteShader)(unsigned int);
    void (*DeleteVertexArrays)(uint32_t, const unsigned int*);
    void (*DisableVertexAttribArray)(unsigned int);
    void (*DrawElements)(uint32_t, unsigned int, uint32_t, const void*);
    void (*EnableVertexAttribArray)(unsigned int);
    void (*FlushMappedBufferRange)(uint32_t, intptr_t, uintptr_t);
    void (*FramebufferTexture)(uint32_t, uint32_t, unsigned int, int);
    void (*FramebufferTextureLayer)(uint32_t, uint32_t, unsigned int, int, int);
    void (*GenBuffers)(uint32_t, unsigned int*);
    void (*GenFramebuffers)(uint32_t, unsigned int*);
    void (*GenVertexArrays)(uint32_t, unsigned int*);
    void (*GetActiveUniformBlockiv)(unsigned int, unsigned int, uint32_t, int*);
    void (*GetActiveUniformsiv)(unsigned int, uint32_t, const unsigned int*, uint32_t, int*);
    void (*GetFramebufferAttachmentParameteriv)(uint32_t, uint32_t, uint32_t, int*);
    void (*GetIntegeri_v)(uint32_t, unsigned int, int*);
    void (*GetIntegerv)(uint32_t, int*);
    unsigned int (*GetUniformBlockIndex)(uint32_t, const char*);
    void (*GetUniformfv)(unsigned int, int, float*);
    void (*GetUniformIndices)(uint32_t, uint32_t, const char**, unsigned int*);
    void (*GetUniformiv)(unsigned int, int, int*);
    int (*GetUniformLocation)(unsigned int, const char*);
    void (*LinkProgram)(unsigned int);
    void* (*MapBufferRange)(uint32_t, intptr_t, uintptr_t, uint32_t);
    void (*MultiDrawElements)(uint32_t, uint32_t*, uint32_t, const void**, uint32_t);
    void (*ShaderSource)(unsigned int, uint32_t, const char**, const int*);
    void (*TexStorage2D)(uint32_t, int, uint32_t, unsigned int, unsigned int);
    void (*Uniform1i)(int, int);
    void (*Uniform4i)(int, int, int, int, int);
    void (*UniformMatrix4fv)(int, unsigned int, uint8_t, const float*);
    uint8_t (*UnmapBuffer)(uint32_t);
    void (*UseProgram)(unsigned int);
    void (*VertexAttribPointer)(unsigned int, int, uint32_t, uint8_t, unsigned int, const void*);
};

/// Struct representing all the OpenGL functions of interest to us that the game gets from static or dynamic linkage
struct GLLibFunctions {
    void (*BindTexture)(uint32_t, unsigned int);
    void (*Clear)(uint32_t);
    void (*ClearColor)(float, float, float, float);
    void (*DeleteTextures)(unsigned int, const unsigned int*);
    void (*DrawArrays)(uint32_t, int, unsigned int);
    void (*DrawElements)(uint32_t, unsigned int, uint32_t, const void*);
    void (*Flush)();
    void (*GenTextures)(uint32_t, unsigned int*);
    uint32_t (*GetError)();
    void (*TexSubImage2D)(uint32_t, int, int, int, unsigned int, unsigned int, uint32_t, uint32_t, const void*);
};

/* consts used from libgl */
#define GL_TEXTURE 5890
#define GL_TEXTURE_2D 3553
#define GL_RGBA 6408
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
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
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

/* bolt re-implementation of some gl objects, storing only the things we need */

struct GLArrayBuffer {
    unsigned int id;
    void* data;
    uint8_t* mapping;
    int32_t mapping_offset;
    uint32_t mapping_len;
    uint32_t mapping_access_type;
};

struct GLTexture2D {
    unsigned int id;
    unsigned char* data;
    unsigned int width;
    unsigned int height;
    double minimap_center_x;
    double minimap_center_y;
    uint8_t is_minimap_tex_big;
    uint8_t is_minimap_tex_small;
};

struct GLProgram {
    unsigned int id;
    unsigned int loc_aVertexPosition2D;
    unsigned int loc_aVertexColour;
    unsigned int loc_aTextureUV;
    unsigned int loc_aTextureUVAtlasMin;
    unsigned int loc_aTextureUVAtlasExtents;
    unsigned int loc_aMaterialSettingsSlotXY_TilePositionXZ;
    unsigned int loc_aVertexPosition_BoneLabel;
    int loc_uProjectionMatrix;
    int loc_uDiffuseMap;
    int loc_uTextureAtlas;
    int loc_uTextureAtlasSettings;
    int loc_uAtlasMeta;
    int loc_uModelMatrix;
    int loc_uGridSize;
    int loc_uVertexScale;
    int loc_sSceneHDRTex;
    int loc_sSourceTex;
    int block_index_ViewTransforms;
    int offset_uCameraPosition;
    int offset_uViewProjMatrix;
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
    unsigned int id;
    struct GLAttrBinding attributes[16];
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
    unsigned int active_texture;
    unsigned int current_draw_framebuffer;
    unsigned int current_read_framebuffer;
    unsigned int game_view_framebuffer;
    unsigned int game_view_tex;
    unsigned int game_view_tex_front;
    int target_3d_tex;
    int target_minimap_tex;
    int game_view_x;
    int game_view_y;
    int game_view_w;
    int game_view_h;
    uint8_t is_attached;
    uint8_t deferred_destroy;
    uint8_t is_shared_owner;
    uint8_t need_3d_tex;
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
/// Provide a pointer returned by the OS, which will be used to identify this context, the shared context,
/// if any. Protect onCreateContext, onMakeCurrent and onDestroyContext with a mutex.
/// Also provide your libgl functions struct, which you should have already populated by now.
/// Finally, provide the generic GetProcAddress function.
void _bolt_gl_onCreateContext(void*, void*, const struct GLLibFunctions*, void* (*)(const char*));

/// Call this in response to eglMakeCurrent or equivalent, if the call is successful (returns nonzero).
/// Protect onCreateContext, onMakeCurrent and onDestroyContext with a mutex.
void _bolt_gl_onMakeCurrent(void*);

/// Call this in response to eglDestroyContext or equivalent, if the call is successful (returns non-zero).
/// If this function itself returns non-zero, the returned value is a context, which you should call
/// MakeCurrent on, then call _bolt_gl_close(), then unbind the context and call eglTerminate or equivalent.
/// Don't allow the same Terminate function to be used normally by the game.
void* _bolt_gl_onDestroyContext(void*);

/// Call this in response to glGenTextures, which needs to be hooked from libgl.
void _bolt_gl_onGenTextures(uint32_t, unsigned int*);

/// Call this in response to glDrawElements, which needs to be hooked from libgl.
void _bolt_gl_onDrawElements(uint32_t, unsigned int, uint32_t, const void*);

/// Call this in response to glDrawArrays, which needs to be hooked from libgl.
void _bolt_gl_onDrawArrays(uint32_t, int, unsigned int);

/// Call this in response to glBindTexture, which needs to be hooked from libgl.
void _bolt_gl_onBindTexture(uint32_t, unsigned int);

/// Call this in response to glTexSubImage2D, which needs to be hooked from libgl.
void _bolt_gl_onTexSubImage2D(uint32_t, int, int, int, unsigned int, unsigned int, uint32_t, uint32_t, const void*);

// Call this in response to glDeleteTextures, which needs to be hooked from libgl.
void _bolt_gl_onDeleteTextures(unsigned int, const unsigned int*);

/// Call this in response to glClear, which needs to be hooked from libgl.
void _bolt_gl_onClear(uint32_t);

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
};
void _bolt_gl_plugin_drawelements_vertex2d_xy(size_t index, void* userdata, int32_t* out);
void _bolt_gl_plugin_drawelements_vertex2d_atlas_xy(size_t index, void* userdata, int32_t* out);
void _bolt_gl_plugin_drawelements_vertex2d_atlas_wh(size_t index, void* userdata, int32_t* out);
void _bolt_gl_plugin_drawelements_vertex2d_uv(size_t index, void* userdata, double* out);
void _bolt_gl_plugin_drawelements_vertex2d_colour(size_t index, void* userdata, double* out);

struct GLPluginTextureUserData {
    struct GLTexture2D* tex;
};
size_t _bolt_gl_plugin_texture_id(void* userdata);
void _bolt_gl_plugin_texture_size(void* userdata, size_t* out);
uint8_t _bolt_gl_plugin_texture_compare(void* userdata, size_t x, size_t y, size_t len, const unsigned char* data);

void _bolt_gl_plugin_surface_init(struct SurfaceFunctions* out, unsigned int width, unsigned int height);
void _bolt_gl_plugin_surface_destroy(void* userdata);
void _bolt_gl_plugin_surface_clear(void* userdata, double r, double g, double b, double a);
void _bolt_gl_plugin_surface_drawtoscreen(void* userdata, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);

#endif
