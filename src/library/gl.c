#include "plugin/plugin.h"
#include "gl.h"
#include "rwlock/rwlock.h"
#include "../../modules/hashmap/hashmap.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -D BOLT_LIBRARY_VERBOSE=1
#if defined(VERBOSE)
#if defined(WIN32)
#define gettid() (unsigned long)GetCurrentThreadId()
#else
#include <unistd.h>
#include <sys/syscall.h>
#define gettid() (unsigned long)syscall(SYS_gettid)
#endif

static FILE* logfile;
void _bolt_gl_set_logfile(FILE* f) { logfile = f; }
#define LOG(STR) if(fprintf(logfile, "[tid %lu] " STR, gettid()))fflush(logfile)
#define LOGF(STR, ...) if(fprintf(logfile, "[tid %lu] " STR, gettid(), __VA_ARGS__))fflush(logfile)
#else
#define LOG(...)
#define LOGF(...)
#endif

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
    GLenum internalformat;
    GLint compare_mode;
    struct Icon icon;
    struct hashmap* icons;
    uint8_t is_minimap_tex_big;
    uint8_t is_minimap_tex_small;
    uint8_t is_multisample;
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
    GLint loc_aParticleOrigin_CreationTime;
    GLint loc_aParticleOffset;
    GLint loc_aParticleVelocityAndUVScrollOffset;
    GLint loc_aParticleAlignmentUpAxis;
    GLint loc_aMaterialSettingsSlotXY_Rotation;
    GLint loc_aParticleUVAnimationData;
    GLint loc_aVertexPositionDepthOffset;
    GLint loc_aBillboardSize;
    GLint loc_aMaterialSettingsSlotXY_UV;
    GLint loc_uDiffuseMap;
    GLint loc_uTextureAtlas;
    GLint loc_uTextureAtlasSettings;
    GLint loc_uSecondsSinceStart;
    GLint loc_sSceneHDRTex;
    GLint loc_sSourceTex;
    GLint loc_sBlurFarTex;
    GLint loc_sTexture;
    GLuint block_index_ViewTransforms;
    GLint offset_uCameraPosition;
    GLint offset_uViewMatrix;
    GLint offset_uProjectionMatrix;
    GLint offset_uViewProjMatrix;
    GLint offset_uInvViewMatrix;
    GLuint block_index_BatchConsts;
    GLint offset_uAtlasMeta;
    GLint offset_uVertexScale;
    GLuint block_index_ModelConsts;
    GLint offset_uModelMatrix;
    GLuint block_index_VertexTransformData;
    GLint offset_uBoneTransforms;
    GLint offset_uSmoothSkinning;
    GLuint block_index_ParticleConsts;
    GLint offset_uParticleEmitterTransforms;
    GLint offset_uParticleEmitterTransformRanges;
    GLuint block_index_TerrainConsts;
    GLint offset_uGridSize;
    GLint block_index_GUIConsts;
    GLint block_index_BilloardConsts; // not a mistake, the game engine spells it like that
    uint8_t is_minimap;
    uint8_t is_2d;
    uint8_t is_3d;
    uint8_t is_particle;
    uint8_t is_billboard;
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

struct TextureUnit {
    struct GLTexture2D* texture_2d;
    struct GLTexture2D* texture_2d_multisample;
    struct GLTexture2D* recent; // useful for guessing which of the above a shader is intending to use
    GLenum target;
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
    struct TextureUnit* texture_units;
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
    GLint player_model_tex;
    GLint depth_tex;
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
    uint8_t recalculate_depth_tex;
    GLint viewport_x;
    GLint viewport_y;
    GLsizei viewport_w;
    GLsizei viewport_h;
    GLenum blend_rgb_s;
    GLenum blend_rgb_d;
    GLenum blend_alpha_s;
    GLenum blend_alpha_d;
    int32_t player_model_x;
    int32_t player_model_y;
    int32_t player_model_z;
};

static unsigned int gl_width;
static unsigned int gl_height;

static size_t egl_init_count = 0;
static uintptr_t egl_main_context = 0;
static uint8_t egl_main_context_destroy_pending = 0;
static uint8_t egl_main_context_makecurrent_pending = 0;

static struct GLProcFunctions gl = {0};
static const struct GLLibFunctions* lgl = NULL;
struct ProgramDirectData {
    GLuint id;
    GLint sampler;
    GLint d_xywh;
    GLint s_xywh;
    GLint src_wh_dest_wh;
    GLint rgba;
};
static struct ProgramDirectData program_direct_screen;
static struct ProgramDirectData program_direct_surface;

// this stuff should probably be moved to custom shader API in plugin.c now that it actually exists
static GLuint program_region;
static GLint program_region_xywh;
static GLint program_region_dest_wh;
static GLuint program_direct_vao;
static GLuint buffer_vertices_square;

static uint8_t player_model_tex_seen = false;

#define GLSLHEADER "#version 330 core\n"
#define GLSLPLUGINEXTENSIONHEADER "#extension GL_ARB_explicit_uniform_location : require\n"

// "direct" program is basically a blit but with transparency.
// there are different vertex shaders for targeting the screen vs targeting a surface.
// the only difference is that "screen" inverts Y, whereas surface-to-surface doesn't.
static const GLchar program_direct_screen_vs[] = GLSLHEADER
"layout (location = 0) in vec2 aPos;"
"out vec2 vPos;"
"uniform ivec4 d_xywh;"
"uniform ivec4 src_wh_dest_wh;"
"void main() {"
  "vPos = aPos;"
  "gl_Position = vec4(((((aPos * d_xywh.pq) + d_xywh.st) * vec2(2.0, 2.0) / src_wh_dest_wh.pq) - vec2(1.0, 1.0)) * vec2(1.0, -1.0), 0.0, 1.0);"
"}";
static const GLchar program_direct_surface_vs[] = GLSLHEADER
"layout (location = 0) in vec2 aPos;"
"out vec2 vPos;"
"uniform ivec4 d_xywh;"
"uniform ivec4 src_wh_dest_wh;"
"void main() {"
  "vPos = aPos;"
  "gl_Position = vec4(((((aPos * d_xywh.pq) + d_xywh.st) * vec2(2.0, 2.0) / src_wh_dest_wh.pq) - vec2(1.0, 1.0)), 0.0, 1.0);"
"}";
static const GLchar program_direct_fs[] = GLSLHEADER
"in vec2 vPos;"
"layout (location = 0) out vec4 col;"
"uniform sampler2D tex;"
"uniform ivec4 s_xywh;"
"uniform ivec4 src_wh_dest_wh;"
"uniform vec4 rgba;"
"void main() {"
  "col = texture(tex, ((vPos * s_xywh.pq) + s_xywh.st) / src_wh_dest_wh.st) * clamp(rgba, 0.0, 1.0);"
"}";

// "region" program draws an outline of alternating black and white pixels on a rectangular region of the screen
static const GLchar program_region_vs[] = GLSLHEADER
"layout (location = 0) in vec2 aPos;"
"out vec2 vPos;"
"uniform ivec4 xywh;"
"uniform ivec2 dest_wh;"
"void main() {"
  "vPos = aPos;"
  "gl_Position = vec4(((((aPos * xywh.pq) + xywh.st) * vec2(2.0, 2.0) / vec2(dest_wh)) - vec2(1.0, 1.0)), 0.0, 1.0);"
"}";
static const GLchar program_region_fs[] = GLSLHEADER
"in vec2 vPos;"
"layout (location = 0) out vec4 col;"
"uniform ivec4 xywh;"
"void main() {"
  "float xpixel = vPos.x * xywh.p;"
  "float ypixel = vPos.y * xywh.q;"
  "float rgb = mod(floor(xpixel / 2.0) + floor(ypixel / 2.0), 2.0) >= 1.0 ? 1.0 : 0.0;"
  "float alpha = (xpixel >= 6.0 && xpixel < (xywh.p - 6.0) && ypixel >= 6.0 && ypixel < (xywh.q - 6.0)) ? 0.0 : 1.0;"
  "col = vec4(rgb, rgb, rgb, alpha);"
"}";

static void context_init(struct GLContext*, void*, void*);
static void context_free(struct GLContext*);

