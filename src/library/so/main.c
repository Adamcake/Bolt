#define _GNU_SOURCE
#include <link.h>
#include <unistd.h>
#undef _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "x.h"
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

xcb_window_t main_window_xcb = 0;
uint32_t main_window_width = 0;
uint32_t main_window_height = 0;

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

xcb_generic_event_t* (*real_xcb_poll_for_event)(xcb_connection_t*) = NULL;
xcb_generic_event_t* (*real_xcb_poll_for_queued_event)(xcb_connection_t*) = NULL;
xcb_generic_event_t* (*real_xcb_wait_for_event)(xcb_connection_t*) = NULL;
xcb_get_geometry_reply_t* (*real_xcb_get_geometry_reply)(xcb_connection_t*, xcb_get_geometry_cookie_t, xcb_generic_error_t**) = NULL;

struct GLLibFunctions lgl = {0};

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
    const ElfW(Sym)* sym = _bolt_lookup_symbol("glBindTexture", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) lgl.BindTexture = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glClear", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) lgl.Clear = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glClearColor", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) lgl.ClearColor = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDeleteTextures", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) lgl.DeleteTextures = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDrawArrays", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) lgl.DrawArrays = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDrawElements", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) lgl.DrawElements = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glFlush", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) lgl.Flush = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glGenTextures", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) lgl.GenTextures = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glGetError", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) lgl.GetError = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glTexSubImage2D", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) lgl.TexSubImage2D = sym->st_value + libgl_addr;
}

