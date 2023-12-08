#define _GNU_SOURCE
#include <link.h>
#include <unistd.h>
#undef _GNU_SOURCE

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "../gl.h"

// note: this is currently always triggered by single-threaded dlopen calls so no locking necessary
uint8_t inited = 0;
#define INIT() if (!inited) _bolt_init_functions();

enum BoltMessageType {
    Message_Quit,
    Message_Context,
    Message_glCreateProgram,
    Message_glLinkProgram,
    Message_glUseProgram,
    Message_glEnableVertexAttribArray,
    Message_glDisableVertexAttribArray,
    Message_glBindAttribLocation,
    Message_glVertexAttribPointer,
    Message_glBufferData,
    Message_glBufferStorage,
    Message_glMapBufferRange,
    Message_glUnmapBuffer,
    Message_glDeleteBuffers,
    Message_glBindFramebuffer,
    Message_glCompressedTexSubImage2D,
    Message_glCopyImageSubData,
    Message_glFlushMappedBufferRange,
    Message_glDrawElements,
    Message_glBindTexture,
    Message_glTexStorage2D,
    Message_glTexSubImage2D,
    Message_glDeleteTextures,
    Message_eglSwapBuffers,
    Message_glFlush,
};
int read_socket;
int write_socket;
pthread_t worker_thread;
uint8_t worker_thread_running = 0;
uint8_t worker_context_exists = 0;
void* _bolt_worker_thread(void*);
struct BoltMessage {
    struct GLContext* context;
    void* data;
    unsigned int x;
    unsigned int y;
    unsigned int w;
    unsigned int h;
    unsigned int index;
    unsigned int asset;
    unsigned int stride;
    uint32_t type;
    uint32_t target;
    uint32_t format;
    uint8_t bool_value;
    uint8_t do_free_data;
    enum BoltMessageType instruction;
};
#define SEND_MSG(...) {struct BoltMessage _message = __VA_ARGS__; write(write_socket, &_message, sizeof(struct BoltMessage));}

struct BoltSyncData {
    void* ptr;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint8_t done;
};


pthread_mutex_t egl_lock;
atomic_bool sync_before_next_draw = 0;

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

/* opengl functions that are usually loaded dynamically from libGL.so */
void (*real_glDrawElements)(uint32_t, unsigned int, uint32_t, const void*) = NULL;
void (*real_glDrawArrays)(uint32_t, int, unsigned int) = NULL;
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
    pthread_mutex_init(&egl_lock, NULL);
    dl_iterate_phdr(_bolt_dl_iterate_callback, NULL);
    inited = 1;
}

void glFlush();

unsigned int _bolt_glCreateProgram() {
    unsigned int id = real_glCreateProgram();
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glCreateProgram, .asset = id})
    return id;
}

void _bolt_glBindAttribLocation(unsigned int program, unsigned int index, const char* name) {
    real_glBindAttribLocation(program, index, name);
    // I think `name` always points to an embedded string in the exe, so we don't need to reallocate it here
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glBindAttribLocation, .asset = program, .index = index, .data = (void*)name, .do_free_data = 0})
}

void _bolt_glGetUniformLocation(unsigned int program, const char* name) {
    real_glGetUniformLocation(program, name);
}

void _bolt_glGetUniformfv(unsigned int program, int location, float* params) {
    real_glGetUniformfv(program, location, params);
}

void _bolt_glGetUniformiv(unsigned int program, int location, int* params) {
    real_glGetUniformiv(program, location, params);
}

void _bolt_glLinkProgram(unsigned int program) {
    real_glLinkProgram(program);
    int uDiffuseMap = real_glGetUniformLocation(program, "uDiffuseMap");
    int uProjectionMatrix = real_glGetUniformLocation(program, "uProjectionMatrix");
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glLinkProgram, .asset = program, .x = uDiffuseMap, .y = uProjectionMatrix})
}

void _bolt_glUseProgram(unsigned int program) {
    real_glUseProgram(program);
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glUseProgram, .asset = program})
}

void _bolt_glTexStorage2D(uint32_t target, int levels, uint32_t internalformat, unsigned int width, unsigned int height) {
    real_glTexStorage2D(target, levels, internalformat, width, height);
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glTexStorage2D, .target = target, .w = width, .h = height})
}