static void glplugin_drawelements_vertex2d_xy(size_t index, void* userdata, int32_t* out);
static void glplugin_drawelements_vertex2d_atlas_details(size_t index, void* userdata, int32_t* out, uint8_t* wrapx, uint8_t* wrapy);
static void glplugin_drawelements_vertex2d_uv(size_t index, void* userdata, double* out, uint8_t* discard);
static void glplugin_drawelements_vertex2d_colour(size_t index, void* userdata, double* out);
static void glplugin_drawelements_vertex3d_xyz(size_t index, void* userdata, struct Point3D* out);
static size_t glplugin_drawelements_vertex3d_atlas_meta(size_t index, void* userdata);
static void glplugin_drawelements_vertex3d_meta_xywh(size_t meta, void* userdata, int32_t* out);
static void glplugin_drawelements_vertex3d_uv(size_t index, void* userdata, double* out);
static void glplugin_drawelements_vertex3d_colour(size_t index, void* userdata, double* out);
static uint8_t glplugin_drawelements_vertex3d_boneid(size_t index, void* userdata);
static void glplugin_drawelements_vertex3d_transform(size_t vertex, void* userdata, struct Transform3D* out);
static void glplugin_drawelements_vertexparticles_xyz(size_t index, void* userdata, struct Point3D* out);
static void glplugin_drawelements_vertexparticles_world_offset(size_t index, void* userdata, double* out);
static void glplugin_drawelements_vertexparticles_eye_offset(size_t index, void* userdata, double* out);
static void glplugin_drawelements_vertexparticles_uv(size_t index, void* userdata, double* out);
static size_t glplugin_drawelements_vertexparticles_atlas_meta(size_t index, void* userdata);
static void glplugin_drawelements_vertexparticles_meta_xywh(size_t index, void* userdata, int32_t* out);
static void glplugin_drawelements_vertexparticles_colour(size_t index, void* userdata, double* out);
static void glplugin_drawelements_vertexbillboard_xyz(size_t index, void* userdata, struct Point3D* out);
static void glplugin_drawelements_vertexbillboard_eye_offset(size_t index, void* userdata, double* out);
static void glplugin_drawelements_vertexbillboard_uv(size_t index, void* userdata, double* out);
static size_t glplugin_drawelements_vertexbillboard_atlas_meta(size_t index, void* userdata);
static void glplugin_drawelements_vertexbillboard_meta_xywh(size_t index, void* userdata, int32_t* out);
static void glplugin_drawelements_vertexbillboard_colour(size_t index, void* userdata, double* out);
static void glplugin_matrixparticles_viewmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrixparticles_projectionmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrixparticles_viewprojmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrixparticles_inv_viewmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrixbillboard_modelmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrixbillboard_viewmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrixbillboard_projectionmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrixbillboard_viewprojmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrixbillboard_inv_viewmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrix3d_modelmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrix3d_viewmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrix3d_projectionmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrix3d_viewprojmatrix(void* userdata, struct Transform3D* out);
static void glplugin_matrix3d_inv_viewmatrix(void* userdata, struct Transform3D* out);
static size_t glplugin_texture_id(void* userdata);
static void glplugin_texture_size(void* userdata, size_t* out);
static uint8_t glplugin_texture_compare(void* userdata, size_t x, size_t y, size_t len, const unsigned char* data);
static uint8_t* glplugin_texture_data(void* userdata, size_t x, size_t y);
static void glplugin_gameview_size(void* userdata, int* w, int* h);
static void glplugin_surface_init(struct SurfaceFunctions* out, unsigned int width, unsigned int height, const void* data);
static void glplugin_surface_destroy(void* userdata);
static void glplugin_surface_resize(void* userdata, unsigned int width, unsigned int height);
static void glplugin_surface_clear(void* userdata, double r, double g, double b, double a);
static void glplugin_surface_subimage(void* userdata, int x, int y, int w, int h, const void* pixels, uint8_t is_bgra);
static void glplugin_surface_drawtoscreen(void* userdata, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
static void glplugin_surface_drawtosurface(void* userdata, void* target, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
static void glplugin_surface_drawtogameview(void* userdata, void* _gameview, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
static void glplugin_surface_set_tint(void* userdata, double r, double g, double b);
static void glplugin_surface_set_alpha(void* userdata, double alpha);
static void glplugin_draw_region_outline(void* userdata, int16_t x, int16_t y, uint16_t width, uint16_t height);
static void glplugin_read_screen_pixels(int16_t x, int16_t y, uint32_t width, uint32_t height, void* data);
static void glplugin_copy_screen(void* userdata, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
static void glplugin_game_view_rect(int* x, int* y, int* w, int* h);
static void glplugin_player_position(int32_t* x, int32_t* y, int32_t* z);
static uint8_t glplugin_vertex_shader_init(struct ShaderFunctions* out, const char* source, int len, char* output, int output_len);
static uint8_t glplugin_fragment_shader_init(struct ShaderFunctions* out, const char* source, int len, char* output, int output_len);
static uint8_t glplugin_shaderprogram_init(struct ShaderProgramFunctions* out, void* vertex, void* fragment, char* output, int output_len);
static void glplugin_shader_destroy(void* userdata);
static void glplugin_shaderprogram_destroy(void* userdata);
static uint8_t glplugin_shaderprogram_set_attribute(void* userdata, uint8_t attribute, uint8_t type_width, uint8_t type_is_signed, uint8_t type_is_float, uint8_t size, uint32_t offset, uint32_t stride);
static void glplugin_shaderprogram_set_uniform_floats(void* userdata, int location, uint8_t count, double* values);
static void glplugin_shaderprogram_set_uniform_ints(void* userdata, int location, uint8_t count, int* values);
static void glplugin_shaderprogram_set_uniform_matrix(void* userdata, int location, uint8_t transpose, uint8_t size, double* values);
static void glplugin_shaderprogram_set_uniform_surface(void* userdata, int location, void* target);
static void glplugin_shaderprogram_set_uniform_depthbuffer(void* userdata, void* event, int location);
static void glplugin_shaderprogram_drawtosurface(void* userdata, void* surface_, void* buffer_, uint32_t count);
static void glplugin_shaderprogram_drawtogameview(void* userdata, void* gameview_, void* buffer_, uint32_t count);
static void glplugin_shaderbuffer_init(struct ShaderBufferFunctions* out, const void* data, uint32_t len);
static void glplugin_shaderbuffer_destroy(void* userdata);

/* forward-declared functions to be called from DrawElements */
static void drawelements_handle_2d(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes);
static void drawelements_handle_3d(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes);
static void drawelements_handle_particles(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes);
static void drawelements_handle_billboard(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes);
static void drawelements_handle_2d_renderminimap(const struct GLTexture2D* tex, const GLfloat* projection_matrix, struct GLContext* c, const struct GLAttrBinding* attributes);
static void drawelements_handle_2d_minimapterrain(const unsigned short* indices, const struct GLTexture2D* tex, const GLfloat* projection_matrix, struct GLContext* c, const struct GLAttrBinding* attributes);
static void drawelements_handle_2d_bigicon(const unsigned short* indices, struct GLTexture2D* tex, const GLfloat* projection_matrix, struct GLContext* c, const struct GLAttrBinding* attributes);
static void drawelements_handle_2d_normal(GLsizei count, const unsigned short* indices, struct GLTexture2D* tex, const GLfloat* projection_matrix, struct GLContext* c, const struct GLAttrBinding* attributes, void (*handler_2d)(const struct RenderBatch2D*), void (*handler_icon)(const struct RenderIconEvent*));
static void drawelements_handle_3d_silhouette(struct GLContext* c);
static void drawelements_handle_3d_normal(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes);
static void drawelements_handle_3d_iconrender(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes, GLint draw_tex);

#define MAX_TEXTURE_UNITS 4096 // would be nice if there was a way to query this at runtime, but it would be awkward to set up
#define BUFFER_LIST_CAPACITY 256 * 256
#define TEXTURE_LIST_CAPACITY 256
#define PROGRAM_LIST_CAPACITY 256 * 8
#define VAO_LIST_CAPACITY 256 * 256
#define CONTEXTS_CAPACITY 64 // not growable so we just have to hard-code a number and hope it's enough forever
#define GAME_MINIMAP_BIG_SIZE 2048
#define GAME_ITEM_ICON_SIZE 64
#define GAME_ITEM_BIGICON_SIZE 512
static struct GLContext contexts[CONTEXTS_CAPACITY];

// since GL contexts are bound only to the thread that binds them, we use thread-local storage (TLS)
// to keep track of which context struct is relevant to each thread. One TLS slot can store exactly
// one pointer, which happens to be exactly what we need to store.
// Win32 and POSIX both define that the initial value will be 0 on all threads, current and future.
#if defined(_WIN32)
#include <Windows.h>
static DWORD current_context_tls;
#else
static pthread_key_t current_context_tls;
#endif

struct GLPluginDrawElementsVertex2DUserData {
    struct GLContext* c;
    const unsigned short* indices;
    const struct GLTexture2D* atlas;
    const struct GLAttrBinding* position;
    const struct GLAttrBinding* atlas_min;
    const struct GLAttrBinding* atlas_size;
    const struct GLAttrBinding* tex_uv;
    const struct GLAttrBinding* colour;
    uint32_t screen_height;
};

struct GLPluginDrawElementsVertex3DUserData {
    struct GLContext* c;
    const unsigned short* indices;
    int atlas_scale;
    const struct GLTexture2D* atlas;
    const struct GLTexture2D* settings_atlas;
    const struct GLAttrBinding* xy_xz;
    const struct GLAttrBinding* xyz_bone;
    const struct GLAttrBinding* tex_uv;
    const struct GLAttrBinding* colour;
    const struct GLAttrBinding* skin_bones;
    const struct GLAttrBinding* skin_weights;
    const struct GLArrayBuffer* transforms_ubo;
};

struct GLPluginDrawElementsVertexParticlesUserData {
    struct GLContext* c;
    const unsigned short* indices;
    int atlas_scale;
    const struct GLTexture2D* atlas;
    const struct GLTexture2D* settings_atlas;
    const struct GLAttrBinding* origin;
    const struct GLAttrBinding* offset;
    const struct GLAttrBinding* velocity;
    const struct GLAttrBinding* up_axis;
    const struct GLAttrBinding* xy_rotation;
    const struct GLAttrBinding* colour;
    const struct GLAttrBinding* uv_animation_data;
    const float* camera_position;

    GLint ranges[14];
    const GLfloat* transform[8];
    GLfloat animation_time;
};

struct GLPluginDrawElementsVertexBillboardUserData {
    struct GLContext* c;
    const unsigned short* indices;
    int atlas_scale;
    const struct GLTexture2D* atlas;
    const struct GLTexture2D* settings_atlas;
    const struct GLAttrBinding* vertex_position;
    const struct GLAttrBinding* billboard_size;
    const struct GLAttrBinding* vertex_colour;
    const struct GLAttrBinding* material_xy_uv;
};

struct GLPluginDrawElementsMatrixParticlesUserData {
    const GLfloat* view_matrix;
    const GLfloat* projection_matrix;
    const GLfloat* viewproj_matrix;
    const GLfloat* inv_view_matrix;
};

struct GLPlugin3DMatrixUserData {
    const GLfloat* model_matrix;
    const GLfloat* view_matrix;
    const GLfloat* projection_matrix;
    const GLfloat* viewproj_matrix;
    const GLfloat* inv_view_matrix;
};

struct GLPluginDrawElementsMatrixBillboardUserData {
    const GLfloat* model_matrix;
    const GLfloat* view_matrix;
    const GLfloat* projection_matrix;
    const GLfloat* viewproj_matrix;
    const GLfloat* inv_view_matrix;
};

struct GLPluginRenderGameViewUserData {
    GLuint target_fb;
    GLint depth_tex;
    GLint width;
    GLint height;
};

struct GLPluginTextureUserData {
    struct GLTexture2D* tex;
};

struct PluginSurfaceUserdata {
    unsigned int width;
    unsigned int height;
    GLuint framebuffer;
    GLuint renderbuffer;
    GLfloat rgba[4];
};

struct PluginShaderUserdata {
    GLuint shader;
};

struct PluginProgramAttrBinding {
    uintptr_t offset;
    unsigned int stride;
    GLenum type;
    uint8_t size;
    uint8_t is_double;
};

struct PluginProgramUserdata {
    GLuint program;
    GLuint vao;
    GLbitfield bindings_enabled;
    struct hashmap* uniforms;
    struct PluginProgramAttrBinding bindings[MAX_PROGRAM_BINDINGS];
};

struct PluginShaderBufferUserdata {
    GLuint buffer;
};

struct PluginProgramUniform {
    GLint location;
    uint8_t count;
    uint8_t type; // 0 = vec, 1 = matrix, 2 = sampler
    union {
        uint8_t is_float; // for type=0
        uint8_t transpose; // for type=1
        GLuint sampler; // for type=2
    } sub;
    union {
        GLint i[4]; // for type=0 && !is_float
        GLfloat f[16]; // for everything else
    } values;
};

static int uniform_compare(const void* a, const void* b, void* udata) {
    return (*(GLuint*)a) - (*(GLuint*)b);
}

static uint64_t uniform_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const GLint* const id = item;
    return hashmap_sip(id, sizeof *id, seed0, seed1);
}

#define UCASE(N,TYPE) case N: gl.Uniform##N##TYPE##v(uniform->location, 1, uniform->values.TYPE); break;
#define UMATCASE(N) case N: gl.UniformMatrix##N##fv(uniform->location, 1, uniform->sub.transpose, uniform->values.f); break;
static void uniform_upload(const struct PluginProgramUniform* uniform, uint32_t* active_texture) {
    switch (uniform->type) {
        case 0:
            if (uniform->sub.is_float) {
                switch (uniform->count) {
                    UCASE(1,f)
                    UCASE(2,f)
                    UCASE(3,f)
                    UCASE(4,f)
                }
            } else {
                switch (uniform->count) {
                    UCASE(1,i)
                    UCASE(2,i)
                    UCASE(3,i)
                    UCASE(4,i)
                }
            }
            break;
        case 1:
            switch (uniform->count) {
                UMATCASE(2)
                UMATCASE(3)
                UMATCASE(4)
            }
            break;
        case 2:
            gl.ActiveTexture(GL_TEXTURE0 + *active_texture);
            lgl->BindTexture(GL_TEXTURE_2D, uniform->sub.sampler);
            gl.Uniform1i(uniform->location, *active_texture);
            *active_texture += 1;
            break;
    }
}
#undef UCASE
#undef UMATCASE

struct GLContext* _bolt_context() {
#if defined(_WIN32)
    return (struct GLContext*)TlsGetValue(current_context_tls);
#else
    return (struct GLContext*)pthread_getspecific(current_context_tls);
#endif
}

static void set_context(struct GLContext* context) {
#if defined(_WIN32)
    TlsSetValue(current_context_tls, (LPVOID)context);
#else
    pthread_setspecific(current_context_tls, context);
#endif
}

static size_t context_count() {
    size_t ret = 0;
    for (size_t i = 0; i < CONTEXTS_CAPACITY; i += 1) {
        if (contexts[i].id != 0) {
            ret += 1;
        }
    }
    return ret;
}

static void context_create(void* egl_context, void* shared) {
    for (size_t i = 0; i < CONTEXTS_CAPACITY; i += 1) {
        struct GLContext* ptr = &contexts[i];
        if (ptr->id == 0) {
            context_init(ptr, egl_context, shared);
            ptr->is_attached = 1;
            return;
        }
    }
}

static void context_destroy(void* egl_context) {
    for (size_t i = 0; i < CONTEXTS_CAPACITY; i += 1) {
        struct GLContext* ptr = &contexts[i];
        if (ptr->id == (uintptr_t)egl_context) {
            if (ptr->is_attached) {
                ptr->deferred_destroy = 1;
            } else {
                context_free(ptr);
                ptr->id = 0;
            }
            break;
        }
    }
}

static float f16_to_f32(uint16_t bits) {
    const uint16_t bits_exp_component = (bits & 0b0111110000000000);
    if (bits_exp_component == 0) return 0.0f; // truncate subnormals to 0
    const uint32_t sign_component = (bits & 0b1000000000000000) << 16;
    const uint32_t exponent = (bits_exp_component >> 10) + (127 - 15); // adjust exp bias
    const uint32_t mantissa = bits & 0b0000001111111111;
    const union { uint32_t b; float f; } u = {.b = sign_component | (exponent << 23) | (mantissa << 13)};
    return u.f;
}

static uint8_t attr_get_binding(struct GLContext* c, const struct GLAttrBinding* binding, size_t index, size_t num_out, float* out) {
    struct GLArrayBuffer* buffer = binding->buffer;
    if (!buffer || !buffer->data) return 0;
    uintptr_t buf_offset = binding->offset + (binding->stride * index);

    const uint8_t* ptr = (uint8_t*)buffer->data + buf_offset;
    if (!binding->normalise) {
        switch (binding->type) {
            case GL_FLOAT:
                memcpy(out, ptr, num_out * sizeof(float));
                break;
            case GL_HALF_FLOAT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = f16_to_f32(*(uint16_t*)(ptr + (i * 2)));
                break;
            case GL_UNSIGNED_BYTE:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (float)*(uint8_t*)(ptr + i);
                break;
            case GL_UNSIGNED_SHORT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (float)*(uint16_t*)(ptr + (i * 2));
                break;
            case GL_UNSIGNED_INT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (float)*(uint32_t*)(ptr + (i * 4));
                break;
            case GL_BYTE:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (float)*(int8_t*)(ptr + i);
                break;
            case GL_SHORT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (float)*(int16_t*)(ptr + (i * 2));
                break;
            case GL_INT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (float)*(int32_t*)(ptr + (i * 4));
                break;
            default:
                printf("warning: unsupported non-normalise type %u\n", binding->type);
                memset(out, 0, num_out * sizeof(float));
                break;
        }
    } else {
        switch (binding->type) {
            case GL_FLOAT:
                memcpy(out, ptr, num_out * sizeof(float));
                break;
            case GL_UNSIGNED_BYTE:
                for (size_t i = 0; i < num_out; i += 1) out[i] = ((float)*(uint8_t*)(ptr + i)) / 255.0;
                break;
            case GL_UNSIGNED_SHORT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = ((float)*(uint16_t*)(ptr + (i * 2))) / 65535.0;
                break;
            case GL_UNSIGNED_INT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = ((float)*(uint32_t*)(ptr + (i * 4))) / 4294967295.0;
                break;
            case GL_BYTE:
                for (size_t i = 0; i < num_out; i += 1) out[i] = ((((float)*(int8_t*)(ptr + i)) + 128.0) * 2.0 / 255.0) - 1.0;
                break;
            default:
                printf("warning: unsupported normalise type %u\n", binding->type);
                memset(out, 0, num_out * sizeof(float));
                break;
        }
    }
    return 1;
}

static uint8_t attr_get_binding_int(struct GLContext* c, const struct GLAttrBinding* binding, size_t index, size_t num_out, int32_t* out) {
    struct GLArrayBuffer* buffer = binding->buffer;
    if (!buffer || !buffer->data) return 0;
    uintptr_t buf_offset = binding->offset + (binding->stride * index);

    const uint8_t* ptr = (uint8_t*)buffer->data + buf_offset;
    if (!binding->normalise) {
        switch (binding->type) {
            case GL_UNSIGNED_BYTE:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (int32_t)*(uint8_t*)(ptr + i);
                break;
            case GL_UNSIGNED_SHORT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (int32_t)*(uint16_t*)(ptr + (i * 2));
                break;
            case GL_UNSIGNED_INT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (int32_t)*(uint32_t*)(ptr + (i * 4));
                break;
            case GL_BYTE:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (int32_t)*(int8_t*)(ptr + i);
                break;
            case GL_SHORT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (int32_t)*(int16_t*)(ptr + (i * 2));
                break;
            case GL_INT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (int32_t)*(int32_t*)(ptr + (i * 4));
                break;
            default:
                return 0;
        }
    } else {
        return 0;
    }
    return 1;
}

static int glhashmap_compare(const void* a, const void* b, void* udata) {
    return (**(GLuint**)a) - (**(GLuint**)b);
}

static uint64_t glhashmap_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const GLuint* const* const id = item;
    return hashmap_sip(*id, sizeof(GLuint), seed0, seed1);
}

static void glhashmap_init(struct HashMap* map, size_t cap) {
    _bolt_rwlock_init(&map->rwlock);
    map->map = hashmap_new(sizeof(void*), cap, 0, 0, glhashmap_hash, glhashmap_compare, NULL, NULL);
}

static void glhashmap_destroy(struct HashMap* map) {
    _bolt_rwlock_destroy(&map->rwlock);
    hashmap_free(map->map);
}

static void context_init(struct GLContext* context, void* egl_context, void* egl_shared) {
    struct GLContext* shared = NULL;
    if (egl_shared) {
        for (size_t i = 0; i < CONTEXTS_CAPACITY; i += 1) {
            struct GLContext* ptr = &contexts[i];
            if (ptr->id == (uintptr_t)egl_shared) {
                shared = ptr;
                break;
            }
        }
    }
    memset(context, 0, sizeof(*context));
    context->id = (uintptr_t)egl_context;
    context->texture_units = calloc(MAX_TEXTURE_UNITS, sizeof(struct TextureUnit));
    context->player_model_tex = -1;
    context->depth_tex = -1;
    context->game_view_part_framebuffer = -1;
    context->game_view_sSourceTex = -1;
    context->recalculate_depth_tex = true;
    context->blend_rgb_s = GL_ONE;
    //context->blend_rgb_d = GL_ZERO;
    context->blend_alpha_s = GL_ONE;
    //context->blend_alpha_d = GL_ZERO;
    if (shared) {
        context->programs = shared->programs;
        context->buffers = shared->buffers;
        context->textures = shared->textures;
        context->vaos = shared->vaos;
    } else {
        context->is_shared_owner = 1;
        context->programs = malloc(sizeof(struct HashMap));
        glhashmap_init(context->programs, PROGRAM_LIST_CAPACITY);
        context->buffers = malloc(sizeof(struct HashMap));
        glhashmap_init(context->buffers, BUFFER_LIST_CAPACITY);
        context->textures = malloc(sizeof(struct HashMap));
        glhashmap_init(context->textures, TEXTURE_LIST_CAPACITY);
        context->vaos = malloc(sizeof(struct HashMap));
        glhashmap_init(context->vaos, VAO_LIST_CAPACITY);
    }
}

static void context_free(struct GLContext* context) {
    free(context->texture_units);
    if (context->is_shared_owner) {
        glhashmap_destroy(context->programs);
        free(context->programs);
        glhashmap_destroy(context->buffers);
        free(context->buffers);
        glhashmap_destroy(context->textures);
        free(context->textures);
        glhashmap_destroy(context->vaos);
        free(context->vaos);
    }
}

static GLenum buffer_binding_enum(GLuint target) {
    switch (target) {
        case GL_ARRAY_BUFFER:
            return GL_ARRAY_BUFFER_BINDING;
        case GL_ELEMENT_ARRAY_BUFFER:
            return GL_ELEMENT_ARRAY_BUFFER_BINDING;
        case GL_UNIFORM_BUFFER:
            return GL_UNIFORM_BUFFER_BINDING;
        default:
            // we don't care about other types of buffer
            return -1;
    }
}

static struct GLProgram* context_get_program(struct GLContext* c, GLuint index) {
    struct HashMap* map = c->programs;
    const GLuint* index_ptr = &index;
    _bolt_rwlock_lock_read(&map->rwlock);
    struct GLProgram** program = (struct GLProgram**)hashmap_get(map->map, &index_ptr);
    struct GLProgram* ret = program ? *program : NULL;
    _bolt_rwlock_unlock_read(&map->rwlock);
    return ret;
}

static struct GLArrayBuffer* context_get_buffer(struct GLContext* c, GLuint index) {
    struct HashMap* map = c->buffers;
    const GLuint* index_ptr = &index;
    _bolt_rwlock_lock_read(&map->rwlock);
    struct GLArrayBuffer** buffer = (struct GLArrayBuffer**)hashmap_get(map->map, &index_ptr);
    struct GLArrayBuffer* ret = buffer ? *buffer : NULL;
    _bolt_rwlock_unlock_read(&map->rwlock);
    return ret;
}

static struct GLTexture2D* context_get_texture(struct GLContext* c, GLuint index) {
    struct HashMap* map = c->textures;
    const GLuint* index_ptr = &index;
    _bolt_rwlock_lock_read(&map->rwlock);
    struct GLTexture2D** tex = (struct GLTexture2D**)hashmap_get(map->map, &index_ptr);
    struct GLTexture2D* ret = tex ? *tex : NULL;
    _bolt_rwlock_unlock_read(&map->rwlock);
    return ret;
}

static struct GLVertexArray* context_get_vao(struct GLContext* c, GLuint index) {
    struct HashMap* map = c->vaos;
    const GLuint* index_ptr = &index;
    _bolt_rwlock_lock_read(&map->rwlock);
    struct GLVertexArray** vao = (struct GLVertexArray**)hashmap_get(map->map, &index_ptr);
    struct GLVertexArray* ret = vao ? *vao : NULL;
    _bolt_rwlock_unlock_read(&map->rwlock);
    return ret;
}

static void attr_set_binding(struct GLContext* c, struct GLAttrBinding* binding, unsigned int buffer, int size, const void* offset, unsigned int stride, uint32_t type, uint8_t normalise) {
    binding->buffer = context_get_buffer(c, buffer);
    binding->offset = (uintptr_t)offset;
    binding->size = size;
    binding->stride = stride;
    binding->normalise = normalise;
    binding->type = type;
}

static void bone_index_transform(struct GLContext* c, const struct GLArrayBuffer** transforms_ubo, uint8_t bone_id, struct Transform3D* out) {
    if (!*transforms_ubo) {
        GLint ubo_binding, ubo_index;
        gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_VertexTransformData, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
        gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_index);
        *transforms_ubo = context_get_buffer(c, ubo_index);
    }
    const uint8_t* ubo_transforms_buf = (uint8_t*)((*transforms_ubo)->data);
    const float* values = (float*)(ubo_transforms_buf + c->bound_program->offset_uBoneTransforms) + (bone_id * 12);
    out->matrix[0] = (double)values[0];
    out->matrix[1] = (double)values[1];
    out->matrix[2] = (double)values[2];
    out->matrix[3] = 0.0;
    out->matrix[4] = (double)values[3];
    out->matrix[5] = (double)values[4];
    out->matrix[6] = (double)values[5];
    out->matrix[7] = 0.0;
    out->matrix[8] = (double)values[6];
    out->matrix[9] = (double)values[7];
    out->matrix[10] = (double)values[8];
    out->matrix[11] = 0.0;
    out->matrix[12] = (double)values[9];
    out->matrix[13] = (double)values[10];
    out->matrix[14] = (double)values[11];
    out->matrix[15] = 1.0;
}

// multiplies the rows of `left` with the columns of `right`
static void multiply_transforms(const struct Transform3D* left, const struct Transform3D* right, struct Transform3D* out) {
    for (size_t row = 0; row < 4; row += 1) {
        for (size_t col = 0; col < 4; col += 1) {
            out->matrix[4 * row + col] =
                (left->matrix[row * 4    ] * right->matrix[col     ]) +
                (left->matrix[row * 4 + 1] * right->matrix[col + 4 ]) +
                (left->matrix[row * 4 + 2] * right->matrix[col + 8 ]) +
                (left->matrix[row * 4 + 3] * right->matrix[col + 12]);
        }
    }
}

static void unpack_rgb565(uint16_t packed, uint8_t out[3]) {
    out[0] = (packed >> 11) & 0b00011111;
    out[0] = (out[0] << 3) | (out[0] >> 2);
    out[1] = (packed >> 5) & 0b00111111;
    out[1] = (out[1] << 2) | (out[1] >> 4);
    out[2] = packed & 0b00011111;
    out[2] = (out[2] << 3) | (out[2] >> 2);
}

// use a lookup table for each of the possible 5-bit and 6-bit values for consistency across platforms
// `lut5 = [round(pow(((x << 3) | (x >> 2)) / 255.0, 2.2) * 255.0) for x in range(32)]`
// `lut6 = [round(pow(((x << 2) | (x >> 4)) / 255.0, 2.2) * 255.0) for x in range(64)]`
//const uint8_t lut5[] = {0, 0, 1, 1, 3, 5, 7, 9, 13, 17, 21, 26, 32, 38, 44, 51, 60, 68, 77, 87, 98, 109, 120, 132, 146, 159, 173, 188, 205, 221, 238, 255};
//const uint8_t lut6[] = {0, 0, 0, 0, 1, 1, 1, 2, 3, 3, 4, 5, 6, 8, 9, 11, 13, 14, 16, 18, 20, 23, 25, 28, 30, 33, 36, 39, 43, 46, 49, 53, 58, 62, 66, 70, 75, 79, 84, 89, 94, 99, 105, 110, 116, 121, 127, 133, 141, 148, 154, 161, 168, 175, 182, 190, 197, 205, 213, 221, 229, 238, 246, 255};
static void unpack_srgb565(uint16_t packed, uint8_t out[3]) {
    // game seems to be giving us RGBA and telling us it's SRGB, so for now, just don't convert it
    unpack_rgb565(packed, out);
    //out[0] = lut5[(packed >> 11) & 0b00011111];
    //out[1] = lut6[(packed >> 5) & 0b00111111];
    //out[2] = lut5[packed & 0b00011111];
}

// this function is called at the earliest possible opportunity, and is never undone
static void gl_load(void* (*GetProcAddress)(const char*)) {
#if defined(_WIN32)
    current_context_tls = TlsAlloc();
#else
    pthread_key_create(&current_context_tls, NULL);
#endif
#define INIT_GL_FUNC(NAME) gl.NAME = GetProcAddress("gl"#NAME);
    INIT_GL_FUNC(ActiveTexture)
    INIT_GL_FUNC(AttachShader)
    INIT_GL_FUNC(BindAttribLocation)
    INIT_GL_FUNC(BindBuffer)
    INIT_GL_FUNC(BindFramebuffer)
    INIT_GL_FUNC(BindVertexArray)
    INIT_GL_FUNC(BlendFuncSeparate)
    INIT_GL_FUNC(BlitFramebuffer)
    INIT_GL_FUNC(BufferData)
    INIT_GL_FUNC(BufferStorage)
    INIT_GL_FUNC(BufferSubData)
    INIT_GL_FUNC(CompileShader)
    INIT_GL_FUNC(CompressedTexSubImage2D)
    INIT_GL_FUNC(CopyImageSubData)
    INIT_GL_FUNC(CreateProgram)
    INIT_GL_FUNC(CreateShader)
    INIT_GL_FUNC(DeleteBuffers)
    INIT_GL_FUNC(DeleteFramebuffers)
    INIT_GL_FUNC(DeleteProgram)
    INIT_GL_FUNC(DeleteShader)
    INIT_GL_FUNC(DeleteVertexArrays)
    INIT_GL_FUNC(DetachShader)
    INIT_GL_FUNC(DisableVertexAttribArray)
    INIT_GL_FUNC(EnableVertexAttribArray)
    INIT_GL_FUNC(FlushMappedBufferRange)
    INIT_GL_FUNC(FramebufferTexture)
    INIT_GL_FUNC(FramebufferTextureLayer)
    INIT_GL_FUNC(GenBuffers)
    INIT_GL_FUNC(GenFramebuffers)
    INIT_GL_FUNC(GenVertexArrays)
    INIT_GL_FUNC(GetActiveUniformBlockiv)
    INIT_GL_FUNC(GetActiveUniformsiv)
    INIT_GL_FUNC(GetFramebufferAttachmentParameteriv)
    INIT_GL_FUNC(GetIntegeri_v)
    INIT_GL_FUNC(GetProgramInfoLog)
    INIT_GL_FUNC(GetProgramiv)
    INIT_GL_FUNC(GetShaderInfoLog)
    INIT_GL_FUNC(GetShaderiv)
    INIT_GL_FUNC(GetUniformBlockIndex)
    INIT_GL_FUNC(GetUniformfv)
    INIT_GL_FUNC(GetUniformIndices)
    INIT_GL_FUNC(GetUniformiv)
    INIT_GL_FUNC(GetUniformLocation)
    INIT_GL_FUNC(LinkProgram)
    INIT_GL_FUNC(MapBufferRange)
    INIT_GL_FUNC(MultiDrawElements)
    INIT_GL_FUNC(ShaderSource)
    INIT_GL_FUNC(TexStorage2D)
    INIT_GL_FUNC(TexStorage2DMultisample)
    INIT_GL_FUNC(Uniform1f)
    INIT_GL_FUNC(Uniform2f)
    INIT_GL_FUNC(Uniform3f)
    INIT_GL_FUNC(Uniform4f)
    INIT_GL_FUNC(Uniform1fv)
    INIT_GL_FUNC(Uniform2fv)
    INIT_GL_FUNC(Uniform3fv)
    INIT_GL_FUNC(Uniform4fv)
    INIT_GL_FUNC(Uniform1i)
    INIT_GL_FUNC(Uniform2i)
    INIT_GL_FUNC(Uniform3i)
    INIT_GL_FUNC(Uniform4i)
    INIT_GL_FUNC(Uniform1iv)
    INIT_GL_FUNC(Uniform2iv)
    INIT_GL_FUNC(Uniform3iv)
    INIT_GL_FUNC(Uniform4iv)
    INIT_GL_FUNC(UniformMatrix2fv)
    INIT_GL_FUNC(UniformMatrix3fv)
    INIT_GL_FUNC(UniformMatrix4fv)
    INIT_GL_FUNC(UnmapBuffer)
    INIT_GL_FUNC(UseProgram)
    INIT_GL_FUNC(VertexAttribLPointer)
    INIT_GL_FUNC(VertexAttribPointer)
#undef INIT_GL_FUNC
}

// this function is called when the "main" gl context gets created, and is undone by _bolt_gl_close()
// when all the contexts are destroyed.
static void gl_init() {
    GLint size;
    const GLchar* source;
    GLuint direct_screen_vs = gl.CreateShader(GL_VERTEX_SHADER);
    source = &program_direct_screen_vs[0];
    size = sizeof(program_direct_screen_vs) - sizeof(*program_direct_screen_vs);
    gl.ShaderSource(direct_screen_vs, 1, &source, &size);
    gl.CompileShader(direct_screen_vs);

    GLuint direct_surface_vs = gl.CreateShader(GL_VERTEX_SHADER);
    source = &program_direct_surface_vs[0];
    size = sizeof(program_direct_surface_vs) - sizeof(*program_direct_surface_vs);
    gl.ShaderSource(direct_surface_vs, 1, &source, &size);
    gl.CompileShader(direct_surface_vs);

    GLuint direct_fs = gl.CreateShader(GL_FRAGMENT_SHADER);
    source = &program_direct_fs[0];
    size = sizeof(program_direct_fs) - sizeof(*program_direct_fs);
    gl.ShaderSource(direct_fs, 1, &source, &size);
    gl.CompileShader(direct_fs);

    program_direct_screen.id = gl.CreateProgram();
    gl.AttachShader(program_direct_screen.id, direct_screen_vs);
    gl.AttachShader(program_direct_screen.id, direct_fs);
    gl.LinkProgram(program_direct_screen.id);
    program_direct_screen.sampler = gl.GetUniformLocation(program_direct_screen.id, "tex");
    program_direct_screen.d_xywh = gl.GetUniformLocation(program_direct_screen.id, "d_xywh");
    program_direct_screen.s_xywh = gl.GetUniformLocation(program_direct_screen.id, "s_xywh");
    program_direct_screen.src_wh_dest_wh = gl.GetUniformLocation(program_direct_screen.id, "src_wh_dest_wh");
    program_direct_screen.rgba = gl.GetUniformLocation(program_direct_screen.id, "rgba");

    program_direct_surface.id = gl.CreateProgram();
    gl.AttachShader(program_direct_surface.id, direct_surface_vs);
    gl.AttachShader(program_direct_surface.id, direct_fs);
    gl.LinkProgram(program_direct_surface.id);
    program_direct_surface.sampler = gl.GetUniformLocation(program_direct_surface.id, "tex");
    program_direct_surface.d_xywh = gl.GetUniformLocation(program_direct_surface.id, "d_xywh");
    program_direct_surface.s_xywh = gl.GetUniformLocation(program_direct_surface.id, "s_xywh");
    program_direct_surface.src_wh_dest_wh = gl.GetUniformLocation(program_direct_surface.id, "src_wh_dest_wh");
    program_direct_surface.rgba = gl.GetUniformLocation(program_direct_surface.id, "rgba");

    gl.DetachShader(program_direct_screen.id, direct_screen_vs);
    gl.DetachShader(program_direct_screen.id, direct_fs);
    gl.DetachShader(program_direct_surface.id, direct_surface_vs);
    gl.DetachShader(program_direct_surface.id, direct_fs);
    gl.DeleteShader(direct_screen_vs);
    gl.DeleteShader(direct_surface_vs);
    gl.DeleteShader(direct_fs);

    GLuint region_vs = gl.CreateShader(GL_VERTEX_SHADER);
    source = &program_region_vs[0];
    size = sizeof(program_region_vs) - sizeof(*program_region_vs);
    gl.ShaderSource(region_vs, 1, &source, &size);
    gl.CompileShader(region_vs);

    GLuint region_fs = gl.CreateShader(GL_FRAGMENT_SHADER);
    source = &program_region_fs[0];
    size = sizeof(program_region_fs) - sizeof(*program_region_fs);
    gl.ShaderSource(region_fs, 1, &source, &size);
    gl.CompileShader(region_fs);

    program_region = gl.CreateProgram();
    gl.AttachShader(program_region, region_vs);
    gl.AttachShader(program_region, region_fs);
    gl.LinkProgram(program_region);
    program_region_xywh = gl.GetUniformLocation(program_region, "xywh");
    program_region_dest_wh = gl.GetUniformLocation(program_region, "dest_wh");
    gl.DetachShader(program_region, region_vs);
    gl.DetachShader(program_region, region_fs);
    gl.DeleteShader(region_vs);
    gl.DeleteShader(region_fs);

    gl.GenVertexArrays(1, &program_direct_vao);
    gl.BindVertexArray(program_direct_vao);
    gl.GenBuffers(1, &buffer_vertices_square);
    gl.BindBuffer(GL_ARRAY_BUFFER, buffer_vertices_square);
    const GLfloat square[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    gl.BufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 8, square, GL_STATIC_DRAW);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 2, GL_FLOAT, 0, 2 * sizeof(float), NULL);
    gl.BindVertexArray(0);
}

