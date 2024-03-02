#include "gl.h"
#include "plugin.h"
#include "rwlock.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// comment or uncomment this to enable verbose logging of hooks in this file
//#define VERBOSE

// don't change this part, change the line above this instead
#if defined(VERBOSE)
#define LOG printf
#else
#define LOG(...)
#endif

const char* lua_test_plugin =
"local bolt = require(\"bolt\")"                                "\n"
"local function callback2d (batch)"                             "\n"
"  if batch:isminimap() then print(\"minimap render 2d\") end"  "\n"
"end"                                                           "\n"
"local function callbackminimap (render)"                       "\n"
"  print(\"minimap \", render:angle(), render:scale(), render:position())\n"
"end"                                                           "\n"
"local function callbackswapbuffers (s)"                        "\n"
"  print(\"swap\")"                                             "\n"
"end"                                                           "\n"
"bolt.setcallback2d(callback2d)"                                "\n"
"bolt.setcallbackswapbuffers(callbackswapbuffers)"              "\n"
"bolt.setcallbackminimap(callbackminimap)"                      "\n"
;

size_t egl_init_count = 0;
uintptr_t egl_main_context = 0;
uint8_t egl_main_context_destroy_pending = 0;
uint8_t egl_main_context_makecurrent_pending = 0;

struct GLProcFunctions gl = {0};
unsigned int framebuffer2d;
unsigned int renderbuffer2d;
uint32_t framebuffer2d_width = 0;
uint32_t framebuffer2d_height = 0;
uint8_t framebuffer2d_inited = 0;
unsigned int program_direct;
unsigned int program_direct_sampler;
unsigned int buffer_vertices_square;

// "direct" program is basically a blit but with transparency
const char* program_direct_vs = "#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"out vec2 vPos;\n"
"void main(){gl_Position=vec4(aPos.x,aPos.y,0.0,1.0); vPos=aPos;}\n";
const char* program_direct_fs = "#version 330 core\n"
"in vec2 vPos;\n"
"out vec4 col;\n"
"uniform sampler2D tex;"
"void main(){col=texture(tex,vPos);}\n";

struct GLProgram* _bolt_context_get_program(struct GLContext*, unsigned int);
struct GLArrayBuffer* _bolt_context_get_buffer(struct GLContext*, unsigned int);
struct GLTexture2D* _bolt_context_get_texture(struct GLContext*, unsigned int);
struct GLVertexArray* _bolt_context_get_vao(struct GLContext*, unsigned int);
void _bolt_glcontext_init(struct GLContext*, void*, void*);
void _bolt_glcontext_free(struct GLContext*);

#define MAX_TEXTURE_UNITS 4096 // would be nice if there was a way to query this at runtime, but it would be awkward to set up
#define BUFFER_LIST_CAPACITY 256 * 256
#define TEXTURE_LIST_CAPACITY 256
#define PROGRAM_LIST_CAPACITY 256 * 8
#define VAO_LIST_CAPACITY 256 * 256
#define CONTEXTS_CAPACITY 64 // not growable so we just have to hard-code a number and hope it's enough forever
struct GLContext contexts[CONTEXTS_CAPACITY];
_Thread_local struct GLContext* current_context = NULL;

struct GLContext* _bolt_context() {
    return current_context;
}

size_t _bolt_context_count() {
    size_t ret = 0;
    for (size_t i = 0; i < CONTEXTS_CAPACITY; i += 1) {
        if (contexts[i].id != 0) {
            ret += 1;
        }
    }
    return ret;
}

void _bolt_create_context(void* egl_context, void* shared) {
    for (size_t i = 0; i < CONTEXTS_CAPACITY; i += 1) {
        struct GLContext* ptr = &contexts[i];
        if (ptr->id == 0) {
            _bolt_glcontext_init(ptr, egl_context, shared);
            ptr->is_attached = 1;
            return;
        }
    }
}

void _bolt_destroy_context(void* egl_context) {
    for (size_t i = 0; i < CONTEXTS_CAPACITY; i += 1) {
        struct GLContext* ptr = &contexts[i];
        if (ptr->id == (uintptr_t)egl_context) {
            if (ptr->is_attached) {
                ptr->deferred_destroy = 1;
            } else {
                _bolt_glcontext_free(ptr);
                ptr->id = 0;
            }
            break;
        }
    }
}

void _bolt_set_attr_binding(struct GLContext* c, struct GLAttrBinding* binding, unsigned int buffer, int size, const void* offset, unsigned int stride, uint32_t type, uint8_t normalise) {
    binding->buffer = _bolt_context_get_buffer(c, buffer);
    binding->offset = (uintptr_t)offset;
    binding->size = size;
    binding->stride = stride;
    binding->normalise = normalise;
    binding->type = type;
}

float _bolt_f16_to_f32(uint16_t bits) {
    const uint16_t bits_exp_component = (bits & 0b0111110000000000);
    if (bits_exp_component == 0) return 0.0f; // truncate subnormals to 0
    const uint32_t sign_component = (bits & 0b1000000000000000) << 16;
    const uint32_t exponent = (bits_exp_component >> 10) + (127 - 15); // adjust exp bias
    const uint32_t mantissa = bits & 0b0000001111111111;
    const union { uint32_t b; float f; } u = {.b = sign_component | (exponent << 23) | (mantissa << 13)};
    return u.f;
}

