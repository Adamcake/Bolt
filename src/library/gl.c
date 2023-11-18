#include "gl.h"

#include <stdio.h>
#include <string.h>

void _bolt_glcontext_init(struct GLContext*, void*, void*);
void _bolt_glcontext_free(struct GLContext*);

struct GLList contexts = {0};
_Thread_local struct GLContext* current_context = NULL;

#define LIST_GROWTH_STEP 256
#define CONTEXTS_CAPACITY 64 // not growable so we just have to hard-code a number and hope it's enough forever

#define MAKE_GETTERS(STRUCT, NAME, ID_TYPE) \
struct STRUCT* _bolt_find_##NAME(struct GLList* list, ID_TYPE id) { \
    if (id == 0) return NULL; \
    for (size_t i = 0; i < list->capacity; i += 1) { \
        struct STRUCT* ptr = &((struct STRUCT*)(list->data))[i]; \
        if (ptr->id == id) { \
            return ptr; \
        } \
    } \
    return NULL; \
} \
struct STRUCT* _bolt_get_##NAME(struct GLList* list, ID_TYPE id) { \
    if (id == 0) return NULL; \
    struct STRUCT* first_zero = NULL; \
    for (size_t i = 0; i < list->capacity; i += 1) { \
        struct STRUCT* ptr = &((struct STRUCT*)(list->data))[i]; \
        if (ptr->id == id) { \
            return ptr; \
        } else if (ptr->id == 0) { \
            first_zero = ptr; \
        } \
    } \
    if (first_zero) { \
        first_zero->id = id; \
        return first_zero; \
    } else { \
        size_t old_capacity = list->capacity; \
        list->capacity += LIST_GROWTH_STEP; \
        struct STRUCT* new_ptr = calloc(list->capacity, sizeof(struct STRUCT)); \
        memcpy(new_ptr, list->data, old_capacity * sizeof(struct STRUCT)); \
        new_ptr[old_capacity].id = id; \
        free(list->data); \
        list->data = new_ptr; \
        return &new_ptr[old_capacity]; \
    } \
}
MAKE_GETTERS(GLArrayBuffer, array, unsigned int)
MAKE_GETTERS(GLProgram, program, unsigned int)
MAKE_GETTERS(GLTexture2D, texture, unsigned int)

struct GLContext* _bolt_context() {
    return current_context;
}