void _bolt_gl_close() {
    gl.DeleteBuffers(1, &buffer_vertices_square);
    gl.DeleteProgram(program_direct_screen.id);
    gl.DeleteProgram(program_direct_surface.id);
    gl.DeleteVertexArrays(1, &program_direct_vao);
    context_destroy((void*)egl_main_context);
}

/* glproc function hooks */

static GLuint glCreateProgram() {
    LOG("glCreateProgram\n");
    GLuint id = gl.CreateProgram();
    struct GLContext* c = _bolt_context();
    struct GLProgram* program = malloc(sizeof(struct GLProgram));
    program->id = id;
    program->loc_aVertexPosition2D = -1;
    program->loc_aVertexColour = -1;
    program->loc_aTextureUV = -1;
    program->loc_aTextureUVAtlasMin = -1;
    program->loc_aTextureUVAtlasExtents = -1;
    program->loc_aMaterialSettingsSlotXY_TilePositionXZ = -1;
    program->loc_aVertexPosition_BoneLabel = -1;
    program->loc_aVertexSkinBones = -1;
    program->loc_aVertexSkinWeights = -1;
    program->loc_aParticleOrigin_CreationTime = -1;
    program->loc_aParticleOffset = -1;
    program->loc_aParticleVelocityAndUVScrollOffset = -1;
    program->loc_aParticleAlignmentUpAxis = -1;
    program->loc_aMaterialSettingsSlotXY_Rotation = -1;
    program->loc_aParticleUVAnimationData = -1;
    program->loc_aVertexPositionDepthOffset = -1;
    program->loc_aBillboardSize = -1;
    program->loc_aMaterialSettingsSlotXY_UV = -1;
    program->loc_uDiffuseMap = -1;
    program->loc_uTextureAtlas = -1;
    program->loc_uTextureAtlasSettings = -1;
    program->loc_uSecondsSinceStart = -1;
    program->loc_sSceneHDRTex = -1;
    program->loc_sTexture = -1;
    program->block_index_ViewTransforms = -1;
    program->offset_uCameraPosition = -1;
    program->offset_uViewMatrix = -1;
    program->offset_uProjectionMatrix = -1;
    program->offset_uViewProjMatrix = -1;
    program->offset_uInvViewMatrix = -1;
    program->block_index_BatchConsts = -1;
    program->offset_uAtlasMeta = -1;
    program->offset_uVertexScale = -1;
    program->block_index_ModelConsts = -1;
    program->offset_uModelMatrix = -1;
    program->block_index_VertexTransformData = -1;
    program->offset_uBoneTransforms = -1;
    program->offset_uSmoothSkinning = -1;
    program->block_index_ParticleConsts = -1;
    program->offset_uParticleEmitterTransforms = -1;
    program->offset_uParticleEmitterTransformRanges = -1;
    program->block_index_TerrainConsts = -1;
    program->offset_uGridSize = -1;
    program->block_index_GUIConsts = -1;
    program->is_2d = 0;
    program->is_3d = 0;
    program->is_minimap = 0;
    program->is_particle = 0;
    program->is_billboard = 0;
    _bolt_rwlock_lock_write(&c->programs->rwlock);
    hashmap_set(c->programs->map, &program);
    _bolt_rwlock_unlock_write(&c->programs->rwlock);
    LOG("glCreateProgram end\n");
    return id;
}

static void glDeleteProgram(GLuint program) {
    LOG("glDeleteProgram\n");
    struct GLContext* c = _bolt_context();
    unsigned int* ptr = &program;
    _bolt_rwlock_lock_write(&c->programs->rwlock);
    struct GLProgram* const* p = hashmap_delete(c->programs->map, &ptr);
    free(*p);
    _bolt_rwlock_unlock_write(&c->programs->rwlock);
    gl.DeleteProgram(program);
    LOG("glDeleteProgram end\n");
}

static void glBindAttribLocation(GLuint program, GLuint index, const GLchar* name) {
    LOG("glBindAttribLocation\n");
    gl.BindAttribLocation(program, index, name);
    struct GLContext* c = _bolt_context();
    struct GLProgram* p = context_get_program(c, program);
#define ATTRIB_MAP(NAME) if (!strcmp(name, #NAME)) p->loc_##NAME = index;
    ATTRIB_MAP(aVertexPosition2D)
    ATTRIB_MAP(aVertexColour)
    ATTRIB_MAP(aTextureUV)
    ATTRIB_MAP(aTextureUVAtlasMin)
    ATTRIB_MAP(aTextureUVAtlasExtents)
    ATTRIB_MAP(aMaterialSettingsSlotXY_TilePositionXZ)
    ATTRIB_MAP(aVertexPosition_BoneLabel)
    ATTRIB_MAP(aVertexSkinBones)
    ATTRIB_MAP(aVertexSkinWeights)
    ATTRIB_MAP(aParticleOrigin_CreationTime)
    ATTRIB_MAP(aParticleOffset)
    ATTRIB_MAP(aParticleVelocityAndUVScrollOffset)
    ATTRIB_MAP(aParticleAlignmentUpAxis)
    ATTRIB_MAP(aMaterialSettingsSlotXY_Rotation)
    ATTRIB_MAP(aParticleUVAnimationData)
    ATTRIB_MAP(aVertexPositionDepthOffset)
    ATTRIB_MAP(aBillboardSize)
    ATTRIB_MAP(aMaterialSettingsSlotXY_UV)
#undef ATTRIB_MAP
    LOG("glBindAttribLocation end\n");
}

static void glLinkProgram(GLuint program) {
    LOG("glLinkProgram\n");
    gl.LinkProgram(program);
    struct GLContext* c = _bolt_context();
    struct GLProgram* p = context_get_program(c, program);

    const GLchar* ViewTransforms_var_names[] = {"uCameraPosition", "uViewMatrix", "uProjectionMatrix", "uViewProjMatrix", "uInvViewMatrix"};
    const GLchar* BatchConsts_var_names[] = {"uAtlasMeta", "uVertexScale"};
    const GLchar* VertexTransformData_var_names[] = {"uBoneTransforms", "uSmoothSkinning"};
    const GLchar* ModelConsts_var_names[] = {"uModelMatrix"};
    const GLchar* ParticleConsts_var_names[] = {"uParticleEmitterTransforms", "uParticleEmitterTransformRanges", "uAtlasMeta"};
    const GLchar* TerrainConsts_var_names[] = {"uGridSize", "uModelMatrix"};
    const GLchar* GUIConsts_var_names[] = {"uProjectionMatrix"};
    const GLchar* BilloardConsts_var_names[] = {"uModelMatrix", "uAtlasMeta"};

#define DEFINEUBO(NAME) \
    const GLuint block_index_##NAME = gl.GetUniformBlockIndex(program, #NAME); \
    GLuint ubo_indices_##NAME[sizeof(NAME##_var_names) / sizeof(*NAME##_var_names)]; \
    GLint NAME##_offsets[sizeof(NAME##_var_names) / sizeof(*NAME##_var_names)]; \
    if ((GLint)block_index_##NAME != -1) { \
        gl.GetUniformIndices(program, sizeof(NAME##_var_names) / sizeof(*NAME##_var_names), NAME##_var_names, ubo_indices_##NAME); \
        gl.GetActiveUniformsiv(program, sizeof(NAME##_var_names) / sizeof(*NAME##_var_names), ubo_indices_##NAME, GL_UNIFORM_OFFSET, NAME##_offsets); \
    }
    DEFINEUBO(ViewTransforms)
    DEFINEUBO(BatchConsts)
    DEFINEUBO(VertexTransformData)
    DEFINEUBO(ModelConsts)
    DEFINEUBO(ParticleConsts)
    DEFINEUBO(TerrainConsts)
    DEFINEUBO(GUIConsts)
    DEFINEUBO(BilloardConsts) // not a mistake, the game engine spells it like that
#undef DEFINEUBO

    if (p) {
        p->loc_uDiffuseMap = gl.GetUniformLocation(program, "uDiffuseMap");
        p->loc_uTextureAtlas = gl.GetUniformLocation(program, "uTextureAtlas");
        p->loc_uTextureAtlasSettings = gl.GetUniformLocation(program, "uTextureAtlasSettings");
        p->loc_uSecondsSinceStart = gl.GetUniformLocation(program, "uSecondsSinceStart");
        p->loc_sSceneHDRTex = gl.GetUniformLocation(program, "sSceneHDRTex");
        p->loc_sSourceTex = gl.GetUniformLocation(program, "sSourceTex");
        p->loc_sBlurFarTex = gl.GetUniformLocation(program, "sBlurFarTex");
        p->loc_sTexture = gl.GetUniformLocation(program, "sTexture");
        if (block_index_ViewTransforms != -1 && block_index_TerrainConsts != -1) {
            p->block_index_ViewTransforms = block_index_ViewTransforms;
            p->offset_uCameraPosition = ViewTransforms_offsets[0];
            p->offset_uViewMatrix = ViewTransforms_offsets[1];
            p->offset_uProjectionMatrix = ViewTransforms_offsets[2];
            p->offset_uViewProjMatrix = ViewTransforms_offsets[3];
            p->offset_uInvViewMatrix = ViewTransforms_offsets[4];
            p->block_index_ModelConsts = block_index_TerrainConsts;
            p->offset_uGridSize = TerrainConsts_offsets[0];
            p->offset_uModelMatrix = TerrainConsts_offsets[1];
            p->is_minimap = 1;
        }
        if (p->loc_aVertexPosition2D != -1 && p->loc_aVertexColour != -1 && p->loc_aTextureUV != -1 && p->loc_aTextureUVAtlasMin != -1 && p->loc_aTextureUVAtlasExtents != -1 && p->loc_uDiffuseMap != -1 && block_index_GUIConsts != -1) {
            p->block_index_GUIConsts = block_index_GUIConsts;
            p->offset_uProjectionMatrix = GUIConsts_offsets[0];
            p->is_2d = 1;
        }
        if (p->loc_aTextureUV != -1 && p->loc_aVertexColour != -1 && p->loc_aVertexPosition_BoneLabel != -1 && p->loc_aMaterialSettingsSlotXY_TilePositionXZ != -1 && p->loc_uTextureAtlas != -1 && p->loc_uTextureAtlasSettings != -1 && block_index_ViewTransforms != -1 && block_index_ModelConsts != -1 && block_index_BatchConsts != -1) {
            p->block_index_ViewTransforms = block_index_ViewTransforms;
            p->offset_uCameraPosition = ViewTransforms_offsets[0];
            p->offset_uViewMatrix = ViewTransforms_offsets[1];
            p->offset_uProjectionMatrix = ViewTransforms_offsets[2];
            p->offset_uViewProjMatrix = ViewTransforms_offsets[3];
            p->offset_uInvViewMatrix = ViewTransforms_offsets[4];
            p->block_index_BatchConsts = block_index_BatchConsts;
            p->offset_uAtlasMeta = BatchConsts_offsets[0];
            p->offset_uVertexScale = BatchConsts_offsets[1];
            p->block_index_ModelConsts = block_index_ModelConsts;
            p->offset_uModelMatrix = ModelConsts_offsets[0];
            p->block_index_VertexTransformData = block_index_VertexTransformData;
            p->offset_uBoneTransforms = VertexTransformData_offsets[0];
            p->offset_uSmoothSkinning = VertexTransformData_offsets[1];
            p->is_3d = 1;
        }
        if (p->loc_aParticleOrigin_CreationTime != -1 && p->loc_aParticleOffset != -1 && p->loc_aParticleVelocityAndUVScrollOffset != -1 && p->loc_aParticleAlignmentUpAxis != -1 && p->loc_aMaterialSettingsSlotXY_Rotation != -1 && p->loc_aVertexColour != -1 && p->loc_aParticleUVAnimationData != -1 && p->loc_uTextureAtlas != -1 && p->loc_uTextureAtlasSettings != -1 && p->loc_uSecondsSinceStart != -1 && block_index_ViewTransforms != -1 && block_index_ParticleConsts != -1) {
            p->block_index_ParticleConsts = block_index_ParticleConsts;
            p->offset_uParticleEmitterTransforms = ParticleConsts_offsets[0];
            p->offset_uParticleEmitterTransformRanges = ParticleConsts_offsets[1];
            p->offset_uAtlasMeta = ParticleConsts_offsets[2];
            p->block_index_ViewTransforms = block_index_ViewTransforms;
            p->offset_uCameraPosition = ViewTransforms_offsets[0];
            p->offset_uViewMatrix = ViewTransforms_offsets[1];
            p->offset_uProjectionMatrix = ViewTransforms_offsets[2];
            p->offset_uViewProjMatrix = ViewTransforms_offsets[3];
            p->offset_uInvViewMatrix = ViewTransforms_offsets[4];
            p->is_particle = 1;
        }
        if (p->loc_aVertexPositionDepthOffset != -1 && p->loc_aVertexColour != -1 && p->loc_aBillboardSize != -1 && p->loc_aMaterialSettingsSlotXY_UV != -1 && p->loc_uTextureAtlas != -1 && p->loc_uTextureAtlasSettings != -1 && block_index_ViewTransforms != -1 && block_index_BilloardConsts != -1) {
            p->block_index_BilloardConsts = block_index_BilloardConsts;
            p->offset_uModelMatrix = BilloardConsts_offsets[0];
            p->offset_uAtlasMeta = BilloardConsts_offsets[1];
            p->block_index_ViewTransforms = block_index_ViewTransforms;
            p->offset_uCameraPosition = ViewTransforms_offsets[0];
            p->offset_uViewMatrix = ViewTransforms_offsets[1];
            p->offset_uProjectionMatrix = ViewTransforms_offsets[2];
            p->offset_uViewProjMatrix = ViewTransforms_offsets[3];
            p->offset_uInvViewMatrix = ViewTransforms_offsets[4];
            p->is_billboard = 1;
        }
    }
    LOG("glLinkProgram end\n");
}

static void glUseProgram(GLuint program) {
    LOG("glUseProgram\n");
    gl.UseProgram(program);
    struct GLContext* c = _bolt_context();
    c->bound_program = context_get_program(c, program);
    LOG("glUseProgram end\n");
}

static void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    LOG("glTexStorage2D\n");
    gl.TexStorage2D(target, levels, internalformat, width, height);
    struct GLContext* c = _bolt_context();
    if (target == GL_TEXTURE_2D) {
        struct GLTexture2D* tex = c->texture_units[c->active_texture].texture_2d;
        free(tex->data);
        tex->data = calloc(width * height * 4, sizeof(*tex->data));
        tex->internalformat = internalformat;
        tex->width = width;
        tex->height = height;
        tex->icon.model_count = 0;
    }
    LOG("glTexStorage2D end\n");
}

static void glTexStorage2DMultisample(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    LOG("glTexStorage2DMultisample\n");
    gl.TexStorage2DMultisample(target, levels, internalformat, width, height, fixedsamplelocations);
    struct GLContext* c = _bolt_context();
    if (target == GL_TEXTURE_2D_MULTISAMPLE) {
        struct GLTexture2D* tex = c->texture_units[c->active_texture].texture_2d_multisample;
        free(tex->data);
        tex->data = calloc(width * height * 4, sizeof(*tex->data));
        tex->internalformat = internalformat;
        tex->width = width;
        tex->height = height;
        tex->icon.model_count = 0;
        tex->is_multisample = true;
    }
    LOG("glTexStorage2DMultisample end\n");
}

static void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalised, GLsizei stride, const void* pointer) {
    LOG("glVertexAttribPointer\n");
    gl.VertexAttribPointer(index, size, type, normalised, stride, pointer);
    struct GLContext* c = _bolt_context();
    GLint array_binding;
    lgl->GetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_binding);
    attr_set_binding(c, &c->bound_vao->attributes[index], array_binding, size, pointer, stride, type, normalised);
    LOG("glVertexAttribPointer end\n");
}

static void glGenBuffers(GLsizei n, GLuint* buffers) {
    LOG("glGenBuffers\n");
    gl.GenBuffers(n, buffers);
    struct GLContext* c = _bolt_context();
    _bolt_rwlock_lock_write(&c->buffers->rwlock);
    for (GLsizei i = 0; i < n; i += 1) {
        struct GLArrayBuffer* buffer = calloc(1, sizeof(struct GLArrayBuffer));
        buffer->id = buffers[i];
        hashmap_set(c->buffers->map, &buffer);
    }
    _bolt_rwlock_unlock_write(&c->buffers->rwlock);
    LOG("glGenBuffers end\n");
}

static void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    LOG("glBufferData\n");
    gl.BufferData(target, size, data, usage);
    struct GLContext* c = _bolt_context();
    GLenum binding_type = buffer_binding_enum(target);
    if (binding_type != -1) {
        GLint buffer_id;
        lgl->GetIntegerv(binding_type, &buffer_id);
        void* buffer_content = malloc(size);
        if (data) memcpy(buffer_content, data, size);
        struct GLArrayBuffer* buffer = context_get_buffer(c, buffer_id);
        free(buffer->data);
        buffer->data = buffer_content;
    }
    LOG("glBufferData end\n");
}

static void glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    LOG("glDeleteBuffers\n");
    struct GLContext* c = _bolt_context();
    _bolt_rwlock_lock_write(&c->buffers->rwlock);
    for (GLsizei i = 0; i < n; i += 1) {
        const GLuint* ptr = &buffers[i];
        struct GLArrayBuffer* const* buffer = hashmap_delete(c->buffers->map, &ptr);
        free((*buffer)->data);
        free((*buffer)->mapping);
        free(*buffer);
    }
    _bolt_rwlock_unlock_write(&c->buffers->rwlock);
    gl.DeleteBuffers(n, buffers);
    LOG("glDeleteBuffers end\n");
}

static void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    LOG("glBindFramebuffer\n");
    gl.BindFramebuffer(target, framebuffer);
    struct GLContext* c = _bolt_context();
    switch (target) {
        case GL_READ_FRAMEBUFFER:
            c->current_read_framebuffer = framebuffer;
            break;
        case GL_DRAW_FRAMEBUFFER:
            c->current_draw_framebuffer = framebuffer;
            break;
        case GL_FRAMEBUFFER:
            c->current_read_framebuffer = framebuffer;
            c->current_draw_framebuffer = framebuffer;
            break;
    }
    LOG("glBindFramebuffer end\n");
}