uint8_t _bolt_get_attr_binding(struct GLContext* c, const struct GLAttrBinding* binding, size_t index, size_t num_out, float* out) {
    struct GLArrayBuffer* buffer = binding->buffer;
    if (!buffer || !buffer->data) return 0;
    uintptr_t buf_offset = binding->offset + (binding->stride * index);

    const void* ptr = buffer->data + buf_offset;
    if (!binding->normalise) {
        switch (binding->type) {
            case GL_FLOAT:
                memcpy(out, ptr, num_out * sizeof(float));
                break;
            case GL_HALF_FLOAT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = _bolt_f16_to_f32(*(uint16_t*)(ptr + (i * 2)));
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

uint8_t _bolt_get_attr_binding_int(struct GLContext* c, const struct GLAttrBinding* binding, size_t index, size_t num_out, int32_t* out) {
    struct GLArrayBuffer* buffer = binding->buffer;
    if (!buffer || !buffer->data) return 0;
    uintptr_t buf_offset = binding->offset + (binding->stride * index);

    const void* ptr = buffer->data + buf_offset;
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

int _bolt_hashmap_compare(const void* a, const void* b, void* udata) {
    return (**(unsigned int**)a) - (**(unsigned int**)b);
}

uint64_t _bolt_hashmap_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const unsigned int* const* const id = item;
    return hashmap_sip(*id, sizeof(unsigned int), seed0, seed1);
}

void _bolt_hashmap_init(struct HashMap* map, size_t cap) {
    _bolt_rwlock_init(&map->rwlock);
    map->map = hashmap_new(sizeof(void*), cap, 0, 0, _bolt_hashmap_hash, _bolt_hashmap_compare, NULL, NULL);
}

void _bolt_hashmap_destroy(struct HashMap* map) {
    _bolt_rwlock_destroy(&map->rwlock);
    hashmap_free(map->map);
}

void _bolt_glcontext_init(struct GLContext* context, void* egl_context, void* egl_shared) {
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
    context->texture_units = calloc(MAX_TEXTURE_UNITS, sizeof(unsigned int));
    context->game_view_tex = -1;
    context->target_3d_tex = -1;
    context->game_view_framebuffer = -1;
    context->need_3d_tex = 0;
    if (shared) {
        context->programs = shared->programs;
        context->buffers = shared->buffers;
        context->textures = shared->textures;
        context->vaos = shared->vaos;
    } else {
        context->is_shared_owner = 1;
        context->programs = malloc(sizeof(struct HashMap));
        _bolt_hashmap_init(context->programs, PROGRAM_LIST_CAPACITY);
        context->buffers = malloc(sizeof(struct HashMap));
        _bolt_hashmap_init(context->buffers, BUFFER_LIST_CAPACITY);
        context->textures = malloc(sizeof(struct HashMap));
        _bolt_hashmap_init(context->textures, TEXTURE_LIST_CAPACITY);
        context->vaos = malloc(sizeof(struct HashMap));
        _bolt_hashmap_init(context->vaos, VAO_LIST_CAPACITY);
    }
}

void _bolt_glcontext_free(struct GLContext* context) {
    free(context->texture_units);
    if (context->is_shared_owner) {
        _bolt_hashmap_destroy(context->programs);
        free(context->programs);
        _bolt_hashmap_destroy(context->buffers);
        free(context->buffers);
        _bolt_hashmap_destroy(context->textures);
        free(context->textures);
        _bolt_hashmap_destroy(context->vaos);
        free(context->vaos);
    }
}

uint32_t _bolt_binding_for_buffer(uint32_t target) {
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

struct GLProgram* _bolt_context_get_program(struct GLContext* c, unsigned int index) {
    struct HashMap* map = c->programs;
    const unsigned int* index_ptr = &index;
    _bolt_rwlock_lock_read(&map->rwlock);
    struct GLProgram** program = (struct GLProgram**)hashmap_get(map->map, &index_ptr);
    struct GLProgram* ret = program ? *program : NULL;
    _bolt_rwlock_unlock_read(&map->rwlock);
    return ret;
}

struct GLArrayBuffer* _bolt_context_get_buffer(struct GLContext* c, unsigned int index) {
    struct HashMap* map = c->buffers;
    const unsigned int* index_ptr = &index;
    _bolt_rwlock_lock_read(&map->rwlock);
    struct GLArrayBuffer** buffer = (struct GLArrayBuffer**)hashmap_get(map->map, &index_ptr);
    struct GLArrayBuffer* ret = buffer ? *buffer : NULL;
    _bolt_rwlock_unlock_read(&map->rwlock);
    return ret;
}

struct GLTexture2D* _bolt_context_get_texture(struct GLContext* c, unsigned int index) {
    struct HashMap* map = c->textures;
    const unsigned int* index_ptr = &index;
    _bolt_rwlock_lock_read(&map->rwlock);
    struct GLTexture2D** tex = (struct GLTexture2D**)hashmap_get(map->map, &index_ptr);
    struct GLTexture2D* ret = tex ? *tex : NULL;
    _bolt_rwlock_unlock_read(&map->rwlock);
    return ret;
}

struct GLVertexArray* _bolt_context_get_vao(struct GLContext* c, unsigned int index) {
    struct HashMap* map = c->vaos;
    const unsigned int* index_ptr = &index;
    _bolt_rwlock_lock_read(&map->rwlock);
    struct GLVertexArray** vao = (struct GLVertexArray**)hashmap_get(map->map, &index_ptr);
    struct GLVertexArray* ret = vao ? *vao : NULL;
    _bolt_rwlock_unlock_read(&map->rwlock);
    return ret;
}

void _bolt_unpack_rgb565(uint16_t packed, uint8_t out[3]) {
    out[0] = (packed >> 11) & 0b00011111;
    out[0] = (out[0] << 3) | (out[0] >> 2);
    out[1] = (packed >> 5) & 0b00111111;
    out[1] = (out[1] << 2) | (out[1] >> 4);
    out[2] = packed & 0b00011111;
    out[2] = (out[2] << 3) | (out[2] >> 2);
}

void _bolt_gl_load(void* (*GetProcAddress)(const char*)) {
#define INIT_GL_FUNC(NAME) gl.NAME = GetProcAddress("gl"#NAME);
    INIT_GL_FUNC(ActiveTexture)
    INIT_GL_FUNC(AttachShader)
    INIT_GL_FUNC(BindAttribLocation)
    INIT_GL_FUNC(BindBuffer)
    INIT_GL_FUNC(BindFramebuffer)
    INIT_GL_FUNC(BindVertexArray)
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
    INIT_GL_FUNC(DeleteShader)
    INIT_GL_FUNC(DeleteVertexArrays)
    INIT_GL_FUNC(DisableVertexAttribArray)
    INIT_GL_FUNC(DrawElements)
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
    INIT_GL_FUNC(GetIntegerv)
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
    INIT_GL_FUNC(Uniform1i)
    INIT_GL_FUNC(UniformMatrix4fv)
    INIT_GL_FUNC(UnmapBuffer)
    INIT_GL_FUNC(UseProgram)
    INIT_GL_FUNC(VertexAttribPointer)
#undef INIT_GL_FUNC
}

void _bolt_gl_init() {
    unsigned int direct_vs = gl.CreateShader(GL_VERTEX_SHADER);
    gl.ShaderSource(direct_vs, 1, &program_direct_vs, NULL);
    gl.CompileShader(direct_vs);
    unsigned int direct_fs = gl.CreateShader(GL_FRAGMENT_SHADER);
    gl.ShaderSource(direct_fs, 1, &program_direct_fs, NULL);
    gl.CompileShader(direct_fs);
    program_direct = gl.CreateProgram();
    gl.AttachShader(program_direct, direct_vs);
    gl.AttachShader(program_direct, direct_fs);
    gl.LinkProgram(program_direct);
    program_direct_sampler = gl.GetUniformLocation(program_direct, "tex");
    gl.GenBuffers(1, &buffer_vertices_square);
    gl.BindBuffer(GL_ARRAY_BUFFER, buffer_vertices_square);
    const float square[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    gl.BufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, square, GL_STATIC_DRAW);
    gl.DeleteShader(direct_vs);
    gl.DeleteShader(direct_fs);
}

unsigned int _bolt_glCreateProgram() {
    LOG("glCreateProgram\n");
    unsigned int id = gl.CreateProgram();
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
    program->loc_uProjectionMatrix = -1;
    program->loc_uDiffuseMap = -1;
    program->loc_uTextureAtlas = -1;
    program->loc_uTextureAtlasSettings = -1;
    program->loc_uAtlasMeta = -1;
    program->loc_uModelMatrix = -1;
    program->loc_uGridSize = -1;
    program->loc_uVertexScale = -1;
    program->loc_sSceneHDRTex = -1;
    program->block_index_ViewTransforms = -1;
    program->offset_uCameraPosition = -1;
    program->offset_uViewProjMatrix = -1;
    program->is_2d = 0;
    program->is_3d = 0;
    program->is_minimap = 0;
    _bolt_rwlock_lock_write(&c->programs->rwlock);
    hashmap_set(c->programs->map, &program);
    _bolt_rwlock_unlock_write(&c->programs->rwlock);
    LOG("glCreateProgram end\n");
    return id;
}

void _bolt_glBindAttribLocation(unsigned int program, unsigned int index, const char* name) {
    LOG("glBindAttribLocation\n");
    gl.BindAttribLocation(program, index, name);
    struct GLContext* c = _bolt_context();
    struct GLProgram* p = _bolt_context_get_program(c, program);
#define ATTRIB_MAP(NAME) if (!strcmp(name, #NAME)) p->loc_##NAME = index;
    ATTRIB_MAP(aVertexPosition2D)
    ATTRIB_MAP(aVertexColour)
    ATTRIB_MAP(aTextureUV)
    ATTRIB_MAP(aTextureUVAtlasMin)
    ATTRIB_MAP(aTextureUVAtlasExtents)
    ATTRIB_MAP(aMaterialSettingsSlotXY_TilePositionXZ)
    ATTRIB_MAP(aVertexPosition_BoneLabel)
#undef ATTRIB_MAP
    LOG("glBindAttribLocation end\n");
}

void _bolt_glLinkProgram(unsigned int program) {
    LOG("glLinkProgram\n");
    gl.LinkProgram(program);
    struct GLContext* c = _bolt_context();
    struct GLProgram* p = _bolt_context_get_program(c, program);
    const int uDiffuseMap = gl.GetUniformLocation(program, "uDiffuseMap");
    const int uProjectionMatrix = gl.GetUniformLocation(program, "uProjectionMatrix");
    const int uTextureAtlas = gl.GetUniformLocation(program, "uTextureAtlas");
    const int uTextureAtlasSettings = gl.GetUniformLocation(program, "uTextureAtlasSettings");
    const int uAtlasMeta = gl.GetUniformLocation(program, "uAtlasMeta");
    const int loc_uModelMatrix = gl.GetUniformLocation(program, "uModelMatrix");
    const int loc_uGridSize = gl.GetUniformLocation(program, "uGridSize");
    const int loc_uVertexScale = gl.GetUniformLocation(program, "uVertexScale");
    p->loc_sSceneHDRTex = gl.GetUniformLocation(program, "sSceneHDRTex");
    p->loc_sSourceTex = gl.GetUniformLocation(program, "sSourceTex");

    const char* view_var_names[] = {"uCameraPosition", "uViewProjMatrix"};
    const int block_index_ViewTransforms = gl.GetUniformBlockIndex(program, "ViewTransforms");
    unsigned int ubo_indices[2];
    int view_offsets[2];
    int viewport_offset;
    if (block_index_ViewTransforms != -1) {
        gl.GetUniformIndices(program, 2, view_var_names, ubo_indices);
        gl.GetActiveUniformsiv(program, 2, ubo_indices, GL_UNIFORM_OFFSET, view_offsets);
    }

    if (loc_uModelMatrix != -1 && loc_uGridSize != -1 && block_index_ViewTransforms != -1) {
        p->loc_uModelMatrix = loc_uModelMatrix;
        p->loc_uGridSize = loc_uGridSize;
        p->block_index_ViewTransforms = block_index_ViewTransforms;
        p->offset_uCameraPosition = view_offsets[0];
        p->offset_uViewProjMatrix = view_offsets[1];
        p->is_minimap = 1;
    }
    if (p && p->loc_aVertexPosition2D != -1 && p->loc_aVertexColour != -1 && p->loc_aTextureUV != -1 && p->loc_aTextureUVAtlasMin != -1 && p->loc_aTextureUVAtlasExtents != -1 && uDiffuseMap != -1 && uProjectionMatrix != -1) {
        p->loc_uDiffuseMap = uDiffuseMap;
        p->loc_uProjectionMatrix = uProjectionMatrix;
        p->is_2d = 1;
    }
    if (p && p->loc_aTextureUV != -1 && p->loc_aVertexColour != -1 && p->loc_aVertexPosition_BoneLabel != -1 && p->loc_aMaterialSettingsSlotXY_TilePositionXZ != -1 && uTextureAtlas != -1 && uAtlasMeta != -1 && uTextureAtlasSettings != -1 && loc_uModelMatrix != -1 && loc_uVertexScale != -1 && block_index_ViewTransforms != -1) {
        p->loc_uTextureAtlas = uTextureAtlas;
        p->loc_uTextureAtlasSettings = uTextureAtlasSettings;
        p->loc_uAtlasMeta = uAtlasMeta;
        p->loc_uModelMatrix = loc_uModelMatrix;
        p->loc_uVertexScale = loc_uVertexScale;
        p->block_index_ViewTransforms = block_index_ViewTransforms;
        p->offset_uCameraPosition = view_offsets[0];
        p->offset_uViewProjMatrix = view_offsets[1];
        p->is_3d = 1;
    }
    LOG("glLinkProgram end\n");
}

void _bolt_glUseProgram(unsigned int program) {
    LOG("glUseProgram\n");
    gl.UseProgram(program);
    struct GLContext* c = _bolt_context();
    c->bound_program = _bolt_context_get_program(c, program);
    LOG("glUseProgram end\n");
}

void _bolt_glTexStorage2D(uint32_t target, int levels, uint32_t internalformat, unsigned int width, unsigned int height) {
    LOG("glTexStorage2D\n");
    gl.TexStorage2D(target, levels, internalformat, width, height);
    struct GLContext* c = _bolt_context();
    if (target == GL_TEXTURE_2D) {
        struct GLTexture2D* tex = c->texture_units[c->active_texture];
        free(tex->data);
        tex->data = malloc(width * height * 4);
        tex->width = width;
        tex->height = height;
    }
    LOG("glTexStorage2D end\n");
}

void _bolt_glVertexAttribPointer(unsigned int index, int size, uint32_t type, uint8_t normalised, unsigned int stride, const void* pointer) {
    LOG("glVertexAttribPointer\n");
    gl.VertexAttribPointer(index, size, type, normalised, stride, pointer);
    struct GLContext* c = _bolt_context();
    int array_binding;
    gl.GetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_binding);
    _bolt_set_attr_binding(c, &c->bound_vao->attributes[index], array_binding, size, pointer, stride, type, normalised);
    LOG("glVertexAttribPointer end\n");
}

void _bolt_glGenBuffers(uint32_t n, unsigned int* buffers) {
    LOG("glGenBuffers\n");
    gl.GenBuffers(n, buffers);
    struct GLContext* c = _bolt_context();
    _bolt_rwlock_lock_write(&c->textures->rwlock);
    for (size_t i = 0; i < n; i += 1) {
        struct GLArrayBuffer* buffer = calloc(1, sizeof(struct GLArrayBuffer));
        buffer->id = buffers[i];
        hashmap_set(c->buffers->map, &buffer);
    }
    _bolt_rwlock_unlock_write(&c->textures->rwlock);
    LOG("glGenBuffers end\n");
}

void _bolt_glBufferData(uint32_t target, uintptr_t size, const void* data, uint32_t usage) {
    LOG("glBufferData\n");
    gl.BufferData(target, size, data, usage);
    struct GLContext* c = _bolt_context();
    uint32_t binding_type = _bolt_binding_for_buffer(target);
    if (binding_type != -1) {
        int buffer_id;
        gl.GetIntegerv(binding_type, &buffer_id);
        void* buffer_content = malloc(size);
        if (data) memcpy(buffer_content, data, size);
        struct GLArrayBuffer* buffer = _bolt_context_get_buffer(c, buffer_id);
        free(buffer->data);
        buffer->data = buffer_content;
    }
    LOG("glBufferData end\n");
}

void _bolt_glDeleteBuffers(unsigned int n, const unsigned int* buffers) {
    LOG("glDeleteBuffers\n");
    gl.DeleteBuffers(n, buffers);
    struct GLContext* c = _bolt_context();
    for (unsigned int i = 0; i < n; i += 1) {
        struct GLArrayBuffer* buffer = _bolt_context_get_buffer(c, buffers[i]);
        free(buffer->data);
        free(buffer->mapping);
        free(buffer);
    }
    LOG("glDeleteBuffers end\n");
}

void _bolt_glBindFramebuffer(uint32_t target, unsigned int framebuffer) {
    LOG("glBindFramebuffer\n", (uintptr_t)gl.BindFramebuffer);
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

void _bolt_glCompressedTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, unsigned int width, unsigned int height, uint32_t format, unsigned int imageSize, const void* data) {
    LOG("glCompressedTexSubImage2D\n");
    gl.CompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    if (target != GL_TEXTURE_2D || level != 0) return;
    struct GLContext* c = _bolt_context();
    if (format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT || format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT || format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT) {
        struct GLTexture2D* tex = c->texture_units[c->active_texture];
        int out_xoffset = xoffset;
        int out_yoffset = yoffset;
        for (size_t ii = 0; ii < (width * height); ii += 16) {
            const uint8_t* ptr = data + ii;
            uint8_t* out_ptr = tex->data + (out_yoffset * tex->width * 4) + (out_xoffset * 4);
            uint16_t c0 = *(ptr + 8) + (*(ptr + 9) << 8);
            uint16_t c1 = *(ptr + 10) + (*(ptr + 11) << 8);
            uint8_t c0_rgb[3];
            uint8_t c1_rgb[3];
            _bolt_unpack_rgb565(c0, c0_rgb);
            _bolt_unpack_rgb565(c1, c1_rgb);
            const uint8_t c0_greater = c0 > c1;
            const uint32_t ctable = *(ptr + 12) + (*(ptr + 13) << 8) + (*(ptr + 14) << 16) + (*(ptr + 15) << 24);
            
            for (size_t j = 0; j < 4; j += 1) {
                for (size_t i = 0; i < 4; i += 1) {
                    if (out_xoffset + i >= 0 && out_yoffset + j >= 0 && out_xoffset + i < tex->width && out_yoffset + j < tex->height) {
                        uint8_t* pixel_ptr = out_ptr + (tex->width * i * 4) + (j * 4);
                        const uint32_t code = (ctable >> (2 * (4 * i + j))) & 3;
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
                    }
                }
            }
    
            out_xoffset += 4;
            if (out_xoffset >= xoffset + width) {
                out_xoffset = xoffset;
                out_yoffset += 4;
            }
        }
    }
    LOG("glCompressedTexSubImage2D end\n");
}

void _bolt_glCopyImageSubData(unsigned int srcName, uint32_t srcTarget, int srcLevel, int srcX, int srcY, int srcZ,
                              unsigned int dstName, uint32_t dstTarget, int dstLevel, int dstX, int dstY, int dstZ,
                              unsigned int srcWidth, unsigned int srcHeight, unsigned int srcDepth) {
    LOG("glCopyImageSubData\n");
    gl.CopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    struct GLContext* c = _bolt_context();
    if (srcTarget == GL_TEXTURE_2D && dstTarget == GL_TEXTURE_2D && srcLevel == 0 && dstLevel == 0) {
        struct GLTexture2D* src = _bolt_context_get_texture(c, srcName);
        struct GLTexture2D* dst = _bolt_context_get_texture(c, dstName);
        for (size_t i = 0; i < srcHeight; i += 1) {
            memcpy(dst->data + (dstY * dst->width * 4) + (dstX * 4), src->data + (srcY * src->width * 4) + (srcX * 4), srcWidth * 4);
        }
    }
    LOG("glCopyImageSubData end\n");
}

void _bolt_glEnableVertexAttribArray(unsigned int index) {
    LOG("glEnableVertexAttribArray\n");
    gl.EnableVertexAttribArray(index);
    struct GLContext* c = _bolt_context();
    c->bound_vao->attributes[index].enabled = 1;
    LOG("glEnableVertexAttribArray end\n");
}

void _bolt_glDisableVertexAttribArray(unsigned int index) {
    LOG("glDisableVertexAttribArray\n");
    gl.EnableVertexAttribArray(index);
    struct GLContext* c = _bolt_context();
    c->bound_vao->attributes[index].enabled = 0;
    LOG("glDisableVertexAttribArray end\n");
}

void* _bolt_glMapBufferRange(uint32_t target, intptr_t offset, uintptr_t length, uint32_t access) {
    LOG("glMapBufferRange\n");
    struct GLContext* c = _bolt_context();
    uint32_t binding_type = _bolt_binding_for_buffer(target);
    if (binding_type != -1) {
        int buffer_id;
        gl.GetIntegerv(binding_type, &buffer_id);
        struct GLArrayBuffer* buffer = _bolt_context_get_buffer(c, buffer_id);
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

uint8_t _bolt_glUnmapBuffer(uint32_t target) {
    LOG("glUnmapBuffer\n");
    struct GLContext* c = _bolt_context();
    uint32_t binding_type = _bolt_binding_for_buffer(target);
    if (binding_type != -1) {
        int buffer_id;
        gl.GetIntegerv(binding_type, &buffer_id);
        struct GLArrayBuffer* buffer = _bolt_context_get_buffer(c, buffer_id);
        free(buffer->mapping);
        buffer->mapping = NULL;
        LOG("glUnmapBuffer end (intercepted)\n");
        return 1;
    } else {
        uint8_t ret = gl.UnmapBuffer(target);
        LOG("glUnmapBuffer end (not intercepted)\n");
        return ret;
    }
}

void _bolt_glBufferStorage(uint32_t target, uintptr_t size, const void* data, uintptr_t flags) {
    LOG("glBufferStorage\n");
    gl.BufferStorage(target, size, data, flags);
    struct GLContext* c = _bolt_context();
    uint32_t binding_type = _bolt_binding_for_buffer(target);
    if (binding_type != -1) {
        int buffer_id;
        gl.GetIntegerv(binding_type, &buffer_id);
        void* buffer_content = malloc(size);
        if (data) memcpy(buffer_content, data, size);
        struct GLArrayBuffer* buffer = _bolt_context_get_buffer(c, buffer_id);
        free(buffer->data);
        buffer->data = buffer_content;
    }
    LOG("glBufferStorage end (%s)\n", binding_type == -1 ? "not intercepted" : "intercepted");
}

void _bolt_glFlushMappedBufferRange(uint32_t target, intptr_t offset, uintptr_t length) {
    LOG("glFlushMappedBufferRange\n");
    struct GLContext* c = _bolt_context();
    uint32_t binding_type = _bolt_binding_for_buffer(target);
    if (binding_type != -1) {
        int buffer_id;
        gl.GetIntegerv(binding_type, &buffer_id);
        struct GLArrayBuffer* buffer = _bolt_context_get_buffer(c, buffer_id);
        gl.BufferSubData(target, buffer->mapping_offset + offset, length, buffer->mapping + offset);
        memcpy(buffer->data + buffer->mapping_offset + offset, buffer->mapping + offset, length);
    } else {
        gl.FlushMappedBufferRange(target, offset, length);
    }
    LOG("glFlushMappedBufferRange end (%s)\n", binding_type == -1 ? "not intercepted" : "intercepted");
}

void _bolt_glActiveTexture(uint32_t texture) {
    LOG("glActiveTexture\n");
    gl.ActiveTexture(texture);
    struct GLContext* c = _bolt_context();
    c->active_texture = texture - GL_TEXTURE0;
    LOG("glActiveTexture end\n");
}

void _bolt_glMultiDrawElements(uint32_t mode, uint32_t* count, uint32_t type, const void** indices, size_t drawcount) {
    LOG("glMultiDrawElements\n");
    gl.MultiDrawElements(mode, count, type, indices, drawcount);
    for (size_t i = 0; i < drawcount; i += 1) {
        _bolt_gl_onDrawElements(mode, count[i], type, indices[i]);
    }
    LOG("glMultiDrawElements end\n");
}

void _bolt_glGenVertexArrays(uint32_t n, unsigned int* arrays) {
    LOG("glGenVertexArrays\n");
    gl.GenVertexArrays(n, arrays);
    struct GLContext* c = _bolt_context();
    _bolt_rwlock_lock_write(&c->vaos->rwlock);
    for (size_t i = 0; i < n; i += 1) {
        struct GLVertexArray* array = calloc(1, sizeof(struct GLVertexArray));
        array->id = arrays[i];
        hashmap_set(c->vaos->map, &array);
    }
    _bolt_rwlock_unlock_write(&c->vaos->rwlock);
    LOG("glGenVertexArrays end\n");
}

void _bolt_glDeleteVertexArrays(uint32_t n, const unsigned int* arrays) {
    LOG("glDeleteVertexArrays\n");
    gl.DeleteVertexArrays(n, arrays);
    struct GLContext* c = _bolt_context();
    for (size_t i = 0; i < n; i += 1) {
        struct GLVertexArray* vao = _bolt_context_get_vao(c, arrays[i]);
        free(vao);
    }
    LOG("glDeleteVertexArrays end\n");
}

void _bolt_glBindVertexArray(uint32_t array) {
    LOG("glBindVertexArray\n");
    gl.BindVertexArray(array);
    struct GLContext* c = _bolt_context();
    c->bound_vao = _bolt_context_get_vao(c, array);
    LOG("glBindVertexArray end\n");
}

void _bolt_glBlitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1, int dstY1, uint32_t mask, uint32_t filter) {
    LOG("glBlitFramebuffer\n");
    gl.BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    struct GLContext* c = _bolt_context();
    if (c->current_draw_framebuffer == 0 && c->game_view_framebuffer != c->current_read_framebuffer) {
        c->game_view_framebuffer = c->current_read_framebuffer;
        c->game_view_x = dstX0;
        c->game_view_y = dstY0;
        c->need_3d_tex = 1;
        printf("new game_view_framebuffer %u...\n", c->current_read_framebuffer);
    } else if (c->need_3d_tex) {
        int draw_tex;
        gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &draw_tex);
        if (draw_tex == c->game_view_tex) {
            c->need_3d_tex = 0;
            c->game_view_w = dstX1 - dstX0;
            c->game_view_h = dstY1 - dstY0;
            gl.GetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &c->target_3d_tex);
            printf("new target_3d_tex %i\n", c->target_3d_tex);
        }
    }
    LOG("glBlitFramebuffer end\n");
}

void* _bolt_gl_GetProcAddress(const char* name) {
#define PROC_ADDRESS_MAP(FUNC) if (!strcmp(name, "gl"#FUNC)) { return _bolt_gl##FUNC; }
    PROC_ADDRESS_MAP(CreateProgram)
    PROC_ADDRESS_MAP(BindAttribLocation)
    PROC_ADDRESS_MAP(LinkProgram)
    PROC_ADDRESS_MAP(UseProgram)
    PROC_ADDRESS_MAP(TexStorage2D)
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
#undef PROC_ADDRESS_MAP
    return NULL;
}

void _bolt_gl_onSwapBuffers(const struct GLLibFunctions* libgl, uint32_t window_width, uint32_t window_height) {
    struct GLContext* c = _bolt_context();
    if (_bolt_plugin_is_inited()) {
        _bolt_plugin_handle_swapbuffers(NULL);
        int texture_unit;
        gl.GetIntegerv(GL_ACTIVE_TEXTURE, &texture_unit);
        texture_unit -= GL_TEXTURE0;
        libgl->ClearColor(0.0, 0.0, 0.0, 0.0);
        if (framebuffer2d_width != window_width || framebuffer2d_height != window_height) {
            unsigned int new_fbid, new_rbid;
            gl.GenFramebuffers(1, &new_fbid);
            libgl->GenTextures(1, &new_rbid);
            gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, new_fbid);
            libgl->BindTexture(GL_TEXTURE_2D, new_rbid);
            gl.TexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, window_width, window_height);
            gl.FramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, new_rbid, 0);
            libgl->Clear(GL_COLOR_BUFFER_BIT);
            if (framebuffer2d_inited) {
                gl.BindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer2d);
                gl.BlitFramebuffer(0, 0, framebuffer2d_width, framebuffer2d_height, 0, 0, framebuffer2d_width, framebuffer2d_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                gl.DeleteFramebuffers(1, &framebuffer2d);
                libgl->DeleteTextures(1, &renderbuffer2d);
            } else {
                framebuffer2d_inited = 1;
            }
            framebuffer2d = new_fbid;
            renderbuffer2d = new_rbid;
            framebuffer2d_width = window_width;
            framebuffer2d_height = window_height;
        }
        if (framebuffer2d_inited && c->bound_program) {
            int array_binding;
            gl.GetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_binding);
            gl.UseProgram(program_direct);
            libgl->BindTexture(GL_TEXTURE_2D, renderbuffer2d);
            gl.BindBuffer(GL_ARRAY_BUFFER, buffer_vertices_square);
            gl.EnableVertexAttribArray(0);
            gl.VertexAttribPointer(0, 2, GL_FLOAT, 0, 2 * sizeof(float), NULL);
            gl.Uniform1i(program_direct_sampler, texture_unit);
            gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            libgl->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer2d);
            libgl->Clear(GL_COLOR_BUFFER_BIT);
            gl.UseProgram(c->bound_program->id);
            gl.BindBuffer(GL_ARRAY_BUFFER, array_binding);
        }
        const struct GLTexture2D* original_tex = c->texture_units[texture_unit];
        libgl->BindTexture(GL_TEXTURE_2D, original_tex ? original_tex->id : 0);
        gl.BindFramebuffer(GL_READ_FRAMEBUFFER, c->current_read_framebuffer);
        gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, c->current_draw_framebuffer);
    }
}