void _bolt_glVertexAttribPointer(unsigned int index, int size, uint32_t type, uint8_t normalised, unsigned int stride, const void* pointer) {
    real_glVertexAttribPointer(index, size, type, normalised, stride, pointer);
    int array_binding;
    real_glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_binding);
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glVertexAttribPointer, .index = index, .type = type, .asset = array_binding, .w = size, .bool_value = normalised, .stride = stride, .data = (void*)(uintptr_t)pointer})
}

void _bolt_glBindBuffer(uint32_t target, unsigned int buffer) {
    real_glBindBuffer(target, buffer);
}

void _bolt_glBufferData(uint32_t target, uintptr_t size, const void* data, uint32_t usage) {
    real_glBufferData(target, size, data, usage);
    if (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        int bound;
        real_glGetIntegerv(target == GL_ARRAY_BUFFER ? GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING, &bound);
        void* buffer = malloc(size);
        if (data) memcpy(buffer, data, size);
        SEND_MSG({.context = _bolt_context(), .instruction = Message_glBufferData, .target = target, .asset = bound, .data = buffer, .w = size, .do_free_data = 1})
    }
}

void _bolt_glDeleteBuffers(unsigned int n, const unsigned int* buffers) {
    real_glDeleteBuffers(n, buffers);
    void* ptr = malloc(n * sizeof(unsigned int));
    memcpy(ptr, buffers, n * sizeof(unsigned int));
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glDeleteBuffers, .w = n, .data = ptr, .do_free_data = 1})
}

void _bolt_glBindFramebuffer(uint32_t target, unsigned int framebuffer) {
    real_glBindFramebuffer(target, framebuffer);
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glBindFramebuffer, .target = target, .asset = framebuffer})
}

void _bolt_glFramebufferTextureLayer(uint32_t target, uint32_t attachment, unsigned int texture, int level, int layer) {
    real_glFramebufferTextureLayer(target, attachment, texture, level, layer);
}

void _bolt_glCompressedTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, unsigned int width, unsigned int height, uint32_t format, unsigned int imageSize, const void* data) {
    real_glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    if (target != GL_TEXTURE_2D || level != 0) return;
    struct GLContext* c = _bolt_context();
    SEND_MSG({.context = c, .instruction = Message_glCompressedTexSubImage2D, .x = xoffset, .y = yoffset, .w = width, .h = height, .format = format, .data = (void*)(uintptr_t)data, .do_free_data = 0})
}

void _bolt_glCopyImageSubData(unsigned int srcName, uint32_t srcTarget, int srcLevel, int srcX, int srcY, int srcZ,
                              unsigned int dstName, uint32_t dstTarget, int dstLevel, int dstX, int dstY, int dstZ,
                              unsigned int srcWidth, unsigned int srcHeight, unsigned int srcDepth) {
    real_glCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    if (srcTarget == GL_TEXTURE_2D && dstTarget == GL_TEXTURE_2D && srcLevel == 0 && dstLevel == 0) {
        struct {
            struct BoltMessage head;
            int tail[8];
        } message = {
            .head = {.context = _bolt_context(), .instruction = Message_glCopyImageSubData},
            .tail = {srcName, srcX, srcY, dstName, dstX, dstY, srcWidth, srcHeight},
        };
        write(write_socket, &message, sizeof(message));
    }
}

void _bolt_glEnableVertexAttribArray(unsigned int index) {
    real_glEnableVertexAttribArray(index);
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glEnableVertexAttribArray, .index = index})
}

void _bolt_glDisableVertexAttribArray(unsigned int index) {
    real_glEnableVertexAttribArray(index);
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glDisableVertexAttribArray, .index = index})
}

void* _bolt_glMapBufferRange(uint32_t target, intptr_t offset, uintptr_t length, uint32_t access) {
    if (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        int buffer;
        real_glGetIntegerv(target == GL_ARRAY_BUFFER ? GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING, &buffer);
        struct BoltSyncData data;
        data.done = 0;
        pthread_mutex_init(&data.mutex, NULL);
        pthread_cond_init(&data.cond, NULL);
        SEND_MSG({.context = _bolt_context(), .data = &data, .instruction = Message_glMapBufferRange, .target = target, .asset = buffer, .x = offset, .w = length, .format = access})
        pthread_mutex_lock(&data.mutex);
        while (!data.done) pthread_cond_wait(&data.cond, &data.mutex);
        pthread_mutex_unlock(&data.mutex);
        pthread_mutex_destroy(&data.mutex);
        pthread_cond_destroy(&data.cond);
        return data.ptr;
    } else {
        return real_glMapBufferRange(target, offset, length, access);
    }
}