// https://www.khronos.org/opengl/wiki/S3_Texture_Compression
static void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
    LOG("glCompressedTexSubImage2D\n");
    gl.CompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    if (target != GL_TEXTURE_2D || level != 0 || width <= 0 || height <= 0) return;
    const uint8_t is_dxt1 = (format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT || format == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT || format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT);
    const uint8_t is_srgb = (format == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT || format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT || format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT || format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT);
    const size_t input_stride = is_dxt1 ? 8 : 16;
    void (*const unpack565)(uint16_t, uint8_t*) = is_srgb ? unpack_srgb565 : unpack_rgb565;
    struct GLContext* c = _bolt_context();
    struct GLTexture2D* tex = c->texture_units[c->active_texture].texture_2d;
    GLint out_xoffset = xoffset;
    GLint out_yoffset = yoffset;
    for (size_t ii = 0; ii < ((size_t)width * (size_t)height); ii += input_stride) {
        const uint8_t* ptr = (uint8_t*)data + ii;
        const uint8_t* cptr = is_dxt1 ? ptr : (ptr + 8);
        uint8_t* out_ptr = tex->data + (out_yoffset * tex->width * 4) + (out_xoffset * 4);
        uint16_t c0 = cptr[0] + (cptr[1] << 8);
        uint16_t c1 = cptr[2] + (cptr[3] << 8);
        uint8_t c0_rgb[3];
        uint8_t c1_rgb[3];
        unpack565(c0, c0_rgb);
        unpack565(c1, c1_rgb);
        const uint8_t c0_greater = c0 > c1;
        const uint32_t ctable = cptr[4] + (cptr[5] << 8) + (cptr[6] << 16) + (cptr[7] << 24);
        const uint8_t alpha0 = ptr[0];
        const uint8_t alpha1 = ptr[1];
        const uint8_t a0_greater = alpha0 > alpha1;
        const uint64_t atable = ptr[2] + ((uint64_t)ptr[3] << 8) + ((uint64_t)ptr[4] << 16) + ((uint64_t)ptr[5] << 24) + ((uint64_t)ptr[6] << 32) + ((uint64_t)ptr[7] << 40);

        for (size_t j = 0; j < 4; j += 1) {
            for (size_t i = 0; i < 4; i += 1) {
                if (out_xoffset + i >= 0 && out_yoffset + j >= 0 && out_xoffset + i < tex->width && out_yoffset + j < tex->height) {
                    uint8_t* pixel_ptr = out_ptr + (tex->width * j * 4) + (i * 4);
                    uint8_t pixel_index = (4 * j) + i;
                    const uint8_t code = (ctable >> (pixel_index * 2)) & 0b11;

                    // parse colour table and set R, G and B bytes for this pixel
                    switch(code) {
                        case 0:
                            memcpy(pixel_ptr, c0_rgb, 3);
                            break;
                        case 1:
                            memcpy(pixel_ptr, c1_rgb, 3);
                            break;
                        case 2:
                            if (c0_greater) {
                                pixel_ptr[0] = (2*c0_rgb[0]+c1_rgb[0])/3;
                                pixel_ptr[1] = (2*c0_rgb[1]+c1_rgb[1])/3;
                                pixel_ptr[2] = (2*c0_rgb[2]+c1_rgb[2])/3;
                            } else {
                                pixel_ptr[0] = (c0_rgb[0]+c1_rgb[0])/2;
                                pixel_ptr[1] = (c0_rgb[1]+c1_rgb[1])/2;
                                pixel_ptr[2] = (c0_rgb[2]+c1_rgb[2])/2;
                            }
                            break;
                        case 3:
                            if (c0_greater) {
                                pixel_ptr[0] = (2*c1_rgb[0]+c0_rgb[0])/3;
                                pixel_ptr[1] = (2*c1_rgb[1]+c0_rgb[1])/3;
                                pixel_ptr[2] = (2*c1_rgb[2]+c0_rgb[2])/3;
                            } else {
                                memset(pixel_ptr, 0, 3);
                            }
                            break;
                    }

                    // parse alpha table (if any) and set alpha byte for this pixel
                    switch (format) {
                        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
                        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT: {
                            // no alpha channel at all
                            pixel_ptr[3] = 0xFF;
                            break;
                        }
                        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT: {
                            // black means zero alpha
                            pixel_ptr[3] = (code == 3 && !c0_greater) ? 0 : 0xFF;
                            break;
                        }
                        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
                        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT: {
                            // 4-bit alpha value from the alpha chunk
                            pixel_ptr[3] = ((ptr[pixel_index / 2] >> ((i & 1) ? 4 : 0)) & 0b1111) * 17;
                            break;
                        }
                        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: {
                            // 3-bit instruction value from atable
                            const uint8_t code = (atable >> (pixel_index * 3)) & 0b111;
                            switch (code) {
                                case 0:
                                    pixel_ptr[3] = alpha0;
                                    break;
                                case 1:
                                    pixel_ptr[3] = alpha1;
                                    break;
                                case 6:
                                case 7:
                                    if (!a0_greater) {
                                        pixel_ptr[3] = code == 6 ? 0 : 0xFF;
                                        break;
                                    }
                                    // fall through is intentional
                                default:
                                    if (a0_greater) pixel_ptr[3] = (((8 - code) * (uint16_t)alpha0) + ((code - 1) * (uint16_t)alpha1)) / 7;
                                    else pixel_ptr[3] = (((6 - code) * (uint16_t)alpha0) + ((code - 1) * (uint16_t)alpha1)) / 5;
                                    break;
                            }
                            break;
                        }
                    }
                }
            }
        }

        out_xoffset += 4;
        if (out_xoffset >= xoffset + width) {
            out_xoffset = xoffset;
            out_yoffset += 4;
        }
    }
    LOG("glCompressedTexSubImage2D end\n");
}

static void glCopyImageSubData(
    GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ,
    GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
    GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth
) {
    LOG("glCopyImageSubData\n");
    gl.CopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    struct GLContext* c = _bolt_context();
    if (srcTarget == GL_TEXTURE_2D && dstTarget == GL_TEXTURE_2D && srcLevel == 0 && dstLevel == 0) {
        struct GLTexture2D* src = context_get_texture(c, srcName);
        struct GLTexture2D* dst = context_get_texture(c, dstName);
        if (!c->does_blit_3d_target && c->depth_of_field_enabled && dst->id == c->depth_of_field_sSourceTex) {
            if (srcX == 0 && srcY == 0 && dstX == 0 && dstY == 0 && src->width == dst->width && src->height == dst->height && src->width == srcWidth && src->height == srcHeight) {
                printf("copy to depth-of-field tex from tex %i\n", src->id);
                c->does_blit_3d_target = true;
                c->target_3d_tex = src->id;
            }
        } else if (!src->icon.is_big_icon && src->icon.model_count && dst->width > GAME_ITEM_ICON_SIZE && dst->height > GAME_ITEM_ICON_SIZE) {
            if (!dst->icons) {
                dst->icons = hashmap_new(sizeof(struct Icon), 256, 0, 0, _bolt_plugin_itemicon_hash, _bolt_plugin_itemicon_compare, NULL, NULL);
            }
            src->icon.x = dstX;
            src->icon.y = dstY;
            src->icon.w = srcWidth;
            src->icon.h = srcHeight;
            const struct Icon* old_icon = hashmap_set(dst->icons, &src->icon);
            if (old_icon) {
                for (size_t i = 0; i < old_icon->model_count; i += 1) {
                    free(old_icon->models[i].vertices);
                }
            }
            src->icon.model_count = 0;
        } else if (src->id != c->target_3d_tex) {
            for (GLsizei i = 0; i < srcHeight; i += 1) {
                memcpy(dst->data + (dstY * dst->width * 4) + (dstX * 4), src->data + (srcY * src->width * 4) + (srcX * 4), srcWidth * 4);
            }
        }
    }
    LOG("glCopyImageSubData end\n");
}

static void glEnableVertexAttribArray(GLuint index) {
    LOG("glEnableVertexAttribArray\n");
    gl.EnableVertexAttribArray(index);
    struct GLContext* c = _bolt_context();
    c->bound_vao->attributes[index].enabled = 1;
    LOG("glEnableVertexAttribArray end\n");
}

static void glDisableVertexAttribArray(GLuint index) {
    LOG("glDisableVertexAttribArray\n");
    gl.DisableVertexAttribArray(index);
    struct GLContext* c = _bolt_context();
    c->bound_vao->attributes[index].enabled = 0;
    LOG("glDisableVertexAttribArray end\n");
}

static void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    LOG("glMapBufferRange\n");
    struct GLContext* c = _bolt_context();
    GLenum binding_type = buffer_binding_enum(target);
    if (binding_type != -1) {
        GLint buffer_id;
        lgl->GetIntegerv(binding_type, &buffer_id);
        struct GLArrayBuffer* buffer = context_get_buffer(c, buffer_id);
        buffer->mapping = malloc(length);
        buffer->mapping_offset = offset;
        buffer->mapping_len = length;
        buffer->mapping_access_type = access;
        LOG("glMapBufferRange end (intercepted)\n");
        return buffer->mapping;
    } else {
        void* ret = gl.MapBufferRange(target, offset, length, access);
        LOG("glMapBufferRange end (not intercepted)\n");
        return ret;
    }
}

static GLboolean glUnmapBuffer(GLuint target) {
    LOG("glUnmapBuffer\n");
    struct GLContext* c = _bolt_context();
    GLenum binding_type = buffer_binding_enum(target);
    if (binding_type != -1) {
        GLint buffer_id;
        lgl->GetIntegerv(binding_type, &buffer_id);
        struct GLArrayBuffer* buffer = context_get_buffer(c, buffer_id);
        free(buffer->mapping);
        buffer->mapping = NULL;
        LOG("glUnmapBuffer end (intercepted)\n");
        return 1;
    } else {
        GLboolean ret = gl.UnmapBuffer(target);
        LOG("glUnmapBuffer end (not intercepted)\n");
        return ret;
    }
}

static void glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags) {
    LOG("glBufferStorage\n");
    gl.BufferStorage(target, size, data, flags);
    struct GLContext* c = _bolt_context();
    GLenum binding_type = buffer_binding_enum(target);
    if (binding_type != -1) {
        GLint buffer_id;
        lgl->GetIntegerv(binding_type, &buffer_id);
        void* buffer_content = malloc(size);
        if (data) memcpy(buffer_content, data, size);
        struct GLArrayBuffer* buffer = context_get_buffer(c, buffer_id);
        free(buffer->data);
        buffer->data = buffer_content;
    }
    LOGF("glBufferStorage end (%s)\n", binding_type == -1 ? "not intercepted" : "intercepted");
}

static void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    LOG("glFlushMappedBufferRange\n");
    struct GLContext* c = _bolt_context();
    GLenum binding_type = buffer_binding_enum(target);
    if (binding_type != -1) {
        GLint buffer_id;
        lgl->GetIntegerv(binding_type, &buffer_id);
        struct GLArrayBuffer* buffer = context_get_buffer(c, buffer_id);
        gl.BufferSubData(target, buffer->mapping_offset + offset, length, buffer->mapping + offset);
        memcpy((uint8_t*)buffer->data + buffer->mapping_offset + offset, buffer->mapping + offset, length);
    } else {
        gl.FlushMappedBufferRange(target, offset, length);
    }
    LOGF("glFlushMappedBufferRange end (%s)\n", binding_type == -1 ? "not intercepted" : "intercepted");
}

static void glActiveTexture(GLenum texture) {
    LOG("glActiveTexture\n");
    gl.ActiveTexture(texture);
    struct GLContext* c = _bolt_context();
    c->active_texture = texture - GL_TEXTURE0;
    LOG("glActiveTexture end\n");
}

static void glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount) {
    LOG("glMultiDrawElements\n");
    gl.MultiDrawElements(mode, count, type, indices, drawcount);
    for (GLsizei i = 0; i < drawcount; i += 1) {
        _bolt_gl_onDrawElements(mode, count[i], type, indices[i]);
    }
    LOG("glMultiDrawElements end\n");
}

static void glGenVertexArrays(GLsizei n, GLuint* arrays) {
    LOG("glGenVertexArrays\n");
    gl.GenVertexArrays(n, arrays);
    struct GLContext* c = _bolt_context();
    GLint attrib_count;
    lgl->GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attrib_count);
    _bolt_rwlock_lock_write(&c->vaos->rwlock);
    for (GLsizei i = 0; i < n; i += 1) {
        struct GLVertexArray* array = malloc(sizeof(struct GLVertexArray));
        array->id = arrays[i];
        array->attributes = calloc(attrib_count, sizeof(struct GLAttrBinding));
        hashmap_set(c->vaos->map, &array);
    }
    _bolt_rwlock_unlock_write(&c->vaos->rwlock);
    LOG("glGenVertexArrays end\n");
}

static void glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    LOG("glDeleteVertexArrays\n");
    struct GLContext* c = _bolt_context();
    _bolt_rwlock_lock_write(&c->vaos->rwlock);
    for (GLsizei i = 0; i < n; i += 1) {
        const GLuint* ptr = &arrays[i];
        struct GLVertexArray* const* vao = hashmap_delete(c->vaos->map, &ptr);
        free((*vao)->attributes);
        free(*vao);
    }
    _bolt_rwlock_unlock_write(&c->vaos->rwlock);
    gl.DeleteVertexArrays(n, arrays);
    LOG("glDeleteVertexArrays end\n");
}

static void glBindVertexArray(GLuint array) {
    LOG("glBindVertexArray\n");
    gl.BindVertexArray(array);
    struct GLContext* c = _bolt_context();
    c->bound_vao = context_get_vao(c, array);
    LOG("glBindVertexArray end\n");
}

static void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    LOG("glBlitFramebuffer\n");
    gl.BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    struct GLContext* c = _bolt_context();
    if (c->current_draw_framebuffer == 0 && c->game_view_part_framebuffer != c->current_read_framebuffer) {
        c->game_view_part_framebuffer = c->current_read_framebuffer;
        c->recalculate_sSceneHDRTex = true;
        c->game_view_sSceneHDRTex = -1;
        c->game_view_sSourceTex = -1;
        c->does_blit_3d_target = false;
        c->depth_of_field_enabled = false;
        c->game_view_x = dstX0;
        c->game_view_y = dstY0;
        printf("new game_view_part_framebuffer %u...\n", c->current_read_framebuffer);
    } else if (srcX0 == 0 && dstX0 == 0 && srcY0 == 0 && dstY0 == 0 && srcX1 == dstX1 && srcY1 == dstY1 && c->current_draw_framebuffer != 0) {
        GLint tex_id;
        gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex_id);
        struct GLTexture2D* target_tex = context_get_texture(c, tex_id);
        if (!c->does_blit_3d_target && target_tex && target_tex->width == dstX1 && target_tex->height == dstY1) {
            if (!c->depth_of_field_enabled && target_tex->id == c->game_view_sSceneHDRTex) {
                printf("does blit to sSceneHDRTex from fb %u\n", c->current_read_framebuffer);
                c->does_blit_3d_target = true;
                gl.GetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex_id);
                c->target_3d_tex = tex_id;
            } else if (c->depth_of_field_enabled && target_tex->id == c->depth_of_field_sSourceTex) {
                printf("blit to depth-of-field tex from fb %u\n", c->current_read_framebuffer);
                c->does_blit_3d_target = true;
                gl.GetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex_id);
                c->target_3d_tex = tex_id;
            }
        } else if (target_tex) {
            gl.GetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex_id);
            struct GLTexture2D* read_tex = context_get_texture(c, tex_id);
            if (read_tex && read_tex->icon.model_count && read_tex->icon.is_big_icon) {
                target_tex->icon = read_tex->icon;
                read_tex->icon.model_count = false;
            }
        }
    }
    LOG("glBlitFramebuffer end\n");
}

static void glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    gl.BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
    struct GLContext* c = _bolt_context();
    c->blend_rgb_s = srcRGB;
    c->blend_rgb_d = dstRGB;
    c->blend_alpha_s = srcAlpha;
    c->blend_alpha_d = dstAlpha;
}

static void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    LOG("glBufferSubData\n");
    gl.BufferSubData(target, offset, size, data);
    struct GLContext* c = _bolt_context();
    GLenum binding_type = buffer_binding_enum(target);
    if (binding_type != -1) {
        GLint buffer_id;
        lgl->GetIntegerv(binding_type, &buffer_id);
        struct GLArrayBuffer* buffer = context_get_buffer(c, buffer_id);
        memcpy((uint8_t*)buffer->data + offset, data, size);
    }
    LOG("glBufferSubData end\n");
}

void* _bolt_gl_GetProcAddress(const char* name) {
#define PROC_ADDRESS_MAP(FUNC) if (!strcmp(name, "gl"#FUNC)) { return gl.FUNC ? gl##FUNC : NULL; }
    PROC_ADDRESS_MAP(CreateProgram)
    PROC_ADDRESS_MAP(DeleteProgram)
    PROC_ADDRESS_MAP(BindAttribLocation)
    PROC_ADDRESS_MAP(LinkProgram)
    PROC_ADDRESS_MAP(UseProgram)
    PROC_ADDRESS_MAP(TexStorage2D)
    PROC_ADDRESS_MAP(TexStorage2DMultisample)
    PROC_ADDRESS_MAP(VertexAttribPointer)
    PROC_ADDRESS_MAP(GenBuffers)
    PROC_ADDRESS_MAP(BufferData)
    PROC_ADDRESS_MAP(DeleteBuffers)
    PROC_ADDRESS_MAP(BindFramebuffer)
    PROC_ADDRESS_MAP(CompressedTexSubImage2D)
    PROC_ADDRESS_MAP(CopyImageSubData)
    PROC_ADDRESS_MAP(EnableVertexAttribArray)
    PROC_ADDRESS_MAP(DisableVertexAttribArray)
    PROC_ADDRESS_MAP(MapBufferRange)
    PROC_ADDRESS_MAP(UnmapBuffer)
    PROC_ADDRESS_MAP(BufferStorage)
    PROC_ADDRESS_MAP(FlushMappedBufferRange)
    PROC_ADDRESS_MAP(ActiveTexture)
    PROC_ADDRESS_MAP(MultiDrawElements)
    PROC_ADDRESS_MAP(GenVertexArrays)
    PROC_ADDRESS_MAP(DeleteVertexArrays)
    PROC_ADDRESS_MAP(BindVertexArray)
    PROC_ADDRESS_MAP(BlitFramebuffer)
    PROC_ADDRESS_MAP(BlendFuncSeparate)
    PROC_ADDRESS_MAP(BufferSubData)
#undef PROC_ADDRESS_MAP
    return NULL;
}

/* libgl function hooks (called from os-specific main.c) */

void _bolt_gl_onSwapBuffers(uint32_t window_width, uint32_t window_height) {
    gl_width = window_width;
    gl_height = window_height;
    player_model_tex_seen = false;
    if (_bolt_plugin_is_inited()) _bolt_plugin_end_frame(window_width, window_height);
}

void _bolt_gl_onCreateContext(void* context, void* shared_context, const struct GLLibFunctions* libgl, void* (*GetProcAddress)(const char*), bool is_important) {
    if (!shared_context && is_important) {
        lgl = libgl;
        if (egl_init_count == 0) {
            gl_load(GetProcAddress);
        } else {
            egl_main_context = (uintptr_t)context;
            egl_main_context_makecurrent_pending = 1;
        }
        egl_init_count += 1;
    }
    context_create(context, shared_context);
}

void _bolt_gl_onMakeCurrent(void* context) {
    struct GLContext* const current_context = _bolt_context();
    if (current_context) {
        current_context->is_attached = 0;
        if (current_context->deferred_destroy) context_destroy(current_context);
    }
    if (!context) {
        set_context(NULL);
        return;
    }
    for (size_t i = 0; i < CONTEXTS_CAPACITY; i += 1) {
        struct GLContext* const ptr = &contexts[i];
        if (ptr->id == (uintptr_t)context) {
            set_context(ptr);
            break;
        }
    }
    if (egl_main_context_makecurrent_pending && (uintptr_t)context == egl_main_context) {
        egl_main_context_makecurrent_pending = 0;
        gl_init();
        const struct PluginManagedFunctions functions = {
            .flash_window = _bolt_flash_window,
            .window_has_focus = _bolt_window_has_focus,
            .surface_init = glplugin_surface_init,
            .surface_destroy = glplugin_surface_destroy,
            .surface_resize_and_clear = glplugin_surface_resize,
            .draw_region_outline = glplugin_draw_region_outline,
            .read_screen_pixels = glplugin_read_screen_pixels,
            .copy_screen = glplugin_copy_screen,
            .game_view_rect = glplugin_game_view_rect,
            .player_position = glplugin_player_position,
            .vertex_shader_init = glplugin_vertex_shader_init,
            .fragment_shader_init = glplugin_fragment_shader_init,
            .shader_program_init = glplugin_shaderprogram_init,
            .shader_buffer_init = glplugin_shaderbuffer_init,
            .shader_destroy = glplugin_shader_destroy,
            .shader_program_destroy = glplugin_shaderprogram_destroy,
            .shader_buffer_destroy = glplugin_shaderbuffer_destroy,
        };
        _bolt_plugin_init(&functions);
    }
}

void* _bolt_gl_onDestroyContext(void* context) {
    uint8_t do_destroy_main = 0;
    if ((uintptr_t)context != egl_main_context) {
        context_destroy(context);
    } else {
        egl_main_context_destroy_pending = 1;
    }
    if (context_count() == 1 && egl_main_context_destroy_pending) {
        do_destroy_main = 1;
        if (egl_init_count > 1) _bolt_plugin_close();
    }
    return do_destroy_main ? (void*)egl_main_context : NULL;
}

void _bolt_gl_onGenTextures(GLsizei n, GLuint* textures) {
    struct GLContext* c = _bolt_context();
    _bolt_rwlock_lock_write(&c->textures->rwlock);
    for (GLsizei i = 0; i < n; i += 1) {
        struct GLTexture2D* tex = calloc(1, sizeof(struct GLTexture2D));
        tex->id = textures[i];
        tex->internalformat = -1;
        tex->compare_mode = -1;
        tex->icons = NULL;
        tex->is_minimap_tex_big = false;
        tex->is_minimap_tex_small = false;
        tex->is_multisample = false;
        hashmap_set(c->textures->map, &tex);
    }
    _bolt_rwlock_unlock_write(&c->textures->rwlock);
}

void _bolt_gl_onDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices_offset) {
    struct GLContext* c = _bolt_context();
    struct GLAttrBinding* attributes = c->bound_vao->attributes;
    GLint element_binding;
    lgl->GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_binding);
    struct GLArrayBuffer* element_buffer = context_get_buffer(c, element_binding);
    const unsigned short* indices = (unsigned short*)((uint8_t*)element_buffer->data + (uintptr_t)indices_offset);
    if (type == GL_UNSIGNED_SHORT && mode == GL_TRIANGLES && count > 0) {
        if (c->bound_program->is_2d && !c->bound_program->is_minimap) {
            return drawelements_handle_2d(count, indices, c, attributes);
        }
        if (c->bound_program->is_3d) {
            return drawelements_handle_3d(count, indices, c, attributes);
        }
        if (c->bound_program->is_particle) {
            GLint draw_tex = 0;
            if (c->current_draw_framebuffer) {
                gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &draw_tex);
            }
            if (draw_tex == c->target_3d_tex) {
                return drawelements_handle_particles(count, indices, c, attributes);
            }
        }
        if (c->bound_program->is_billboard) {
            return drawelements_handle_billboard(count, indices, c, attributes);
        }
    }
}