void _bolt_gl_onCreateContext(void* context, void* shared_context, void* (*GetProcAddress)(const char*)) {
    if (!shared_context) {
        if (egl_init_count == 0) {
            _bolt_gl_load(GetProcAddress);
        } else {
            egl_main_context = (uintptr_t)context;
            egl_main_context_makecurrent_pending = 1;
        }
        egl_init_count += 1;
    }
    _bolt_create_context(context, shared_context);
}

void _bolt_gl_onMakeCurrent(void* context) {
    if (current_context) {
        current_context->is_attached = 0;
        if (current_context->deferred_destroy) _bolt_destroy_context(current_context);
    }
    if (!context) {
        current_context = 0;
        return;
    }
    for (size_t i = 0; i < CONTEXTS_CAPACITY; i += 1) {
        struct GLContext* ptr = &contexts[i];
        if (ptr->id == (uintptr_t)context) {
            current_context = ptr;
            break;
        }
    }
    if (egl_main_context_makecurrent_pending && (uintptr_t)context == egl_main_context) {
        egl_main_context_makecurrent_pending = 0;
        _bolt_gl_init();
        _bolt_plugin_init();
        _bolt_plugin_add(lua_test_plugin);
    }
}

uint8_t _bolt_gl_onDestroyContext(void* context, const struct GLLibFunctions* libgl) {
    uint8_t do_destroy_main = 0;
    if ((uintptr_t)context != egl_main_context) {
        _bolt_destroy_context(context);
    } else {
        egl_main_context_destroy_pending = 1;
        if (framebuffer2d_inited) {
            gl.DeleteFramebuffers(1, &framebuffer2d);
            libgl->DeleteTextures(1, &renderbuffer2d);
            framebuffer2d_inited = 0;
        }
    }
    if (_bolt_context_count() == 1 && egl_main_context_destroy_pending) {
        do_destroy_main = 1;
        if (egl_init_count > 1) _bolt_plugin_close();
    }
    if (do_destroy_main) {
        _bolt_destroy_context((void*)egl_main_context);
    }
    return do_destroy_main;
}