uint8_t _bolt_glUnmapBuffer(uint32_t target) {
    if (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        int buffer;
        real_glGetIntegerv(target == GL_ARRAY_BUFFER ? GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING, &buffer);
        SEND_MSG({.context = _bolt_context(), .instruction = Message_glUnmapBuffer, .target = target, .asset = buffer})
        sync_before_next_draw = 1;
        return 1;
    } else {
        return real_glUnmapBuffer(target);
    }
}

void _bolt_glBufferStorage(uint32_t target, uintptr_t size, const void* data, uintptr_t flags) {
    real_glBufferStorage(target, size, data, flags);
    if (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        int bound;
        real_glGetIntegerv(target == GL_ARRAY_BUFFER ? GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING, &bound);
        void* buffer = malloc(size);
        if (data) memcpy(buffer, data, size);
        SEND_MSG({.context = _bolt_context(), .instruction = Message_glBufferStorage, .target = target, .asset = bound, .data = buffer, .w = size, .do_free_data = 1})
    }
}

void _bolt_glFlushMappedBufferRange(uint32_t target, intptr_t offset, uintptr_t length) {
    if (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) {
        int buffer;
        real_glGetIntegerv(target == GL_ARRAY_BUFFER ? GL_ARRAY_BUFFER_BINDING : GL_ELEMENT_ARRAY_BUFFER_BINDING, &buffer);
        SEND_MSG({.context = _bolt_context(), .instruction = Message_glFlushMappedBufferRange, .target = target, .asset = buffer, .x = offset, .w = length})
        sync_before_next_draw = 1;
    } else {
        real_glFlushMappedBufferRange(target, offset, length);
    }
}

void _bolt_glBufferSubData(uint32_t target, intptr_t offset, uintptr_t size, const void* data) {
    real_glBufferSubData(target, offset, size, data);
}

void _bolt_glGetIntegerv(uint32_t pname, int* data) {
    real_glGetIntegerv(pname, data);
}

void glDrawElements(uint32_t mode, unsigned int count, uint32_t type, const void* indices) {
    if (sync_before_next_draw) glFlush();
    real_glDrawElements(mode, count, type, indices);

    if (type == GL_UNSIGNED_SHORT && mode == GL_TRIANGLES) {
        int element_binding;
        real_glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_binding);
        SEND_MSG({.context = _bolt_context(), .instruction = Message_glDrawElements, .data = (void*)(uintptr_t)indices, .asset = element_binding, .w = count})
    }
}

void glDrawArrays(uint32_t mode, int first, unsigned int count) {
    if (sync_before_next_draw) glFlush();
    real_glDrawArrays(mode, first, count);
}

void glBindTexture(uint32_t target, unsigned int texture) {
    real_glBindTexture(target, texture);
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glBindTexture, .target = target, .asset = texture})
}

void glTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, unsigned int width, unsigned int height, uint32_t format, uint32_t type, const void* pixels) {
    real_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    struct GLContext* c = _bolt_context();
    if (level == 0 && format == GL_RGBA) {
        SEND_MSG({.context = c, .instruction = Message_glTexSubImage2D, .target = target, .x = xoffset, .y = yoffset, .w = width, .h = height, .format = format, .type = type, .data = (void*)(uintptr_t)pixels, .do_free_data = 0})
    }
}

void glDeleteTextures(unsigned int n, const unsigned int* textures) {
    real_glDeleteTextures(n, textures);
    void* ptr = malloc(n * sizeof(unsigned int));
    memcpy(ptr, textures, n * sizeof(unsigned int));
    SEND_MSG({.context = _bolt_context(), .instruction = Message_glDeleteTextures, .w = n, .data = ptr, .do_free_data = 1})
}

uint32_t glGetError() {
    return real_glGetError();
}