/*
some notes on this game's OpenGL rendering pipeline for the 3D view

the pipeline changes based on three things:
- anti-aliasing graphics setting
- depth-of-field graphics setting (sometimes)
- whether the game view fills the entire window
if these conditions change, old textures and framebuffers are destroyed and new ones are created.
the same generally happens when the window is resized.

pipelines for different anti-aliasing settings are as follows:

None: backbuffer <- blit* <- sSceneHDRTex <- (sSourceTex <- full glCopyImageSubData)** <- (individual 3d objects)
FXAA: backbuffer <- blit* <- sSourceTex <- sSceneHDRTex <- (sSourceTex <- full glCopyImageSubData)** <- (individual 3d objects)
MSAA: backbuffer <- blit* <- sSceneHDRTex <- sSourceTex** <- full-blit <- (individual 3d objects)
FXAA+MSAA: backbuffer <- blit* <- sSourceTex <- sSceneHDRTex <- sSourceTex** <- full-blit <- (individual 3d objects)

*blits to the game view area of the screen; omitted if the game view fills the entire window
**only done if "depth of field" is enabled - identifiable by sBlurNearTex and sBlurFarTex being present

sSourceTex and sSceneHDRTex are 6-vertex renders using glDrawArrays, the individual objects are drawn using glDrawElements.

hunt strategy is as follows:
1.  start a hunt (recalculate_sSceneHDRTex=true) if part_framebuffer, sSourceTex or sSceneHDRTex are seen to have changed
2a. find part_framebuffer by looking for any framebuffers that are blitted to the backbuffer
2b. find sSourceTex by looking for an sSourceTex glDrawArrays call to the backbuffer or the part_framebuffer
2c. find sSceneHDRTex by looking for an sSceneHDRTex glDrawArrays call to the backbuffer, part_framebuffer, or sSourceTex
3.  assume sSceneHDRTex is the render target until further notice, and set recalculate_sSceneHDRTex=false
4.  if a same-size texture is full-blitted to sSceneHDRTex, and depth-of-field is not enabled, take that as the render target
5a. if a glDrawArrays happens to sSceneHDRTex with sSourceTex and sBlurFarTex present, enable depth-of-field and take depth_of_field_sSourceTex
5b. if a full-blit or full-glCopyImageSubData occurs targeting depth_of_field_sSourceTex, take that as the render target
*/
void _bolt_gl_onDrawArrays(GLenum mode, GLint first, GLsizei count) {
    struct GLContext* c = _bolt_context();
    GLint target_tex_id, source_tex_unit;
    gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &target_tex_id);
    struct GLTexture2D* target_tex = context_get_texture(c, target_tex_id);

    if (c->current_draw_framebuffer && c->bound_program->is_minimap && target_tex->width == GAME_MINIMAP_BIG_SIZE && target_tex->height == GAME_MINIMAP_BIG_SIZE) {
        GLint ubo_binding, ubo_view_index;
        gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_ViewTransforms, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
        gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, (GLuint)ubo_binding, &ubo_view_index);
        const float* camera_position = (float*)((uint8_t*)(context_get_buffer(c, ubo_view_index)->data) + c->bound_program->offset_uCameraPosition);
        target_tex->is_minimap_tex_big = 1;
        target_tex->minimap_center_x = camera_position[0];
        target_tex->minimap_center_y = camera_position[2];
    } else if (mode == GL_TRIANGLE_STRIP && count == 4) {
        if (c->bound_program->loc_sSceneHDRTex != -1) {
            gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_sSceneHDRTex, &source_tex_unit);
            struct GLTexture2D* source_tex = c->texture_units[source_tex_unit].texture_2d;
            if (c->current_draw_framebuffer == 0 && c->game_view_sSceneHDRTex != source_tex->id) {
                c->game_view_sSceneHDRTex = source_tex->id;
                c->target_3d_tex = c->game_view_sSceneHDRTex;
                c->game_view_x = 0;
                c->game_view_y = 0;
                c->game_view_w = source_tex->width;
                c->game_view_h = source_tex->height;
                c->recalculate_sSceneHDRTex = false;
                c->does_blit_3d_target = false;
                c->depth_of_field_enabled = false;
                printf("new direct sSceneHDRTex %i\n", c->target_3d_tex);
            } else if (c->recalculate_sSceneHDRTex && !c->depth_of_field_enabled && (c->game_view_part_framebuffer == c->current_draw_framebuffer || c->game_view_sSourceTex == target_tex_id)) {
                c->game_view_sSceneHDRTex = source_tex->id;
                c->target_3d_tex = c->game_view_sSceneHDRTex;
                c->game_view_w = source_tex->width;
                c->game_view_h = source_tex->height;
                c->recalculate_sSceneHDRTex = false;
                printf("new sSceneHDRTex %i\n", c->target_3d_tex);
            } else if (target_tex && source_tex->icon.model_count && source_tex->width == GAME_ITEM_BIGICON_SIZE && source_tex->height == GAME_ITEM_BIGICON_SIZE && target_tex->width == GAME_ITEM_BIGICON_SIZE && target_tex->height == GAME_ITEM_BIGICON_SIZE) {
                target_tex->icon = source_tex->icon;
                source_tex->icon.model_count = 0;
            }

            if (c->game_view_sSceneHDRTex == source_tex->id && c->depth_tex > 0 && !c->recalculate_depth_tex) {
                struct GLPluginRenderGameViewUserData userdata;
                userdata.target_fb = c->current_draw_framebuffer;
                userdata.depth_tex = c->depth_tex;
                userdata.width = c->game_view_w;
                userdata.height = c->game_view_h;

                struct RenderGameViewEvent event;
                event.functions.userdata = &userdata;
                event.functions.size = glplugin_gameview_size;
                _bolt_plugin_handle_rendergameview(&event);

                c->recalculate_depth_tex = true;
            }
        } else if (c->bound_program->loc_sSourceTex != -1) {
            gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_sSourceTex, &source_tex_unit);
            struct GLTexture2D* source_tex = c->texture_units[source_tex_unit].texture_2d;
            if (c->current_draw_framebuffer && c->bound_program->loc_sBlurFarTex != -1) {
                if (c->depth_of_field_enabled || c->game_view_sSceneHDRTex != target_tex_id) return;
                c->depth_of_field_sSourceTex = source_tex->id;
                c->depth_of_field_enabled = true;
                printf("new depth-of-field sSourceTex %i...\n", c->depth_of_field_sSourceTex);
            } else if (c->current_draw_framebuffer == 0 && c->game_view_sSourceTex != source_tex->id) {
                c->game_view_sSceneHDRTex = -1;
                c->game_view_sSourceTex = source_tex->id;
                c->does_blit_3d_target = false;
                c->depth_of_field_enabled = false;
                c->game_view_x = 0;
                c->game_view_y = 0;
                c->recalculate_sSceneHDRTex = true;
                printf("new direct sSourceTex %i...\n", c->game_view_sSourceTex);
            } else if (c->recalculate_sSceneHDRTex && c->game_view_part_framebuffer == c->current_draw_framebuffer) {
                c->game_view_sSourceTex = source_tex->id;
                printf("new sSourceTex %i...\n", c->game_view_sSourceTex);
            } else if (target_tex && source_tex->icon.model_count && source_tex->width == GAME_ITEM_BIGICON_SIZE && source_tex->height == GAME_ITEM_BIGICON_SIZE && target_tex->width == GAME_ITEM_BIGICON_SIZE && target_tex->height == GAME_ITEM_BIGICON_SIZE) {
                target_tex->icon = source_tex->icon;
                source_tex->icon.model_count = 0;
            }
        } else if (!player_model_tex_seen && c->bound_program->loc_sTexture != -1 && c->current_draw_framebuffer == 0) {
            gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_sTexture, &source_tex_unit);
            struct GLTexture2D* source_tex = c->texture_units[source_tex_unit].recent;
            if (source_tex && source_tex->width == c->game_view_w && source_tex->height == c->game_view_h) {
                c->player_model_tex = source_tex->id;
                player_model_tex_seen = true;
            }
        }
    }
}

void _bolt_gl_onBindTexture(GLenum target, GLuint texture) {
    struct GLContext* c = _bolt_context();
    struct TextureUnit* unit = &c->texture_units[c->active_texture];
    unit->recent = context_get_texture(c, texture);
    switch (target) {
        case GL_TEXTURE_2D:
            unit->texture_2d = unit->recent;
            break;
        case GL_TEXTURE_2D_MULTISAMPLE:
            unit->texture_2d_multisample = unit->recent;
            break;
    }
    unit->target = target;
}

void _bolt_gl_onTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    struct GLContext* c = _bolt_context();
    if (target == GL_TEXTURE_2D && level == 0 && format == GL_RGBA) {
        struct GLTexture2D* tex = c->texture_units[c->active_texture].texture_2d;
        if (tex && !(xoffset < 0 || yoffset < 0 || xoffset + width > tex->width || yoffset + height > tex->height)) {
            for (GLsizei y = 0; y < height; y += 1) {
                uint8_t* dest_ptr = tex->data + ((tex->width * (y + yoffset)) + xoffset) * 4;
                const uint8_t* src_ptr = (uint8_t*)pixels + (width * y * 4);
                memcpy(dest_ptr, src_ptr, width * 4);
            }
        }
    }
}

void _bolt_gl_onDeleteTextures(GLsizei n, const GLuint* textures) {
    struct GLContext* c = _bolt_context();
    _bolt_rwlock_lock_write(&c->textures->rwlock);
    for (GLsizei i = 0; i < n; i += 1) {
        const GLuint* ptr = &textures[i];
        struct GLTexture2D* const* texture = hashmap_delete(c->textures->map, &ptr);
        free((*texture)->data);
        if ((*texture)->icons) {
            size_t iter = 0;
            void* item;
            while (hashmap_iter((*texture)->icons, &iter, &item)) {
                const struct Icon* icon = (struct Icon*)item;
                for (size_t i = 0; i < icon->model_count; i += 1) {
                    free(icon->models[i].vertices);
                }
            }
            hashmap_free((*texture)->icons);
        }
        for (size_t i = 0; i < (*texture)->icon.model_count; i += 1) {
            free((*texture)->icon.models[i].vertices);
        }
        free(*texture);
    }
    _bolt_rwlock_unlock_write(&c->textures->rwlock);
}

void _bolt_gl_onClear(GLbitfield mask) {
    struct GLContext* c = _bolt_context();
    if ((mask & GL_COLOR_BUFFER_BIT) && c->current_draw_framebuffer != 0) {
        GLint draw_tex;
        gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &draw_tex);
        struct GLTexture2D* tex = context_get_texture(c, draw_tex);
        if (tex) {
            tex->is_minimap_tex_big = 0;
            for (size_t i = 0; i < tex->icon.model_count; i += 1) {
                free(tex->icon.models[i].vertices);
            }
            tex->icon.model_count = 0;
        }
    }
}

void _bolt_gl_onViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    struct GLContext* c = _bolt_context();
    c->viewport_x = x;
    c->viewport_y = y;
    c->viewport_w = width;
    c->viewport_h = height;
}

void _bolt_gl_onTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (target == GL_TEXTURE_2D && pname == GL_TEXTURE_COMPARE_MODE) {
        struct GLContext* c = _bolt_context();
        struct GLTexture2D* tex = c->texture_units[c->active_texture].texture_2d;
        tex->compare_mode = param;
    }
}

void _bolt_gl_onBlendFunc(GLenum sfactor, GLenum dfactor) {
    struct GLContext* c = _bolt_context();
    c->blend_rgb_s = sfactor;
    c->blend_rgb_d = dfactor;
    c->blend_alpha_s = sfactor;
    c->blend_alpha_d = dfactor;
}

/* DrawElements sub-functions */

static void drawelements_update_depth_tex(struct GLContext* c) {
    if (c->recalculate_depth_tex) {
        GLint depth_tex = 0;
        gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depth_tex);
        if (depth_tex) {
            c->depth_tex = depth_tex;
            c->recalculate_depth_tex = false;
        }
    }
}

static void drawelements_handle_2d(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes) {
    GLint diffuse_map, ubo_gui_index;
    gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uDiffuseMap, &diffuse_map);
    GLint ubo_binding;
    gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_GUIConsts, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
    gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_gui_index);
    const uint8_t* gui_consts_buf = (uint8_t*)context_get_buffer(c, ubo_gui_index)->data;
    const GLfloat* projection_matrix = (float*)(gui_consts_buf + c->bound_program->offset_uProjectionMatrix);
    GLint draw_tex = 0;
    if (c->current_draw_framebuffer) {
        gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &draw_tex);
    }
    struct GLTexture2D* tex = c->texture_units[diffuse_map].texture_2d;
    struct GLTexture2D* tex_target = context_get_texture(c, draw_tex);
    const uint8_t is_minimap2d_target = tex_target && tex_target->is_minimap_tex_small;

    if (tex->is_minimap_tex_small && count == 6) {
        drawelements_handle_2d_renderminimap(tex, projection_matrix, c, attributes);
    } else if (tex->is_minimap_tex_big) {
        tex_target->is_minimap_tex_small = 1;
        if (count == 6) {
            drawelements_handle_2d_minimapterrain(indices, tex, projection_matrix, c, attributes);
        }
    } else if (tex->icon.model_count && tex->icon.is_big_icon && count == 6) {
        drawelements_handle_2d_bigicon(indices, tex, projection_matrix, c, attributes);
    } else {
        void (*handler_2d)(const struct RenderBatch2D*) = is_minimap2d_target ? _bolt_plugin_handle_minimaprender2d : _bolt_plugin_handle_render2d;
        void (*handler_icon)(const struct RenderIconEvent*) = _bolt_plugin_handle_rendericon;
        drawelements_handle_2d_normal(count, indices, tex, projection_matrix, c, attributes, handler_2d, handler_icon);
    }
}

static void drawelements_handle_3d(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes) {
    GLint draw_tex = 0;
    if (c->current_draw_framebuffer) {
        gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &draw_tex);
    }

    if (c->current_draw_framebuffer && draw_tex == c->player_model_tex) {
        drawelements_handle_3d_silhouette(c);
    } else if (draw_tex == c->target_3d_tex) {
        drawelements_handle_3d_normal(count, indices, c, attributes);
    } else {
        drawelements_handle_3d_iconrender(count, indices, c, attributes, draw_tex);
    }
}

static void drawelements_handle_particles(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes) {
    drawelements_update_depth_tex(c);
    GLint atlas, settings_atlas, seconds, ubo_binding, ubo_view_index, ubo_particle_index;
    gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uTextureAtlas, &atlas);
    gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uTextureAtlasSettings, &settings_atlas);
    gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uSecondsSinceStart, &seconds);
    struct GLTexture2D* tex = c->texture_units[atlas].texture_2d;
    struct GLTexture2D* tex_settings = c->texture_units[settings_atlas].texture_2d;
    gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_ViewTransforms, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
    gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_view_index);
    const uint8_t* ubo_view_buf = (uint8_t*)context_get_buffer(c, ubo_view_index)->data;
    gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_ParticleConsts, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
    gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_particle_index);
    const uint8_t* particle_buf_data = (uint8_t*)context_get_buffer(c, ubo_particle_index)->data;
    const GLfloat* transforms = (GLfloat*)(particle_buf_data + c->bound_program->offset_uParticleEmitterTransforms);
    const uint32_t* ranges = (uint32_t*)(particle_buf_data + c->bound_program->offset_uParticleEmitterTransformRanges);
    const GLfloat atlas_scale = *((GLfloat*)(particle_buf_data + c->bound_program->offset_uAtlasMeta) + 1);

    struct GLPluginDrawElementsVertexParticlesUserData vertex_userdata;
    vertex_userdata.c = c;
    vertex_userdata.indices = indices;
    vertex_userdata.atlas_scale = roundf(atlas_scale);
    vertex_userdata.atlas = tex;
    vertex_userdata.settings_atlas = tex_settings;
    vertex_userdata.origin = &attributes[c->bound_program->loc_aParticleOrigin_CreationTime];
    vertex_userdata.offset = &attributes[c->bound_program->loc_aParticleOffset];
    vertex_userdata.velocity = &attributes[c->bound_program->loc_aParticleVelocityAndUVScrollOffset];
    vertex_userdata.up_axis = &attributes[c->bound_program->loc_aParticleAlignmentUpAxis];
    vertex_userdata.xy_rotation = &attributes[c->bound_program->loc_aMaterialSettingsSlotXY_Rotation];
    vertex_userdata.colour = &attributes[c->bound_program->loc_aVertexColour];
    vertex_userdata.uv_animation_data = &attributes[c->bound_program->loc_aParticleUVAnimationData];
    vertex_userdata.camera_position = (float*)(ubo_view_buf + c->bound_program->offset_uCameraPosition);
    vertex_userdata.animation_time = seconds;
    for (size_t i = 0; i < 8; i += 1) {
        // the last range isn't actually used for anything, so no point copying it or having space for it
        if (i < 7) {
            vertex_userdata.ranges[i * 2] = ranges[i * 4];
            vertex_userdata.ranges[i * 2 + 1] = ranges[i * 4 + 1];
        }
        vertex_userdata.transform[i] = &transforms[i * (4 * 4)];
    }

    struct GLPluginDrawElementsMatrixParticlesUserData matrix_userdata;
    matrix_userdata.view_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uViewMatrix);
    matrix_userdata.projection_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uProjectionMatrix);
    matrix_userdata.viewproj_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uViewProjMatrix);
    matrix_userdata.inv_view_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uInvViewMatrix);

    struct GLPluginTextureUserData tex_userdata;
    tex_userdata.tex = tex;

    struct RenderParticles render;
    render.vertex_count = count;
    render.vertex_functions.userdata = &vertex_userdata;
    render.vertex_functions.xyz = glplugin_drawelements_vertexparticles_xyz;
    render.vertex_functions.world_offset = glplugin_drawelements_vertexparticles_world_offset;
    render.vertex_functions.eye_offset = glplugin_drawelements_vertexparticles_eye_offset;
    render.vertex_functions.uv = glplugin_drawelements_vertexparticles_uv;
    render.vertex_functions.atlas_meta = glplugin_drawelements_vertexparticles_atlas_meta;
    render.vertex_functions.atlas_xywh = glplugin_drawelements_vertexparticles_meta_xywh;
    render.vertex_functions.colour = glplugin_drawelements_vertexparticles_colour;
    render.matrix_functions.userdata = &matrix_userdata;
    render.matrix_functions.model_matrix = NULL;
    render.matrix_functions.view_matrix = glplugin_matrixparticles_viewmatrix;
    render.matrix_functions.proj_matrix = glplugin_matrixparticles_projectionmatrix;
    render.matrix_functions.viewproj_matrix = glplugin_matrixparticles_viewprojmatrix;
    render.matrix_functions.inverse_view_matrix = glplugin_matrixparticles_inv_viewmatrix;
    render.texture_functions.userdata = &tex_userdata;
    render.texture_functions.id = glplugin_texture_id;
    render.texture_functions.size = glplugin_texture_size;
    render.texture_functions.compare = glplugin_texture_compare;
    render.texture_functions.data = glplugin_texture_data;

    _bolt_plugin_handle_renderparticles(&render);
}

static void drawelements_handle_billboard(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes) {
    GLint draw_tex = 0;
    if (!c->current_draw_framebuffer) return;
    gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &draw_tex);
    if (draw_tex != c->target_3d_tex) return;

    GLint atlas, settings_atlas, ubo_binding, ubo_view_index, ubo_billboard_index;
    gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uTextureAtlas, &atlas);
    gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uTextureAtlasSettings, &settings_atlas);
    struct GLTexture2D* tex = c->texture_units[atlas].texture_2d;
    struct GLTexture2D* tex_settings = c->texture_units[settings_atlas].texture_2d;
    gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_ViewTransforms, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
    gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_view_index);
    const uint8_t* ubo_view_buf = (uint8_t*)context_get_buffer(c, ubo_view_index)->data;
    gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_BilloardConsts, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
    gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_billboard_index);
    const uint8_t* billboard_buf_data = (uint8_t*)context_get_buffer(c, ubo_billboard_index)->data;
    const GLfloat atlas_scale = *((GLfloat*)(billboard_buf_data + c->bound_program->offset_uAtlasMeta) + 1);

    struct GLPluginDrawElementsVertexBillboardUserData vertex_userdata;
    vertex_userdata.c = c;
    vertex_userdata.indices = indices;
    vertex_userdata.atlas_scale = roundf(atlas_scale);
    vertex_userdata.atlas = tex;
    vertex_userdata.settings_atlas = tex_settings;
    vertex_userdata.vertex_position = &attributes[c->bound_program->loc_aVertexPositionDepthOffset];
    vertex_userdata.billboard_size = &attributes[c->bound_program->loc_aBillboardSize];
    vertex_userdata.vertex_colour = &attributes[c->bound_program->loc_aVertexColour];
    vertex_userdata.material_xy_uv = &attributes[c->bound_program->loc_aMaterialSettingsSlotXY_UV];

    struct GLPluginDrawElementsMatrixBillboardUserData matrix_userdata;
    matrix_userdata.model_matrix = (GLfloat*)(billboard_buf_data + c->bound_program->offset_uModelMatrix);
    matrix_userdata.view_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uViewMatrix);
    matrix_userdata.projection_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uProjectionMatrix);
    matrix_userdata.viewproj_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uViewProjMatrix);
    matrix_userdata.inv_view_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uInvViewMatrix);

    struct GLPluginTextureUserData tex_userdata;
    tex_userdata.tex = tex;

    struct RenderBillboard render;
    render.vertex_count = count;
    render.vertices_per_icon = 6;
    render.vertex_functions.userdata = &vertex_userdata;
    render.vertex_functions.xyz = glplugin_drawelements_vertexbillboard_xyz;
    render.vertex_functions.eye_offset = glplugin_drawelements_vertexbillboard_eye_offset;
    render.vertex_functions.uv = glplugin_drawelements_vertexbillboard_uv;
    render.vertex_functions.atlas_meta = glplugin_drawelements_vertexbillboard_atlas_meta;
    render.vertex_functions.atlas_xywh = glplugin_drawelements_vertexbillboard_meta_xywh;
    render.vertex_functions.colour = glplugin_drawelements_vertexbillboard_colour;
    render.matrix_functions.userdata = &matrix_userdata;
    render.matrix_functions.model_matrix = glplugin_matrixbillboard_modelmatrix;
    render.matrix_functions.view_matrix = glplugin_matrixbillboard_viewmatrix;
    render.matrix_functions.proj_matrix = glplugin_matrixbillboard_projectionmatrix;
    render.matrix_functions.viewproj_matrix = glplugin_matrixbillboard_viewprojmatrix;
    render.matrix_functions.inverse_view_matrix = glplugin_matrixbillboard_inv_viewmatrix;
    render.texture_functions.userdata = &tex_userdata;
    render.texture_functions.id = glplugin_texture_id;
    render.texture_functions.size = glplugin_texture_size;
    render.texture_functions.compare = glplugin_texture_compare;
    render.texture_functions.data = glplugin_texture_data;

    _bolt_plugin_handle_renderbillboard(&render);
}