void _bolt_gl_onGenTextures(uint32_t n, unsigned int* textures) {
    struct GLContext* c = _bolt_context();
    _bolt_rwlock_lock_write(&c->textures->rwlock);
    for (size_t i = 0; i < n; i += 1) {
        struct GLTexture2D* tex = calloc(1, sizeof(struct GLTexture2D));
        tex->id = textures[i];
        tex->is_minimap_tex_big = 0;
        tex->is_minimap_tex_small = 0;
        hashmap_set(c->textures->map, &tex);
    }
    _bolt_rwlock_unlock_write(&c->textures->rwlock);
}

void _bolt_gl_onDrawElements(uint32_t mode, unsigned int count, uint32_t type, const void* indices_offset) {
    struct GLContext* c = _bolt_context();
    struct GLAttrBinding* attributes = c->bound_vao->attributes;
    int element_binding;
    gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_binding);
    struct GLArrayBuffer* element_buffer = _bolt_context_get_buffer(c, element_binding);
    const unsigned short* indices = (unsigned short*)(element_buffer->data + (uintptr_t)indices_offset);
    if (type == GL_UNSIGNED_SHORT && mode == GL_TRIANGLES && count > 0 && c->bound_program->is_2d && !c->bound_program->is_minimap) {
        int diffuse_map;
        gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uDiffuseMap, &diffuse_map);
        float projection_matrix[16];
        gl.GetUniformfv(c->bound_program->id, c->bound_program->loc_uProjectionMatrix, projection_matrix);
        int draw_tex;
        gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &draw_tex);
        struct GLTexture2D* tex = c->texture_units[diffuse_map];
        struct GLTexture2D* tex_target = _bolt_context_get_texture(c, draw_tex);

        if (tex->is_minimap_tex_big) {
            tex_target->is_minimap_tex_small = 1;
            if (count == 6) {
                // get XY and UV of first two vertices
                const struct GLAttrBinding* tex_uv = &attributes[c->bound_program->loc_aTextureUV];
                const struct GLAttrBinding* position_2d = &attributes[c->bound_program->loc_aVertexPosition2D];
                int pos0[2];
                int pos1[2];
                float uv0[2];
                float uv1[2];
                _bolt_get_attr_binding_int(c, position_2d, indices[0], 2, pos0);
                _bolt_get_attr_binding_int(c, position_2d, indices[1], 2, pos1);
                _bolt_get_attr_binding(c, tex_uv, indices[0], 2, uv0);
                _bolt_get_attr_binding(c, tex_uv, indices[1], 2, uv1);
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

                struct RenderMinimapEvent render;
                render.angle = map_angle_rads;
                render.scale = dist_ratio;
                render.x = tex->minimap_center_x + (64.0 * (cx - (double)(tex->width >> 1)));
                render.y = tex->minimap_center_y + (64.0 * (cy - (double)(tex->height >> 1)));
                _bolt_plugin_handle_minimap(&render);
            }
        } else {
            struct GLPluginDrawElementsVertex2DUserData vertex_userdata;
            vertex_userdata.c = c;
            vertex_userdata.indices = (unsigned short*)(element_buffer->data + (uintptr_t)indices_offset);
            vertex_userdata.atlas = c->texture_units[diffuse_map];
            vertex_userdata.position = &attributes[c->bound_program->loc_aVertexPosition2D];
            vertex_userdata.atlas_min = &attributes[c->bound_program->loc_aTextureUVAtlasMin];
            vertex_userdata.atlas_size = &attributes[c->bound_program->loc_aTextureUVAtlasExtents];
            vertex_userdata.tex_uv = &attributes[c->bound_program->loc_aTextureUV];
            vertex_userdata.colour = &attributes[c->bound_program->loc_aVertexColour];

            struct GLPluginTextureUserData tex_userdata;
            tex_userdata.tex = tex;

            struct RenderBatch2D batch;
            batch.screen_width = roundf(2.0 / projection_matrix[0]);
            batch.screen_height = roundf(2.0 / projection_matrix[5]);
            batch.index_count = count;
            batch.vertices_per_icon = 6;
            batch.is_minimap = tex_target && tex_target->is_minimap_tex_small;
            batch.vertex_functions.userdata = &vertex_userdata;
            batch.vertex_functions.xy = _bolt_gl_plugin_drawelements_vertex2d_xy;
            batch.vertex_functions.atlas_xy = _bolt_gl_plugin_drawelements_vertex2d_atlas_xy;
            batch.vertex_functions.atlas_wh = _bolt_gl_plugin_drawelements_vertex2d_atlas_wh;
            batch.vertex_functions.uv = _bolt_gl_plugin_drawelements_vertex2d_uv;
            batch.vertex_functions.colour = _bolt_gl_plugin_drawelements_vertex2d_colour;
            batch.texture_functions.userdata = &tex_userdata;
            batch.texture_functions.id = _bolt_gl_plugin_texture_id;
            batch.texture_functions.size = _bolt_gl_plugin_texture_size;
            batch.texture_functions.compare = _bolt_gl_plugin_texture_compare;

            _bolt_plugin_handle_2d(&batch);
        }
    }
    //if (type == GL_UNSIGNED_SHORT && mode == GL_TRIANGLES && c->bound_program->is_3d && count == 2370) {
    //    int draw_tex;
    //    gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &draw_tex);
    //    if (draw_tex == c->target_3d_tex) {
    //        const struct GLAttrBinding* xyxz = &attributes[c->bound_program->loc_aMaterialSettingsSlotXY_TilePositionXZ];
    //        const struct GLAttrBinding* vertex_xyz_bone = &attributes[c->bound_program->loc_aVertexPosition_BoneLabel];
    //        if (xyxz->enabled && vertex_xyz_bone->enabled) {
    //            int atlas;
    //            int settings_atlas;
    //            float atlas_meta[4];
    //            int32_t settingsxy_tilexz[4];
    //            int32_t vertexpos_bonelabel[4];
    //            gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uTextureAtlas, &atlas);
    //            gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_uTextureAtlasSettings, &settings_atlas);
    //            gl.GetUniformfv(c->bound_program->id, c->bound_program->loc_uAtlasMeta, atlas_meta);
    //            const struct GLTexture2D* tex = c->texture_units[atlas];
    //            const struct GLTexture2D* tex_settings = c->texture_units[settings_atlas];
    //            int32_t current_slot_x, current_slot_y;
    //            uint32_t atlas_scale = (uint32_t)roundf(atlas_meta[1]);
    //            int32_t x_min, x_max, y_min, y_max, z_min, z_max;
    //            uint32_t portal_found = 0;
    //            for (size_t i = 0; i < count; i += 1) {
    //                if (!_bolt_get_attr_binding_int(c, xyxz, indices[i], 4, settingsxy_tilexz)) break;
    //                if (!_bolt_get_attr_binding_int(c, vertex_xyz_bone, indices[i], 4, vertexpos_bonelabel)) break;
    //                if (i != 0 && current_slot_x == settingsxy_tilexz[0] && current_slot_y == settingsxy_tilexz[1]) {
    //                    if (portal_found) {
    //                        if (x_min > vertexpos_bonelabel[0]) x_min = vertexpos_bonelabel[0];
    //                        if (x_max < vertexpos_bonelabel[0]) x_max = vertexpos_bonelabel[0];
    //                        if (y_min > vertexpos_bonelabel[1]) y_min = vertexpos_bonelabel[1];
    //                        if (y_max < vertexpos_bonelabel[1]) y_max = vertexpos_bonelabel[1];
    //                        if (z_min > vertexpos_bonelabel[2]) z_min = vertexpos_bonelabel[2];
    //                        if (z_max < vertexpos_bonelabel[2]) z_max = vertexpos_bonelabel[2];
    //                    }
    //                } else {
    //                    if (portal_found) {
    //                        printf("portal on screen");
    //                        portal_found = 0;
    //                    }
    //        
    //                    // this is pretty wild
    //                    current_slot_x = settingsxy_tilexz[0];
    //                    current_slot_y = settingsxy_tilexz[1];
    //                    const uint8_t* settings_ptr = tex_settings->data + (current_slot_y * tex_settings->width * 4 * 4) + (current_slot_x * 3 * 4);
    //                    const uint8_t bitmask = *(settings_ptr + (tex_settings->width * 2 * 4) + 7);
    //                    const uint32_t tex_x = (uint32_t)(*settings_ptr + (bitmask & 1 ? 256 : 0)) * atlas_scale;
    //                    const uint32_t tex_y = (uint32_t)(*(settings_ptr + 1) + (bitmask & 2 ? 256 : 0)) * atlas_scale;
    //                    const uint32_t tex_wh = (uint32_t)*(settings_ptr + 8) * atlas_scale;
    //                    if (tex_x + tex_wh >= tex->width || tex_y + tex_wh >= tex->height) continue;
    //        
    //                    if (memcmp(tex->data + (((tex_y * tex->width) + tex_x) * 4), portal_pixel_row, 512 * 4) == 0) {
    //                        portal_found = 1;
    //                        x_min = vertexpos_bonelabel[0];
    //                        x_max = vertexpos_bonelabel[0];
    //                        y_min = vertexpos_bonelabel[1];
    //                        y_max = vertexpos_bonelabel[1];
    //                        z_min = vertexpos_bonelabel[2];
    //                        z_max = vertexpos_bonelabel[2];
    //                    }
    //                }
    //            }
    //        }
    //    }
    //}
}

