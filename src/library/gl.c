#include "gl.h"

#include <stdio.h>
#include <string.h>

void _bolt_glcontext_init(struct GLContext*, void*, void*);
void _bolt_glcontext_free(struct GLContext*);

#define PTR_LIST_CAPACITY 256 * 256
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

uint8_t _bolt_get_attr_binding(struct GLContext* c, const struct GLAttrBinding* binding, size_t index, size_t num_out, float* out) {
    struct GLArrayBuffer* buffer = c->buffers[binding->buffer];
    if (!buffer || !buffer->data) return 0;
    uintptr_t buf_offset = binding->offset + (binding->stride * index);

    const void* ptr = buffer->data + buf_offset;
    if (!binding->normalise) {
        switch (binding->type) {
            case GL_FLOAT:
                memcpy(out, ptr, num_out * sizeof(float));
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
    if (shared) {
        context->programs = shared->programs;
        context->buffers = shared->buffers;
        context->textures = shared->textures;
    } else {
        context->is_shared_owner = 1;
        context->programs = calloc(PTR_LIST_CAPACITY, sizeof(void*));
        context->buffers = calloc(PTR_LIST_CAPACITY, sizeof(void*));
        context->textures = calloc(PTR_LIST_CAPACITY, sizeof(void*));
        context->programs = context->programs;
        context->buffers = context->buffers;
        context->textures = context->textures;
    }
}

void _bolt_glcontext_free(struct GLContext* context) {
    if (context->is_shared_owner) {
        free(context->programs);
        free(context->buffers);
        free(context->textures);
    }
}