static void drawelements_handle_2d_renderminimap(const struct GLTexture2D* tex, const GLfloat* projection_matrix, struct GLContext* c, const struct GLAttrBinding* attributes) {
    const struct GLAttrBinding* position = &attributes[c->bound_program->loc_aVertexPosition2D];
    const struct GLAttrBinding* uv = &attributes[c->bound_program->loc_aTextureUV];
    int32_t xy0[2];
    int32_t xy2[2];
    float uv0[2];
    float uv2[2];
    if (!attr_get_binding_int(c, position, 0, 2, xy0)) return;
    if (!attr_get_binding_int(c, position, 2, 2, xy2)) return;
    attr_get_binding(c, uv, 0, 2, uv0);
    attr_get_binding(c, uv, 2, 2, uv2);
    const int16_t x1 = (int16_t)roundf(uv0[0] * tex->width);
    const int16_t x2 = (int16_t)roundf(uv2[0] * tex->width);
    const int16_t y1 = (int16_t)roundf(uv2[1] * tex->height);
    const int16_t y2 = (int16_t)roundf(uv0[1] * tex->height);
    const float screen_height_float = roundf(2.0 / projection_matrix[5]);
    const struct RenderMinimapEvent event = {
        .screen_width = roundf(2.0 / projection_matrix[0]),
        .screen_height = screen_height_float,
        .source_x = x1,
        .source_y = y1,
        .source_w = x2 - x1,
        .source_h = y2 - y1,
        .target_x = (int16_t)xy0[0],
        .target_y = (int16_t)(screen_height_float - xy0[1]),
        .target_w = (uint16_t)(xy2[0] - xy0[0]),
        .target_h = (uint16_t)(xy0[1] - xy2[1]),
    };
    _bolt_plugin_handle_renderminimap(&event);
}

// assumes count is 6
static void drawelements_handle_2d_minimapterrain(const unsigned short* indices, const struct GLTexture2D* tex, const GLfloat* projection_matrix, struct GLContext* c, const struct GLAttrBinding* attributes) {
    // get XY and UV of first two vertices
    const struct GLAttrBinding* tex_uv = &attributes[c->bound_program->loc_aTextureUV];
    const struct GLAttrBinding* position_2d = &attributes[c->bound_program->loc_aVertexPosition2D];
    int32_t pos0[2];
    int32_t pos1[2];
    float uv0[2];
    float uv1[2];
    attr_get_binding_int(c, position_2d, indices[0], 2, pos0);
    attr_get_binding_int(c, position_2d, indices[1], 2, pos1);
    attr_get_binding(c, tex_uv, indices[0], 2, uv0);
    attr_get_binding(c, tex_uv, indices[1], 2, uv1);
    const double x0 = (double)pos0[0];
    const double y0 = (double)pos0[1];
    const double x1 = (double)pos1[0];
    const double y1 = (double)pos1[1];
    const double u0 = (double)uv0[0];
    const double v0 = (double)uv0[1];
    const double u1 = (double)uv1[0];
    const double v1 = (double)uv1[1];

    // find out angle of the map, by comparing atan2 of XYs to atan2 of UVs
    double pos_angle_rads = atan2(y1 - y0, x1 - x0);
    double uv_angle_rads = atan2(v1 - v0, u1 - u0);
    if (pos_angle_rads < uv_angle_rads) pos_angle_rads += M_PI * 2.0;
    const double map_angle_rads = pos_angle_rads - uv_angle_rads;

    // find out scaling of the map, by comparing distance between XYs to distance between UVs
    const double pos_a = x0 - x1;
    const double pos_b = y0 - y1;
    const double uv_a = u0 - u1;
    const double uv_b = v0 - v1;
    const double pos_dist = sqrt(fabs((pos_a * pos_a) + (pos_b * pos_b)));
    const double uv_dist = sqrt(fabs((uv_a * uv_a) + (uv_b * uv_b))) * tex->width;
    // sometimes it renders the same UV coordinate for the first two vertices, we can't
    // work with that. seems to happen only once immediately after logging into a world.
    if (uv_dist >= 1.0) {
        const double dist_ratio = pos_dist / uv_dist;

        // by unrotating the big tex's vertices around any point, and rotating small-tex-relative
        // points by the same method, we can estimate what section of the big tex will be drawn.
        // note: `>> 1` is the same as `/ 2` except that it doesn't cause an erroneous gcc warning
        const double scaled_x = tex->width * dist_ratio;
        const double scaled_y = tex->height * dist_ratio;
        const double angle_sin = sin(-map_angle_rads);
        const double angle_cos = cos(-map_angle_rads);
        const double unrotated_x0 = (x0 * angle_cos) - (y0 * angle_sin);
        const double unrotated_y0 = (x0 * angle_sin) + (y0 * angle_cos);
        const double scaled_xoffset = (u0 * scaled_x) - unrotated_x0;
        const double scaled_yoffset = (v0 * scaled_y) - unrotated_y0;
        const double small_tex_cx = 1.0 / projection_matrix[0];
        const double small_tex_cy = 1.0 / projection_matrix[5];
        const double cx = (scaled_xoffset + (small_tex_cx * angle_cos) - (small_tex_cy * angle_sin)) / dist_ratio;
        const double cy = (scaled_yoffset + (small_tex_cx * angle_sin) + (small_tex_cy * angle_cos)) / dist_ratio;

        struct MinimapTerrainEvent render;
        render.angle = map_angle_rads;
        render.scale = dist_ratio;
        render.x = tex->minimap_center_x + (64.0 * (cx - (double)(tex->width >> 1)));
        render.y = tex->minimap_center_y + (64.0 * (cy - (double)(tex->height >> 1)));
        _bolt_plugin_handle_minimapterrain(&render);
    }
}

// assumes count is 6
static void drawelements_handle_2d_bigicon(const unsigned short* indices, struct GLTexture2D* tex, const GLfloat* projection_matrix, struct GLContext* c, const struct GLAttrBinding* attributes) {
    int32_t xy0[2];
    int32_t xy2[2];
    float abgr[4];
    const struct GLAttrBinding* binding = &attributes[c->bound_program->loc_aVertexPosition2D];
    attr_get_binding_int(c, binding, indices[0], 2, xy0);
    attr_get_binding_int(c, binding, indices[2], 2, xy2);
    struct RenderIconEvent event;
    event.icon = &tex->icon;
    event.screen_width = roundf(2.0 / projection_matrix[0]);
    event.screen_height = roundf(2.0 / projection_matrix[5]);
    event.target_x = (int16_t)xy2[0];
    event.target_y = (int16_t)((int32_t)event.screen_height - xy2[1]);
    event.target_w = (uint16_t)(xy0[0] - xy2[0]);
    event.target_h = (uint16_t)(xy2[1] - xy0[1]);
    attr_get_binding(c, &attributes[c->bound_program->loc_aVertexColour], indices[0], 4, abgr);
    for (size_t i = 0; i < 4; i += 1) {
        event.rgba[i] = abgr[3 - i];
    }
    _bolt_plugin_handle_renderbigicon(&event);
    for (size_t i = 0; i < tex->icon.model_count; i += 1) {
        free(tex->icon.models[i].vertices);
    }
    tex->icon.model_count = 0;
}

// takes a plugin-level handler for render2d and a handler for rendericon, and may call each of them one or more times
static void drawelements_handle_2d_normal(GLsizei count, const unsigned short* indices, struct GLTexture2D* tex, const GLfloat* projection_matrix, struct GLContext* c, const struct GLAttrBinding* attributes, void (*handler_2d)(const struct RenderBatch2D*), void (*handler_icon)(const struct RenderIconEvent*)) {
    struct GLPluginDrawElementsVertex2DUserData vertex_userdata;
    vertex_userdata.c = c;
    vertex_userdata.indices = indices;
    vertex_userdata.atlas = tex;
    vertex_userdata.position = &attributes[c->bound_program->loc_aVertexPosition2D];
    vertex_userdata.atlas_min = &attributes[c->bound_program->loc_aTextureUVAtlasMin];
    vertex_userdata.atlas_size = &attributes[c->bound_program->loc_aTextureUVAtlasExtents];
    vertex_userdata.tex_uv = &attributes[c->bound_program->loc_aTextureUV];
    vertex_userdata.colour = &attributes[c->bound_program->loc_aVertexColour];
    vertex_userdata.screen_height = roundf(2.0 / projection_matrix[5]);

    struct GLPluginTextureUserData tex_userdata;
    tex_userdata.tex = tex;

    struct RenderBatch2D batch;
    batch.screen_width = roundf(2.0 / projection_matrix[0]);
    batch.screen_height = vertex_userdata.screen_height;
    batch.index_count = count;
    batch.vertices_per_icon = 6;
    batch.vertex_functions.userdata = &vertex_userdata;
    batch.vertex_functions.xy = glplugin_drawelements_vertex2d_xy;
    batch.vertex_functions.atlas_details = glplugin_drawelements_vertex2d_atlas_details;
    batch.vertex_functions.uv = glplugin_drawelements_vertex2d_uv;
    batch.vertex_functions.colour = glplugin_drawelements_vertex2d_colour;
    batch.texture_functions.userdata = &tex_userdata;
    batch.texture_functions.id = glplugin_texture_id;
    batch.texture_functions.size = glplugin_texture_size;
    batch.texture_functions.compare = glplugin_texture_compare;
    batch.texture_functions.data = glplugin_texture_data;

    if (tex->icons) {
        size_t batch_start = 0;
        for (size_t i = 0; i < count; i += batch.vertices_per_icon) {
            float xy[2];
            float wh[2];
            attr_get_binding(c, vertex_userdata.atlas_min, indices[i], 2, xy);
            attr_get_binding(c, vertex_userdata.atlas_size, indices[i], 2, wh);
            const struct Icon dummy_icon = {
                .x = (uint16_t)roundf(xy[0] * tex->width),
                .y = (uint16_t)roundf(xy[1] * tex->height),
                .w = (uint16_t)roundf(fabs(wh[0]) * tex->width),
                .h = (uint16_t)roundf(fabs(wh[1]) * tex->height),
            };
            const struct Icon* icon = hashmap_get(tex->icons, &dummy_icon);
            if (icon) {
                batch.index_count = i - batch_start;
                if (batch.index_count) {
                    vertex_userdata.indices = indices + batch_start;
                    handler_2d(&batch);
                }
                int32_t xy0[2];
                int32_t xy2[2];
                float abgr[4];
                attr_get_binding_int(c, vertex_userdata.position, indices[i], 2, xy0);
                attr_get_binding_int(c, vertex_userdata.position, indices[i + 2], 2, xy2);
                struct RenderIconEvent event;
                event.icon = icon;
                event.screen_width = batch.screen_width;
                event.screen_height = batch.screen_height;
                event.target_x = (int16_t)xy2[0];
                event.target_y = (int16_t)((int32_t)batch.screen_height - xy2[1]);
                event.target_w = (uint16_t)(xy0[0] - xy2[0]);
                event.target_h = (uint16_t)(xy2[1] - xy0[1]);
                attr_get_binding(c, vertex_userdata.colour, indices[i], 4, abgr);
                for (size_t i = 0; i < 4; i += 1) {
                    event.rgba[i] = abgr[3 - i];
                }
                handler_icon(&event);
                batch_start = i + batch.vertices_per_icon;
            }
        }
        if (batch_start < count) {
            batch.index_count = count - batch_start;
            vertex_userdata.indices = indices + batch_start;
            handler_2d(&batch);
        }
    } else {
        handler_2d(&batch);
    }
}

static void drawelements_handle_3d_silhouette(struct GLContext* c) {
    GLint ubo_binding, ubo_index;
    gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_ModelConsts, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
    gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_index);
    const GLfloat* model_matrix = (GLfloat*)(((uint8_t*)context_get_buffer(c, ubo_index)->data) + c->bound_program->offset_uModelMatrix);
    c->player_model_x = (int32_t)roundf(model_matrix[12]);
    c->player_model_y = (int32_t)roundf(model_matrix[13]);
    c->player_model_z = (int32_t)roundf(model_matrix[14]);
}

static void drawelements_handle_3d_normal(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes) {
    drawelements_update_depth_tex(c);
    GLint atlas, settings_atlas, ubo_binding, ubo_view_index, ubo_batch_index, ubo_model_index;
    gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uTextureAtlas, &atlas);
    gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uTextureAtlasSettings, &settings_atlas);
    struct GLTexture2D* tex = c->texture_units[atlas].texture_2d;
    struct GLTexture2D* tex_settings = c->texture_units[settings_atlas].texture_2d;
    gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_ViewTransforms, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
    gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_view_index);
    gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_BatchConsts, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
    gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_batch_index);
    gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_ModelConsts, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
    gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_model_index);
    const GLfloat atlas_scale = *((GLfloat*)(((uint8_t*)context_get_buffer(c, ubo_batch_index)->data) + c->bound_program->offset_uAtlasMeta) + 1);

    struct GLPluginDrawElementsVertex3DUserData vertex_userdata;
    vertex_userdata.c = c;
    vertex_userdata.indices = indices;
    vertex_userdata.atlas_scale = roundf(atlas_scale);
    vertex_userdata.atlas = tex;
    vertex_userdata.settings_atlas = tex_settings;
    vertex_userdata.xy_xz = &attributes[c->bound_program->loc_aMaterialSettingsSlotXY_TilePositionXZ];
    vertex_userdata.xyz_bone = &attributes[c->bound_program->loc_aVertexPosition_BoneLabel];
    vertex_userdata.tex_uv = &attributes[c->bound_program->loc_aTextureUV];
    vertex_userdata.colour = &attributes[c->bound_program->loc_aVertexColour];
    vertex_userdata.skin_bones = &attributes[c->bound_program->loc_aVertexSkinBones];
    vertex_userdata.skin_weights = &attributes[c->bound_program->loc_aVertexSkinWeights];
    vertex_userdata.transforms_ubo = NULL;

    struct GLPluginTextureUserData tex_userdata;
    tex_userdata.tex = tex;

    struct GLPlugin3DMatrixUserData matrix_userdata;
    const uint8_t* ubo_view_buf = (uint8_t*)(context_get_buffer(c, ubo_view_index)->data);
    const uint8_t* ubo_model_buf = (uint8_t*)(context_get_buffer(c, ubo_model_index)->data);
    matrix_userdata.model_matrix = (GLfloat*)(ubo_model_buf + c->bound_program->offset_uModelMatrix);
    matrix_userdata.view_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uViewMatrix);
    matrix_userdata.projection_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uProjectionMatrix);
    matrix_userdata.viewproj_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uViewProjMatrix);
    matrix_userdata.inv_view_matrix = (GLfloat*)(ubo_view_buf + c->bound_program->offset_uInvViewMatrix);

    struct Render3D render;
    render.vertex_count = count;
    render.is_animated = c->bound_program->block_index_VertexTransformData != -1;
    render.vertex_functions.userdata = &vertex_userdata;
    render.vertex_functions.xyz = glplugin_drawelements_vertex3d_xyz;
    render.vertex_functions.atlas_meta = glplugin_drawelements_vertex3d_atlas_meta;
    render.vertex_functions.atlas_xywh = glplugin_drawelements_vertex3d_meta_xywh;
    render.vertex_functions.uv = glplugin_drawelements_vertex3d_uv;
    render.vertex_functions.colour = glplugin_drawelements_vertex3d_colour;
    render.vertex_functions.bone_transform = glplugin_drawelements_vertex3d_transform;
    render.texture_functions.userdata = &tex_userdata;
    render.texture_functions.id = glplugin_texture_id;
    render.texture_functions.size = glplugin_texture_size;
    render.texture_functions.compare = glplugin_texture_compare;
    render.texture_functions.data = glplugin_texture_data;
    render.matrix_functions.userdata = &matrix_userdata;
    render.matrix_functions.model_matrix = glplugin_matrix3d_modelmatrix;
    render.matrix_functions.view_matrix = glplugin_matrix3d_viewmatrix;
    render.matrix_functions.proj_matrix = glplugin_matrix3d_projectionmatrix;
    render.matrix_functions.viewproj_matrix = glplugin_matrix3d_viewprojmatrix;
    render.matrix_functions.inverse_view_matrix = glplugin_matrix3d_inv_viewmatrix;

    _bolt_plugin_handle_render3d(&render);
}

static void drawelements_handle_3d_iconrender(GLsizei count, const unsigned short* indices, struct GLContext* c, const struct GLAttrBinding* attributes, GLint draw_tex) {
    struct GLTexture2D* tex = context_get_texture(c, draw_tex);
    const uint8_t is_icon = tex && tex->compare_mode == GL_NONE && tex->internalformat == GL_RGBA8 && tex->width == GAME_ITEM_ICON_SIZE && tex->height == GAME_ITEM_ICON_SIZE;
    const uint8_t is_big_icon = tex && tex->internalformat == GL_RGBA8 && tex->width == GAME_ITEM_BIGICON_SIZE && tex->height == GAME_ITEM_BIGICON_SIZE;
    if ((is_icon || is_big_icon) && tex->icon.model_count < MAX_MODELS_PER_ICON) {
        struct IconModel* model = &tex->icon.models[tex->icon.model_count];
        tex->icon.model_count += 1;
        tex->icon.is_big_icon = is_big_icon;
        GLint ubo_binding, ubo_model_index, ubo_view_index;
        gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_ModelConsts, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
        gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_model_index);
        const GLfloat* model_matrix = (GLfloat*)(((uint8_t*)context_get_buffer(c, ubo_model_index)->data) + c->bound_program->offset_uModelMatrix);
        for (size_t i = 0; i < 16; i += 1) {
            model->model_matrix.matrix[i] = (double)model_matrix[i];
        }
        gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_ViewTransforms, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
        gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_view_index);
        const uint8_t* ubo_view_buf = (uint8_t*)context_get_buffer(c, ubo_view_index)->data;
        const float* viewmatrix = (float*)(ubo_view_buf + c->bound_program->offset_uViewMatrix);
        const float* projmatrix = (float*)(ubo_view_buf + c->bound_program->offset_uProjectionMatrix);
        const float* viewprojmatrix = (float*)(ubo_view_buf + c->bound_program->offset_uViewProjMatrix);
        for (size_t i = 0; i < 16; i += 1) {
            model->view_matrix.matrix[i] = (double)viewmatrix[i];
            model->projection_matrix.matrix[i] = (double)projmatrix[i];
            model->viewproj_matrix.matrix[i] = (double)viewprojmatrix[i];
        }
        model->vertex_count = count;
        model->vertices = malloc(sizeof(*model->vertices) * count);
        for (size_t i = 0; i < count; i += 1) {
            attr_get_binding_int(c, &attributes[c->bound_program->loc_aVertexPosition_BoneLabel], indices[i], 4, model->vertices[i].point.xyzh.ints);
            attr_get_binding(c, &attributes[c->bound_program->loc_aVertexColour], indices[i], 4, model->vertices[i].rgba);
            model->vertices[i].point.integer = true;
        }
    }
}

/* plugin GL function callback helper functions */

// defines a callback for getting a matrix
#define DEFGETMATRIX(TYPE, MAT, STRUCT) \
static void glplugin_##TYPE##_##MAT##matrix(void* userdata, struct Transform3D* out) { \
    const struct STRUCT* data = userdata; \
    for (size_t i = 0; i < 16; i += 1) { \
        out->matrix[i] = (double)data->MAT##_matrix[i]; \
    } \
}

// defines all normal matrix getters (i.e. excluding model)
#define DEFNORMALMATRIXGETTERS(TYPE, STRUCT) \
DEFGETMATRIX(TYPE, view, STRUCT) \
DEFGETMATRIX(TYPE, projection, STRUCT) \
DEFGETMATRIX(TYPE, viewproj, STRUCT) \
DEFGETMATRIX(TYPE, inv_view, STRUCT)

// defines all matrix getters (including model)
#define DEFALLMATRIXGETTERS(TYPE, STRUCT) \
DEFGETMATRIX(TYPE, model, STRUCT) \
DEFNORMALMATRIXGETTERS(TYPE, STRUCT)

static void xywh_from_meta_atlas(const struct GLTexture2D* settings_atlas, size_t slot_x, size_t slot_y, int scale, int32_t* out) {
    // this is pretty wild
    const uint8_t* settings_ptr = settings_atlas->data + (slot_y * settings_atlas->width * 4 * 4) + (slot_x * 3 * 4);
    const uint8_t bitmask = *(settings_ptr + (settings_atlas->width * 2 * 4) + 7);
    out[0] = ((int32_t)(*settings_ptr) + (bitmask & 1 ? 256 : 0)) * scale;
    out[1] = ((int32_t)*(settings_ptr + 1) + (bitmask & 2 ? 256 : 0)) * scale;
    out[2] = (int32_t)*(settings_ptr + 8) * scale;
    out[3] = out[2];
}

// like the glsl function but modifies a vec3 in-place
static void normalise(float* values) {
    const float length = sqrtf((values[0] * values[0]) + (values[1] * values[1]) + (values[2] * values[2]));
    values[0] /= length;
    values[1] /= length;
    values[2] /= length;
}

// like the glsl function, operates on vec3
static void cross(float* x, float* y, float* out) {
    out[0] = (x[1] * y[2]) - (y[1] * x[2]);
    out[1] = (x[2] * y[0]) - (y[2] * x[0]);
    out[2] = (x[0] * y[1]) - (y[0] * x[1]);
}

static const GLfloat* particles_get_transform(unsigned short vertex, const struct GLProgram* program, struct GLPluginDrawElementsVertexParticlesUserData* userdata) {
    size_t i;
    for (i = 0; i < 7; i += 1) {
        if (vertex >= userdata->ranges[i * 2] && vertex < userdata->ranges[i * 2 + 1]) break;
    }
    return userdata->transform[i];
}

