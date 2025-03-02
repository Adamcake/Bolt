#define _GNU_SOURCE
#include <link.h>
#include <unistd.h>
#undef _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "x.h"
#include "../gl.h"
#include "../plugin/plugin.h"
#include "../../../modules/hashmap/hashmap.h"

// -D BOLT_LIBRARY_VERBOSE=1
#if defined(VERBOSE)
#include <sys/syscall.h>
#define LOG(STR) if(printf("[tid %li] " STR, syscall(SYS_gettid)))fflush(stdout)
#define LOGF(STR, ...) if(printf("[tid %li] " STR, syscall(SYS_gettid), __VA_ARGS__))fflush(stdout)
#else
#define LOG(...)
#define LOGF(...)
#endif

#if defined(XCBVERBOSE)
#define XCBLOG(...) if(printf(__VA_ARGS__)) fflush(stdout)
#else
#define XCBLOG(...)
#endif

// note: this is currently always triggered by single-threaded dlopen calls so no locking necessary
static uint8_t inited = 0;
#define INIT() if (!inited) _bolt_init_functions();

static pthread_mutex_t egl_lock;

static pthread_mutex_t window_size_lock;
static xcb_window_t main_window_xcb = 0;
static uint32_t main_window_width = 0;
static uint32_t main_window_height = 0;

static uint8_t mousein_real = 0;
static uint8_t mousein_fake = 0;

static const char* libc_name = "libc.so.6";
static const char* libegl_name = "libEGL.so.1";
static const char* libgl_name = "libGL.so.1";
static const char* libxcb_name = "libxcb.so.1";
static const char* libx11_name = "libX11.so.6";

static void* libc_addr = 0;
static void* libegl_addr = 0;
static void* libgl_addr = 0;
static void* libxcb_addr = 0;
static void* libx11_addr = 0;

static void* (*real_dlopen)(const char*, int) = NULL;
static void* (*real_dlsym)(void*, const char*) = NULL;
static void* (*real_dlvsym)(void*, const char*, const char*) = NULL;
static int (*real_dlclose)(void*) = NULL;

static void* (*real_eglGetProcAddress)(const char*) = NULL;
static unsigned int (*real_eglSwapBuffers)(void*, void*) = NULL;
static unsigned int (*real_eglMakeCurrent)(void*, void*, void*, void*) = NULL;
static unsigned int (*real_eglDestroyContext)(void*, void*) = NULL;
static unsigned int (*real_eglInitialize)(void*, void*, void*) = NULL;
static void* (*real_eglCreateContext)(void*, void*, void*, const void*) = NULL;
static unsigned int (*real_eglTerminate)(void*) = NULL;

static xcb_generic_event_t* (*real_xcb_poll_for_event)(xcb_connection_t*) = NULL;
static xcb_generic_event_t* (*real_xcb_poll_for_queued_event)(xcb_connection_t*) = NULL;
static xcb_generic_event_t* (*real_xcb_wait_for_event)(xcb_connection_t*) = NULL;
static xcb_void_cookie_t (*real_xcb_get_property)(xcb_connection_t*, uint8_t, xcb_window_t, xcb_atom_t, xcb_atom_t, uint32_t, uint32_t) = NULL;
static xcb_get_property_reply_t* (*real_xcb_get_property_reply)(xcb_connection_t*, xcb_void_cookie_t, xcb_generic_event_t**) = NULL;
static int (*real_xcb_get_property_value_length)(const xcb_get_property_reply_t*) = NULL;
static void* (*real_xcb_get_property_value)(const xcb_get_property_reply_t*) = NULL;
static xcb_void_cookie_t (*real_xcb_change_property)(xcb_connection_t*, uint8_t, xcb_window_t, xcb_atom_t, xcb_atom_t, uint8_t, uint32_t, const void*) = NULL;
static xcb_void_cookie_t (*real_xcb_change_window_attributes)(xcb_connection_t*, xcb_window_t, uint32_t, const uint32_t*) = NULL;
static int (*real_xcb_flush)(xcb_connection_t*) = NULL;

static int (*real_XDefineCursor)(void*, XWindow, XCursor) = NULL;
static int (*real_XUndefineCursor)(void*, XWindow) = NULL;
static int (*real_XFreeCursor)(void*, XCursor) = NULL;

static struct GLLibFunctions libgl = {0};

static uint32_t xcursor_cursor;
static uint8_t xcursor_defined = false;
static uint8_t xcursor_change_pending = 0;
static pthread_mutex_t xcursor_lock;