void _bolt_gl_onDrawArrays(uint32_t mode, int first, unsigned int count) {
    struct GLContext* c = _bolt_context();
    if (c->bound_program->is_minimap) {
        int ubo_binding, ubo_view_index, draw_tex;
        gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &draw_tex);
        gl.GetActiveUniformBlockiv(c->bound_program->id, c->bound_program->block_index_ViewTransforms, GL_UNIFORM_BLOCK_BINDING, &ubo_binding);
        gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ubo_binding, &ubo_view_index);
        const float* view_proj_matrix = (float*)(_bolt_context_get_buffer(c, ubo_view_index)->data + c->bound_program->offset_uCameraPosition);
        struct GLTexture2D* tex = _bolt_context_get_texture(c, draw_tex);
        tex->is_minimap_tex_big = 1;
        tex->minimap_center_x = view_proj_matrix[0];
        tex->minimap_center_y = view_proj_matrix[2];
    } else if (mode == GL_TRIANGLE_STRIP && count == 4) {
        if (c->bound_program->loc_sSceneHDRTex != -1) {
            int game_view_tex;
            gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_sSceneHDRTex, &game_view_tex);
            if (c->current_draw_framebuffer == 0 && c->game_view_tex_front != c->texture_units[game_view_tex]->id) {
                c->game_view_tex = c->texture_units[game_view_tex]->id;
                c->game_view_tex_front = c->game_view_tex;
                c->current_draw_framebuffer = -1;
                c->game_view_x = 0;
                c->game_view_y = 0;
                c->need_3d_tex = 1;
                printf("new direct game_view_tex %u...\n", c->game_view_tex);
            } else if (c->need_3d_tex == 1 && c->game_view_framebuffer == c->current_draw_framebuffer) {
                c->game_view_tex = c->texture_units[game_view_tex]->id;
                c->game_view_tex_front = c->game_view_tex;
                printf("new game_view_tex %u...\n", c->game_view_tex);
            }
        } else if (c->bound_program->loc_sSourceTex != -1) {
            if (c->need_3d_tex == 1) {
                int draw_tex;
                gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &draw_tex);
                if (draw_tex == c->game_view_tex) {
                    int game_view_tex;
                    gl.GetUniformiv(c->bound_program->id, c->bound_program->loc_sSourceTex, &game_view_tex);
                    c->game_view_tex = c->texture_units[game_view_tex]->id;
                    printf("updated direct game_view_tex to %u...\n", c->game_view_tex);
                    c->need_3d_tex += 1;
                }
            }

        }
    }
}