// note this function binds GL_DRAW_FRAMEBUFFER and GL_TEXTURE_2D (for the current active texture unit)
// so you'll have to restore the prior values yourself if you need to leave the opengl state unchanged
static void surface_init_buffers(struct PluginSurfaceUserdata* userdata) {
    gl.GenFramebuffers(1, &userdata->framebuffer);
    lgl->GenTextures(1, &userdata->renderbuffer);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, userdata->framebuffer);
    lgl->BindTexture(GL_TEXTURE_2D, userdata->renderbuffer);
    gl.TexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, userdata->width, userdata->height);
    lgl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    lgl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    lgl->TexParameteri(GL_TEXTURE_2D,  GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    lgl->TexParameteri(GL_TEXTURE_2D,  GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.FramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, userdata->renderbuffer, 0);
}

static void surface_destroy_buffers(struct PluginSurfaceUserdata* userdata) {
    gl.DeleteFramebuffers(1, &userdata->framebuffer);
    lgl->DeleteTextures(1, &userdata->renderbuffer);
}

static void surface_draw(const struct GLContext* c, const struct PluginSurfaceUserdata* userdata, struct ProgramDirectData* program, GLuint target_fb, int target_width, int target_height, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    GLboolean depth_test, scissor_test, cull_face;
    lgl->GetBooleanv(GL_DEPTH_TEST, &depth_test);
    lgl->GetBooleanv(GL_SCISSOR_TEST, &scissor_test);
    lgl->GetBooleanv(GL_CULL_FACE, &cull_face);

    gl.UseProgram(program->id);
    lgl->BindTexture(GL_TEXTURE_2D, userdata->renderbuffer);
    gl.BindVertexArray(program_direct_vao);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, target_fb);
    gl.Uniform1i(program->sampler, c->active_texture);
    gl.Uniform4i(program->d_xywh, dx, dy, dw, dh);
    gl.Uniform4i(program->s_xywh, sx, sy, sw, sh);
    gl.Uniform4i(program->src_wh_dest_wh, userdata->width, userdata->height, target_width, target_height);
    gl.Uniform4fv(program->rgba, 1, userdata->rgba);
    lgl->Viewport(0, 0, target_width, target_height);
    lgl->Disable(GL_DEPTH_TEST);
    lgl->Disable(GL_SCISSOR_TEST);
    lgl->Disable(GL_CULL_FACE);
    gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    lgl->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    gl.BlendFuncSeparate(c->blend_rgb_s, c->blend_rgb_d, c->blend_alpha_s, c->blend_alpha_d);
    if (depth_test) lgl->Enable(GL_DEPTH_TEST);
    if (scissor_test) lgl->Enable(GL_SCISSOR_TEST);
    if (cull_face) lgl->Enable(GL_CULL_FACE);
    lgl->Viewport(c->viewport_x, c->viewport_y, c->viewport_w, c->viewport_h);
    const struct TextureUnit* unit = &c->texture_units[c->active_texture];
    lgl->BindTexture(unit->target, unit->recent->id);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, c->current_draw_framebuffer);
    gl.BindVertexArray(c->bound_vao->id);
    gl.UseProgram(c->bound_program ? c->bound_program->id : 0);
}

static void shaderprogram_draw(const struct PluginProgramUserdata* program, const struct PluginShaderBufferUserdata* buffer, uint32_t count, GLuint target_fb, GLsizei width, GLsizei height) {
    const struct GLContext* c = _bolt_context();
    GLint array_binding;
    lgl->GetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_binding);
    GLboolean depth_test, scissor_test, cull_face;
    lgl->GetBooleanv(GL_DEPTH_TEST, &depth_test);
    lgl->GetBooleanv(GL_SCISSOR_TEST, &scissor_test);
    lgl->GetBooleanv(GL_CULL_FACE, &cull_face);

    gl.UseProgram(program->program);
    gl.BindVertexArray(program->vao);
    gl.BindBuffer(GL_ARRAY_BUFFER, buffer->buffer);
    for (uint8_t i = 0; i < MAX_PROGRAM_BINDINGS; i += 1) {
        if (program->bindings_enabled & (1 << i)) {
            const struct PluginProgramAttrBinding* binding = &program->bindings[i];
            gl.EnableVertexAttribArray(i);
            if (binding->is_double) {
                gl.VertexAttribLPointer(i, binding->size, binding->type, binding->stride, (const void*)binding->offset);
            } else {
                gl.VertexAttribPointer(i, binding->size, binding->type, false, binding->stride, (const void*)binding->offset);
            }
        } else {
            gl.DisableVertexAttribArray(i);
        }
    }
    uint32_t active_texture = 0;
    size_t iter = 0;
    void* item;
    while (hashmap_iter(program->uniforms, &iter, &item)) {
        uniform_upload(item, &active_texture);
    }
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, target_fb);
    lgl->Viewport(0, 0, width, height);
    lgl->Disable(GL_DEPTH_TEST);
    lgl->Disable(GL_SCISSOR_TEST);
    lgl->Disable(GL_CULL_FACE);
    lgl->DrawArrays(GL_TRIANGLES, 0, count);

    if (active_texture > 0) {
        for (size_t i = 0; i < active_texture; i += 1) {
            gl.ActiveTexture(GL_TEXTURE0 + i);
            lgl->BindTexture(c->texture_units[i].target, c->texture_units[i].recent->id);
        }
        gl.ActiveTexture(GL_TEXTURE0 + c->active_texture);
    }
    if (depth_test) lgl->Enable(GL_DEPTH_TEST);
    if (scissor_test) lgl->Enable(GL_SCISSOR_TEST);
    if (cull_face) lgl->Enable(GL_CULL_FACE);
    lgl->Viewport(c->viewport_x, c->viewport_y, c->viewport_w, c->viewport_h);
    gl.BindBuffer(GL_ARRAY_BUFFER, array_binding);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, c->current_draw_framebuffer);
    gl.BindVertexArray(c->bound_vao->id);
    gl.UseProgram(c->bound_program ? c->bound_program->id : 0);    
}

/* plugin GL function callbacks */

static void glplugin_drawelements_vertex2d_xy(size_t index, void* userdata, int32_t* out) {
    struct GLPluginDrawElementsVertex2DUserData* data = userdata;
    if (attr_get_binding_int(data->c, data->position, data->indices[index], 2, out)) {
        // this line would seem like it introduces an off-by-one error, but the same error actually exists in the game engine
        // causing the UI to be drawn one pixel higher than the top of the screen, so we're just accounting for that here.
        out[1] = (int32_t)data->screen_height - out[1];
    } else {
        float pos[2];
        attr_get_binding(data->c, data->position, data->indices[index], 2, pos);
        out[0] = (int32_t)roundf(pos[0]);
        // as above
        out[1] = (int32_t)data->screen_height - (int32_t)roundf(pos[1]);
    }
}

static void glplugin_drawelements_vertex2d_atlas_details(size_t index, void* userdata, int32_t* out, uint8_t* wrapx, uint8_t* wrapy) {
    struct GLPluginDrawElementsVertex2DUserData* data = userdata;
    float xy[2];
    float wh[2];
    attr_get_binding(data->c, data->atlas_min, data->indices[index], 2, xy);
    attr_get_binding(data->c, data->atlas_size, data->indices[index], 2, wh);
    out[0] = (int32_t)roundf(xy[0] * data->atlas->width);
    out[1] = (int32_t)roundf(xy[1] * data->atlas->height);
    out[2] = (int32_t)roundf(fabs(wh[0]) * data->atlas->width);
    out[3] = (int32_t)roundf(fabs(wh[1]) * data->atlas->height);
    *wrapx = wh[0] > 0.0;
    *wrapy = wh[1] > 0.0;
}

static void glplugin_drawelements_vertex2d_uv(size_t index, void* userdata, double* out, uint8_t* discard) {
    struct GLPluginDrawElementsVertex2DUserData* data = userdata;
    float uv[2];
    attr_get_binding(data->c, data->tex_uv, data->indices[index], 2, uv);
    out[0] = (double)uv[0];
    out[1] = (double)uv[1];
    *discard = uv[0] < -60000.0; // hard-coded shader value
}

static void glplugin_drawelements_vertex2d_colour(size_t index, void* userdata, double* out) {
    struct GLPluginDrawElementsVertex2DUserData* data = userdata;
    float colour[4];
    attr_get_binding(data->c, data->colour, data->indices[index], 4, colour);
    // these are ABGR for some reason
    out[0] = (double)colour[3];
    out[1] = (double)colour[2];
    out[2] = (double)colour[1];
    out[3] = (double)colour[0];
}

static void glplugin_drawelements_vertex3d_xyz(size_t index, void* userdata, struct Point3D* out) {
    struct GLPluginDrawElementsVertex3DUserData* data = userdata;
    out->integer = true;
    if (!attr_get_binding_int(data->c, data->xyz_bone, data->indices[index], 3, out->xyzh.ints)) {
        float pos[3];
        attr_get_binding(data->c, data->xyz_bone, data->indices[index], 3, pos);
        out->xyzh.floats[0] = (double)pos[0];
        out->xyzh.floats[1] = (double)pos[1];
        out->xyzh.floats[2] = (double)pos[2];
        out->xyzh.floats[3] = 1.0;
        out->integer = false;
    }
}

static size_t glplugin_drawelements_vertex3d_atlas_meta(size_t index, void* userdata) {
    struct GLPluginDrawElementsVertex3DUserData* data = userdata;
    int material_xy[2];
    attr_get_binding_int(data->c, data->xy_xz, data->indices[index], 2, material_xy);
    return ((size_t)material_xy[1] << 16) | (size_t)material_xy[0];
}

static void glplugin_drawelements_vertex3d_meta_xywh(size_t meta, void* userdata, int32_t* out) {
    struct GLPluginDrawElementsVertex3DUserData* data = userdata;
    const size_t slot_x = meta & 0xFF;
    const size_t slot_y = meta >> 16;
    xywh_from_meta_atlas(data->settings_atlas, slot_x, slot_y, data->atlas_scale, out);
}

static void glplugin_drawelements_vertex3d_uv(size_t index, void* userdata, double* out) {
    struct GLPluginDrawElementsVertex3DUserData* data = userdata;
    float uv[2];
    attr_get_binding(data->c, data->tex_uv, data->indices[index], 2, uv);
    out[0] = (double)uv[0];
    out[1] = (double)uv[1];
}

static void glplugin_drawelements_vertex3d_colour(size_t index, void* userdata, double* out) {
    struct GLPluginDrawElementsVertex3DUserData* data = userdata;
    float colour[4];
    attr_get_binding(data->c, data->colour, data->indices[index], 4, colour);
    out[0] = (double)colour[0];
    out[1] = (double)colour[1];
    out[2] = (double)colour[2];
    out[3] = (double)colour[3];
}

static uint8_t glplugin_drawelements_vertex3d_boneid(size_t index, void* userdata) {
    struct GLPluginDrawElementsVertex3DUserData* data = userdata;
    uint32_t ret[4];
    if (!attr_get_binding_int(data->c, data->xyz_bone, data->indices[index], 4, (int32_t*)&ret)) {
        float retf[4];
        attr_get_binding(data->c, data->xyz_bone, data->indices[index], 4, retf);
        return (uint8_t)(uint32_t)(int32_t)roundf(retf[3]);
    }
    return (uint8_t)(ret[3]);
}

static void glplugin_drawelements_vertex3d_transform(size_t vertex, void* userdata, struct Transform3D* out) {
    struct GLPluginDrawElementsVertex3DUserData* data = userdata;
    uint8_t bone_id;
    int32_t xyzbone[4];
    if (attr_get_binding_int(data->c, data->xyz_bone, data->indices[vertex], 4, xyzbone)) {
        bone_id = (uint8_t)xyzbone[3];
    } else {
        float xyzbone_floats[4];
        attr_get_binding(data->c, data->xyz_bone, data->indices[vertex], 4, xyzbone_floats);
        bone_id = (uint8_t)(uint32_t)(int32_t)roundf(xyzbone_floats[3]);
    }
    if (data->c->bound_program->block_index_VertexTransformData == -1) return;
    bone_index_transform(data->c, &data->transforms_ubo, bone_id, out);
    
    GLfloat smooth_skinning = *(GLfloat*)(((uint8_t*)data->transforms_ubo->data) + data->c->bound_program->offset_uSmoothSkinning);
    if (smooth_skinning < 0.0) return;

    int32_t skin_bones[4];
    if (!attr_get_binding_int(data->c, data->skin_bones, data->indices[vertex], 4, skin_bones)) {
        float skin_bone_floats[4];
        attr_get_binding(data->c, data->skin_bones, data->indices[vertex], 4, skin_bone_floats);
        for (size_t i = 0; i < 4; i += 1) {
            skin_bones[i] = (int32_t)roundf(skin_bone_floats[i]);
        }
    }

    struct Transform3D modifier;
    if (smooth_skinning > 0.0) {
        float skin_weights[4];
        attr_get_binding(data->c, data->skin_weights, data->indices[vertex], 4, skin_weights);
        struct Transform3D weighted_modifiers[4];
        for (size_t i = 0; i < 4; i += 1) {
            bone_index_transform(data->c, &data->transforms_ubo, skin_bones[i], &weighted_modifiers[i]);
        }
        for (size_t i = 0; i < 16; i += 1) {
            modifier.matrix[i] =
                weighted_modifiers[0].matrix[i] * skin_weights[0] +
                weighted_modifiers[1].matrix[i] * skin_weights[1] +
                weighted_modifiers[2].matrix[i] * skin_weights[2] +
                weighted_modifiers[3].matrix[i] * skin_weights[3];
        }
    } else {
        bone_index_transform(data->c, &data->transforms_ubo, skin_bones[0], &modifier);
    }

    struct Transform3D ret;
    multiply_transforms(out, &modifier, &ret);
    *out = ret;
}

static void glplugin_drawelements_vertexparticles_xyz(size_t index, void* userdata, struct Point3D* out) {
    struct GLPluginDrawElementsVertexParticlesUserData* data = userdata;
    const unsigned short vertex = data->indices[index];
    float xyz[3];
    attr_get_binding(data->c, data->origin, vertex, 3, xyz);
    int32_t uv_animation_data[4];
    attr_get_binding_int(data->c, data->uv_animation_data, vertex, 4, uv_animation_data);

    if (uv_animation_data[2] & (1 << 7)) {
        const GLfloat* transform = particles_get_transform(vertex, data->c->bound_program, data);
        out->xyzh.floats[0] = (xyz[0] * transform[0]) + (xyz[1] * transform[4]) + (xyz[2] * transform[8]) + transform[12];
        out->xyzh.floats[1] = (xyz[0] * transform[1]) + (xyz[1] * transform[5]) + (xyz[2] * transform[9]) + transform[13];
        out->xyzh.floats[2] = (xyz[0] * transform[2]) + (xyz[1] * transform[6]) + (xyz[2] * transform[10]) + transform[14];
        out->xyzh.floats[3] = (xyz[0] * transform[3]) + (xyz[1] * transform[7]) + (xyz[2] * transform[11]) + transform[15];
        out->integer = false;
        out->homogenous = true;
        return;
    }

    out->xyzh.floats[0] = (double)xyz[0];
    out->xyzh.floats[1] = (double)xyz[1];
    out->xyzh.floats[2] = (double)xyz[2];
    out->xyzh.floats[3] = 1.0;
    out->integer = false;
    out->homogenous = false;
}

static void glplugin_drawelements_vertexparticles_world_offset(size_t index, void* userdata, double* out) {
    struct GLPluginDrawElementsVertexParticlesUserData* data = userdata;
    const unsigned short vertex = data->indices[index];
    float offset[2];
    float velocity[3];
    float up_axis[3];
    int32_t xy_rotation[4];
    int32_t uv_animation_data[4];
    attr_get_binding_int(data->c, data->uv_animation_data, vertex, 4, uv_animation_data);
    if (!(uv_animation_data[2] & (1 << 1))) {
        // this isn't a world-space type of particle
        out[0] = 0.0;
        out[1] = 0.0;
        out[2] = 0.0;
        return;
    }

    attr_get_binding(data->c, data->offset, vertex, 2, offset);
    attr_get_binding(data->c, data->velocity, vertex, 3, velocity);
    attr_get_binding(data->c, data->up_axis, vertex, 3, up_axis);
    attr_get_binding_int(data->c, data->xy_rotation, vertex, 4, xy_rotation);
    const double angle = (double)((xy_rotation[2] & 0xFF) + ((xy_rotation[3] & 0xFF) << 8)) / 65535.0;
    const double anglesin = sin(angle);
    const double anglecos = cos(angle);
    const double offsetx = (double)offset[0];
    const double offsety = (double)offset[1];
    const double offsetx2d = (offsetx * anglecos) - (offsety * anglesin);
    const double offsety2d = (offsetx * anglesin) + (offsety * anglecos);

    // I don't know how most of this works, it's just copied from a GLSL shader from the engine
    if (uv_animation_data[2] & (1 << 7)) {
        const GLfloat* transform = particles_get_transform(vertex, data->c->bound_program, data);
        const float vx = (velocity[0] * transform[0]) + (velocity[1] * transform[4]) + (velocity[2] * transform[8]);
        const float vy = (velocity[0] * transform[1]) + (velocity[1] * transform[5]) + (velocity[2] * transform[9]);
        const float vz = (velocity[0] * transform[2]) + (velocity[1] * transform[6]) + (velocity[2] * transform[10]);
        velocity[0] = vx;
        velocity[1] = vy;
        velocity[2] = vz;
    }
    if (up_axis[0] == 0.0 && up_axis[1] == 0.0 && up_axis[2] == 0.0) {
        float xyz[3];
        attr_get_binding(data->c, data->origin, vertex, 3, xyz);
        up_axis[0] = data->camera_position[0] - xyz[0];
        up_axis[1] = data->camera_position[1] - xyz[1];
        up_axis[2] = data->camera_position[2] - xyz[2];
        normalise(up_axis);
    }
    if (velocity[0] == 0.0 && velocity[1] == 0.0 && velocity[2] == 0.0) {
        velocity[0] = 0.0;
        velocity[1] = 0.0;
        velocity[2] = 1.0;
    } else {
        normalise(velocity);
    }
    float cross_product[3];
    cross(velocity, up_axis, cross_product);
    normalise(cross_product);
    out[0] = velocity[0] * offsety2d + cross_product[0] * offsetx2d;
    out[1] = velocity[1] * offsety2d + cross_product[1] * offsetx2d;
    out[2] = velocity[2] * offsety2d + cross_product[2] * offsetx2d;
}

static void glplugin_drawelements_vertexparticles_eye_offset(size_t index, void* userdata, double* out) {
    const struct GLPluginDrawElementsVertexParticlesUserData* data = userdata;
    const unsigned short vertex = data->indices[index];
    float offset[2];
    int32_t xy_rotation[4];
    int32_t uv_animation_data[4];
    attr_get_binding_int(data->c, data->uv_animation_data, vertex, 4, uv_animation_data);

    if (uv_animation_data[2] & (1 << 1)) {
        // this isn't an eye-space type of particle
        out[0] = 0.0;
        out[1] = 0.0;
        return;
    }

    attr_get_binding(data->c, data->offset, vertex, 2, offset);
    attr_get_binding_int(data->c, data->xy_rotation, vertex, 4, xy_rotation);
    const double angle = (double)((xy_rotation[2] & 0xFF) + ((xy_rotation[3] & 0xFF) << 8)) / 65535.0;
    const double anglesin = sin(angle);
    const double anglecos = cos(angle);
    const double offsetx = (double)offset[0];
    const double offsety = (double)offset[1];
    out[0] = (offsetx * anglecos) - (offsety * anglesin);
    out[1] = (offsetx * anglesin) + (offsety * anglecos);
}

// I ported this function from a vertex shader through great suffering. Don't try to understand it. Seriously.
static void glplugin_drawelements_vertexparticles_uv(size_t index, void* userdata, double* out) {
    struct GLContext* c = _bolt_context();
    struct GLPluginDrawElementsVertexParticlesUserData* data = userdata;
    const unsigned short vertex = data->indices[index];
    int32_t uv_animation_data[4];
    float offset[2];
    attr_get_binding_int(data->c, data->uv_animation_data, vertex, 4, uv_animation_data);
    attr_get_binding(data->c, data->offset, vertex, 2, offset);
    if (uv_animation_data[0] != 0) {
        const int32_t s = (uv_animation_data[0] & 15) + 1;
        const int32_t o = ((uv_animation_data[0] & 240) >> 4) + 1;
        const double ts = offset[0] <= 0.0 ? (1.0 / (double)s) : 0.0;
        const double tt = offset[1] <= 0.0 ? (1.0 / (double)o) : 0.0;
        if (uv_animation_data[2] & (1 << 2)) {
            float origin[4];
            attr_get_binding(data->c, data->origin, vertex, 4, origin);
            const int32_t A = uv_animation_data[1];
            const float m = data->animation_time - origin[3];
            const float T = m * A;
            const int32_t p = o * s;
            const float S = floorf(T);
            const float e = fmodf(S, p);
            const float V = fmodf(e + 1.0, p);
            const float h = fmodf(e, s);
            const float Y = (e - h) / (float)s;
            const float c = fmodf(V, s);
            const float E = (V - c) / (float)s;
            const float us = 1.0 / (float)s;
            const float ds = 1.0 - us - (h / (float)s);
            const float dt = Y / (float)o;
            const float rs = 1.0 - us - (c / (float)s);
            const float rt = E / (float)o;
            const float interpolation_amount = T - S;
            out[0] = ts + (ds * (1.0 - interpolation_amount)) + (rs * interpolation_amount);
            out[1] = tt + (dt * (1.0 - interpolation_amount)) + (rt * interpolation_amount);
        } else {
            const int32_t p = uv_animation_data[1] % s;
            const double T = (double)(uv_animation_data[1] - p) / (double)s;
            const double rs = 1.0 - (1.0 / (double)s) - ((double)p / (double)s);
            const double rt = T / (double)o;
            out[0] = ts + rs;
            out[1] = tt + rt;
        }
    } else {
        out[0] = offset[0] <= 0.0 ? 1.0 : 0.0;
        out[1] = offset[1] <= 0.0 ? 1.0 : 0.0;
    }
}

static size_t glplugin_drawelements_vertexparticles_atlas_meta(size_t index, void* userdata) {
    const struct GLPluginDrawElementsVertexParticlesUserData* data = userdata;
    int material_xy[2];
    attr_get_binding_int(data->c, data->xy_rotation, data->indices[index], 2, material_xy);
    return ((size_t)material_xy[1] << 16) | (size_t)material_xy[0];
}

