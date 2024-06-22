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
#include "../plugin/plugin.h"
#include "../../hashmap/hashmap.h"

// comment or uncomment this to enable verbose logging of hooks in this file
//#define VERBOSE

// don't change this part, change the line above this instead
#if defined(VERBOSE)
#define LOG printf
#else
#define LOG(...)
#endif

// note: this is currently always triggered by single-threaded dlopen calls so no locking necessary
static uint8_t inited = 0;
#define INIT() if (!inited) _bolt_init_functions();

static pthread_mutex_t egl_lock;

static xcb_window_t main_window_xcb = 0;
static uint32_t main_window_width = 0;
static uint32_t main_window_height = 0;

/* whether the mouse is REALLY in the client vs. whether the client thinks it is */
static uint8_t xcb_mousein_real = 0;
static uint8_t xcb_mousein_fake = 0;

/* 0 indicates no window */
static uint64_t grabbed_window_id = 0;

static const char* libc_name = "libc.so.6";
static const char* libegl_name = "libEGL.so.1";
static const char* libgl_name = "libGL.so.1";
static const char* libxcb_name = "libxcb.so.1";

static void* libc_addr = 0;
static void* libegl_addr = 0;
static void* libgl_addr = 0;
static void* libxcb_addr = 0;

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
static xcb_get_geometry_reply_t* (*real_xcb_get_geometry_reply)(xcb_connection_t*, xcb_get_geometry_cookie_t, xcb_generic_error_t**) = NULL;

static struct GLLibFunctions libgl = {0};

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
    sym = _bolt_lookup_symbol("glClear", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.Clear = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glClearColor", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.ClearColor = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDeleteTextures", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.DeleteTextures = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDrawArrays", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.DrawArrays = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glDrawElements", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.DrawElements = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glFlush", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.Flush = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glGenTextures", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.GenTextures = sym->st_value + libgl_addr;
    sym = _bolt_lookup_symbol("glGetError", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) libgl.GetError = sym->st_value + libgl_addr;
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
    sym = _bolt_lookup_symbol("xcb_get_geometry_reply", gnu_hash_table, hash_table, string_table, symbol_table);
    if (sym) real_xcb_get_geometry_reply = sym->st_value + libxcb_addr;
}

static int _bolt_dl_iterate_callback(struct dl_phdr_info* info, size_t size, void* args) {
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

static void _bolt_init_functions() {
    _bolt_plugin_on_startup();
    pthread_mutex_init(&egl_lock, NULL);
    dl_iterate_phdr(_bolt_dl_iterate_callback, NULL);
    inited = 1;
}

void glGenTextures(uint32_t n, unsigned int* textures) {
    LOG("glGenTextures\n");
    libgl.GenTextures(n, textures);
    _bolt_gl_onGenTextures(n, textures);
    LOG("glGenTextures end\n");
}

void glDrawElements(uint32_t mode, unsigned int count, uint32_t type, const void* indices_offset) {
    LOG("glDrawElements\n");
    libgl.DrawElements(mode, count, type, indices_offset);
    _bolt_gl_onDrawElements(mode, count, type, indices_offset);
    LOG("glDrawElements end\n");
}

void glDrawArrays(uint32_t mode, int first, unsigned int count) {
    LOG("glDrawArrays\n");
    libgl.DrawArrays(mode, first, count);
    _bolt_gl_onDrawArrays(mode, first, count);
    LOG("glDrawArrays end\n");
}

void glBindTexture(uint32_t target, unsigned int texture) {
    LOG("glBindTexture\n");
    libgl.BindTexture(target, texture);
    _bolt_gl_onBindTexture(target, texture);
    LOG("glBindTexture end\n");
}

void glTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, unsigned int width, unsigned int height, uint32_t format, uint32_t type, const void* pixels) {
    LOG("glTexSubImage2D\n");
    libgl.TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    _bolt_gl_onTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    LOG("glTexSubImage2D end\n");
}

void glDeleteTextures(unsigned int n, const unsigned int* textures) {
    LOG("glDeleteTextures\n");
    libgl.DeleteTextures(n, textures);
    _bolt_gl_onDeleteTextures(n, textures);
    LOG("glDeleteTextures end\n");
}

void glClear(uint32_t mask) {
    LOG("glClear\n");
    libgl.Clear(mask);
    _bolt_gl_onClear(mask);
    LOG("glClear end\n");
}