void _bolt_gl_onBindTexture(uint32_t target, unsigned int texture) {
    if (target == GL_TEXTURE_2D) {
        struct GLContext* c = _bolt_context();
        c->texture_units[c->active_texture] = _bolt_context_get_texture(c, texture);
    }
}

void _bolt_gl_onTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, unsigned int width, unsigned int height, uint32_t format, uint32_t type, const void* pixels) {
    struct GLContext* c = _bolt_context();
    if (level == 0 && format == GL_RGBA) {
        if (target == GL_TEXTURE_2D && format == GL_RGBA) {
            struct GLTexture2D* tex = c->texture_units[c->active_texture];
            if (tex && !(xoffset < 0 || yoffset < 0 || xoffset + width > tex->width || yoffset + height > tex->height)) {
                for (unsigned int y = 0; y < height; y += 1) {
                    unsigned char* dest_ptr = tex->data + ((tex->width * (y + yoffset)) + xoffset) * 4;
                    const void* src_ptr = pixels + (width * y * 4);
                    memcpy(dest_ptr, src_ptr, width * 4);
                }
            }
        }
    }
}

void _bolt_gl_onDeleteTextures(unsigned int n, const unsigned int* textures) {
    struct GLContext* c = _bolt_context();
    for (unsigned int i = 0; i < n; i += 1) {
        struct GLTexture2D* texture = _bolt_context_get_texture(c, textures[i]);
        free(texture->data);
        free(texture);
    }
}

