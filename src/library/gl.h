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
#define GL_MAP_WRITE_BIT 2
#define GL_MAP_FLUSH_EXPLICIT_BIT 16
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_ARRAY_BUFFER 34962
#define GL_ELEMENT_ARRAY_BUFFER 34963

// my haphazard implementation of an arena allocator
struct GLList {
    void* data;
    void* pointers;
    size_t capacity;
    size_t first_empty;
};

struct GLArrayBuffer {
    void* data;
    unsigned int id;
    unsigned int len;

    void* mapping;
    int32_t mapping_offset;
    uint32_t mapping_len;
    uint32_t mapping_access_type;
};
struct GLArrayBuffer* _bolt_find_buffer(struct GLList*, unsigned int);
struct GLArrayBuffer* _bolt_get_buffer(struct GLList*, unsigned int);

struct GLTexture2D {
    unsigned char* data;
    unsigned int id;
    unsigned int width;
    unsigned int height;
};
struct GLTexture2D* _bolt_find_texture(struct GLList*, unsigned int);
struct GLTexture2D* _bolt_get_texture(struct GLList*, unsigned int);

struct GLProgram {
    unsigned int id;
    unsigned int loc_aVertexPosition2D;
    unsigned int loc_aVertexColour;
    unsigned int loc_aTextureUV;
    unsigned int loc_aTextureUVAtlasMin;
    unsigned int loc_aTextureUVAtlasExtents;
    int loc_uProjectionMatrix;
    int loc_uDiffuseMap;
    uint8_t is_important;
};
struct GLProgram* _bolt_find_program(struct GLList*, unsigned int);
struct GLProgram* _bolt_get_program(struct GLList*, unsigned int);

struct GLAttrBinding {
    const void* ptr;
    unsigned int stride;
    uint32_t type;
    uint8_t normalise;
    uint8_t enabled;
};

void _bolt_set_attr_binding(struct GLAttrBinding*, const void*, const void*, unsigned int, uint32_t, uint8_t);
void _bolt_get_attr_binding(const struct GLAttrBinding*, size_t, size_t, float*);

// Context-specific information - this is thread-specific on EGL, not sure about elsewhere
// this method of context-sharing takes advantage of the fact that the game never chains shares together
// with a depth greater than 1, and always deletes the non-owner before the owner. neither of those things
// are actually safe assumptions in valid OpenGL usage.
struct GLContext {
    uintptr_t id;
    int sockets[2];
    struct GLList programs;
    struct GLList buffers;
    struct GLList textures;
    struct GLList* shared_programs;
    struct GLList* shared_buffers;
    struct GLList* shared_textures;
    size_t bound_program_id;
    size_t bound_vertex_array_id;
    size_t bound_element_array_id;
    size_t bound_texture_id;
    uint8_t current_program_is_important;
    uint8_t is_attached;
    uint8_t deferred_destroy;
    uint8_t is_shared_owner;
    unsigned int current_draw_framebuffer;
    unsigned int current_read_framebuffer;
    struct GLAttrBinding attributes[16];
    unsigned char* uniform_buffer;
};

struct GLContext* _bolt_context();
void _bolt_create_context(void*, void*);
void _bolt_make_context_current(void*);
void _bolt_destroy_context(void*);
struct GLArrayBuffer* _bolt_context_get_buffer(struct GLContext*, uint32_t);
struct GLArrayBuffer* _bolt_context_find_buffer(struct GLContext*, uint32_t);
struct GLArrayBuffer* _bolt_context_find_named_buffer(struct GLContext*, unsigned int);
void _bolt_context_destroy_buffers(struct GLContext*, unsigned int, const unsigned int*);
void _bolt_context_destroy_textures(struct GLContext*, unsigned int, const unsigned int*);

#endif