static void glplugin_drawelements_vertexparticles_meta_xywh(size_t meta, void* userdata, int32_t* out) {
    const struct GLPluginDrawElementsVertexParticlesUserData* data = userdata;
    const size_t slot_x = meta & 0xFF;
    const size_t slot_y = meta >> 16;
    xywh_from_meta_atlas(data->settings_atlas, slot_x, slot_y, data->atlas_scale, out);
}

static void glplugin_drawelements_vertexparticles_colour(size_t index, void* userdata, double* out) {
    const struct GLPluginDrawElementsVertexParticlesUserData* data = userdata;
    float colour[4];
    attr_get_binding(data->c, data->colour, data->indices[index], 4, colour);
    out[0] = (double)colour[0];
    out[1] = (double)colour[1];
    out[2] = (double)colour[2];
    out[3] = (double)colour[3];
}

DEFNORMALMATRIXGETTERS(matrixparticles, GLPluginDrawElementsMatrixParticlesUserData)
DEFALLMATRIXGETTERS(matrixbillboard, GLPluginDrawElementsMatrixBillboardUserData)
DEFALLMATRIXGETTERS(matrix3d, GLPlugin3DMatrixUserData)

static void glplugin_drawelements_vertexbillboard_xyz(size_t index, void* userdata, struct Point3D* out) {
    struct GLPluginDrawElementsVertexBillboardUserData* data = userdata;
    out->integer = true;
    if (!attr_get_binding_int(data->c, data->vertex_position, data->indices[index], 3, out->xyzh.ints)) {
        float pos[3];
        attr_get_binding(data->c, data->vertex_position, data->indices[index], 3, pos);
        out->xyzh.floats[0] = (double)pos[0];
        out->xyzh.floats[1] = (double)pos[1];
        out->xyzh.floats[2] = (double)pos[2];
        out->xyzh.floats[3] = 1.0;
        out->integer = false;
    }
}

static void glplugin_drawelements_vertexbillboard_eye_offset(size_t index, void* userdata, double* out) {
    struct GLPluginDrawElementsVertexBillboardUserData* data = userdata;
    float xy[2];
    attr_get_binding(data->c, data->billboard_size, data->indices[index], 2, xy);
    out[0] = (double)xy[0];
    out[1] = (double)xy[1];
}

static void glplugin_drawelements_vertexbillboard_uv(size_t index, void* userdata, double* out) {
    struct GLPluginDrawElementsVertexBillboardUserData* data = userdata;
    int xyuv[4];
    attr_get_binding_int(data->c, data->material_xy_uv, data->indices[index], 4, xyuv);
    out[0] = (double)xyuv[2];
    out[1] = (double)xyuv[3];
}

static size_t glplugin_drawelements_vertexbillboard_atlas_meta(size_t index, void* userdata) {
    const struct GLPluginDrawElementsVertexBillboardUserData* data = userdata;
    int material_xy[2];
    attr_get_binding_int(data->c, data->material_xy_uv, data->indices[index], 2, material_xy);
    return ((size_t)material_xy[1] << 16) | (size_t)material_xy[0];
}

static void glplugin_drawelements_vertexbillboard_meta_xywh(size_t meta, void* userdata, int32_t* out) {
    const struct GLPluginDrawElementsVertexBillboardUserData* data = userdata;
    const size_t slot_x = meta & 0xFF;
    const size_t slot_y = meta >> 16;
    xywh_from_meta_atlas(data->settings_atlas, slot_x, slot_y, data->atlas_scale, out);
}

static void glplugin_drawelements_vertexbillboard_colour(size_t index, void* userdata, double* out) {
    struct GLPluginDrawElementsVertexBillboardUserData* data = userdata;
    float rgba[4];
    attr_get_binding(data->c, data->vertex_colour, data->indices[index], 4, rgba);
    out[0] = (double)rgba[0];
    out[1] = (double)rgba[1];
    out[2] = (double)rgba[2];
    out[3] = (double)rgba[3];
}

static size_t glplugin_texture_id(void* userdata) {
    const struct GLPluginTextureUserData* data = userdata;
    return data->tex->id;
}

static void glplugin_texture_size(void* userdata, size_t* out) {
    const struct GLPluginTextureUserData* data = userdata;
    out[0] = data->tex->width;
    out[1] = data->tex->height;
}

static uint8_t glplugin_texture_compare(void* userdata, size_t x, size_t y, size_t len, const unsigned char* data) {
    const struct GLPluginTextureUserData* data_ = userdata;
    const struct GLTexture2D* tex = data_->tex;
    const size_t start_offset = (tex->width * y * 4) + (x * 4);
    if (start_offset + len > tex->width * tex->height * 4) {
        printf(
            "warning: out-of-bounds texture compare attempt: tried to read %zu bytes at %zu,%zu of texture id=%u w,h=%u,%u\n",
            len, x, y, tex->id, tex->width, tex->height
        );
        return 0;
    }
    return !memcmp(tex->data + start_offset, data, len);
}

static uint8_t* glplugin_texture_data(void* userdata, size_t x, size_t y) {
    const struct GLPluginTextureUserData* data = userdata;
    const struct GLTexture2D* tex = data->tex;
    return tex->data + (tex->width * y * 4) + (x * 4);
}

static void glplugin_gameview_size(void* userdata, int* w, int* h) {
    struct GLPluginRenderGameViewUserData* gameview = userdata;
    *w = gameview->width;
    *h = gameview->height;
}

static void glplugin_surface_init(struct SurfaceFunctions* functions, unsigned int width, unsigned int height, const void* data) {
    struct PluginSurfaceUserdata* userdata = malloc(sizeof(struct PluginSurfaceUserdata));
    struct GLContext* c = _bolt_context();
    userdata->width = width;
    userdata->height = height;
    for (size_t i = 0; i < 4; i += 1) {
        userdata->rgba[i] = 1.0;
    }
    surface_init_buffers(userdata);
    if (data) {
        lgl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    } else {
        lgl->ClearColor(0.0, 0.0, 0.0, 0.0);
        lgl->Clear(GL_COLOR_BUFFER_BIT);
    }
    functions->userdata = userdata;
    functions->clear = glplugin_surface_clear;
    functions->subimage = glplugin_surface_subimage;
    functions->draw_to_screen = glplugin_surface_drawtoscreen;
    functions->draw_to_surface = glplugin_surface_drawtosurface;
    functions->draw_to_gameview = glplugin_surface_drawtogameview;
    functions->set_tint = glplugin_surface_set_tint;
    functions->set_alpha = glplugin_surface_set_alpha;

    const struct GLTexture2D* original_tex = c->texture_units[c->active_texture].texture_2d;
    lgl->BindTexture(GL_TEXTURE_2D, original_tex ? original_tex->id : 0);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, c->current_draw_framebuffer);
}

static void glplugin_surface_destroy(void* _userdata) {
    struct PluginSurfaceUserdata* userdata = _userdata;
    surface_destroy_buffers(userdata);
    free(userdata);
}

static void glplugin_surface_resize(void* _userdata, unsigned int width, unsigned int height) {
    struct PluginSurfaceUserdata* userdata = _userdata;
    surface_destroy_buffers(userdata);
    userdata->width = width;
    userdata->height = height;
    surface_init_buffers(userdata);
    lgl->ClearColor(0.0, 0.0, 0.0, 0.0);
    lgl->Clear(GL_COLOR_BUFFER_BIT);
}

static void glplugin_surface_clear(void* _userdata, double r, double g, double b, double a) {
    struct PluginSurfaceUserdata* userdata = _userdata;
    struct GLContext* c = _bolt_context();
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, userdata->framebuffer);
    lgl->ClearColor(r, g, b, a);
    lgl->Clear(GL_COLOR_BUFFER_BIT);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, c->current_draw_framebuffer);
}

static void glplugin_surface_subimage(void* _userdata, int x, int y, int w, int h, const void* pixels, uint8_t is_bgra) {
    struct PluginSurfaceUserdata* userdata = _userdata;
    struct GLContext* c = _bolt_context();
    lgl->BindTexture(GL_TEXTURE_2D, userdata->renderbuffer);
    lgl->TexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, is_bgra ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    const struct GLTexture2D* original_tex = c->texture_units[c->active_texture].texture_2d;
    lgl->BindTexture(GL_TEXTURE_2D, original_tex ? original_tex->id : 0);
}

static void glplugin_surface_drawtoscreen(void* userdata, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    const struct GLContext* c = _bolt_context();
    surface_draw(c, userdata, &program_direct_screen, 0, gl_width, gl_height, sx, sy, sw, sh, dx, dy, dw, dh);
}

static void glplugin_surface_drawtosurface(void* userdata, void* _target, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    const struct PluginSurfaceUserdata* target = _target;
    const struct GLContext* c = _bolt_context();
    surface_draw(c, userdata, &program_direct_surface, target->framebuffer, target->width, target->height, sx, sy, sw, sh, dx, dy, dw, dh);
}

static void glplugin_surface_drawtogameview(void* userdata, void* _gameview, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    const struct GLPluginRenderGameViewUserData* gameview = _gameview;
    const struct GLContext* c = _bolt_context();
    surface_draw(c, userdata, &program_direct_screen, gameview->target_fb, gameview->width, gameview->height, sx, sy, sw, sh, dx, dy, dw, dh);
}

static void glplugin_surface_set_tint(void* userdata, double r, double g, double b) {
    struct PluginSurfaceUserdata* surface = userdata;
    surface->rgba[0] = (GLfloat)r;
    surface->rgba[1] = (GLfloat)g;
    surface->rgba[2] = (GLfloat)b;
}

static void glplugin_surface_set_alpha(void* userdata, double alpha) {
    struct PluginSurfaceUserdata* surface = userdata;
    surface->rgba[3] = (GLfloat)alpha;
}

static void glplugin_draw_region_outline(void* userdata, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    struct PluginSurfaceUserdata* target = userdata;
    struct GLContext* c = _bolt_context();
    GLboolean depth_test, scissor_test, cull_face;
    lgl->GetBooleanv(GL_DEPTH_TEST, &depth_test);
    lgl->GetBooleanv(GL_SCISSOR_TEST, &scissor_test);
    lgl->GetBooleanv(GL_CULL_FACE, &cull_face);

    gl.UseProgram(program_region);
    gl.BindVertexArray(program_direct_vao);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, target->framebuffer);
    lgl->Viewport(0, 0, gl_width, gl_height);
    lgl->Disable(GL_DEPTH_TEST);
    lgl->Disable(GL_SCISSOR_TEST);
    lgl->Disable(GL_CULL_FACE);
    gl.Uniform4i(program_region_xywh, x, y, width, height);
    gl.Uniform2i(program_region_dest_wh, gl_width, gl_height);
    lgl->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (depth_test) lgl->Enable(GL_DEPTH_TEST);
    if (scissor_test) lgl->Enable(GL_SCISSOR_TEST);
    if (cull_face) lgl->Enable(GL_CULL_FACE);
    lgl->Viewport(c->viewport_x, c->viewport_y, c->viewport_w, c->viewport_h);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, c->current_draw_framebuffer);
    gl.BindVertexArray(c->bound_vao->id);
    gl.UseProgram(c->bound_program ? c->bound_program->id : 0);
}

static void glplugin_read_screen_pixels(int16_t x, int16_t y, uint32_t width, uint32_t height, void* data) {
    struct GLContext* c = _bolt_context();
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    lgl->ReadPixels(x, gl_height - (y + height), width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, c->current_read_framebuffer);
}

static void glplugin_copy_screen(void* userdata, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    struct GLContext* c = _bolt_context();
    struct PluginSurfaceUserdata* surface = userdata;
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, surface->framebuffer);
    gl.BlitFramebuffer(sx, gl_height - sy, sx + sw, gl_height - (sy + sh), dx, dy, dx + dw, dy + dh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, c->current_read_framebuffer);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, c->current_draw_framebuffer);
}

static void glplugin_game_view_rect(int* x, int* y, int* w, int* h) {
    struct GLContext* c = _bolt_context();
    *x = c->game_view_x;
    *y = c->game_view_y;
    *w = c->game_view_w;
    *h = c->game_view_h;
}

static void glplugin_player_position(int32_t* x, int32_t* y, int32_t* z) {
    struct GLContext* c = _bolt_context();
    *x = c->player_model_x;
    *y = c->player_model_y;
    *z = c->player_model_z;
}

static uint8_t glplugin_vertex_shader_init(struct ShaderFunctions* out, const char* source, int len, char* output, int output_len) {
    const GLuint shader = gl.CreateShader(GL_VERTEX_SHADER);
    const GLchar* sources[] = {GLSLHEADER, GLSLPLUGINEXTENSIONHEADER, source};
    const GLint lengths[] = {sizeof(GLSLHEADER) - sizeof(*GLSLHEADER), sizeof(GLSLPLUGINEXTENSIONHEADER) - sizeof(*GLSLPLUGINEXTENSIONHEADER), len};
    gl.ShaderSource(shader, 3, sources, lengths);
    gl.CompileShader(shader);
    GLint status;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        gl.GetShaderInfoLog(shader, output_len, NULL, output);
        return false;
    }
    out->userdata = malloc(sizeof(struct PluginShaderUserdata));
    ((struct PluginShaderUserdata*)out->userdata)->shader = shader;
    return true;
}

static uint8_t glplugin_fragment_shader_init(struct ShaderFunctions* out, const char* source, int len, char* output, int output_len) {
    const GLuint shader = gl.CreateShader(GL_FRAGMENT_SHADER);
    const GLchar* sources[] = {GLSLHEADER, GLSLPLUGINEXTENSIONHEADER, source};
    const GLint lengths[] = {sizeof(GLSLHEADER) - sizeof(*GLSLHEADER), sizeof(GLSLPLUGINEXTENSIONHEADER) - sizeof(*GLSLPLUGINEXTENSIONHEADER), len};
    gl.ShaderSource(shader, 3, sources, lengths);
    gl.CompileShader(shader);
    GLint status;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        gl.GetShaderInfoLog(shader, output_len, NULL, output);
        return false;
    }
    out->userdata = malloc(sizeof(struct PluginShaderUserdata));
    ((struct PluginShaderUserdata*)out->userdata)->shader = shader;
    return true;
}

static uint8_t glplugin_shaderprogram_init(struct ShaderProgramFunctions* out, void* vertex, void* fragment, char* output, int output_len) {
    const GLuint program = gl.CreateProgram();
    const GLuint vs = ((struct PluginShaderUserdata*)vertex)->shader;
    const GLuint fs = ((struct PluginShaderUserdata*)fragment)->shader;
    gl.AttachShader(program, vs);
    gl.AttachShader(program, fs);
    gl.LinkProgram(program);
    GLint status;
    gl.GetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        gl.GetProgramInfoLog(program, output_len, NULL, output);
        return false;
    }
    gl.DetachShader(program, vs);
    gl.DetachShader(program, fs);
    out->userdata = malloc(sizeof(struct PluginProgramUserdata));
    struct PluginProgramUserdata* userdata = (struct PluginProgramUserdata*)out->userdata;
    userdata->uniforms = hashmap_new(sizeof(struct PluginProgramUniform), 16, 0, 0, uniform_hash, uniform_compare, NULL, NULL);
    userdata->program = program;
    userdata->bindings_enabled = 0;
    gl.GenVertexArrays(1, &userdata->vao);
    out->set_attribute = glplugin_shaderprogram_set_attribute;
    out->draw_to_surface = glplugin_shaderprogram_drawtosurface;
    out->draw_to_gameview = glplugin_shaderprogram_drawtogameview;
    out->set_uniform_floats = glplugin_shaderprogram_set_uniform_floats;
    out->set_uniform_ints = glplugin_shaderprogram_set_uniform_ints;
    out->set_uniform_matrix = glplugin_shaderprogram_set_uniform_matrix;
    out->set_uniform_surface = glplugin_shaderprogram_set_uniform_surface;
    out->set_uniform_depthbuffer = glplugin_shaderprogram_set_uniform_depthbuffer;
    return true;
}

static void glplugin_shader_destroy(void* userdata) {
    gl.DeleteShader(((struct PluginShaderUserdata*)userdata)->shader);
    free(userdata);
}

static void glplugin_shaderprogram_destroy(void* userdata) {
    struct PluginProgramUserdata* program = (struct PluginProgramUserdata*)userdata;
    hashmap_free(program->uniforms);
    gl.DeleteProgram(program->program);
    gl.DeleteVertexArrays(1, &program->vao);
    free(userdata);
}

static uint8_t glplugin_shaderprogram_set_attribute(void* userdata, uint8_t attribute, uint8_t type_width, uint8_t type_is_signed, uint8_t type_is_float, uint8_t size, uint32_t offset, uint32_t stride) {
    struct PluginProgramUserdata* program = (struct PluginProgramUserdata*)userdata;
    const GLbitfield bitmask = 1 << attribute;
    if (program->bindings_enabled & bitmask) return false;
    struct PluginProgramAttrBinding* binding = &program->bindings[attribute];
    if (type_is_float) {
        if (!type_is_signed) return false;
        switch (type_width) {
            case 2:
                binding->type = GL_HALF_FLOAT;
                binding->is_double = false;
                break;
            case 4:
                binding->type = GL_FLOAT;
                binding->is_double = false;
                break;
            case 8:
                binding->type = GL_DOUBLE;
                binding->is_double = true;
                break;
            default:
                return false;
        }
    } else {
        binding->is_double = false;
        switch (type_width) {
            case 1:
                binding->type = type_is_signed ? GL_BYTE : GL_UNSIGNED_BYTE;
                break;
            case 2:
                binding->type = type_is_signed ? GL_SHORT : GL_UNSIGNED_SHORT;
                break;
            case 4:
                binding->type = type_is_signed ? GL_INT : GL_UNSIGNED_INT;
                break;
            default:
                return false;
        }
    }
    binding->size = size;
    binding->offset = offset;
    binding->stride = stride;
    program->bindings_enabled |= bitmask;
    return true;
}

static void glplugin_shaderprogram_set_uniform_floats(void* userdata, int location, uint8_t count, double* values) {
    const struct PluginProgramUserdata* program = (struct PluginProgramUserdata*)userdata;
    struct PluginProgramUniform uniform = {
        .location = location,
        .count = count,
        .type = 0,
        .sub.is_float = true,
    };
    for (size_t i = 0; i < count; i += 1) {
        uniform.values.f[i] = (GLfloat)values[i];
    }
    hashmap_set(program->uniforms, &uniform);
}

static void glplugin_shaderprogram_set_uniform_ints(void* userdata, int location, uint8_t count, int* values) {
    const struct PluginProgramUserdata* program = (struct PluginProgramUserdata*)userdata;
    struct PluginProgramUniform uniform = {
        .location = location,
        .count = count,
        .type = 0,
        .sub.is_float = false,
    };
    for (size_t i = 0; i < count; i += 1) {
        uniform.values.i[i] = (GLint)values[i];
    }
    hashmap_set(program->uniforms, &uniform);
}

static void glplugin_shaderprogram_set_uniform_matrix(void* userdata, int location, uint8_t transpose, uint8_t size, double* values) {
    const struct PluginProgramUserdata* program = (struct PluginProgramUserdata*)userdata;
    struct PluginProgramUniform uniform = {
        .location = location,
        .count = size,
        .type = 1,
        .sub.transpose = transpose,
    };
    for (size_t i = 0; i < (size * size); i += 1) {
        uniform.values.f[i] = (GLfloat)values[i];
    }
    hashmap_set(program->uniforms, &uniform);
}

static void glplugin_shaderprogram_set_uniform_surface(void* userdata, int location, void* target) {
    const struct PluginProgramUserdata* program = (struct PluginProgramUserdata*)userdata;
    const struct PluginSurfaceUserdata* surface = (struct PluginSurfaceUserdata*)target;
    struct PluginProgramUniform uniform = {
        .location = location,
        .count = 1,
        .type = 2,
        .sub.sampler = surface->renderbuffer,
    };
    hashmap_set(program->uniforms, &uniform);
}

static void glplugin_shaderprogram_set_uniform_depthbuffer(void* userdata, void* event, int location) {
    const struct PluginProgramUserdata* program = userdata;
    const struct GLPluginRenderGameViewUserData* gameview = event;
    struct PluginProgramUniform uniform = {
        .location = location,
        .count = 1,
        .type = 2,
        .sub.sampler = gameview->depth_tex,
    };
    hashmap_set(program->uniforms, &uniform);
}

static void glplugin_shaderprogram_drawtosurface(void* userdata, void* surface_, void* buffer_, uint32_t count) {
    const struct PluginProgramUserdata* program = (struct PluginProgramUserdata*)userdata;
    const struct PluginSurfaceUserdata* surface = surface_;
    const struct PluginShaderBufferUserdata* buffer = buffer_;
    shaderprogram_draw(program, buffer, count, surface->framebuffer, surface->width, surface->height);
}

static void glplugin_shaderprogram_drawtogameview(void* userdata, void* gameview_, void* buffer_, uint32_t count) {
    const struct PluginProgramUserdata* program = (struct PluginProgramUserdata*)userdata;
    const struct GLPluginRenderGameViewUserData* gameview = gameview_;
    const struct PluginShaderBufferUserdata* buffer = buffer_;
    shaderprogram_draw(program, buffer, count, gameview->target_fb, gameview->width, gameview->height);
}

static void glplugin_shaderbuffer_init(struct ShaderBufferFunctions* out, const void* data, uint32_t len) {
    out->userdata = malloc(sizeof(struct PluginShaderBufferUserdata));
    struct PluginShaderBufferUserdata* buffer = out->userdata;
    GLint array_binding;
    lgl->GetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_binding);
    gl.GenBuffers(1, &buffer->buffer);
    gl.BindBuffer(GL_ARRAY_BUFFER, buffer->buffer);
    gl.BufferData(GL_ARRAY_BUFFER, len, data, GL_STATIC_DRAW);
    gl.BindBuffer(GL_ARRAY_BUFFER, array_binding);
}

static void glplugin_shaderbuffer_destroy(void* userdata) {
    gl.DeleteBuffers(1, &((struct PluginShaderBufferUserdata*)userdata)->buffer);
    free(userdata);
}
