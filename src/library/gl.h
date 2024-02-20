#ifndef _BOLT_LIBRARY_GL_H_
#define _BOLT_LIBRARY_GL_H_

#include <stdint.h>
#include <stdlib.h>

#include "../../modules/hashmap/hashmap.h"
#include "rwlock.h"
struct hashmap;
struct RenderBatch2D;

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
#define GL_FRAMEBUFFER 36160
#define GL_READ_FRAMEBUFFER 36008
#define GL_DRAW_FRAMEBUFFER 36009
#define GL_ARRAY_BUFFER 34962
#define GL_ELEMENT_ARRAY_BUFFER 34963
#define GL_UNIFORM_BUFFER 35345
#define GL_ARRAY_BUFFER_BINDING 34964
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 34965
#define GL_UNIFORM_BUFFER_BINDING 35368
#define GL_UNIFORM_OFFSET 35387
#define GL_UNIFORM_BLOCK_BINDING 35391
#define GL_TEXTURE0 33984
#define GL_COLOR_ATTACHMENT0 36064
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE 36048
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME 36049

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
    int loc_uVertexScale;
    int loc_sSceneHDRTex;
    int loc_sSourceTex;
    int block_index_ViewTransforms;
    int offset_uCameraPosition;
    int offset_uViewProjMatrix;
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

// Context-specific information - this is thread-specific on EGL, not sure about elsewhere
// this method of context-sharing takes advantage of the fact that the game never chains shares together
// with a depth greater than 1, and always deletes the non-owner before the owner. neither of those things
// are actually safe assumptions in valid OpenGL usage.
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
    int target_3d_tex;
    int game_view_x;
    int game_view_y;
    int game_view_w;
    int game_view_h;
    uint8_t is_attached;
    uint8_t deferred_destroy;
    uint8_t is_shared_owner;
    uint8_t need_3d_tex;
};

struct GLProgram* _bolt_context_get_program(struct GLContext*, unsigned int);
struct GLArrayBuffer* _bolt_context_get_buffer(struct GLContext*, unsigned int);
struct GLTexture2D* _bolt_context_get_texture(struct GLContext*, unsigned int);
struct GLVertexArray* _bolt_context_get_vao(struct GLContext*, unsigned int);

struct GLContext* _bolt_context();
size_t _bolt_context_count();
void _bolt_create_context(void*, void*);
void _bolt_make_context_current(void*);
void _bolt_destroy_context(void*);
void _bolt_set_attr_binding(struct GLContext*, struct GLAttrBinding*, unsigned int, int, const void*, unsigned int, uint32_t, uint8_t);
uint8_t _bolt_get_attr_binding(struct GLContext*, const struct GLAttrBinding*, size_t, size_t, float*);
uint8_t _bolt_get_attr_binding_int(struct GLContext*, const struct GLAttrBinding*, size_t, size_t, int32_t*);

uint32_t _bolt_binding_for_buffer(uint32_t);

/* plugin library interop stuff */

struct GLPluginDrawElementsUserData {
    struct GLContext* c;
    unsigned short* indices;
    struct GLTexture2D* atlas;
    struct GLAttrBinding* position;
    struct GLAttrBinding* atlas_min;
    struct GLAttrBinding* atlas_size;
    struct GLAttrBinding* tex_uv;
    struct GLAttrBinding* colour;
};
void _bolt_gl_plugin_drawelements_xy(const struct RenderBatch2D*, size_t index, void* userdata, int32_t* out);
void _bolt_gl_plugin_drawelements_atlas_xy(const struct RenderBatch2D*, size_t index, void* userdata, int32_t* out);
void _bolt_gl_plugin_drawelements_atlas_wh(const struct RenderBatch2D*, size_t index, void* userdata, int32_t* out);
void _bolt_gl_plugin_drawelements_uv(const struct RenderBatch2D*, size_t index, void* userdata, double* out);
void _bolt_gl_plugin_drawelements_colour(const struct RenderBatch2D*, size_t index, void* userdata, double* out);

#endif
