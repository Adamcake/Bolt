#define _GNU_SOURCE
#include <link.h>
#include <unistd.h>
#undef _GNU_SOURCE

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gl.h"

// comment or uncomment this to enable verbose logging of hooks in this file
//#define VERBOSE

// don't change this part, change the line above this instead
#if defined(VERBOSE)
#define LOG printf
#else
#define LOG(...)
#endif

// note: this is currently always triggered by single-threaded dlopen calls so no locking necessary
uint8_t inited = 0;
#define INIT() if (!inited) _bolt_init_functions();

pthread_mutex_t egl_lock;

const char* libc_name = "libc.so.6";
const char* libegl_name = "libEGL.so.1";
const char* libgl_name = "libGL.so.1";
const char* libxcb_name = "libxcb.so.1";

void* libc_addr = 0;
void* libegl_addr = 0;
void* libgl_addr = 0;
void* libxcb_addr = 0;

void* (*real_dlopen)(const char*, int) = NULL;
void* (*real_dlsym)(void*, const char*) = NULL;
void* (*real_dlvsym)(void*, const char*, const char*) = NULL;
int (*real_dlclose)(void*) = NULL;

void* (*real_eglGetProcAddress)(const char*) = NULL;
unsigned int (*real_eglSwapBuffers)(void*, void*) = NULL;
unsigned int (*real_eglMakeCurrent)(void*, void*, void*, void*) = NULL;
unsigned int (*real_eglDestroyContext)(void*, void*) = NULL;
unsigned int (*real_eglInitialize)(void*, void*, void*) = NULL;
void* (*real_eglCreateContext)(void*, void*, void*, const void*) = NULL;
unsigned int (*real_eglTerminate)(void*) = NULL;

void* (*real_xcb_poll_for_event)(void*) = NULL;
void* (*real_xcb_wait_for_event)(void*) = NULL;

/* opengl functions that are usually queried by eglGetProcAddress */
unsigned int (*real_glCreateProgram)() = NULL;
void (*real_glBindAttribLocation)(unsigned int, unsigned int, const char*) = NULL;
int (*real_glGetUniformLocation)(unsigned int, const char*) = NULL;
void (*real_glGetUniformfv)(unsigned int, int, float*) = NULL;
void (*real_glGetUniformiv)(unsigned int, int, int*) = NULL;
void (*real_glLinkProgram)(unsigned int) = NULL;
void (*real_glUseProgram)(unsigned int) = NULL;
void (*real_glTexStorage2D)(uint32_t, int, uint32_t, unsigned int, unsigned int) = NULL;
void (*real_glUniform1i)(int, int);
void (*real_glUniformMatrix4fv)(int, unsigned int, uint8_t, const float*) = NULL;
void (*real_glVertexAttribPointer)(unsigned int, int, uint32_t, uint8_t, unsigned int, const void*) = NULL;
void (*real_glGenBuffers)(uint32_t, unsigned int*) = NULL;
void (*real_glBindBuffer)(uint32_t, unsigned int) = NULL;
void (*real_glBufferData)(uint32_t, uintptr_t, const void*, uint32_t) = NULL;
void (*real_glDeleteBuffers)(unsigned int, const unsigned int*) = NULL;
void (*real_glBindFramebuffer)(uint32_t, unsigned int) = NULL;
void (*real_glFramebufferTextureLayer)(uint32_t, uint32_t, unsigned int, int, int) = NULL;
void (*real_glCompressedTexSubImage2D)(uint32_t, int, int, int, unsigned int, unsigned int, uint32_t, unsigned int, const void*) = NULL;
void (*real_glCopyImageSubData)(unsigned int, uint32_t, int, int, int, int, unsigned int, uint32_t, int, int, int, int, unsigned int, unsigned int, unsigned int) = NULL;
void (*real_glEnableVertexAttribArray)(unsigned int) = NULL;
void (*real_glDisableVertexAttribArray)(unsigned int) = NULL;
void* (*real_glMapBufferRange)(uint32_t, intptr_t, uintptr_t, uint32_t) = NULL;
uint8_t (*real_glUnmapBuffer)(uint32_t) = NULL;
void (*real_glBufferStorage)(unsigned int, uintptr_t, const void*, uintptr_t) = NULL;
void (*real_glFlushMappedBufferRange)(uint32_t, intptr_t, uintptr_t) = NULL;
void (*real_glBufferSubData)(uint32_t, intptr_t, uintptr_t, const void*) = NULL;
void (*real_glGetIntegerv)(uint32_t, int*) = NULL;
void (*real_glActiveTexture)(uint32_t) = NULL;
void (*real_glDrawElements)(uint32_t, unsigned int, uint32_t, const void*) = NULL;
void (*real_glMultiDrawElements)(uint32_t, uint32_t*, uint32_t, const void**, uint32_t) = NULL;

/* opengl functions that are usually loaded dynamically from libGL.so */
void (*real_glDrawArrays)(uint32_t, int, unsigned int) = NULL;
void (*real_glGenTextures)(uint32_t, unsigned int*) = NULL;
void (*real_glBindTexture)(uint32_t, unsigned int) = NULL;
void (*real_glTexSubImage2D)(uint32_t, int, int, int, unsigned int, unsigned int, uint32_t, uint32_t, const void*) = NULL;
void (*real_glDeleteTextures)(unsigned int, const unsigned int*) = NULL;
uint32_t (*real_glGetError)() = NULL;
void (*real_glFlush)() = NULL;

void _bolt_unpack_rgb565(uint16_t packed, uint8_t out[3]) {
    out[0] = (packed >> 11) & 0b00011111;
    out[0] = (out[0] << 3) | (out[0] >> 2);
    out[1] = (packed >> 5) & 0b00111111;
    out[1] = (out[1] << 2) | (out[1] >> 4);
    out[2] = packed & 0b00011111;
    out[2] = (out[2] << 3) | (out[2] >> 2);
}