static pthread_mutex_t focus_lock;
static uint8_t window_focus = false;

static uint8_t flash_change_pending = false;
static uint8_t pending_flash;

static ElfW(Word) _bolt_hash_elf(const char* name) {
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

static Elf32_Word _bolt_hash_gnu(const char* name) {
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
static const ElfW(Sym)* _bolt_lookup_symbol(const char* symbol_name, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
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

static void _bolt_init_libc(unsigned long addr, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
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

static void _bolt_init_libegl(unsigned long addr, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
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

static void _bolt_init_libgl(unsigned long addr, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
    libgl_addr = (void*)addr;
    const ElfW(Sym)* sym = _bolt_lookup_symbol("glBindTexture", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.BindTexture = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glBlendFunc", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.BlendFunc = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glClear", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.Clear = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glClearColor", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.ClearColor = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDeleteTextures", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.DeleteTextures = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDisable", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.Disable = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDrawArrays", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.DrawArrays = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDrawElements", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.DrawElements = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glEnable", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.Enable = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glFlush", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.Flush = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glGenTextures", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.GenTextures = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glGetBooleanv", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.GetBooleanv = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glGetError", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.GetError = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glGetIntegerv", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.GetIntegerv = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glReadPixels", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.ReadPixels = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glTexParameteri", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.TexParameteri = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glTexSubImage2D", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.TexSubImage2D = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glViewport", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.Viewport = sym->st_value + libgl_addr;
}

static void _bolt_init_libxcb(unsigned long addr, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
    libxcb_addr = (void*)addr;
    const ElfW(Sym)* sym = _bolt_lookup_symbol("xcb_poll_for_event", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_poll_for_event = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_poll_for_queued_event", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_poll_for_queued_event = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_wait_for_event", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_wait_for_event = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_get_property", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_get_property = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_get_property_reply", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_get_property_reply = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_get_property_value_length", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_get_property_value_length = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_get_property_value", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_get_property_value = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_change_property", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_change_property = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_change_window_attributes", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_change_window_attributes = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_flush", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_flush = sym->st_value + libxcb_addr;
}

static void _bolt_init_libx11(unsigned long addr, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
    libx11_addr = (void*)addr;
    const ElfW(Sym)* sym = _bolt_lookup_symbol("XDefineCursor", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_XDefineCursor = sym->st_value + libx11_addr;
    sym = _bolt_lookup_symbol("XUndefineCursor", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_XUndefineCursor = sym->st_value + libx11_addr;
    sym = _bolt_lookup_symbol("XFreeCursor", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_XFreeCursor = sym->st_value + libx11_addr;
}

static int _bolt_dl_iterate_callback(struct dl_phdr_info* info, size_t size, void* args) {
    const size_t name_len = strlen(info->dlpi_name);
    const size_t libc_name_len = strlen(libc_name);
    const size_t libegl_name_len = strlen(libegl_name);
    const size_t libgl_name_len = strlen(libgl_name);
    const size_t libxcb_name_len = strlen(libxcb_name);
    const size_t libx11_name_len = strlen(libx11_name);

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
#define FILENAME_EQ_INIT(NAME) { \
    const size_t lib_name_len = strlen(NAME##_name); \
    if (name_len > lib_name_len && info->dlpi_name[name_len - lib_name_len - 1] == '/' && !strcmp(info->dlpi_name + name_len - lib_name_len, NAME##_name)) { \
        if (NAME##_addr == (void*)info->dlpi_addr) return 0; \
        _bolt_init_##NAME(info->dlpi_addr, gnu_hash_table, hash_table, string_table, symbol_table); \
        return 0; \
    } \
}
    FILENAME_EQ_INIT(libc);
    FILENAME_EQ_INIT(libegl);
    FILENAME_EQ_INIT(libxcb);
    FILENAME_EQ_INIT(libgl);
    FILENAME_EQ_INIT(libx11);
#undef FILENAME_EQ_INIT

	return 0;
}

static void _bolt_init_functions() {
    _bolt_plugin_on_startup();
    pthread_mutex_init(&egl_lock, NULL);
    pthread_mutex_init(&window_size_lock, NULL);
    pthread_mutex_init(&focus_lock, NULL);
    pthread_mutex_init(&xcursor_lock, NULL);
    dl_iterate_phdr(_bolt_dl_iterate_callback, NULL);
#if defined(VERBOSE)
    _bolt_gl_set_logfile(stdout);
#endif
    inited = 1;
}

void _bolt_flash_window(void) {
    pthread_mutex_lock(&focus_lock);
    const uint8_t focus = window_focus;
    pthread_mutex_unlock(&focus_lock);
    if (focus) return;
    pthread_mutex_lock(&xcursor_lock);
    flash_change_pending = true;
    pending_flash = true;
    pthread_mutex_unlock(&xcursor_lock);
}

uint8_t _bolt_window_has_focus(void) {
    pthread_mutex_lock(&focus_lock);
    const uint8_t focus = window_focus;
    pthread_mutex_unlock(&focus_lock);
    return focus;
}

void glGenTextures(GLsizei n, GLuint* textures) {
    LOG("glGenTextures\n");
    libgl.GenTextures(n, textures);
    _bolt_gl_onGenTextures(n, textures);
    LOG("glGenTextures end\n");
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices_offset) {
    LOG("glDrawElements\n");
    libgl.DrawElements(mode, count, type, indices_offset);
    _bolt_gl_onDrawElements(mode, count, type, indices_offset);
    LOG("glDrawElements end\n");
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    LOG("glDrawArrays\n");
    libgl.DrawArrays(mode, first, count);
    _bolt_gl_onDrawArrays(mode, first, count);
    LOG("glDrawArrays end\n");
}

void glBindTexture(GLenum target, GLuint texture) {
    LOG("glBindTexture\n");
    libgl.BindTexture(target, texture);
    _bolt_gl_onBindTexture(target, texture);
    LOG("glBindTexture end\n");
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    LOG("glTexSubImage2D\n");
    libgl.TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    _bolt_gl_onTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    LOG("glTexSubImage2D end\n");
}

void glDeleteTextures(GLsizei n, const GLuint* textures) {
    LOG("glDeleteTextures\n");
    _bolt_gl_onDeleteTextures(n, textures);
    libgl.DeleteTextures(n, textures);
    LOG("glDeleteTextures end\n");
}

void glClear(GLbitfield mask) {
    LOG("glClear\n");
    libgl.Clear(mask);
    _bolt_gl_onClear(mask);
    LOG("glClear end\n");
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    LOG("glViewport\n");
    libgl.Viewport(x, y, width, height);
    _bolt_gl_onViewport(x, y, width, height);
    LOG("glViewport end\n");
}

void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    LOG("glTexParameteri\n");
    libgl.TexParameteri(target, pname, param);
    _bolt_gl_onTexParameteri(target, pname, param);
    LOG("glTexParameteri end\n");
}

void glBlendFunc(GLenum sfactor, GLenum dfactor) {
    LOG("glBlendFunc\n");
    libgl.BlendFunc(sfactor, dfactor);
    _bolt_gl_onBlendFunc(sfactor, dfactor);
    LOG("glBlendFunc end\n");
}

void* eglGetProcAddress(const char* name) {
    LOGF("eglGetProcAddress('%s')\n", name);
    void* ret = _bolt_gl_GetProcAddress(name);
    return ret ? ret : real_eglGetProcAddress(name);
}

unsigned int eglSwapBuffers(void* display, void* surface) {
    LOG("eglSwapBuffers\n");
    pthread_mutex_lock(&window_size_lock);
    const uint32_t w = main_window_width;
    const uint32_t h = main_window_height;
    pthread_mutex_unlock(&window_size_lock);
    _bolt_gl_onSwapBuffers(w, h);
    unsigned int ret = real_eglSwapBuffers(display, surface);
    LOGF("eglSwapBuffers end (returned %u)\n", ret);
    return ret;
}

void* eglCreateContext(void* display, void* config, void* share_context, const void* attrib_list) {
    LOG("eglCreateContext\n");
    void* ret = real_eglCreateContext(display, config, share_context, attrib_list);
    if (ret) {
        pthread_mutex_lock(&egl_lock);
        _bolt_gl_onCreateContext(ret, share_context, &libgl, real_eglGetProcAddress, true);
        pthread_mutex_unlock(&egl_lock);
    }
    LOGF("eglCreateContext end (returned %lu)\n", (uintptr_t)ret);
    return ret;
}

unsigned int eglMakeCurrent(void* display, void* draw, void* read, void* context) {
    LOG("eglMakeCurrent\n");
    unsigned int ret = real_eglMakeCurrent(display, draw, read, context);
    if (ret) {
        pthread_mutex_lock(&egl_lock);
        _bolt_gl_onMakeCurrent(context);
        pthread_mutex_unlock(&egl_lock);
    }
    LOGF("eglMakeCurrent end (returned %u)\n", ret);
    return ret;
}

unsigned int eglDestroyContext(void* display, void* context) {
    LOGF("eglDestroyContext %lu\n", (uintptr_t)context);
    unsigned int ret = real_eglDestroyContext(display, context);
    if (ret) {
        pthread_mutex_lock(&egl_lock);
        void* c = _bolt_gl_onDestroyContext(context);
        if (c) {
            real_eglMakeCurrent(display, NULL, NULL, c);
            _bolt_gl_close();
            real_eglMakeCurrent(display, NULL, NULL, NULL);
            real_eglTerminate(display);
        }
        pthread_mutex_unlock(&egl_lock);
    }
    LOGF("eglDestroyContext end (returned %u)\n", ret);
    return ret;
}

unsigned int eglInitialize(void* display, void* major, void* minor) {
    INIT();
    LOG("eglInitialize\n");
    return real_eglInitialize(display, major, minor);
}

unsigned int eglTerminate(void* display) {
    LOG("eglTerminate\n");
    // the game calls this function once on startup and 27 times on shutdown, despite only needing
    // to call it once... bolt defers it to eglDestroyContext so we can do cleanup more easily.
    //return real_eglTerminate(display);
    return 1;
}

static void _bolt_mouseevent_from_xcb(int16_t x, int16_t y, uint32_t state, struct MouseEvent* out) {
    out->x = x;
    out->y = y;
    out->ctrl = (state >> 2) & 1;
    out->shift = state & 1;
    out->meta = (state >> 6) & 1;
    out->alt = (((1 << 3) | (1 << 7)) & state) ? 1 : 0;
    out->capslock = (state >> 1) & 1;
    out->numlock = (state >> 4) & 1;
    out->mb_left = (state >> 8) & 1;
    out->mb_right = (state >> 10) & 1;
    out->mb_middle = (state >> 9) & 1;
}

static uint8_t point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return rx <= x && rx + rw > x && ry <= y && ry + rh > y;
}

static uint8_t handle_mouse_event(int16_t x, int16_t y, uint32_t state, ptrdiff_t bool_offset, ptrdiff_t event_offset, uint8_t grab_type) {
    struct MouseEvent event;
    _bolt_mouseevent_from_xcb(x, y, state, &event);
    const uint8_t mousein_fake_old = mousein_fake;
    const uint8_t ret = _bolt_plugin_handle_mouse_event(&event, bool_offset, event_offset, grab_type, &mousein_fake, &mousein_real);
    if (mousein_fake_old != mousein_fake) {
        pthread_mutex_lock(&xcursor_lock);
        xcursor_change_pending = ret ? 1 : 2;
        pthread_mutex_unlock(&xcursor_lock);
    }
    return ret;
}

static uint8_t handle_buttonpress_event(int16_t x, int16_t y, uint32_t detail, uint16_t state) {
    switch (detail) {
        case 1:
            return handle_mouse_event(x, y, state, offsetof(struct WindowPendingInput, mouse_left), offsetof(struct WindowPendingInput, mouse_left_event), GRAB_TYPE_START);
        case 2:
            return handle_mouse_event(x, y, state, offsetof(struct WindowPendingInput, mouse_middle), offsetof(struct WindowPendingInput, mouse_middle_event), GRAB_TYPE_NONE);
        case 3:
            return handle_mouse_event(x, y, state, offsetof(struct WindowPendingInput, mouse_right), offsetof(struct WindowPendingInput, mouse_right_event), GRAB_TYPE_NONE);
        case 4:
            return handle_mouse_event(x, y, state, offsetof(struct WindowPendingInput, mouse_scroll_up), offsetof(struct WindowPendingInput, mouse_scroll_up_event), GRAB_TYPE_NONE);
        case 5:
            return handle_mouse_event(x, y, state, offsetof(struct WindowPendingInput, mouse_scroll_down), offsetof(struct WindowPendingInput, mouse_scroll_down_event), GRAB_TYPE_NONE);
    }
    return true;
}

static uint8_t handle_buttonrelease_event(int16_t x, int16_t y, uint32_t detail, uint16_t state) {
    switch (detail) {
        case 1:
            return handle_mouse_event(x, y, state, offsetof(struct WindowPendingInput, mouse_left_up), offsetof(struct WindowPendingInput, mouse_left_up_event), GRAB_TYPE_STOP);
        case 2:
            return handle_mouse_event(x, y, state, offsetof(struct WindowPendingInput, mouse_middle_up), offsetof(struct WindowPendingInput, mouse_middle_up_event), GRAB_TYPE_NONE);
        case 3:
            return handle_mouse_event(x, y, state, offsetof(struct WindowPendingInput, mouse_right_up), offsetof(struct WindowPendingInput, mouse_right_up_event), GRAB_TYPE_NONE);
        case 4:
        case 5: {
            // for mousewheel-up events, we don't need to do anything with them, but we do
            // still need to figure out if they should go to the game window or not.
            struct WindowInfo* windows = _bolt_plugin_windowinfo();
            uint8_t ret = true;
            _bolt_rwlock_lock_read(&windows->lock);
            size_t iter = 0;
            void* item;
            while (hashmap_iter(windows->map, &iter, &item)) {
                struct EmbeddedWindow** window = item;
                _bolt_rwlock_lock_read(&(*window)->lock);
                const uint8_t in_window = point_in_rect(x, y, (*window)->metadata.x, (*window)->metadata.y, (*window)->metadata.width, (*window)->metadata.height);
                _bolt_rwlock_unlock_read(&(*window)->lock);
                ret &= !in_window;
                if (!ret) break;
            }
            _bolt_rwlock_unlock_read(&windows->lock);
            return ret;
        }
    }
    return true;
}

static uint8_t handle_mouseleave_event(int16_t x, int16_t y, uint16_t state) {
    struct WindowInfo* windows = _bolt_plugin_windowinfo();
    _bolt_rwlock_lock_read(&windows->lock);
    const uint64_t p = _bolt_plugin_get_last_mouseevent_windowid();
    const uint64_t* pp = &p;
    struct EmbeddedWindow* const* window = hashmap_get(windows->map, &pp);
    if (window) {
        _bolt_rwlock_lock_read(&(*window)->lock);
        const int16_t wx = (*window)->metadata.x;
        const int16_t wy = (*window)->metadata.y;
        _bolt_rwlock_unlock_read(&(*window)->lock);
        _bolt_rwlock_lock_write(&(*window)->input_lock);
        (*window)->input.mouse_leave = 1;
        _bolt_mouseevent_from_xcb(x - wx, y - wy, state, &(*window)->input.mouse_leave_event);
        _bolt_rwlock_unlock_write(&(*window)->input_lock);
    }
    _bolt_rwlock_unlock_read(&windows->lock);
    const uint8_t ret = mousein_fake;
    mousein_real = 0;
    mousein_fake = 0;
    return ret;
}

// returns true if the event should be passed on to the window, false if not
static uint8_t _bolt_handle_xcb_event(xcb_connection_t* c, xcb_generic_event_t* e) {
    if (!e) return true;
    if (!main_window_xcb) {
        if (e->response_type == XCB_MAP_NOTIFY) {
            const xcb_map_notify_event_t* const event = (const xcb_map_notify_event_t* const)e;
            main_window_xcb = event->window;
            printf("new main window %u\n", event->window);
        }
        return true;
    }

    // comments here are based on what event masks the game normally sets on each window
    const uint8_t response_type = e->response_type & 0b01111111;
    switch (response_type) {
        case XCB_GE_GENERIC: {
            const xcb_ge_generic_event_t* const event = (const xcb_ge_generic_event_t* const)e;
            if (event->extension != XINPUTEXTENSION) break;
            switch (event->event_type) {
                case XCB_INPUT_BUTTON_PRESS: { // when pressing a mouse button that's received by the game window (SDL3 only)
                    const xcb_input_button_press_event_t* event = (xcb_input_button_press_event_t*)e;
                    if (event->event != main_window_xcb) return true;
                    return handle_buttonpress_event(event->event_x >> 16, event->event_y >> 16, event->detail, event->mods.effective);
                }
                case XCB_INPUT_BUTTON_RELEASE: { // when releasing a mouse button that's received by the game window (SDL3 only)
                    const xcb_input_button_release_event_t* event = (xcb_input_button_release_event_t*)e;
                    if (event->event != main_window_xcb) return true;
                    return handle_buttonrelease_event(event->event_x >> 16, event->event_y >> 16, event->detail, event->mods.effective);
                }
                case XCB_INPUT_MOTION: { // when mouse moves (not drag) inside the game window
                    const xcb_input_motion_event_t* event = (xcb_input_motion_event_t*)e;
                    if (event->event != main_window_xcb) return true;
                    return handle_mouse_event(event->event_x >> 16, event->event_y >> 16, event->mods.effective, offsetof(struct WindowPendingInput, mouse_motion), offsetof(struct WindowPendingInput, mouse_motion_event), GRAB_TYPE_NONE);
                }
                case XCB_INPUT_ENTER: { // when mouse moves into this window, having previously been in another window (SDL3 only)
                    const xcb_input_enter_event_t* event = (xcb_input_enter_event_t*)e;
                    if (event->event != main_window_xcb) return true;
                    return handle_mouse_event(event->event_x >> 16, event->event_y >> 16, event->mods.effective, offsetof(struct WindowPendingInput, mouse_motion), offsetof(struct WindowPendingInput, mouse_motion_event), GRAB_TYPE_NONE);
                }
                case XCB_INPUT_LEAVE: { // when mouse moves into another window, having previously been in this window (SDL3 only)
                    const xcb_input_leave_event_t* event = (xcb_input_leave_event_t*)e;
                    if (event->event != main_window_xcb) return true;
                    return handle_mouseleave_event(event->event_x >> 16, event->event_y >> 16, event->mods.effective);
                }
                case XCB_INPUT_RAW_MOTION: // when mouse moves (not drag) anywhere globally on the PC
                case XCB_INPUT_RAW_BUTTON_PRESS: // when pressing a mouse button anywhere globally on the PC
                case XCB_INPUT_RAW_BUTTON_RELEASE: // when releasing a mouse button anywhere globally on the PC
                    break;
                default:
                    printf("unknown xinput event %u\n", event->event_type);
                    break;
            }
            break;
        }
        case XCB_BUTTON_PRESS: { // when pressing a mouse button that's received by the game window (SDL2 only)
            const xcb_button_press_event_t* event = (xcb_button_release_event_t*)e;
            if (event->event != main_window_xcb) return true;
            return handle_buttonpress_event(event->event_x, event->event_y, event->detail, event->state);
        }
        case XCB_BUTTON_RELEASE: { // when releasing a mouse button for which the press was received by the game window (SDL2 only)
            const xcb_button_release_event_t* event = (xcb_button_release_event_t*)e;
            if (event->event != main_window_xcb) return true;
            return handle_buttonrelease_event(event->event_x, event->event_y, event->detail, event->state);
        }
        case XCB_MOTION_NOTIFY: { // when mouse moves while dragging from inside the game window
            const xcb_motion_notify_event_t* event = (xcb_motion_notify_event_t*)e;
            if (event->event != main_window_xcb) return true;
            return handle_mouse_event(event->event_x, event->event_y, event->state, offsetof(struct WindowPendingInput, mouse_motion), offsetof(struct WindowPendingInput, mouse_motion_event), GRAB_TYPE_NONE);
        }
        case XCB_ENTER_NOTIFY: { // when mouse moves into this window, having previously been in another window
            const xcb_enter_notify_event_t* event = (xcb_enter_notify_event_t*)e;
            if (event->event != main_window_xcb) return true;
            return handle_mouse_event(event->event_x, event->event_y, event->state, offsetof(struct WindowPendingInput, mouse_motion), offsetof(struct WindowPendingInput, mouse_motion_event), GRAB_TYPE_NONE);
        }
        case XCB_LEAVE_NOTIFY: { // when mouse moves into another window, having previously been in this window
            const xcb_leave_notify_event_t* event = (xcb_leave_notify_event_t*)e;
            if (event->event != main_window_xcb) return true;
            return handle_mouseleave_event(event->event_x, event->event_y, event->state);
        }
        case XCB_KEY_PRESS: // when a keyboard key is pressed while the game window is focused
        case XCB_KEY_RELEASE: // when a keyboard key is released while the game window is focused
            break;
        case XCB_CONFIGURE_NOTIFY: { // when the game window gets resized (or when a few other things happen)
            const xcb_configure_notify_event_t* event = (xcb_configure_notify_event_t*)e;
            if (event->event != main_window_xcb) return true;
            pthread_mutex_lock(&window_size_lock);
            main_window_width = event->width;
            main_window_height = event->height;
            pthread_mutex_unlock(&window_size_lock);
            break;
        }
        case XCB_FOCUS_IN: { // when the game window gains focus
            const xcb_focus_in_event_t* event = (xcb_focus_in_event_t*)e;
            if (event->event != main_window_xcb) return true;
            pthread_mutex_lock(&focus_lock);
            window_focus = true;
            pthread_mutex_unlock(&focus_lock);
            pthread_mutex_lock(&xcursor_lock);
            flash_change_pending = true;
            pending_flash = false;
            pthread_mutex_unlock(&xcursor_lock);
            break;
        }
        case XCB_FOCUS_OUT: {// when the game window loses focus
            const xcb_focus_out_event_t* event = (xcb_focus_out_event_t*)e;
            if (event->event != main_window_xcb) return true;
            pthread_mutex_lock(&focus_lock);
            window_focus = false;
            pthread_mutex_unlock(&focus_lock);
            break;
        }
        case XCB_KEYMAP_NOTIFY:
        case XCB_EXPOSE:
        case XCB_VISIBILITY_NOTIFY:
        case XCB_MAP_NOTIFY:
        case XCB_UNMAP_NOTIFY:
        case XCB_REPARENT_NOTIFY:
        case XCB_DESTROY_NOTIFY:
        case XCB_PROPERTY_NOTIFY:
        case XCB_CLIENT_MESSAGE:
            break;
        default:
            //printf("unknown xcb event %u\n", response_type);
            break;
    }
    return true;
}

xcb_generic_event_t* xcb_poll_for_event(xcb_connection_t* c) {
    XCBLOG("xcb_poll_for_event\n");
    xcb_generic_event_t* ret = real_xcb_poll_for_event(c);
    while (true) {
        if (_bolt_handle_xcb_event(c, ret)) {
            XCBLOG("xcb_poll_for_event end\n");
            return ret;
        }
        ret = real_xcb_poll_for_queued_event(c);
    }
}

xcb_generic_event_t* xcb_poll_for_queued_event(xcb_connection_t* c) {
    XCBLOG("xcb_poll_for_queued_event\n");
    xcb_generic_event_t* ret;
    while (true) {
        ret = real_xcb_poll_for_queued_event(c);
        if (_bolt_handle_xcb_event(c, ret)) {
            XCBLOG("xcb_poll_for_queued_event end\n");
            return ret;
        }
    }
}

xcb_generic_event_t* xcb_wait_for_event(xcb_connection_t* c) {
    XCBLOG("xcb_wait_for_event\n");
    xcb_generic_event_t* ret;
    while (true) {
        ret = real_xcb_wait_for_event(c);
        if (_bolt_handle_xcb_event(c, ret)) {
            XCBLOG("xcb_wait_for_event end\n");
            return ret;
        }
    }
}

int xcb_flush(xcb_connection_t* c) {
    pthread_mutex_lock(&xcursor_lock);
    const uint8_t change_pending = flash_change_pending;
    uint8_t do_flash = pending_flash;
    flash_change_pending = false;

    switch (xcursor_change_pending) {
        case 1:
            if (xcursor_defined) {
                xcursor_change_pending = 0;
                const uint32_t cursor = xcursor_cursor;
                pthread_mutex_unlock(&xcursor_lock);
                real_xcb_change_window_attributes(c, main_window_xcb, XCB_CW_CURSOR, &cursor);
            } else {
                pthread_mutex_unlock(&xcursor_lock);
            }
            break;
        case 2:
            xcursor_change_pending = 0;
            pthread_mutex_unlock(&xcursor_lock);
            const uint32_t value = XCB_NONE;
            real_xcb_change_window_attributes(c, main_window_xcb, XCB_CW_CURSOR, &value);
            break;
        default:
            pthread_mutex_unlock(&xcursor_lock);
    }

    if (change_pending) {
        bool do_change = true;
        if (do_flash) {
            pthread_mutex_lock(&focus_lock);
            const uint8_t focus = window_focus;
            pthread_mutex_unlock(&focus_lock);
            if (focus) c = false;
        }

        if (do_change) {
            XWMHints hints;
            XWMHints* ptr_hints = NULL;
            uint32_t hints_length_dwords = sizeof(XWMHints) >> 2;
            const xcb_void_cookie_t cookie = real_xcb_get_property(c, false, main_window_xcb, XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 0, hints_length_dwords);
            xcb_get_property_reply_t* const reply = real_xcb_get_property_reply(c, cookie, NULL);

            if (reply) {
                const int length = real_xcb_get_property_value_length(reply);
                if (length > 0) {
                    hints_length_dwords = length;
                    ptr_hints = real_xcb_get_property_value(reply);
                    if (do_flash) {
                        ptr_hints->flags |= (1 << 8);
                    } else {
                        ptr_hints->flags &= ~(1 << 8);
                    }
                }
            }

            if (!ptr_hints) {
                memset(&hints, 0, sizeof hints);
                if (do_flash) hints.flags = (1 << 8);
                ptr_hints = &hints;
            }

            real_xcb_change_property(c, XCB_PROP_MODE_REPLACE, main_window_xcb, XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 32, hints_length_dwords, ptr_hints);
            free(reply);
        }
    }

    return real_xcb_flush(c);
}

int XDefineCursor(void* display, XWindow w, XCursor cursor) {
    if (w != main_window_xcb) return real_XDefineCursor(display, w, cursor);
    if (mousein_fake) {
        const int ret = real_XDefineCursor(display, w, cursor);
        if (!ret) return ret;
    }
    pthread_mutex_lock(&xcursor_lock);
    xcursor_cursor = cursor;
    xcursor_defined = true;
    pthread_mutex_unlock(&xcursor_lock);
    return 1;
}

int XUndefineCursor(void* display, XWindow w) {
    int ret = real_XUndefineCursor(display, w);
    if (!ret) return ret;
    if (w != main_window_xcb) return ret;
    pthread_mutex_lock(&xcursor_lock);
    xcursor_defined = false;
    pthread_mutex_unlock(&xcursor_lock);
    return ret;
}

int XFreeCursor(void* display, XCursor cursor) {
    int ret = real_XFreeCursor(display, cursor);
    if (!ret) return ret;
    pthread_mutex_lock(&xcursor_lock);
    if (cursor == xcursor_cursor) {
        xcursor_defined = false;
    }
    pthread_mutex_unlock(&xcursor_lock);
    return ret;
}

void* dlopen(const char*, int);
void* dlsym(void*, const char*);
void* dlvsym(void*, const char*, const char*);
int dlclose(void*);

static void* _bolt_dl_lookup(void* handle, const char* symbol) {
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
        if (strcmp(symbol, "glClear") == 0) return glClear;
        if (strcmp(symbol, "glViewport") == 0) return glViewport;
        if (strcmp(symbol, "glTexParameteri") == 0) return glTexParameteri;
        if (strcmp(symbol, "glBlendFunc") == 0) return glBlendFunc;
    } else if (handle == libxcb_addr) {
        if (strcmp(symbol, "xcb_poll_for_event") == 0) return xcb_poll_for_event;
        if (strcmp(symbol, "xcb_poll_for_queued_event") == 0) return xcb_poll_for_queued_event;
        if (strcmp(symbol, "xcb_wait_for_event") == 0) return xcb_wait_for_event;
        if (strcmp(symbol, "xcb_flush") == 0) return xcb_flush;
    } else if (handle == libx11_addr) {
        if (strcmp(symbol, "XDefineCursor") == 0) return XDefineCursor;
        if (strcmp(symbol, "XUndefineCursor") == 0) return XUndefineCursor;
        if (strcmp(symbol, "XFreeCursor") == 0) return XFreeCursor;
    }
    return NULL;
}

void* dlopen(const char* filename, int flags) {
    INIT();
    void* ret = real_dlopen(filename, flags);
    LOGF("dlopen('%s', %i) -> %lu\n", filename, flags, (unsigned long)ret);
    // recheck if any of the modules we want to hook are now in our address space.
    // we can't strcmp filename, because it might not be something of interest but depend on something
    // of interest. for example, filename might be "libX11.so.6", which isn't of interest to bolt, but
    // it imports "libxcb.so.1", which is.
    dl_iterate_phdr(_bolt_dl_iterate_callback, NULL);
    return ret;
}

void* dlsym(void* handle, const char* symbol) {
    INIT();
    LOGF("dlsym(%lu, '%s')\n", (uintptr_t)handle, symbol);
    void* f = _bolt_dl_lookup(handle, symbol);
    return f ? f : real_dlsym(handle, symbol);
}

void* dlvsym(void* handle, const char* symbol, const char* version) {
    INIT();
    LOGF("dlvsym(%lu, '%s')\n", (uintptr_t)handle, symbol);
    void* f = _bolt_dl_lookup(handle, symbol);
    return f ? f : real_dlvsym(handle, symbol, version);
}

int dlclose(void* handle) {
    if (handle == libc_addr) libc_addr = NULL;
    if (handle == libegl_addr) libegl_addr = NULL;
    if (handle == libgl_addr) libgl_addr = NULL;
    if (handle == libxcb_addr) libxcb_addr = NULL;
    if (handle == libx11_addr) libx11_addr = NULL;
    return real_dlclose(handle);
}