void _bolt_init_libxcb(unsigned long addr, const Elf32_Word* gnu_hash_table, const ElfW(Word)* hash_table, const char* string_table, const ElfW(Sym)* symbol_table) {
    libxcb_addr = (void*)addr;
    const ElfW(Sym)* sym = _bolt_lookup_symbol("xcb_poll_for_event", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_poll_for_event = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_poll_for_queued_event", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_poll_for_queued_event = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_wait_for_event", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_wait_for_event = sym->st_value + libxcb_addr;
    sym = _bolt_lookup_symbol("xcb_get_geometry_reply", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_get_geometry_reply = sym->st_value + libxcb_addr;
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

void glGenTextures(uint32_t n, unsigned int* textures) {
    LOG("glGenTextures\n");
    lgl.GenTextures(n, textures);
    _bolt_gl_onGenTextures(n, textures);
    LOG("glGenTextures end\n");
}

void glDrawElements(uint32_t mode, unsigned int count, uint32_t type, const void* indices_offset) {
    LOG("glDrawElements\n");
    lgl.DrawElements(mode, count, type, indices_offset);
    _bolt_gl_onDrawElements(mode, count, type, indices_offset);
    LOG("glDrawElements end\n");
}

void glDrawArrays(uint32_t mode, int first, unsigned int count) {
    LOG("glDrawArrays\n");
    lgl.DrawArrays(mode, first, count);
    _bolt_gl_onDrawArrays(mode, first, count);
    LOG("glDrawArrays end\n");
}

void glBindTexture(uint32_t target, unsigned int texture) {
    LOG("glBindTexture\n");
    lgl.BindTexture(target, texture);
    _bolt_gl_onBindTexture(target, texture);
    LOG("glBindTexture end\n");
}

void glTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, unsigned int width, unsigned int height, uint32_t format, uint32_t type, const void* pixels) {
    LOG("glTexSubImage2D\n");
    lgl.TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    _bolt_gl_onTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    LOG("glTexSubImage2D end\n");
}

void glDeleteTextures(unsigned int n, const unsigned int* textures) {
    LOG("glDeleteTextures\n");
    lgl.DeleteTextures(n, textures);
    _bolt_gl_onDeleteTextures(n, textures);
    LOG("glDeleteTextures end\n");
}

void glClear(uint32_t mask) {
    LOG("glClear\n");
    lgl.Clear(mask);
    _bolt_gl_onClear(mask);
    LOG("glClear end\n");
}

void* eglGetProcAddress(const char* name) {
    LOG("eglGetProcAddress('%s')\n", name);
    void* ret = _bolt_gl_GetProcAddress(name);
    return ret ? ret : real_eglGetProcAddress(name);
}

unsigned int eglSwapBuffers(void* display, void* surface) {
    LOG("eglSwapBuffers\n");
    _bolt_gl_onSwapBuffers(&lgl, main_window_width, main_window_height);
    unsigned int ret = real_eglSwapBuffers(display, surface);
    LOG("eglSwapBuffers end (returned %u)\n", ret);
    return ret;
}

void* eglCreateContext(void* display, void* config, void* share_context, const void* attrib_list) {
    LOG("eglCreateContext\n");
    void* ret = real_eglCreateContext(display, config, share_context, attrib_list);
    if (ret) {
        pthread_mutex_lock(&egl_lock);
        _bolt_gl_onCreateContext(ret, share_context, real_eglGetProcAddress);
        pthread_mutex_unlock(&egl_lock);
    }
    LOG("eglCreateContext end (returned %lu)\n", (uintptr_t)ret);
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
    LOG("eglMakeCurrent end (returned %u)\n", ret);
    return ret;
}

unsigned int eglDestroyContext(void* display, void* context) {
    LOG("eglDestroyContext %lu\n", (uintptr_t)context);
    unsigned int ret = real_eglDestroyContext(display, context);
    if (ret) {
        pthread_mutex_lock(&egl_lock);
        void* c = _bolt_gl_onDestroyContext(context, &lgl);
        if (c) {
            real_eglMakeCurrent(display, NULL, NULL, c);
            _bolt_gl_close();
            real_eglMakeCurrent(display, NULL, NULL, NULL);
            real_eglTerminate(display);
        }
        pthread_mutex_unlock(&egl_lock);
    }
    LOG("eglDestroyContext end (returned %u)\n", ret);
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

// returns true if the event should be passed on to the window, false if not
uint8_t _bolt_handle_xcb_event(xcb_connection_t* c, const xcb_generic_event_t* const e) {
    if (!e) return true;
    if (!main_window_xcb) {
        if (e->response_type == XCB_MAP_NOTIFY) {
            const xcb_map_notify_event_t* const event = (const xcb_map_notify_event_t* const)e;
            main_window_xcb = event->window;
            printf("new main window %u\n", event->window);
        }
        return true;
    }

    switch (e->response_type) {
        // todo: handle mouse and keyboard events for main window
    }
    return true;
}

xcb_generic_event_t* xcb_poll_for_event(xcb_connection_t* c) {
    LOG("xcb_poll_for_event\n");
    xcb_generic_event_t* ret = real_xcb_poll_for_event(c);
    while (true) {
        if (_bolt_handle_xcb_event(c, ret)) {
            LOG("xcb_poll_for_event end\n");
            return ret;
        }
        ret = real_xcb_poll_for_queued_event(c);
    }
}

xcb_generic_event_t* xcb_poll_for_queued_event(xcb_connection_t* c) {
    LOG("xcb_poll_for_queued_event\n");
    xcb_generic_event_t* ret;
    while (true) {
        ret = real_xcb_poll_for_queued_event(c);
        if (_bolt_handle_xcb_event(c, ret)) {
            LOG("xcb_poll_for_queued_event end\n");
            return ret;
        }
    }
}

xcb_generic_event_t* xcb_wait_for_event(xcb_connection_t* c) {
    LOG("xcb_wait_for_event\n");
    xcb_generic_event_t* ret;
    while (true) {
        ret = real_xcb_wait_for_event(c);
        if (_bolt_handle_xcb_event(c, ret)) {
            LOG("xcb_wait_for_event end\n");
            return ret;
        }
    }
}

xcb_get_geometry_reply_t* xcb_get_geometry_reply(xcb_connection_t* c, xcb_get_geometry_cookie_t cookie, xcb_generic_error_t** e) {
    LOG("xcb_get_geometry_reply\n");
    // currently the game appears to call this twice per frame for the main window and never for
    // any other window, so we can assume the result here applies to the main window.
    xcb_get_geometry_reply_t* ret = real_xcb_get_geometry_reply(c, cookie, e);
    main_window_width = ret->width;
    main_window_height = ret->height;
    LOG("xcb_get_geometry_reply end (returned %u %u)\n", main_window_width, main_window_height);
    return ret;
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
        if (strcmp(symbol, "glClear") == 0) return glClear;
    } else if (handle == libxcb_addr) {
        if (strcmp(symbol, "xcb_poll_for_event") == 0) return xcb_poll_for_event;
        if (strcmp(symbol, "xcb_poll_for_queued_event") == 0) return xcb_poll_for_queued_event;
        if (strcmp(symbol, "xcb_wait_for_event") == 0) return xcb_wait_for_event;
        if (strcmp(symbol, "xcb_get_geometry_reply") == 0) return xcb_get_geometry_reply;
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
            lgl.BindTexture = real_dlsym(ret, "glBindTexture");
            lgl.Clear = real_dlsym(ret, "glClear");
            lgl.ClearColor = real_dlsym(ret, "glClearColor");
            lgl.DeleteTextures = real_dlsym(ret, "glDeleteTextures");
            lgl.DrawArrays = real_dlsym(ret, "glDrawArrays");
            lgl.DrawElements = real_dlsym(ret, "glDrawElements");
            lgl.Flush = real_dlsym(ret, "glFlush");
            lgl.GenTextures = real_dlsym(ret, "glGenTextures");
            lgl.GetError = real_dlsym(ret, "glGetError");
            lgl.TexSubImage2D = real_dlsym(ret, "glTexSubImage2D");
        }
        if (!libxcb_addr && !strcmp(filename, libxcb_name)) {
            libxcb_addr = ret;
            if (!libxcb_addr) return NULL;
            real_xcb_poll_for_event = real_dlsym(ret, "xcb_poll_for_event");
            real_xcb_poll_for_queued_event = real_dlsym(ret, "xcb_poll_for_queued_event");
            real_xcb_wait_for_event = real_dlsym(ret, "xcb_wait_for_event");
            real_xcb_get_geometry_reply = real_dlsym(ret, "xcb_get_geometry_reply");
        }
    }
    return ret;
}

void* dlsym(void* handle, const char* symbol) {
    INIT();
    LOG("dlsym(%lu, '%s')\n", (uintptr_t)handle, symbol);
    void* f = _bolt_dl_lookup(handle, symbol);
    return f ? f : real_dlsym(handle, symbol);
}

void* dlvsym(void* handle, const char* symbol, const char* version) {
    INIT();
    LOG("dlvsym(%lu, '%s')\n", (uintptr_t)handle, symbol);
    void* f = _bolt_dl_lookup(handle, symbol);
    return f ? f : real_dlvsym(handle, symbol, version);
}

int dlclose(void* handle) {
    if (handle == libc_addr) libc_addr = NULL;
    if (handle == libegl_addr) libegl_addr = NULL;
    if (handle == libgl_addr) libgl_addr = NULL;
    if (handle == libxcb_addr) libxcb_addr = NULL;
    return real_dlclose(handle);
}