void glFlush() {
    struct BoltSyncData data;
    data.done = 0;
    pthread_mutex_init(&data.mutex, NULL);
    pthread_cond_init(&data.cond, NULL);
    SEND_MSG({.instruction = Message_glFlush, .data = &data})
    real_glFlush();
    pthread_mutex_lock(&data.mutex);
    while (!data.done) {
        pthread_mutex_lock(&egl_lock);
        uint8_t running = worker_thread_running;
        pthread_mutex_unlock(&egl_lock);
        if (!running) break;
        pthread_cond_wait(&data.cond, &data.mutex);
    }
    pthread_mutex_unlock(&data.mutex);
    pthread_mutex_destroy(&data.mutex);
    pthread_cond_destroy(&data.cond);
    sync_before_next_draw = 0;
}

unsigned int eglSwapBuffers(void*, void*);
unsigned int eglMakeCurrent(void*, void*, void*, void*);
unsigned int eglDestroyContext(void*, void*);
unsigned int eglInitialize(void*, void*, void*);
void* eglCreateContext(void*, void*, void*, const void*);
unsigned int eglTerminate(void*);

void* eglGetProcAddress(const char* name) {
    //printf("eglGetProcAddress(%s)\n", name);
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
#undef PROC_ADDRESS_MAP
    return real_eglGetProcAddress(name);
}

unsigned int eglSwapBuffers(void* display, void* surface) {
    struct BoltSyncData data;
    data.done = 0;
    pthread_mutex_init(&data.mutex, NULL);
    pthread_cond_init(&data.cond, NULL);
    SEND_MSG({.context = _bolt_context(), .instruction = Message_eglSwapBuffers, .data = &data})
    pthread_mutex_lock(&data.mutex);
    while (!data.done) pthread_cond_wait(&data.cond, &data.mutex);
    pthread_mutex_unlock(&data.mutex);
    pthread_mutex_destroy(&data.mutex);
    pthread_cond_destroy(&data.cond);
    return real_eglSwapBuffers(display, surface);
}

unsigned int eglMakeCurrent(void* display, void* draw, void* read, void* context) {
    unsigned int ret = real_eglMakeCurrent(display, draw, read, context);
    if (ret) {
        pthread_mutex_lock(&egl_lock);
        _bolt_make_context_current(context);
        pthread_mutex_unlock(&egl_lock);
    }
    return ret;
}

unsigned int eglDestroyContext(void* display, void* context) {
    unsigned int ret = real_eglDestroyContext(display, context);
    if (ret) {
        pthread_mutex_lock(&egl_lock);
        _bolt_destroy_context(context);
        pthread_mutex_unlock(&egl_lock);
    }
    return ret;
}

unsigned int eglInitialize(void* display, void* major, void* minor) {
    INIT();
    unsigned int ret = real_eglInitialize(display, major, minor);
    pthread_mutex_lock(&egl_lock);
    if (ret && !worker_thread_running) {
        int socks[2];
        socketpair(AF_UNIX, SOCK_STREAM, 0, socks);
        read_socket = socks[1];
        int err = pthread_create(&worker_thread, NULL, _bolt_worker_thread, NULL);
        if (err) {
            printf("warning: pthread_create returned error %i\n", err);
        }
        write_socket = socks[0];
        worker_thread_running = 1;
    }
    pthread_mutex_unlock(&egl_lock);
    return ret;
}

void* eglCreateContext(void* display, void* config, void* share_context, const void* attrib_list) {
    void* ret = real_eglCreateContext(display, config, share_context, attrib_list);
    pthread_mutex_lock(&egl_lock);
    _bolt_create_context(ret, share_context);
    if (!worker_context_exists && share_context) {
        void* worker_context = real_eglCreateContext(display, config, share_context, attrib_list);
        SEND_MSG({.context = worker_context, .data = display, .instruction = Message_Context})
        worker_context_exists = 1;
    }
    pthread_mutex_unlock(&egl_lock);
    return ret;
}

unsigned int eglTerminate(void* display) {
    pthread_mutex_lock(&egl_lock);
    if (worker_thread_running) {
        SEND_MSG({ .instruction = Message_Quit });
        pthread_join(worker_thread, NULL);
        close(write_socket);
        worker_thread_running = 0;
        worker_context_exists = 0;
    }
    pthread_mutex_unlock(&egl_lock);
    return real_eglTerminate(display);
}

void* xcb_poll_for_event(void* c) {
    if (inited) {

    }
    return real_xcb_poll_for_event(c);
}