ElfW(Word) _bolt_hash_elf(const char* name) {
	ElfW(Word) tmp, hash = 0;
	const unsigned char* uname = (const unsigned char*)name;
	int c;

	while ((c = *uname++) != '\0') {
		hash = (hash << 4) + c;
		if ((tmp = (hash & 0xf0000000)) != 0) {
			hash ^= tmp >> 24;
			hash ^= tmp;
		}
	}

	return hash;
}

Elf32_Word _bolt_hash_gnu(const char* name) {
	Elf32_Word hash = 5381;
	const unsigned char* uname = (const unsigned char*) name;
	int c;
	while ((c = *uname++) != '\0') {
		hash = (hash << 5) + hash + c;
    }
	return hash & -1;
}

// Note: it'd be possible to one-pass all the symbols in the module instead of seeking them one-at-a-time,
// but I'm pretty sure the DT hash lookup mechanisms make the one-at-a-time method faster for this use-case.
const ElfW(Sym)* _bolt_lookup_symbol(const char* symbol_name, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
    if (gnu_hash_table && gnu_hash_table[0] != 0) {
        const Elf32_Word nbuckets = gnu_hash_table[0];
        const Elf32_Word symbias = gnu_hash_table[1];
        const Elf32_Word bitmask_nwords = gnu_hash_table[2];
        const Elf32_Word bitmask_idxbits = bitmask_nwords - 1;
        const Elf32_Word shift = gnu_hash_table[3];
        const ElfW(Addr)* bitmask = (ElfW(Addr)*)&gnu_hash_table[4];
        const Elf32_Word* buckets = &gnu_hash_table[4 + (__ELF_NATIVE_CLASS / 32) * bitmask_nwords];
        const Elf32_Word* chain_zero = &buckets[nbuckets] - symbias;

        Elf32_Word hash = _bolt_hash_gnu(symbol_name);

        ElfW(Addr) bitmask_word = bitmask[(hash / __ELF_NATIVE_CLASS) & bitmask_idxbits];
        Elf32_Word hashbit1 = hash & (__ELF_NATIVE_CLASS - 1);
        Elf32_Word hashbit2 = (hash >> shift) & (__ELF_NATIVE_CLASS - 1);

        if (!((bitmask_word >> hashbit1) & (bitmask_word >> hashbit2) & 1)) return NULL;

        Elf32_Word bucket = buckets[hash % nbuckets];
        if (bucket == 0) return NULL;

        const Elf32_Word* hasharr = &chain_zero[bucket];
        do {
            if (((*hasharr ^ hash) >> 1) == 0) {
                const ElfW(Sym)* sym = &symbol_table[hasharr - chain_zero];
                if (sym->st_name) {
                    if (!strcmp(&string_table[sym->st_name], symbol_name)) {
                        return sym;
                    }
                }
            }
        } while ((*hasharr++ & 1u) == 0);
    }
    
    if (hash_table && hash_table[0] != 0) {
        ElfW(Word) hash = _bolt_hash_elf(symbol_name);
        unsigned int bucket_index = hash_table[2 + (hash % hash_table[0])];
        const ElfW(Word)* chain = &hash_table[2 + hash_table[0] + bucket_index];
        const ElfW(Sym)* sym = &symbol_table[bucket_index];
        if (sym->st_name && !strcmp(&string_table[sym->st_name], symbol_name)) {
            return sym;
        }
        for (size_t i = 0; chain[i] != STN_UNDEF; i += 1) {
            sym = &symbol_table[chain[i]];
            if (sym->st_name && !strcmp(&string_table[sym->st_name], symbol_name)) {
                return sym;
            }
        }
    }
    return NULL;
}

