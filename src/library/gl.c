#include "gl.h"
#include "rwlock.h"

#include <stdio.h>
#include <string.h>

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

void _bolt_make_context_current(void* egl_context) {
    if (current_context) {
        current_context->is_attached = 0;
        if (current_context->deferred_destroy) _bolt_destroy_context(current_context);
    }
    if (!egl_context) {
        current_context = 0;
        return;
    }
    for (size_t i = 0; i < CONTEXTS_CAPACITY; i += 1) {
        struct GLContext* ptr = &contexts[i];
        if (ptr->id == (uintptr_t)egl_context) {
            current_context = ptr;
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

void _bolt_set_attr_binding(struct GLAttrBinding* binding, unsigned int buffer, int size, const void* offset, unsigned int stride, uint32_t type, uint8_t normalise) {
    binding->buffer = buffer;
    binding->offset = (uintptr_t)offset;
    binding->size = size;
    binding->stride = stride;
    binding->normalise = normalise;
    binding->type = type;
}

float _bolt_f16_to_f32(uint16_t bits) {
    const uint16_t bits_exp_component = (bits & 0b0111110000000000);
    if (bits_exp_component == 0) { printf("asd"); return 0.0f; } // truncate subnormals to 0
    const uint32_t sign_component = (bits & 0b1000000000000000) << 16;
    const uint32_t exponent = (bits_exp_component >> 10) + (127 - 15); // adjust exp bias
    const uint32_t mantissa = bits & 0b0000001111111111;
    const union { uint32_t b; float f; } u = {.b = sign_component | (exponent << 23) | (mantissa << 13)};
    return u.f;
}

uint8_t _bolt_get_attr_binding(struct GLContext* c, const struct GLAttrBinding* binding, size_t index, size_t num_out, float* out) {
    struct GLArrayBuffer* buffer = _bolt_context_get_buffer(c, binding->buffer);
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

uint8_t _bolt_get_attr_binding_int(struct GLContext* c, const struct GLAttrBinding* binding, size_t index, size_t num_out, uint32_t* out) {
    struct GLArrayBuffer* buffer = _bolt_context_get_buffer(c, binding->buffer);
    if (!buffer || !buffer->data) return 0;
    uintptr_t buf_offset = binding->offset + (binding->stride * index);

    const void* ptr = buffer->data + buf_offset;
    if (!binding->normalise) {
        switch (binding->type) {
            case GL_UNSIGNED_BYTE:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (uint32_t)*(uint8_t*)(ptr + i);
                break;
            case GL_UNSIGNED_SHORT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (uint32_t)*(uint16_t*)(ptr + (i * 2));
                break;
            case GL_UNSIGNED_INT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (uint32_t)*(uint32_t*)(ptr + (i * 4));
                break;
            case GL_BYTE:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (uint32_t)*(int8_t*)(ptr + i);
                break;
            case GL_SHORT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (uint32_t)*(int16_t*)(ptr + (i * 2));
                break;
            case GL_INT:
                for (size_t i = 0; i < num_out; i += 1) out[i] = (uint32_t)*(int32_t*)(ptr + (i * 4));
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


void _bolt_mul_vec4_mat4(const float x, const float y, const float z, const float w, const float* mat4, float* out_vec4) {
    out_vec4[0] = (mat4[0] * x) + (mat4[4] * y) + (mat4[8]  * z) + (mat4[12] * w);
    out_vec4[1] = (mat4[1] * x) + (mat4[5] * y) + (mat4[9]  * z) + (mat4[13] * w);
    out_vec4[2] = (mat4[2] * x) + (mat4[6] * y) + (mat4[10] * z) + (mat4[14] * w);
    out_vec4[3] = (mat4[3] * x) + (mat4[7] * y) + (mat4[11] * z) + (mat4[15] * w);
}