void _bolt_gl_onClear(uint32_t mask) {
    // TODO: check for clearing the big minimap texture
}

void _bolt_gl_plugin_drawelements_vertex2d_xy(size_t index, void* userdata, int32_t* out) {
    struct GLPluginDrawElementsVertex2DUserData* data = userdata;
    if (!_bolt_get_attr_binding_int(data->c, data->position, data->indices[index], 2, out)) {
        float pos[2];
        _bolt_get_attr_binding(data->c, data->position, data->indices[index], 2, pos);
        out[0] = (int32_t)roundf(pos[0]);
        out[1] = (int32_t)roundf(pos[1]);
    }
}

void _bolt_gl_plugin_drawelements_vertex2d_atlas_xy(size_t index, void* userdata, int32_t* out) {
    struct GLPluginDrawElementsVertex2DUserData* data = userdata;
    float xy[2];
    _bolt_get_attr_binding(data->c, data->atlas_min, data->indices[index], 2, xy);
    out[0] = (int32_t)roundf(xy[0] * data->atlas->width);
    out[1] = (int32_t)roundf(xy[1] * data->atlas->height);
}

void _bolt_gl_plugin_drawelements_vertex2d_atlas_wh(size_t index, void* userdata, int32_t* out) {
    struct GLPluginDrawElementsVertex2DUserData* data = userdata;
    float wh[2];
    _bolt_get_attr_binding(data->c, data->atlas_size, data->indices[index], 2, wh);
    // these are negative for some reason
    out[0] = -(int32_t)roundf(wh[0] * data->atlas->width);
    out[1] = -(int32_t)roundf(wh[1] * data->atlas->height);
}