void* xcb_wait_for_event(void* c) {
    if (inited) {

    }
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
    //printf("dlopen('%s', %i) -> %lu\n", filename, flags, (unsigned long)ret);
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

// dedicated thread for handling most tasks in a synchronous order, invoked by eglInitialize
void* _bolt_worker_thread(void* arg) {
    void* display = NULL;
    void* context = NULL;
    struct BoltMessage message;
    while (1) {
        if (read(read_socket, &message, sizeof(message)) != sizeof(message)) continue;
        struct GLContext* c = message.context;
        switch (message.instruction) {
            case Message_Quit: {
                close(read_socket);
                if (display) real_eglDestroyContext(display, context);
                return NULL;
            }
            case Message_Context: {
                display = message.data;
                context = c;
                uint32_t b = real_eglMakeCurrent(display, NULL, NULL, context);
                break;
            }
            case Message_glCreateProgram: {
                struct GLProgram* program = _bolt_get_program(c->shared_programs, message.asset);
                program->loc_aVertexPosition2D = -1;
                program->loc_aVertexColour = -1;
                program->loc_aTextureUV = -1;
                program->loc_aTextureUVAtlasMin = -1;
                program->loc_aTextureUVAtlasExtents = -1;
                program->loc_uProjectionMatrix = -1;
                program->loc_uDiffuseMap = -1;
                program->is_important = 0;
                break;
            }
            case Message_glBindAttribLocation: {
                struct GLProgram* p = _bolt_find_program(c->shared_programs, message.asset);
                const char* name = (const char*)message.data;
                if (p) {
                    if (!strcmp(name, "aVertexPosition2D")) p->loc_aVertexPosition2D = message.index;
                    if (!strcmp(name, "aVertexColour")) p->loc_aVertexColour = message.index;
                    if (!strcmp(name, "aTextureUV")) p->loc_aTextureUV = message.index;
                    if (!strcmp(name, "aTextureUVAtlasMin")) p->loc_aTextureUVAtlasMin = message.index;
                    if (!strcmp(name, "aTextureUVAtlasExtents")) p->loc_aTextureUVAtlasExtents = message.index;
                }
                if (message.do_free_data) free(message.data);
                break;
            }
            case Message_glLinkProgram: {
                struct GLProgram* p = _bolt_find_program(c->shared_programs, message.asset);
                if (p && p->loc_aVertexPosition2D != -1 && p->loc_aVertexColour != -1 && p->loc_aTextureUV != -1 && p->loc_aTextureUVAtlasMin != -1 && p->loc_aTextureUVAtlasExtents != -1) {
                    // yeah, this is lazy
                    int uDiffuseMap = (int)message.x;
                    int uProjectionMatrix = (int)message.y;
                    if (uDiffuseMap != -1 && uProjectionMatrix != -1) {
                        p->loc_uDiffuseMap = uDiffuseMap;
                        p->loc_uProjectionMatrix = uProjectionMatrix;
                        p->is_important = 1;
                    }
                }
                break;
            }
            case Message_glUseProgram: {
                if (message.asset != c->bound_program_id) {
                    c->bound_program_id = message.asset;
                    const struct GLProgram* p = _bolt_find_program(c->shared_programs, message.asset);
                    c->current_program_is_important = (p && p->is_important);
                }
                break;
            }
            case Message_glTexStorage2D: {
                struct GLTexture2D* tex = _bolt_get_texture(c->shared_textures, c->bound_texture_id);
                if (tex) {
                    free(tex->data);
                    tex->data = malloc(message.w * message.h * 4);
                    tex->width = message.w;
                    tex->height = message.h;
                }
                break;
            }
            case Message_glVertexAttribPointer: {
                _bolt_set_attr_binding(&c->attributes[message.index], message.asset, message.w, message.data, message.stride, message.type, message.bool_value);
                break;
            }
            case Message_glBufferData:
            case Message_glBufferStorage: {
                struct GLArrayBuffer* buffer = _bolt_get_buffer(c->shared_buffers, message.asset);
                if (buffer) {
                    free(buffer->data);
                    buffer->data = message.data;
                } else if (message.do_free_data) {
                    free(message.data);
                }
                break;
            }
            case Message_glDeleteBuffers: {
                _bolt_context_destroy_buffers(c, message.w, message.data);
                if (message.do_free_data) free(message.data);
                break;
            }
            case Message_glBindFramebuffer: {
                switch (message.target) {
                    case 36008:
                        c->current_read_framebuffer = message.asset;
                        break;
                    case 36009:
                        c->current_draw_framebuffer = message.asset;
                        break;
                    case 36160:
                        c->current_read_framebuffer = message.asset;
                        c->current_draw_framebuffer = message.asset;
                        break;
                }
                break;
            }
            case Message_glCompressedTexSubImage2D: {
                // weird lossy-compression formats with RGB-565 and way too much space dedicated to alpha channels
                // https://www.khronos.org/opengl/wiki/S3_Texture_Compression
                if (message.format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT || message.format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT || message.format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT) {
                    struct GLTexture2D* tex = _bolt_find_texture(c->shared_textures, c->bound_texture_id);
                    if (tex) {
                        int out_xoffset = message.x;
                        int out_yoffset = message.y;
                        for (size_t ii = 0; ii < (message.w * message.h); ii += 16) {
                            const uint8_t* ptr = message.data + ii;
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
                            if (out_xoffset >= message.x + message.w) {
                                out_xoffset = message.x;
                                out_yoffset += 4;
                            }
                        }
                    }
                }
                if (message.do_free_data) free(message.data);
                break;
            }
            case Message_glCopyImageSubData: {
                // {srcName, srcX, srcY, dstName, dstX, dstY, srcWidth, srcHeight}
                int tail[8];
                read(read_socket, tail, 8 * sizeof(int));
                struct GLTexture2D* src = _bolt_find_texture(c->shared_textures, tail[0]);
                struct GLTexture2D* dst = _bolt_find_texture(c->shared_textures, tail[3]);
                if (!src || !dst) break;
                for (size_t i = 0; i < tail[7]; i += 1) {
                    memcpy(dst->data + (tail[5] * dst->width * 4) + (tail[4] * 4), src->data + (tail[2] * src->width * 4) + (tail[1] * 4), tail[6] * 4);
                }
                break;
            }
            case Message_glEnableVertexAttribArray: {
                c->attributes[message.index].enabled = 1;
                break;
            }
            case Message_glDisableVertexAttribArray: {
                c->attributes[message.index].enabled = 0;
                break;
            }
            case Message_glMapBufferRange: {
                struct GLArrayBuffer* buffer = _bolt_find_buffer(c->shared_buffers, message.asset);
                buffer->mapped = 1;
                buffer->mapping_offset = message.x;
                buffer->mapping_len = message.w;
                buffer->mapping_access_type = message.format;
                struct BoltSyncData* data = message.data;
                pthread_mutex_lock(&data->mutex);
                data->ptr = buffer->data + message.x;
                data->done = 1;
                pthread_cond_signal(&data->cond);
                pthread_mutex_unlock(&data->mutex);
                break;
            }
            case Message_glUnmapBuffer: {
                struct GLArrayBuffer* buffer = _bolt_find_buffer(c->shared_buffers, message.asset);
                if (!(buffer->mapping_access_type & GL_MAP_FLUSH_EXPLICIT_BIT)) {
                    real_glBindBuffer(message.target, message.asset);
                    real_glBufferSubData(message.target, buffer->mapping_offset, buffer->mapping_len, buffer->data + buffer->mapping_offset);
                }
                buffer->mapped = 0;
                break;
            }
            case Message_glFlushMappedBufferRange: {
                struct GLArrayBuffer* buffer = _bolt_find_buffer(c->shared_buffers, message.asset);
                real_glBindBuffer(message.target, message.asset);
                real_glBufferSubData(message.target, buffer->mapping_offset + message.x, message.w, buffer->data + buffer->mapping_offset + message.x);
                break;
            }
            case Message_glDrawElements: {
                //if (c->current_program_is_important && c->current_draw_framebuffer == 0 && message.w > 0) {
                //    struct GLArrayBuffer* element_buffer = _bolt_find_buffer(c->shared_buffers, message.asset);
                //    if (!element_buffer || !element_buffer->data) break;
                //    const unsigned short* indices = element_buffer->data + (uintptr_t)message.data;
                //    struct GLProgram* current_program = _bolt_find_program(c->shared_programs, c->bound_program_id);
                //    const struct GLAttrBinding* atlas_min = &c->attributes[current_program->loc_aTextureUVAtlasMin];
                //    const struct GLAttrBinding* atlas_max = &c->attributes[current_program->loc_aTextureUVAtlasExtents];
                //    const struct GLAttrBinding* tex_uv = &c->attributes[current_program->loc_aTextureUV];
                //    int diffuse_map;
                //    real_glGetUniformiv(c->bound_program_id, current_program->loc_uDiffuseMap, &diffuse_map);
                //    if (atlas_min->enabled && atlas_max->enabled && tex_uv->enabled) {
                //        //printf("(worker thread) glDrawElements ebo=%u diffuse_map=%i (%u) atlas_min normalised=%u\n", message.asset, diffuse_map, current_program->loc_uDiffuseMap, (unsigned int)atlas_min->normalise);
                //        for (size_t i = 0; i + 2 < message.w; i += 3) {
                //            unsigned short index1 = indices[i];
                //            unsigned short index2 = indices[i + 1];
                //            unsigned short index3 = indices[i + 2];
                //            float min1[2];
                //            float min2[2];
                //            float min3[2];
                //            float max1[2];
                //            float max2[2];
                //            float max3[2];
                //            float uv1[2];
                //            float uv2[2];
                //            float uv3[2];
                //            _bolt_get_attr_binding(c, atlas_min, index1, 2, min1);
                //            _bolt_get_attr_binding(c, atlas_min, index2, 2, min2);
                //            _bolt_get_attr_binding(c, atlas_min, index3, 2, min3);
                //            _bolt_get_attr_binding(c, atlas_max, index1, 2, max1);
                //            _bolt_get_attr_binding(c, atlas_max, index2, 2, max2);
                //            _bolt_get_attr_binding(c, atlas_max, index3, 2, max3);
                //            _bolt_get_attr_binding(c, tex_uv, index1, 2, uv1);
                //            _bolt_get_attr_binding(c, tex_uv, index2, 2, uv2);
                //            _bolt_get_attr_binding(c, tex_uv, index3, 2, uv3);
                //            //printf("drawing triangle, uDiffuseMap=%i, atlas extents %f,%f,%f,%f | %f,%f,%f,%f | %f,%f,%f,%f UV %f,%f %f,%f %f,%f\n",
                //            //    diffuse_map, min1[0], min1[1], max1[0], max1[1], min2[0], min2[1], max2[0], max2[1], min3[0], min3[1], max3[0], max3[1],
                //            //    uv1[0], uv1[1], uv2[0], uv2[1], uv3[0], uv3[1]);
                //        }
                //    }
                //}
                break;
            }
            case Message_glBindTexture: {
                if (message.target == GL_TEXTURE_2D) {
                    c->bound_texture_id = message.asset;
                }
                break;
            }
            case Message_glTexSubImage2D: {
                if (message.target == GL_TEXTURE_2D && message.format == GL_RGBA) {
                    struct GLTexture2D* tex = _bolt_find_texture(c->shared_textures, c->bound_texture_id);
                    if (tex && !(message.x < 0 || message.y < 0 || message.x + message.w > tex->width || message.y + message.h > tex->height)) {
                        for (unsigned int y = 0; y < message.h; y += 1) {
                            unsigned char* dest_ptr = tex->data + ((tex->width * (y + message.y)) + message.x) * 4;
                            const void* src_ptr = message.data + (message.w * y * 4);
                            memcpy(dest_ptr, src_ptr, message.w * 4);
                        }
                    }
                }
                if (message.do_free_data) free(message.data);
                break;
            }
            case Message_glDeleteTextures: {
                _bolt_context_destroy_textures(c, message.w, message.data);
                if (message.do_free_data) free(message.data);
                break;
            }
            case Message_eglSwapBuffers: {
                struct BoltSyncData* data = message.data;
                pthread_mutex_lock(&data->mutex);
                data->done = 1;
                pthread_cond_signal(&data->cond);
                pthread_mutex_unlock(&data->mutex);
            }
            case Message_glFlush: {
                struct BoltSyncData* data = message.data;
                pthread_mutex_lock(&data->mutex);
                data->done = 1;
                pthread_cond_signal(&data->cond);
                pthread_mutex_unlock(&data->mutex);
                real_glFlush();
            }
        }
    }
}
