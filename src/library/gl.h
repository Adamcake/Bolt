#ifndef _BOLT_LIBRARY_GL_H_
#define _BOLT_LIBRARY_GL_H_

#include <stdint.h>
#include <stdlib.h>

/* consts used from libgl */
#define GL_TEXTURE_2D 3553
#define GL_RGBA 6408
#define GL_UNSIGNED_BYTE 5121
#define GL_UNSIGNED_SHORT 5123
#define GL_UNSIGNED_INT 5125
#define GL_BYTE 5120
#define GL_SHORT 5122
#define GL_INT 5124
#define GL_FLOAT 5126
#define GL_TRIANGLES 4
#define GL_MAP_READ_BIT 1
#define GL_MAP_WRITE_BIT 2
#define GL_MAP_FLUSH_EXPLICIT_BIT 16
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_ARRAY_BUFFER 34962
#define GL_ELEMENT_ARRAY_BUFFER 34963
#define GL_ARRAY_BUFFER_BINDING 34964
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 34965

struct GLArrayBuffer {
    void* data;
    uint8_t* mapping;
    int32_t mapping_offset;
    uint32_t mapping_len;
    uint32_t mapping_access_type;
};

struct GLTexture2D {
    unsigned char* data;
    unsigned int width;
    unsigned int height;
};

struct GLProgram {
    unsigned int loc_aVertexPosition2D;
    unsigned int loc_aVertexColour;
    unsigned int loc_aTextureUV;
    unsigned int loc_aTextureUVAtlasMin;
    unsigned int loc_aTextureUVAtlasExtents;
    int loc_uProjectionMatrix;
    int loc_uDiffuseMap;
    uint8_t is_important;
};

struct GLAttrBinding {
    unsigned int buffer;
    unsigned int stride;
    uintptr_t offset;
    int size;
    uint32_t type;
    uint8_t normalise;
    uint8_t enabled;
};

// Context-specific information - this is thread-specific on EGL, not sure about elsewhere
// this method of context-sharing takes advantage of the fact that the game never chains shares together
// with a depth greater than 1, and always deletes the non-owner before the owner. neither of those things
// are actually safe assumptions in valid OpenGL usage.
struct GLContext {
    uintptr_t id;
    struct GLProgram** programs;
    struct GLArrayBuffer** buffers;
    struct GLTexture2D** textures;
    size_t bound_program_id;
    size_t bound_texture_id;
    uint8_t current_program_is_important;
    uint8_t is_attached;
    uint8_t deferred_destroy;
    uint8_t is_shared_owner;
    unsigned int current_draw_framebuffer;
    unsigned int current_read_framebuffer;
    struct GLAttrBinding attributes[16];
};

struct GLContext* _bolt_context();
void _bolt_create_context(void*, void*);
void _bolt_make_context_current(void*);
void _bolt_destroy_context(void*);
void _bolt_set_attr_binding(struct GLAttrBinding*, unsigned int, int, const void*, unsigned int, uint32_t, uint8_t);
uint8_t _bolt_get_attr_binding(struct GLContext*, const struct GLAttrBinding*, size_t, size_t, float*);

#endif