void _bolt_init_libc(unsigned long addr, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
    libc_addr = (void*)addr;
    const ElfW(Sym)* sym = _bolt_lookup_symbol("dlopen", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_dlopen = sym->st_value + libc_addr;
    sym = _bolt_lookup_symbol("dlsym", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_dlsym = sym->st_value + libc_addr;
    sym = _bolt_lookup_symbol("dlvsym", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_dlvsym = sym->st_value + libc_addr;
    sym = _bolt_lookup_symbol("dlclose", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_dlclose = sym->st_value + libc_addr;
}

void _bolt_init_libegl(unsigned long addr, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
    libegl_addr = (void*)addr;
    const ElfW(Sym)* sym = _bolt_lookup_symbol("eglGetProcAddress", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_eglGetProcAddress = sym->st_value + libegl_addr;
    sym = _bolt_lookup_symbol("eglSwapBuffers", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_eglSwapBuffers = sym->st_value + libegl_addr;
    sym = _bolt_lookup_symbol("eglMakeCurrent", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_eglMakeCurrent = sym->st_value + libegl_addr;
    sym = _bolt_lookup_symbol("eglDestroyContext", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_eglDestroyContext = sym->st_value + libegl_addr;
    sym = _bolt_lookup_symbol("eglInitialize", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_eglInitialize = sym->st_value + libegl_addr;
    sym = _bolt_lookup_symbol("eglCreateContext", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_eglCreateContext = sym->st_value + libegl_addr;
    sym = _bolt_lookup_symbol("eglTerminate", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_eglTerminate = sym->st_value + libegl_addr;
}

void _bolt_init_libgl(unsigned long addr, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
    libgl_addr = (void*)addr;
    const ElfW(Sym)* sym = _bolt_lookup_symbol("glDrawElements", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_glDrawElements = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glGenTextures", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_glGenTextures = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDrawArrays", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_glDrawArrays = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glBindTexture", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_glBindTexture = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glTexSubImage2D", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_glTexSubImage2D = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDeleteTextures", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_glDeleteTextures = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glGetError", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_glGetError = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glFlush", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_glFlush = sym->st_value + libgl_addr;
}

void _bolt_init_libxcb(unsigned long addr, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
    libxcb_addr = (void*)addr;
    const ElfW(Sym)* sym = _bolt_lookup_symbol("xcb_poll_for_event", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_poll_for_event = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_wait_for_event", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_wait_for_event = sym->st_value + libxcb_addr;
}

int _bolt_dl_iterate_callback(struct dl_phdr_info* info, size_t size, void* args) {
    const size_t name_len = strlen(info->dlpi_name);
    const size_t libc_name_len = strlen(libc_name);
    const size_t libegl_name_len = strlen(libegl_name);
    const size_t libgl_name_len = strlen(libgl_name);
    const size_t libxcb_name_len = strlen(libxcb_name);

    const Elf32_Word* gnu_hash_table = NULL;
    const ElfW(Word)* hash_table = NULL;
    const char* string_table = NULL;
    const ElfW(Sym)* symbol_table = NULL;
    for (Elf64_Half p = 0; p < info->dlpi_phnum; p++) {
        if (info->dlpi_phdr[p].p_type == PT_DYNAMIC) {
            ElfW(Dyn)* dynamic = (ElfW(Dyn)*)(info->dlpi_phdr[p].p_vaddr + info->dlpi_addr);
            for (size_t i = 0; dynamic[i].d_tag != DT_NULL; i += 1) {
                switch (dynamic[i].d_tag) {
                    case DT_GNU_HASH:
                        gnu_hash_table = (Elf32_Word*)dynamic[i].d_un.d_ptr;
                        break;
                    case DT_HASH:
                        hash_table = (ElfW(Word)*)dynamic[i].d_un.d_ptr;
                        break;
                    case DT_STRTAB:
                        string_table = (const char*)dynamic[i].d_un.d_ptr;
                        break;
                    case DT_SYMTAB:
                        symbol_table = (ElfW(Sym)*)dynamic[i].d_un.d_ptr;
                        break;
                }
            }
        }
    }

    // checks if the currently-iterated module name ends with '/'+name
    // and if it does, calls the init function for that module, then returns 0
#define FILENAME_EQ_INIT(NAME) { size_t lib_name_len = strlen(NAME##_name); if (name_len > lib_name_len && info->dlpi_name[name_len - lib_name_len - 1] == '/' && !strcmp(info->dlpi_name + name_len - lib_name_len, NAME##_name)) { _bolt_init_##NAME(info->dlpi_addr, gnu_hash_table, hash_table, string_table, symbol_table); return 0; } }
    FILENAME_EQ_INIT(libc);
    FILENAME_EQ_INIT(libegl);
    FILENAME_EQ_INIT(libxcb);
    FILENAME_EQ_INIT(libgl);
#undef FILENAME_EQ_INIT

	return 0;
}

void _bolt_init_functions() {
    dl_iterate_phdr(_bolt_dl_iterate_callback, NULL);
    inited = 1;
}

unsigned int _bolt_glCreateProgram() {
    LOG("glCreateProgram\n");
    unsigned int id = real_glCreateProgram();
    struct GLContext* c = _bolt_context();
    c->programs[id] = malloc(sizeof(struct GLProgram));
    struct GLProgram* program = c->programs[id];
    program->loc_aVertexPosition2D = -1;
    program->loc_aVertexColour = -1;
    program->loc_aTextureUV = -1;
    program->loc_aTextureUVAtlasMin = -1;
    program->loc_aTextureUVAtlasExtents = -1;
    program->loc_uProjectionMatrix = -1;
    program->loc_uDiffuseMap = -1;
    program->is_2d = 0;
    program->is_3d = 0;
    LOG("glCreateProgram end\n");
    return id;
}

void _bolt_glBindAttribLocation(unsigned int program, unsigned int index, const char* name) {
    LOG("glBindAttribLocation\n");
    real_glBindAttribLocation(program, index, name);
    struct GLContext* c = _bolt_context();
    struct GLProgram* p = c->programs[program];
    if (p) {
        if (!strcmp(name, "aVertexPosition2D")) p->loc_aVertexPosition2D = index;
        if (!strcmp(name, "aVertexColour")) p->loc_aVertexColour = index;
        if (!strcmp(name, "aTextureUV")) p->loc_aTextureUV = index;
        if (!strcmp(name, "aTextureUVAtlasMin")) p->loc_aTextureUVAtlasMin = index;
        if (!strcmp(name, "aTextureUVAtlasExtents")) p->loc_aTextureUVAtlasExtents = index;
    }
    LOG("glBindAttribLocation end\n");
}

int _bolt_glGetUniformLocation(unsigned int program, const char* name) {
    LOG("glGetUniformLocation\n");
    int ret = real_glGetUniformLocation(program, name);
    LOG("glGetUniformLocation end (returned %i)\n", ret);
    return ret;
}

void _bolt_glGetUniformfv(unsigned int program, int location, float* params) {
    LOG("glGetUniformfv\n");
    real_glGetUniformfv(program, location, params);
    LOG("glGetUniformfv end\n");
}

void _bolt_glGetUniformiv(unsigned int program, int location, int* params) {
    LOG("glGetUniformiv\n");
    real_glGetUniformiv(program, location, params);
    LOG("glGetUniformiv end\n");
}

void _bolt_glLinkProgram(unsigned int program) {
    LOG("glLinkProgram\n");
    real_glLinkProgram(program);
    struct GLContext* c = _bolt_context();
    int uDiffuseMap = real_glGetUniformLocation(program, "uDiffuseMap");
    int uProjectionMatrix = real_glGetUniformLocation(program, "uProjectionMatrix");
    struct GLProgram* p = c->programs[program];
    if (p && p->loc_aVertexPosition2D != -1 && p->loc_aVertexColour != -1 && p->loc_aTextureUV != -1 && p->loc_aTextureUVAtlasMin != -1 && p->loc_aTextureUVAtlasExtents != -1) {
        if (uDiffuseMap != -1 && uProjectionMatrix != -1) {
            p->loc_uDiffuseMap = uDiffuseMap;
            p->loc_uProjectionMatrix = uProjectionMatrix;
            p->is_2d = 1;
        }
    }
    LOG("glLinkProgram end\n");
}

void _bolt_glUseProgram(unsigned int program) {
    LOG("glUseProgram\n");
    real_glUseProgram(program);
    struct GLContext* c = _bolt_context();
    c->bound_program_id = program;
    LOG("glUseProgram end\n");
}

void glGenTextures(uint32_t n, unsigned int* textures) {
    LOG("glGenTextures\n");
    real_glGenTextures(n, textures);
    struct GLContext* c = _bolt_context();
    for (size_t i = 0; i < n; i += 1) {
        c->textures[textures[i]] = calloc(1, sizeof(struct GLTexture2D));
    }
    LOG("glGenTextures end\n");
}

void _bolt_glTexStorage2D(uint32_t target, int levels, uint32_t internalformat, unsigned int width, unsigned int height) {
    LOG("glTexStorage2D\n");
    real_glTexStorage2D(target, levels, internalformat, width, height);
    struct GLContext* c = _bolt_context();
    struct GLTexture2D* tex = c->textures[c->texture_units[c->active_texture]];
    if (tex) {
        free(tex->data);
        tex->data = malloc(width * height * 4);
        tex->width = width;
        tex->height = height;
    }
    LOG("glTexStorage2D end\n");
}

void _bolt_glVertexAttribPointer(unsigned int index, int size, uint32_t type, uint8_t normalised, unsigned int stride, const void* pointer) {
    LOG("glVertexAttribPointer\n");
    real_glVertexAttribPointer(index, size, type, normalised, stride, pointer);
    struct GLContext* c = _bolt_context();
    int array_binding;
    real_glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_binding);
    _bolt_set_attr_binding(&c->attributes[index], array_binding, size, pointer, stride, type, normalised);
    LOG("glVertexAttribPointer end\n");
}

void _bolt_glGenBuffers(uint32_t n, unsigned int* buffers) {
    LOG("glGenBuffers\n");
    real_glGenBuffers(n, buffers);
    struct GLContext* c = _bolt_context();
    for (size_t i = 0; i < n; i += 1) {
        c->buffers[buffers[i]] = calloc(1, sizeof(struct GLArrayBuffer));
    }
    LOG("glGenBuffers end\n");
}

void _bolt_glBindBuffer(uint32_t target, unsigned int buffer) {
    LOG("glBindBuffer\n");
    real_glBindBuffer(target, buffer);
    LOG("glBindBuffer end\n");
}

void _bolt_glBufferData(uint32_t target, uintptr_t size, const void* data, uint32_t usage) {
    LOG("glBufferData(%u, %lu, %lu, ..)\n", target, size, (uintptr_t)data);
    real_glBufferData(target, size, data, usage);
    struct GLContext* c = _bolt_context();
    if (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        int buffer_id;
        real_glGetIntegerv(target == GL_ARRAY_BUFFER ? GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING, &buffer_id);
        void* buffer_content = malloc(size);
        if (data) memcpy(buffer_content, data, size);
        struct GLArrayBuffer* buffer = c->buffers[buffer_id];
        free(buffer->data);
        buffer->data = buffer_content;
    }
    LOG("glBufferData end\n");
}

void _bolt_glDeleteBuffers(unsigned int n, const unsigned int* buffers) {
    LOG("glDeleteBuffers\n");
    real_glDeleteBuffers(n, buffers);
    struct GLContext* c = _bolt_context();
    for (unsigned int i = 0; i < n; i += 1) {
        struct GLArrayBuffer** buffer = &c->buffers[buffers[i]];
        free((*buffer)->data);
        free((*buffer)->mapping);
        free(*buffer);
    }
    LOG("glDeleteBuffers end\n");
}

void _bolt_glBindFramebuffer(uint32_t target, unsigned int framebuffer) {
    LOG("glBindFramebuffer\n");
    real_glBindFramebuffer(target, framebuffer);
    struct GLContext* c = _bolt_context();
    switch (target) {
        case 36008:
            c->current_read_framebuffer = framebuffer;
            break;
        case 36009:
            c->current_draw_framebuffer = framebuffer;
            break;
        case 36160:
            c->current_read_framebuffer = framebuffer;
            c->current_draw_framebuffer = framebuffer;
            break;
    }
    LOG("glBindFramebuffer end\n");
}

void _bolt_glFramebufferTextureLayer(uint32_t target, uint32_t attachment, unsigned int texture, int level, int layer) {
    LOG("glFramebufferTextureLayer\n");
    real_glFramebufferTextureLayer(target, attachment, texture, level, layer);
    LOG("glFramebufferTextureLayer end\n");
}

void _bolt_glCompressedTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, unsigned int width, unsigned int height, uint32_t format, unsigned int imageSize, const void* data) {
    LOG("glCompressedTexSubImage2D\n");
    real_glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    if (target != GL_TEXTURE_2D || level != 0) return;
    struct GLContext* c = _bolt_context();
    if (format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT || format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT || format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT) {
        struct GLTexture2D* tex = c->textures[c->texture_units[c->active_texture]];
        if (tex) {
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
                if (out_xoffset >= xoffset + yoffset) {
                    out_xoffset = xoffset;
                    out_yoffset += 4;
                }
            }
        }
    }
    LOG("glCompressedTexSubImage2D end\n");
}

void _bolt_glCopyImageSubData(unsigned int srcName, uint32_t srcTarget, int srcLevel, int srcX, int srcY, int srcZ,
                              unsigned int dstName, uint32_t dstTarget, int dstLevel, int dstX, int dstY, int dstZ,
                              unsigned int srcWidth, unsigned int srcHeight, unsigned int srcDepth) {
    LOG("glCopyImageSubData\n");
    real_glCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    struct GLContext* c = _bolt_context();
    if (srcTarget == GL_TEXTURE_2D && dstTarget == GL_TEXTURE_2D && srcLevel == 0 && dstLevel == 0) {
        struct GLTexture2D* src = c->textures[srcName];
        struct GLTexture2D* dst = c->textures[dstName];
        if (!src || !dst) return;
        for (size_t i = 0; i < srcHeight; i += 1) {
            memcpy(dst->data + (dstY * dst->width * 4) + (dstX * 4), src->data + (srcY * src->width * 4) + (srcX * 4), srcWidth * 4);
        }
    }
    LOG("glCopyImageSubData end\n");
}

void _bolt_glEnableVertexAttribArray(unsigned int index) {
    LOG("glEnableVertexAttribArray\n");
    real_glEnableVertexAttribArray(index);
    _bolt_context()->attributes[index].enabled = 1;
    LOG("glEnableVertexAttribArray end\n");
}

void _bolt_glDisableVertexAttribArray(unsigned int index) {
    LOG("glDisableVertexAttribArray\n");
    real_glEnableVertexAttribArray(index);
    _bolt_context()->attributes[index].enabled = 0;
    LOG("glDisableVertexAttribArray end\n");
}

void* _bolt_glMapBufferRange(uint32_t target, intptr_t offset, uintptr_t length, uint32_t access) {
    LOG("glMapBufferRange\n");
    struct GLContext* c = _bolt_context();
    if (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        int buffer_id;
        real_glGetIntegerv(target == GL_ARRAY_BUFFER ? GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING, &buffer_id);
        struct GLArrayBuffer* buffer = c->buffers[buffer_id];
        buffer->mapping = malloc(length);
        buffer->mapping_offset = offset;
        buffer->mapping_len = length;
        buffer->mapping_access_type = access;
        LOG("glMapBufferRange good end\n");
        return buffer->mapping;
    } else {
        LOG("glMapBufferRange bad end\n");
        return real_glMapBufferRange(target, offset, length, access);
    }
}

uint8_t _bolt_glUnmapBuffer(uint32_t target) {
    LOG("glUnmapBuffer\n");
    struct GLContext* c = _bolt_context();
    if (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        int buffer_id;
        real_glGetIntegerv(target == GL_ARRAY_BUFFER ? GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING, &buffer_id);
        struct GLArrayBuffer* buffer = c->buffers[buffer_id];
        free(buffer->mapping);
        buffer->mapping = NULL;
        LOG("glUnmapBuffer good end\n");
        return 1;
    } else {
        LOG("glUnmapBuffer bad end\n");
        return real_glUnmapBuffer(target);
    }
}

void _bolt_glBufferStorage(uint32_t target, uintptr_t size, const void* data, uintptr_t flags) {
    LOG("glBufferStorage\n");
    real_glBufferStorage(target, size, data, flags);
    struct GLContext* c = _bolt_context();
    if (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        int buffer_id;
        real_glGetIntegerv(target == GL_ARRAY_BUFFER ? GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING, &buffer_id);
        void* buffer_content = malloc(size);
        if (data) memcpy(buffer_content, data, size);
        struct GLArrayBuffer* buffer = c->buffers[buffer_id];
        free(buffer->data);
        buffer->data = buffer_content;
    }
    LOG("glBufferStorage end\n");
}

void _bolt_glFlushMappedBufferRange(uint32_t target, intptr_t offset, uintptr_t length) {
    LOG("glFlushMappedBufferRange\n");
    struct GLContext* c = _bolt_context();
    if (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        int buffer_id;
        real_glGetIntegerv(target == GL_ARRAY_BUFFER ? GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING, &buffer_id);
        struct GLArrayBuffer* buffer = c->buffers[buffer_id];
        real_glBufferSubData(target, buffer->mapping_offset + offset, length, buffer->mapping + offset);
        memcpy(buffer->data + buffer->mapping_offset + offset, buffer->mapping + offset, length);
    } else {
        real_glFlushMappedBufferRange(target, offset, length);
    }
    LOG("glFlushMappedBufferRange end\n");
}

void _bolt_glBufferSubData(uint32_t target, intptr_t offset, uintptr_t size, const void* data) {
    LOG("glBufferSubData\n");
    real_glBufferSubData(target, offset, size, data);
    LOG("glBufferSubData end\n");
}

void _bolt_glGetIntegerv(uint32_t pname, int* data) {
    LOG("glGetIntegerv\n");
    real_glGetIntegerv(pname, data);
    LOG("glGetIntegerv end\n");
}

void _bolt_glActiveTexture(uint32_t texture) {
    LOG("glActiveTexture\n");
    real_glActiveTexture(texture);
    struct GLContext* c = _bolt_context();
    c->active_texture = texture - GL_TEXTURE0;
    LOG("glActiveTexture end\n");
}

const uint8_t compass_pixel_row[] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x11, 0x38, 0x34, 0x34, 0xA0, 0x2D, 0x2A, 0x29, 0xFF, 0x6F, 0x50, 0x1A, 0xFF, 0xAB, 0x82, 0x34, 0xFF, 0xFD, 0xD2, 0x81, 0xFF, 0x9B, 0x72, 0x25, 0xFF, 0x6C, 0x4F, 0x1A, 0xFF, 0x2D, 0x2A, 0x29, 0xFF, 0x36, 0x32, 0x32, 0x98, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00};
void glDrawElements(uint32_t mode, unsigned int count, uint32_t type, const void* indices) {
    LOG("glDrawElements\n");
    real_glDrawElements(mode, count, type, indices);
    struct GLContext* c = _bolt_context();
    struct GLProgram* current_program = c->programs[c->bound_program_id];
    if (type == GL_UNSIGNED_SHORT && mode == GL_TRIANGLES && count > 0 && current_program->is_2d && c->current_draw_framebuffer == 0) {
        int element_binding;
        real_glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_binding);
        struct GLArrayBuffer* element_buffer = c->buffers[element_binding];
        const unsigned short* indices = element_buffer->data + (uintptr_t)indices;
        const struct GLAttrBinding* atlas_min = &c->attributes[current_program->loc_aTextureUVAtlasMin];
        const struct GLAttrBinding* atlas_size = &c->attributes[current_program->loc_aTextureUVAtlasExtents];
        const struct GLAttrBinding* tex_uv = &c->attributes[current_program->loc_aTextureUV];
        const struct GLAttrBinding* position_2d = &c->attributes[current_program->loc_aVertexPosition2D];
        int diffuse_map;
        real_glGetUniformiv(c->bound_program_id, current_program->loc_uDiffuseMap, &diffuse_map);
        float projection_matrix[16];
        real_glGetUniformfv(c->bound_program_id, current_program->loc_uProjectionMatrix, projection_matrix);
        const struct GLTexture2D* tex = c->textures[c->texture_units[diffuse_map]];
        if (atlas_min->enabled && atlas_size->enabled && tex_uv->enabled && position_2d->enabled) {
            uint8_t icon_detected = 0;
            uint8_t angle_calculated = 0;
            int current_atlas_x = -1;
            int current_atlas_y = -1;
            int current_atlas_w = -1;
            int current_atlas_h = -1;
            float pos_x_min, pos_x_max, pos_y_min, pos_y_max, first_vertex_u, first_vertex_v, angle;
            for (size_t i = 0; i < count; i += 1) {
                unsigned short index = indices[i];
                float min[2];
                float size[2];
                float pos[2];
                float uv[2];
                _bolt_get_attr_binding(c, atlas_min, index, 2, min);
                _bolt_get_attr_binding(c, atlas_size, index, 2, size);
                _bolt_get_attr_binding(c, position_2d, index, 2, pos);
                _bolt_get_attr_binding(c, tex_uv, index, 2, uv);
                const int image_x = fabs(roundf(min[0] * tex->width));
                const int image_y = fabs(roundf(min[1] * tex->height));
                const int image_width = fabs(roundf(size[0] * tex->width));
                const int image_height = fabs(roundf(size[1] * tex->height));
                if (image_x != current_atlas_x || image_y != current_atlas_y || image_width != current_atlas_w || image_height != current_atlas_h) {
                    if (icon_detected) {
                        printf("compass at %f,%f to %f,%f\n", pos_x_min, pos_y_min, pos_x_max, pos_y_max);
                        if (angle_calculated) printf("angle is %f\n", angle * 180.0 / M_PI);
                    }

                    icon_detected = 0;
                    current_atlas_x = image_x;
                    current_atlas_y = image_y;
                    current_atlas_w = image_width;
                    current_atlas_h = image_height;
                    if (image_width == 51 && image_height == 51) {
                        for (size_t y = 0; y < image_height; y += 1) {
                            const uint8_t* ptr = tex->data + ((((y + image_y) * tex->width) + image_x) * 4);
                            if (!memcmp(ptr, compass_pixel_row, image_width * 4)) {
                                icon_detected = 1;
                                pos_x_min = pos[0];
                                pos_x_max = pos[0];
                                pos_y_min = pos[1];
                                pos_y_max = pos[1];
                                first_vertex_u = uv[0];
                                first_vertex_v = uv[1];
                            }
                        }
                    }
                } else if(icon_detected) {
                    if (!angle_calculated) {
                        // angle is in radians from 0..2pi where upright is 0 and angle increases CCW.
                        float pos_angle_rads = atan2f(pos_y_min - pos[1], pos_x_min - pos[0]);
                        float uv_angle_rads = atan2f(uv[1] - first_vertex_v, uv[0] - first_vertex_u);
                        angle = fmod(pos_angle_rads - uv_angle_rads, M_PI * 2);
                        if (angle < 0) angle += M_PI * 2;
                        angle_calculated = 1;
                    }
                    if (pos_x_min > pos[0]) pos_x_min = pos[0];
                    if (pos_x_max < pos[0]) pos_x_max = pos[0];
                    if (pos_y_min > pos[1]) pos_y_min = pos[1];
                    if (pos_y_max < pos[1]) pos_y_max = pos[1];
                }
            }
        }
    }
    LOG("glDrawElements end\n");
}

void _bolt_glMultiDrawElements(uint32_t mode, uint32_t* count, uint32_t type, const void** indices, size_t drawcount) {
    for (size_t i = 0; i < drawcount; i += 1) {
        glDrawElements(mode, count[i], type, indices[i]);
    }
}

void glDrawArrays(uint32_t mode, int first, unsigned int count) {
    LOG("glDrawArrays\n");
    real_glDrawArrays(mode, first, count);
    LOG("glDrawArrays end\n");
}

void glBindTexture(uint32_t target, unsigned int texture) {
    LOG("glBindTexture\n");
    real_glBindTexture(target, texture);
    if (target == GL_TEXTURE_2D) {
        struct GLContext* c = _bolt_context();
        c->texture_units[c->active_texture] = texture;
    }
    LOG("glBindTexture end\n");
}

void glTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, unsigned int width, unsigned int height, uint32_t format, uint32_t type, const void* pixels) {
    LOG("glTexSubImage2D\n");
    real_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    struct GLContext* c = _bolt_context();
    if (level == 0 && format == GL_RGBA) {
        if (target == GL_TEXTURE_2D && format == GL_RGBA) {
            struct GLTexture2D* tex = c->textures[c->texture_units[c->active_texture]];
            if (tex && !(xoffset < 0 || yoffset < 0 || xoffset + width > tex->width || yoffset + height > tex->height)) {
                for (unsigned int y = 0; y < height; y += 1) {
                    unsigned char* dest_ptr = tex->data + ((tex->width * (y + yoffset)) + xoffset) * 4;
                    const void* src_ptr = pixels + (width * y * 4);
                    memcpy(dest_ptr, src_ptr, width * 4);
                }
            }
        }
    }
    LOG("glTexSubImage2D end\n");
}

void glDeleteTextures(unsigned int n, const unsigned int* textures) {
    LOG("glDeleteTextures\n");
    real_glDeleteTextures(n, textures);
    struct GLContext* c = _bolt_context();
    for (unsigned int i = 0; i < n; i += 1) {
        struct GLTexture2D** texture = &c->textures[textures[i]];
        free((*texture)->data);
        free(*texture);
    }
    LOG("glDeleteTextures end\n");
}

uint32_t glGetError() {
    LOG("glGetError\n");
    uint32_t ret = real_glGetError();
    LOG("glGetError end (returned %u)\n", ret);
    return ret;
}

void glFlush() {
    LOG("glFlush\n");
    real_glFlush();
    LOG("glFlush end\n");
}

unsigned int eglSwapBuffers(void*, void*);
unsigned int eglMakeCurrent(void*, void*, void*, void*);
unsigned int eglDestroyContext(void*, void*);
unsigned int eglInitialize(void*, void*, void*);
void* eglCreateContext(void*, void*, void*, const void*);
unsigned int eglTerminate(void*);

void* eglGetProcAddress(const char* name) {
    LOG("eglGetProcAddress(%s)\n", name);
    if (!strcmp(name, "eglGetProcAddress")) {
        return eglGetProcAddress;
    }
    if (!strcmp(name, "eglSwapBuffers")) {
        real_eglSwapBuffers = real_eglGetProcAddress("eglSwapBuffers");
        return eglSwapBuffers;
    }
    if (!strcmp(name, "eglMakeCurrent")) {
        real_eglMakeCurrent = real_eglGetProcAddress("eglMakeCurrent");
        return eglMakeCurrent;
    }
    if (!strcmp(name, "eglInitialize")) {
        real_eglInitialize = real_eglGetProcAddress("eglInitialize");
        return eglInitialize;
    }
    if (!strcmp(name, "eglDestroyContext")) {
        real_eglDestroyContext = real_eglGetProcAddress("eglDestroyContext");
        return eglDestroyContext;
    }
    if (!strcmp(name, "eglCreateContext")) {
        real_eglCreateContext = real_eglGetProcAddress("eglCreateContext");
        return eglCreateContext;
    }
    if (!strcmp(name, "eglTerminate")) {
        real_eglTerminate = real_eglGetProcAddress("eglTerminate");
        return eglTerminate;
    }
#define PROC_ADDRESS_MAP(FUNC) if (!strcmp(name, #FUNC)) { real_##FUNC = real_eglGetProcAddress(#FUNC); return real_##FUNC ? _bolt_##FUNC : NULL; }
    PROC_ADDRESS_MAP(glCreateProgram)
    PROC_ADDRESS_MAP(glBindAttribLocation)
    PROC_ADDRESS_MAP(glGetUniformLocation)
    PROC_ADDRESS_MAP(glGetUniformfv)
    PROC_ADDRESS_MAP(glGetUniformiv)
    PROC_ADDRESS_MAP(glLinkProgram)
    PROC_ADDRESS_MAP(glUseProgram)
    PROC_ADDRESS_MAP(glTexStorage2D)
    PROC_ADDRESS_MAP(glVertexAttribPointer)
    PROC_ADDRESS_MAP(glGenBuffers)
    PROC_ADDRESS_MAP(glBindBuffer)
    PROC_ADDRESS_MAP(glBufferData)
    PROC_ADDRESS_MAP(glDeleteBuffers)
    PROC_ADDRESS_MAP(glBindFramebuffer)
    PROC_ADDRESS_MAP(glFramebufferTextureLayer)
    PROC_ADDRESS_MAP(glCompressedTexSubImage2D)
    PROC_ADDRESS_MAP(glCopyImageSubData)
    PROC_ADDRESS_MAP(glEnableVertexAttribArray)
    PROC_ADDRESS_MAP(glDisableVertexAttribArray)
    PROC_ADDRESS_MAP(glMapBufferRange)
    PROC_ADDRESS_MAP(glUnmapBuffer)
    PROC_ADDRESS_MAP(glBufferStorage)
    PROC_ADDRESS_MAP(glFlushMappedBufferRange)
    PROC_ADDRESS_MAP(glBufferSubData)
    PROC_ADDRESS_MAP(glGetIntegerv)
    PROC_ADDRESS_MAP(glActiveTexture)
    PROC_ADDRESS_MAP(glMultiDrawElements)
#undef PROC_ADDRESS_MAP
    return real_eglGetProcAddress(name);
}

unsigned int eglSwapBuffers(void* display, void* surface) {
    LOG("eglSwapBuffers\n");
    unsigned int ret = real_eglSwapBuffers(display, surface);
    LOG("eglSwapBuffers end (returned %u)\n", ret);
    return ret;
}

unsigned int eglMakeCurrent(void* display, void* draw, void* read, void* context) {
    LOG("eglMakeCurrent\n");
    unsigned int ret = real_eglMakeCurrent(display, draw, read, context);
    if (ret) {
        pthread_mutex_lock(&egl_lock);
        _bolt_make_context_current(context);
        pthread_mutex_unlock(&egl_lock);
    }
    LOG("eglMakeCurrent end (returned %u)\n", ret);
    return ret;
}

unsigned int eglDestroyContext(void* display, void* context) {
    LOG("eglDestroyContext\n");
    unsigned int ret = real_eglDestroyContext(display, context);
    if (ret) {
        pthread_mutex_lock(&egl_lock);
        _bolt_destroy_context(context);
        pthread_mutex_unlock(&egl_lock);
    }
    LOG("eglDestroyContext end (returned %u)\n", ret);
    return ret;
}

unsigned int eglInitialize(void* display, void* major, void* minor) {
    INIT();
    LOG("eglInitialize\n");
    unsigned int ret = real_eglInitialize(display, major, minor);
    if (ret) {
        pthread_mutex_init(&egl_lock, NULL);
    }
    LOG("eglInitialize end (returned %u)\n", ret);
    return ret;
}

void* eglCreateContext(void* display, void* config, void* share_context, const void* attrib_list) {
    LOG("eglCreateContext\n");
    void* ret = real_eglCreateContext(display, config, share_context, attrib_list);
    pthread_mutex_lock(&egl_lock);
    _bolt_create_context(ret, share_context);
    pthread_mutex_unlock(&egl_lock);
    LOG("eglCreateContext end (returned %lu)\n", (uintptr_t)ret);
    return ret;
}

unsigned int eglTerminate(void* display) {
    LOG("eglTerminate\n");
    unsigned int ret = real_eglTerminate(display);
    pthread_mutex_destroy(&egl_lock);
    LOG("eglTerminate end (returned %u)\n", ret);
    return ret;
}

void* xcb_poll_for_event(void* c) {
    return real_xcb_poll_for_event(c);
}

void* xcb_wait_for_event(void* c) {
    return real_xcb_wait_for_event(c);
}

void* dlopen(const char*, int);
void* dlsym(void*, const char*);
void* dlvsym(void*, const char*, const char*);
int dlclose(void*);

void* _bolt_dl_lookup(void* handle, const char* symbol) {
    if (!handle) return NULL;
    if (handle == libc_addr) {
        if (strcmp(symbol, "dlopen") == 0) return dlopen;
        if (strcmp(symbol, "dlsym") == 0) return dlsym;
        if (strcmp(symbol, "dlvsym") == 0) return dlvsym;
        if (strcmp(symbol, "dlclose") == 0) return dlclose;
    } else if (handle == libegl_addr) {
        if (strcmp(symbol, "eglGetProcAddress") == 0) return eglGetProcAddress;
        if (strcmp(symbol, "eglSwapBuffers") == 0) return eglSwapBuffers;
        if (strcmp(symbol, "eglMakeCurrent") == 0) return eglMakeCurrent;
        if (strcmp(symbol, "eglDestroyContext") == 0) return eglDestroyContext;
        if (strcmp(symbol, "eglInitialize") == 0) return eglInitialize;
        if (strcmp(symbol, "eglCreateContext") == 0) return eglCreateContext;
        if (strcmp(symbol, "eglTerminate") == 0) return eglTerminate;
    } else if (handle == libgl_addr) {
        if (strcmp(symbol, "glDrawElements") == 0) return glDrawElements;
        if (strcmp(symbol, "glDrawArrays") == 0) return glDrawArrays;
        if (strcmp(symbol, "glGenTextures") == 0) return glGenTextures;
        if (strcmp(symbol, "glBindTexture") == 0) return glBindTexture;
        if (strcmp(symbol, "glTexSubImage2D") == 0) return glTexSubImage2D;
        if (strcmp(symbol, "glDeleteTextures") == 0) return glDeleteTextures;
        if (strcmp(symbol, "glGetError") == 0) return glGetError;
        if (strcmp(symbol, "glFlush") == 0) return glFlush;
    } else if (handle == libxcb_addr) {
        if (strcmp(symbol, "xcb_poll_for_event") == 0) return xcb_poll_for_event;
        if (strcmp(symbol, "xcb_wait_for_event") == 0) return xcb_wait_for_event;
    }
    return NULL;
}

void* dlopen(const char* filename, int flags) {
    INIT();
    void* ret = real_dlopen(filename, flags);
    LOG("dlopen('%s', %i) -> %lu\n", filename, flags, (unsigned long)ret);
    if (filename) {
        if (!libegl_addr && !strcmp(filename, libegl_name)) {
            libegl_addr = ret;
            if (!libegl_addr) return NULL;
            real_eglGetProcAddress = real_dlsym(ret, "eglGetProcAddress");
            real_eglSwapBuffers = real_dlsym(ret, "eglSwapBuffers");
            real_eglMakeCurrent = real_dlsym(ret, "eglMakeCurrent");
            real_eglDestroyContext = real_dlsym(ret, "eglDestroyContext");
            real_eglInitialize = real_dlsym(ret, "eglInitialize");
            real_eglCreateContext = real_dlsym(ret, "eglCreateContext");
            real_eglTerminate = real_dlsym(ret, "eglTerminate");
        }
        if (!libgl_addr && !strcmp(filename, libgl_name)) {
            libgl_addr = ret;
            if (!libgl_addr) return NULL;
            real_glDrawElements = real_dlsym(ret, "glDrawElements");
            real_glDrawArrays = real_dlsym(ret, "glDrawArrays");
            real_glGenTextures = real_dlsym(ret, "glGenTextures");
            real_glBindTexture = real_dlsym(ret, "glBindTexture");
            real_glTexSubImage2D = real_dlsym(ret, "glTexSubImage2D");
            real_glDeleteTextures = real_dlsym(ret, "glDeleteTextures");
            real_glGetError = real_dlsym(ret, "glGetError");
            real_glFlush = real_dlsym(ret, "glFlush");
        }
        if (!libxcb_addr && !strcmp(filename, libxcb_name)) {
            libxcb_addr = ret;
            if (!libxcb_addr) return NULL;
            real_xcb_poll_for_event = real_dlsym(ret, "xcb_poll_for_event");
            real_xcb_wait_for_event = real_dlsym(ret, "xcb_wait_for_event");
        }
    }
    return ret;
}

void* dlsym(void* handle, const char* symbol) {
    INIT();
    LOG("dlsym('%lu', %s)\n", (uintptr_t)handle, symbol);
    void* f = _bolt_dl_lookup(handle, symbol);
    return f ? f : real_dlsym(handle, symbol);
}

void* dlvsym(void* handle, const char* symbol, const char* version) {
    INIT();
    void* f = _bolt_dl_lookup(handle, symbol);
    return f ? f : real_dlvsym(handle, symbol, version);
}

int dlclose(void* handle) {
    if (handle == libc_addr) libc_addr = NULL;
    if (handle == libegl_addr) libegl_addr = NULL;
    return real_dlclose(handle);
}