void _bolt_create_context(void* egl_context, void* shared) {
    if (!contexts.capacity) {
        contexts.capacity = CONTEXTS_CAPACITY;
        contexts.data = malloc(CONTEXTS_CAPACITY * sizeof(struct GLContext));
    }
    for (size_t i = 0; i < contexts.capacity; i += 1) {
        struct GLContext* ptr = &((struct GLContext*)(contexts.data))[i];
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
    if (!contexts.capacity) {
        contexts.capacity = CONTEXTS_CAPACITY;
        contexts.data = malloc(CONTEXTS_CAPACITY * sizeof(struct GLContext));
    }
    for (size_t i = 0; i < contexts.capacity; i += 1) {
        struct GLContext* ptr = &((struct GLContext*)(contexts.data))[i];
        if (ptr->id == (uintptr_t)egl_context) {
            current_context = ptr;
            return;
        }
    }
}

void _bolt_destroy_context(void* egl_context) {
    for (size_t i = 0; i < contexts.capacity; i += 1) {
        struct GLContext* ptr = &((struct GLContext*)(contexts.data))[i];
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

struct GLArrayBuffer* _bolt_get_buffer_internal(struct GLContext* context, uint32_t target, uint8_t create) {
    switch (target) {
        case GL_ARRAY_BUFFER:
            if (create) return _bolt_get_array(context->shared_arrays, context->arrays.current);
            else return _bolt_find_array(context->shared_arrays, context->arrays.current);
        case GL_ELEMENT_ARRAY_BUFFER:
            if (create) return _bolt_get_array(context->shared_element_arrays, context->element_arrays.current);
            else return _bolt_find_array(context->shared_element_arrays, context->element_arrays.current);
        default:
            return NULL;
    }
}

// gets pointer to the current active buffer or makes a new one if one doesn't already exist
// can still return NULL if `target` is a value irrelevant to this program, or there is no space left for buffers
struct GLArrayBuffer* _bolt_context_get_buffer(struct GLContext* context, uint32_t target) {
    return _bolt_get_buffer_internal(context, target, 1);
}

// gets a pointer to the current active buffer of the given type, or returns NULL if there isn't one
struct GLArrayBuffer* _bolt_context_find_buffer(struct GLContext* context, uint32_t target) {
    return _bolt_get_buffer_internal(context, target, 0);
}

// gets a pointer to the specific named buffer, or returns NULL if it isn't tracked
// note: there's no "get" equivalent of this function because it wouldn't know which list to create it in
struct GLArrayBuffer* _bolt_context_find_named_buffer(struct GLContext* context, unsigned int buffer) {
    struct GLArrayBuffer* ret = _bolt_find_array(context->shared_arrays, buffer);
    if (!ret) _bolt_find_array(context->shared_element_arrays, buffer);
    return ret;
}

void _bolt_set_attr_binding(struct GLAttrBinding* binding, const void* buffer, const void* offset, unsigned int stride, uint32_t type, uint8_t normalise) {
    binding->ptr = buffer + (uintptr_t)offset;
    binding->stride = stride;
    binding->normalise = normalise;
    binding->type = type;
}

void _bolt_get_attr_binding(const struct GLAttrBinding* binding, size_t index, size_t num_out, float* out) {
    if (binding->ptr == 0) {
        // I don't think this is meant to happen, but in this game it does
        memset(out, 0, num_out * sizeof(float));
        return;
    }

    const void* ptr = binding->ptr + (binding->stride * index);
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
}

void _bolt_glcontext_init(struct GLContext* context, void* egl_context, void* egl_shared) {
    struct GLContext* shared = NULL;
    if (egl_shared) {
        for (size_t i = 0; i < contexts.capacity; i += 1) {
            struct GLContext* ptr = &((struct GLContext*)(contexts.data))[i];
            if (ptr->id == (uintptr_t)egl_shared) {
                shared = ptr;
                break;
            }
        }
    }
    memset(context, 0, sizeof(*context));
    context->id = (uintptr_t)egl_context;
    if (shared) {
        context->uniform_buffer = shared->uniform_buffer;
        context->shared_programs = &shared->programs;
        context->shared_arrays = &shared->arrays;
        context->shared_element_arrays = &shared->element_arrays;
        context->shared_textures = &shared->textures;
    } else {
        context->is_shared_owner = 1;
        context->uniform_buffer = malloc(16384); // seems to be the actual size on GPUs
        context->shared_programs = &context->programs;
        context->shared_arrays = &context->arrays;
        context->shared_element_arrays = &context->element_arrays;
        context->shared_textures = &context->textures;
    }
}

void _bolt_glcontext_free(struct GLContext* context) {
    if (context->is_shared_owner) {
        free(context->uniform_buffer);
        free(context->arrays.data);
        free(context->element_arrays.data);
        free(context->programs.data);
        free(context->textures.data);
    }
}

void _bolt_context_destroy_buffers(struct GLContext* context, unsigned int n, const unsigned int* list) {
    for (size_t i = 0; i < n; i += 1) {
        struct GLArrayBuffer* array = _bolt_find_array(context->shared_arrays, list[i]);
        if (!array) array = _bolt_find_array(context->shared_element_arrays, list[i]);
        if (!array) continue;
        free(array->data);
        array->data = NULL;
        array->id = 0;
        array->len = 0;
    }
}

void _bolt_context_destroy_textures(struct GLContext* context, unsigned int n, const unsigned int* list) {
    for (size_t i = 0; i < n; i += 1) {
        struct GLTexture2D* tex = _bolt_find_texture(context->shared_textures, list[i]);
        if (tex) {
            free(tex->data);
            tex->data = 0;
            tex->width = 0;
            tex->height = 0;
        }
    }
}