void glViewport(int x, int y, unsigned int width, unsigned int height) {
    LOG("glViewport\n");
    libgl.Viewport(x, y, width, height);
    _bolt_gl_onViewport(x, y, width, height);
    LOG("glViewport end\n");
}

void* eglGetProcAddress(const char* name) {
    LOG("eglGetProcAddress('%s')\n", name);
    void* ret = _bolt_gl_GetProcAddress(name);
    return ret ? ret : real_eglGetProcAddress(name);
}

unsigned int eglSwapBuffers(void* display, void* surface) {
    LOG("eglSwapBuffers\n");
    _bolt_gl_onSwapBuffers(main_window_width, main_window_height);
    unsigned int ret = real_eglSwapBuffers(display, surface);
    LOG("eglSwapBuffers end (returned %u)\n", ret);
    return ret;
}

void* eglCreateContext(void* display, void* config, void* share_context, const void* attrib_list) {
    LOG("eglCreateContext\n");
    void* ret = real_eglCreateContext(display, config, share_context, attrib_list);
    if (ret) {
        pthread_mutex_lock(&egl_lock);
        _bolt_gl_onCreateContext(ret, share_context, &libgl, real_eglGetProcAddress);
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
        void* c = _bolt_gl_onDestroyContext(context);
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

static uint8_t _bolt_point_in_rect(int16_t x, int16_t y, int rx, int ry, int rw, int rh) {
    return rx <= x && rx + rw > x && ry <= y && ry + rh > y;
}

static void _bolt_xcb_to_mouse_event(int16_t x, int16_t y, uint16_t state, struct MouseEvent* out) {
    // TODO: the rest of this
    out->x = x;
    out->y = y;
    out->ctrl = (state >> 2) & 1;
    out->shift = state & 1;
    //out->meta;
    //out->alt;
    //out->capslock;
    //out->numlock;
    out->mb_left = (state >> 8) & 1;
    out->mb_right = (state >> 10) & 1;
    out->mb_middle = (state >> 9) & 1;
}

// called by handle_xcb_event
// policy is as follows: if the target window is NOT main_window, do not interfere at all.
// if the target window IS main_window, the event can be swallowed if it's above a window or we're
// currently in a drag action, otherwise the event may also mutate into an enter or leave event.
static uint8_t _bolt_handle_xcb_motion_event(xcb_window_t win, int16_t x, int16_t y, uint16_t state, xcb_generic_event_t* e) {
    if (win != main_window_xcb) return true;
    struct WindowInfo* windows = _bolt_plugin_windowinfo();
    uint8_t ret = true;
    _bolt_rwlock_lock_read(&windows->lock);
    size_t iter = 0;
    void* item;
    while (hashmap_iter(windows->map, &iter, &item)) {
        struct EmbeddedWindow** window = item;
        _bolt_rwlock_lock_read(&(*window)->lock);
        if (_bolt_point_in_rect(x, y, (*window)->metadata.x, (*window)->metadata.y, (*window)->metadata.width, (*window)->metadata.height)) {
            ret = false;
        }
        _bolt_rwlock_unlock_read(&(*window)->lock);

        if (!ret) {
            _bolt_rwlock_lock_write(&(*window)->input_lock);
            (*window)->input.mouse_motion = 1;
            _bolt_xcb_to_mouse_event(x - (*window)->metadata.x, y - (*window)->metadata.y, state, &(*window)->input.mouse_motion_event);
            _bolt_rwlock_unlock_write(&(*window)->input_lock);
            break;
        }
    }
    _bolt_rwlock_unlock_read(&windows->lock);

    if (ret) {
        _bolt_rwlock_lock_write(&windows->input_lock);
        windows->input.mouse_motion = 1;
        _bolt_xcb_to_mouse_event(x, y, state, &windows->input.mouse_motion_event);
        _bolt_rwlock_unlock_write(&windows->input_lock);
    }
    if (ret != xcb_mousein_fake) {
        xcb_mousein_fake = ret;
        // mutate the event - note that motion_notify, enter_notify and exit_notify have the exact
        // same memory layout, except for the last two bytes, so this is a fairly easy mutation
        xcb_enter_notify_event_t* event = (xcb_enter_notify_event_t*)e;
        event->response_type = ret ? XCB_ENTER_NOTIFY : XCB_LEAVE_NOTIFY;
        event->same_screen_focus = event->mode;
        event->mode = 0;
        ret = true;
    }
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
                case XCB_INPUT_MOTION: { // when mouse moves (not drag) inside the game window
                    xcb_input_motion_event_t* event = (xcb_input_motion_event_t*)e;
                    return _bolt_handle_xcb_motion_event(event->event, event->event_x >> 16, event->event_y >> 16, event->flags, e);
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
        case XCB_BUTTON_PRESS: { // when pressing a mouse button that's received by the game window
            xcb_button_release_event_t* event = (xcb_button_release_event_t*)e;
            if (event->event != main_window_xcb) return true;
            struct WindowInfo* windows = _bolt_plugin_windowinfo();
            uint8_t ret = true;
            _bolt_rwlock_lock_read(&windows->lock);
            size_t iter = 0;
            void* item;
            while (hashmap_iter(windows->map, &iter, &item)) {
                struct EmbeddedWindow** window = item;
                _bolt_rwlock_lock_read(&(*window)->lock);
                struct EmbeddedWindowMetadata metadata = (*window)->metadata;
                _bolt_rwlock_unlock_read(&(*window)->lock);

                if (_bolt_point_in_rect(event->event_x, event->event_y, metadata.x, metadata.y, metadata.width, metadata.height)) {
                    grabbed_window_id = (*window)->id;
                    ret = false;
                }

                if (!ret) {
                    _bolt_rwlock_lock_write(&(*window)->input_lock);
                    switch (event->detail) {
                        case 1:
                            (*window)->input.mouse_left = 1;
                            _bolt_xcb_to_mouse_event(event->event_x - metadata.x, event->event_y - metadata.y, event->state, &(*window)->input.mouse_left_event);
                            break;
                        case 2:
                            (*window)->input.mouse_middle = 1;
                            _bolt_xcb_to_mouse_event(event->event_x - metadata.x, event->event_y - metadata.y, event->state, &(*window)->input.mouse_middle_event);
                            break;
                        case 3:
                            (*window)->input.mouse_right = 1;
                            _bolt_xcb_to_mouse_event(event->event_x - metadata.x, event->event_y - metadata.y, event->state, &(*window)->input.mouse_right_event);
                            break;
                        case 4:
                            (*window)->input.mouse_scroll_up = 1;
                            _bolt_xcb_to_mouse_event(event->event_x - metadata.x, event->event_y - metadata.y, event->state, &(*window)->input.mouse_scroll_up_event);
                            break;
                        case 5:
                            (*window)->input.mouse_scroll_down = 1;
                            _bolt_xcb_to_mouse_event(event->event_x - metadata.x, event->event_y - metadata.y, event->state, &(*window)->input.mouse_scroll_down_event);
                            break;
                    }
                    _bolt_rwlock_unlock_write(&(*window)->input_lock);
                    break;
                }
            }
            _bolt_rwlock_unlock_read(&windows->lock);
            if (ret) {
                _bolt_rwlock_lock_write(&windows->input_lock);
                switch (event->detail) {
                    case 1:
                        windows->input.mouse_left = 1;
                        _bolt_xcb_to_mouse_event(event->event_x, event->event_y, event->state, &windows->input.mouse_left_event);
                        break;
                    case 2:
                        windows->input.mouse_middle = 1;
                        _bolt_xcb_to_mouse_event(event->event_x, event->event_y, event->state, &windows->input.mouse_middle_event);
                        break;
                    case 3:
                        windows->input.mouse_right = 1;
                        _bolt_xcb_to_mouse_event(event->event_x, event->event_y, event->state, &windows->input.mouse_right_event);
                        break;
                    case 4:
                        windows->input.mouse_scroll_up = 1;
                        _bolt_xcb_to_mouse_event(event->event_x, event->event_y, event->state, &windows->input.mouse_scroll_up_event);
                        break;
                    case 5:
                        windows->input.mouse_scroll_down = 1;
                        _bolt_xcb_to_mouse_event(event->event_x, event->event_y, event->state, &windows->input.mouse_scroll_down_event);
                        break;
                }
                _bolt_rwlock_unlock_write(&windows->input_lock);
            }
            return ret;
        }
        case XCB_BUTTON_RELEASE: { // when releasing a mouse button, for which the press was received by the game window
            xcb_button_release_event_t* event = (xcb_button_release_event_t*)e;
            if (event->event != main_window_xcb) return true;
            struct WindowInfo* windows = _bolt_plugin_windowinfo();
            uint8_t on_any_window = false;
            _bolt_rwlock_lock_read(&windows->lock);
            size_t iter = 0;
            void* item;
            while (hashmap_iter(windows->map, &iter, &item)) {
                struct EmbeddedWindow** window = item;
                _bolt_rwlock_lock_read(&(*window)->lock);
                if (_bolt_point_in_rect(event->event_x, event->event_y, (*window)->metadata.x, (*window)->metadata.y, (*window)->metadata.width, (*window)->metadata.height)) {
                    on_any_window = true;
                }
                _bolt_rwlock_unlock_read(&(*window)->lock);
            }
            _bolt_rwlock_unlock_read(&windows->lock);
            grabbed_window_id = 0;
            if (!xcb_mousein_fake) {
                if (on_any_window) {
                    return false;
                } else {
                    xcb_mousein_fake = true;
                    event->response_type = XCB_ENTER_NOTIFY;
                    event->state = 2;
                    event->pad0 = event->same_screen; // actually sets same_screen_focus
                    event->same_screen = 0; // actually sets pad0
                    return true;
                }
            } else {
                if (on_any_window) {
                    // would be nice to send_event here with XCB_LEAVE_NOTIFY mode=2
                }
                return true;
            }
        }
        case XCB_MOTION_NOTIFY: { // when mouse moves while dragging from inside the game window
            xcb_motion_notify_event_t* event = (xcb_motion_notify_event_t*)e;
            if (event->event != main_window_xcb) return true;
            return xcb_mousein_fake;
        }
        case XCB_KEY_PRESS: // when a keyboard key is pressed while the game window is focused
        case XCB_KEY_RELEASE: // when a keyboard key is released while the game window is focused
            break;
        case XCB_ENTER_NOTIFY: { // when the mouse pointer moves inside the game view
            xcb_enter_notify_event_t* event = (xcb_enter_notify_event_t*)e;
            if (event->event != main_window_xcb) return true;
            xcb_mousein_real = true;
            struct WindowInfo* windows = _bolt_plugin_windowinfo();
            uint8_t ret = true;
            _bolt_rwlock_lock_read(&windows->lock);
            size_t iter = 0;
            void* item;
            while (hashmap_iter(windows->map, &iter, &item)) {
                struct EmbeddedWindow** window = item;
                _bolt_rwlock_lock_read(&(*window)->lock);
                if (_bolt_point_in_rect(event->event_x, event->event_y, (*window)->metadata.x, (*window)->metadata.y, (*window)->metadata.width, (*window)->metadata.height)) {
                    ret = false;
                }
                _bolt_rwlock_unlock_read(&(*window)->lock);
            }
            _bolt_rwlock_unlock_read(&windows->lock);
            if (ret) xcb_mousein_fake = true;
            return ret;
        }
        case XCB_LEAVE_NOTIFY: { // when the mouse pointer moves out of the game view
            xcb_leave_notify_event_t* event = (xcb_leave_notify_event_t*)e;
            if (event->event != main_window_xcb) return true;
            const uint8_t ret = xcb_mousein_fake;
            xcb_mousein_real = false;
            xcb_mousein_fake = false;
            return ret;
        }
        case XCB_FOCUS_IN: // when the game window gains focus
        case XCB_FOCUS_OUT: // when the game window loses focus
        case XCB_KEYMAP_NOTIFY:
        case XCB_EXPOSE:
        case XCB_VISIBILITY_NOTIFY:
        case XCB_MAP_NOTIFY:
        case XCB_UNMAP_NOTIFY:
        case XCB_REPARENT_NOTIFY:
        case XCB_DESTROY_NOTIFY:
        case XCB_CONFIGURE_NOTIFY:
        case XCB_PROPERTY_NOTIFY:
        case XCB_CLIENT_MESSAGE:
            break;
        default:
            printf("unknown xcb event %u\n", response_type);
            break;
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
            libgl.BindTexture = real_dlsym(ret, "glBindTexture");
            libgl.Clear = real_dlsym(ret, "glClear");
            libgl.ClearColor = real_dlsym(ret, "glClearColor");
            libgl.DeleteTextures = real_dlsym(ret, "glDeleteTextures");
            libgl.DrawArrays = real_dlsym(ret, "glDrawArrays");
            libgl.DrawElements = real_dlsym(ret, "glDrawElements");
            libgl.Flush = real_dlsym(ret, "glFlush");
            libgl.GenTextures = real_dlsym(ret, "glGenTextures");
            libgl.GetError = real_dlsym(ret, "glGetError");
            libgl.TexParameteri = real_dlsym(ret, "glTexParameteri");
            libgl.TexSubImage2D = real_dlsym(ret, "glTexSubImage2D");
            libgl.Viewport = real_dlsym(ret, "glViewport");
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