void _bolt_gl_plugin_drawelements_vertex2d_uv(size_t index, void* userdata, double* out) {
    struct GLPluginDrawElementsVertex2DUserData* data = userdata;
    float uv[2];
    _bolt_get_attr_binding(data->c, data->tex_uv, data->indices[index], 2, uv);
    out[0] = (double)uv[0];
    out[1] = (double)uv[1];
}

void _bolt_gl_plugin_drawelements_vertex2d_colour(size_t index, void* userdata, double* out) {
    struct GLPluginDrawElementsVertex2DUserData* data = userdata;
    float colour[4];
    _bolt_get_attr_binding(data->c, data->colour, data->indices[index], 4, colour);
    // these are ABGR for some reason
    out[0] = (double)colour[3];
    out[1] = (double)colour[2];
    out[2] = (double)colour[1];
    out[3] = (double)colour[0];
}

size_t _bolt_gl_plugin_texture_id(void* userdata) {
    const struct GLPluginTextureUserData* data = userdata;
    return data->tex->id;
}

void _bolt_gl_plugin_texture_size(void* userdata, size_t* out) {
    const struct GLPluginTextureUserData* data = userdata;
    out[0] = data->tex->width;
    out[1] = data->tex->height;
}

uint8_t _bolt_gl_plugin_texture_compare(void* userdata, size_t x, size_t y, size_t len, const unsigned char* data) {
    const struct GLPluginTextureUserData* data_ = userdata;
    const struct GLTexture2D* tex = data_->tex;
    const size_t start_offset = (tex->width * y * 4) + (x * 4);
    if (start_offset + len > tex->width * tex->height * 4) {
        printf(
            "warning: out-of-bounds texture compare attempt: tried to read %lu bytes at %lu,%lu of texture id=%u w,h=%u,%u\n",
            len, x, y, tex->id, tex->width, tex->height
        );
        return 0;
    }
    return !memcmp(tex->data + start_offset, data, len);
}
