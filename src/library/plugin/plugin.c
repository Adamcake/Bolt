#include "../ipc.h"

#include "plugin.h"
#include "lua.h"
#include "plugin_api.h"
#include "../../../modules/hashmap/hashmap.h"

#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <afunix.h>
#define getpid _getpid
#define close closesocket
LARGE_INTEGER performance_frequency;
#else
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

static struct PluginManagedFunctions managed_functions;

static uint64_t next_window_id;
static struct WindowInfo windows;

static struct SurfaceFunctions overlay;
static struct SurfaceFunctions overlay_windows;
static uint32_t overlay_width;
static uint32_t overlay_height;
static uint8_t overlay_inited;

static bool inited = false;
static int _bolt_api_init(lua_State* state);

static BoltSocketType fd = 0;

static struct BoltSHM capture_shm;
static uint64_t next_capture_time = 0;
static uint64_t capture_id;
static uint32_t capture_width;
static uint32_t capture_height;
static uint8_t capture_inited;
#define CAPTURE_COOLDOWN_MICROS 250000

/* 0 indicates no window */
static uint64_t last_mouseevent_window_id = 0;
static uint64_t grabbed_window_id = 0;

static char character_hash[SHA256_BLOCK_SIZE * 2] = {0};

// called mainly from the input thread - see comment in header for thread-safety observations
uint64_t _bolt_plugin_get_last_mouseevent_windowid() { return last_mouseevent_window_id; }

static void _bolt_plugin_ipc_init(BoltSocketType*);
static void _bolt_plugin_ipc_close(BoltSocketType);

static void _bolt_plugin_window_onreposition(struct EmbeddedWindow*, const struct RepositionEvent*);
static void _bolt_plugin_window_onmousemotion(struct EmbeddedWindow*, const struct MouseMotionEvent*);
static void _bolt_plugin_window_onmousebutton(struct EmbeddedWindow*, const struct MouseButtonEvent*);
static void _bolt_plugin_window_onmousebuttonup(struct EmbeddedWindow*, const struct MouseButtonEvent*);
static void _bolt_plugin_window_onscroll(struct EmbeddedWindow*, const struct MouseScrollEvent*);
static void _bolt_plugin_window_onmouseleave(struct EmbeddedWindow*, const struct MouseMotionEvent*);
static void _bolt_plugin_handle_mousemotion(const struct MouseMotionEvent*);
static void _bolt_plugin_handle_mousebutton(const struct MouseButtonEvent*);
static void _bolt_plugin_handle_mousebuttonup(const struct MouseButtonEvent*);
static void _bolt_plugin_handle_scroll(const struct MouseScrollEvent*);

static void _bolt_plugin_handle_swapbuffers(const struct SwapBuffersEvent*);

const struct PluginManagedFunctions* _bolt_plugin_managed_functions() {
    return &managed_functions;
}

const char* _bolt_plugin_character_hash(void) {
    return character_hash;
}

// these functions are only called from the thread that runs lua code, and are used to protect
// against re-entry on that thread specifically, because re-entry is undefined with posix rwlock.
static size_t windows_read_lock_count = 0;
static void lock_windows_for_reading() {
    if (windows_read_lock_count == 0) {
        _bolt_rwlock_lock_read(&windows.lock);
    }
    windows_read_lock_count += 1;
}
static void unlock_windows_for_reading() {
    windows_read_lock_count -= 1;
    if (windows_read_lock_count == 0) {
        _bolt_rwlock_unlock_read(&windows.lock);
    }
}

uint64_t _bolt_plugin_new_windowid() {
    const uint64_t ret = next_window_id;
    next_window_id += 1;
    return ret;
}

BoltSocketType _bolt_plugin_fd() {
    return fd;
}

void _bolt_plugin_free(struct Plugin* plugin) {
    hashmap_free(plugin->external_browsers);
    free(plugin->path);
    free(plugin->config_path);
    lua_close(plugin->state);
    free(plugin);
}

static int _bolt_window_map_compare(const void* a, const void* b, void* udata) {
    return **(uint64_t**)a != **(uint64_t**)b;
}

static uint64_t _bolt_window_map_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    return hashmap_sip(*(uint64_t**)item, sizeof(uint64_t), seed0, seed1);
}

static int _bolt_plugin_map_compare(const void* a, const void* b, void* udata) {
    const struct Plugin* p1 = *(const struct Plugin* const*)a;
    const struct Plugin* p2 = *(const struct Plugin* const*)b;
    return p2->id - p1->id;
}

static uint64_t _bolt_plugin_map_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const struct Plugin* p = *(const struct Plugin* const*)item;
    return hashmap_sip(&p->id, sizeof(p->id), seed0, seed1);
}

int _bolt_plugin_itemicon_compare(const void* a, const void* b, void* udata) {
    const struct Icon* i1 = a;
    const struct Icon* i2 = b;
    uint64_t xywh1, xywh2;
    memcpy(&xywh1, &i1->x, sizeof xywh1);
    memcpy(&xywh2, &i2->x, sizeof xywh2);
    return xywh2 - xywh1;
}

uint64_t _bolt_plugin_itemicon_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const struct Icon* icon = item;
    return hashmap_sip(&icon->x, 4 * sizeof icon->x, seed0, seed1);
}

static struct hashmap* plugins;

#define DEFINE_CALLBACK(APINAME, STRUCTNAME) \
void _bolt_plugin_handle_##APINAME(const struct STRUCTNAME* e) { \
    if (!overlay_inited) return; \
    size_t iter = 0; \
    void* item; \
    while (hashmap_iter(plugins, &iter, &item)) { \
        struct Plugin* plugin = *(struct Plugin* const*)item; \
        if (plugin->is_deleted) continue; \
        void* newud = lua_newuserdata(plugin->state, sizeof(struct STRUCTNAME)); /*stack: userdata*/ \
        memcpy(newud, e, sizeof(struct STRUCTNAME)); \
        lua_getfield(plugin->state, LUA_REGISTRYINDEX, #APINAME "meta"); /*stack: userdata, metatable*/ \
        lua_setmetatable(plugin->state, -2); /*stack: userdata*/ \
        lua_pushliteral(plugin->state, #APINAME "cb"); /*stack: userdata, enumname*/ \
        lua_gettable(plugin->state, LUA_REGISTRYINDEX); /*stack: userdata, callback*/ \
        if (!lua_isfunction(plugin->state, -1)) { \
            lua_pop(plugin->state, 2); \
            continue; \
        } \
        lua_pushvalue(plugin->state, -2); /*stack: userdata, callback, userdata*/ \
        if (lua_pcall(plugin->state, 1, 0, 0)) { /*stack: userdata, ?error*/ \
            const char* e = lua_tolstring(plugin->state, -1, 0); \
            printf("plugin callback on" #APINAME " error: %s\n", e); \
            lua_pop(plugin->state, 2); /*stack: (empty)*/ \
            _bolt_plugin_stop(plugin->id); \
            _bolt_plugin_notify_stopped(plugin->id); \
            break; \
        } else { \
            lua_pop(plugin->state, 1); /*stack: (empty)*/ \
        } \
    } \
} \

// same as DEFINE_CALLBACK except _bolt_plugin_handle_... will be defined as static
#define DEFINE_CALLBACK_STATIC(APINAME, STRUCTNAME) static DEFINE_CALLBACK(APINAME, STRUCTNAME)

#define DEFINE_WINDOWEVENT(APINAME, REGNAME, EVNAME) \
void _bolt_plugin_window_on##APINAME(struct EmbeddedWindow* window, const struct EVNAME* event) { \
    if (window->is_deleted) return; \
    lua_State* state = window->plugin; \
    if (window->is_browser) { \
        const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_EV##REGNAME; \
        const struct BoltIPCEvHeader header = { .plugin_id = window->plugin_id, .window_id = window->id }; \
        _bolt_ipc_send(fd, &msg_type, sizeof(msg_type)); \
        _bolt_ipc_send(fd, &header, sizeof(header)); \
        _bolt_ipc_send(fd, event, sizeof(struct EVNAME)); \
    } \
    lua_getfield(state, LUA_REGISTRYINDEX, window->is_browser ? BROWSERS_REGISTRYNAME : WINDOWS_REGISTRYNAME); /*stack: window table*/ \
    lua_pushinteger(state, window->id); /*stack: window table, window id*/ \
    lua_gettable(state, -2); /*stack: window table, event table*/ \
    lua_pushinteger(state, window->is_browser ? BROWSER_ON##REGNAME : WINDOW_ON##REGNAME); /*stack: window table, event table, event id*/ \
    lua_gettable(state, -2); /*stack: window table, event table, function or nil*/ \
    if (lua_isfunction(state, -1)) { \
        void* newud = lua_newuserdata(state, sizeof(struct EVNAME)); /*stack: window table, event table, function, event*/ \
        memcpy(newud, event, sizeof(struct EVNAME)); \
        lua_getfield(state, LUA_REGISTRYINDEX, #APINAME "meta"); /*stack: window table, event table, function, event, event metatable*/ \
        lua_setmetatable(state, -2); /*stack: window table, event table, function, event*/ \
        if (lua_pcall(state, 1, 0, 0)) { /*stack: window table, event table, ?error*/ \
            const char* e = lua_tolstring(state, -1, 0); \
            printf("plugin %s:on" #APINAME " error: %s\n", window->is_browser ? "browser" : "window", e); \
            lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME); /*stack: window table, event table, error, plugin*/ \
            const struct Plugin* plugin = lua_touserdata(state, -1); \
            lua_pop(state, 4); /*stack: (empty)*/ \
            _bolt_plugin_stop(plugin->id); \
            _bolt_plugin_notify_stopped(plugin->id); \
        } else { \
            lua_pop(state, 2); /*stack: (empty)*/ \
        } \
    } else { \
        lua_pop(state, 3); \
    } \
}

static void delete_windows_by_uid(uint64_t uid) {
    size_t iter = 0;
    void* item;
    lock_windows_for_reading();
    while (hashmap_iter(windows.map, &iter, &item)) {
        struct EmbeddedWindow* window = *(struct EmbeddedWindow**)item;
        if (window->plugin_id == uid) window->is_deleted = true;
    }
    unlock_windows_for_reading();
}

void _bolt_plugin_on_startup() {
    windows.map = hashmap_new(sizeof(struct EmbeddedWindow*), 8, 0, 0, _bolt_window_map_hash, _bolt_window_map_compare, NULL, NULL);
    _bolt_rwlock_init(&windows.lock);
    memset(&windows.input, 0, sizeof(windows.input));
    overlay_width = 0;
    overlay_height = 0;
    overlay_inited = false;
    capture_inited = false;
#if defined(_WIN32)
    QueryPerformanceFrequency(&performance_frequency);
#endif
}

// assumes only the bottom 4 bits are used
char u4_to_char(uint8_t u4) {
    if (u4 < 10) {
        return '0' + (char)u4;
    } else {
        return 'a' + (char)(u4 - 10);
    }
}

void _bolt_plugin_init(const struct PluginManagedFunctions* functions) {
    _bolt_plugin_ipc_init(&fd);

    const char* display_name = getenv("JX_DISPLAY_NAME");
    if (display_name && *display_name) {
        size_t name_len = strlen(display_name);
        const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_IDENTIFY;
        const struct BoltIPCIdentifyHeader header = { .name_length = name_len };
        _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
        _bolt_ipc_send(fd, &header, sizeof(header));
        _bolt_ipc_send(fd, display_name, name_len);
    }

    const char* character_id = getenv("JX_CHARACTER_ID");
    SHA256_CTX ctx;
    unsigned char hash[SHA256_BLOCK_SIZE];
    sha256_init(&ctx);
    sha256_update(&ctx, (const unsigned char*)character_id, character_id ? strlen(character_id) : 0);
    sha256_final(&ctx, hash);
    for (size_t i = 0; i < SHA256_BLOCK_SIZE; i += 1) {
        character_hash[i * 2] = u4_to_char((hash[i] >> 4) & 0b1111);
        character_hash[(i * 2) + 1] = u4_to_char(hash[i] & 0b1111);
    }

    managed_functions = *functions;
    _bolt_rwlock_lock_write(&windows.lock);
    next_window_id = 1;
    plugins = hashmap_new(sizeof(struct Plugin*), 8, 0, 0, _bolt_plugin_map_hash, _bolt_plugin_map_compare, NULL, NULL);
    inited = 1;
    _bolt_rwlock_unlock_write(&windows.lock);
}

static int _bolt_api_init(lua_State* state) {
    _bolt_api_push_bolt_table(state);
    return 1;
}

uint8_t _bolt_plugin_is_inited() {
    return inited;
}

uint8_t _bolt_monotonic_microseconds(uint64_t* microseconds) {
#if defined(_WIN32)
    LARGE_INTEGER ticks;
    if (QueryPerformanceCounter(&ticks)) {
        *microseconds = (ticks.QuadPart * 1000000) / performance_frequency.QuadPart;
        return true;
    }
    return false;
#else
    struct timespec s;
    clock_gettime(CLOCK_MONOTONIC_RAW, &s);
    *microseconds = (s.tv_sec * 1000000) + (s.tv_nsec / 1000);
    return true;
#endif
}

static bool embedded_window_meta_eq(const struct EmbeddedWindowMetadata* a, const struct EmbeddedWindowMetadata* b) {
    return a->x == b->x && a->y == b->y && a->width == b->width && a->height == b->height;
}

// populates the window->repos_target_... variables, without accessing any of the window's mutex-protected members
#define WINDOW_REPOSITION_THRESHOLD 6
static void _bolt_window_calc_repos_target(struct EmbeddedWindow* window, const struct EmbeddedWindowMetadata* meta, int16_t x, int16_t y, uint32_t window_width, uint32_t window_height) {
    int16_t diff_x = x - window->drag_xstart;
    int16_t diff_y = y - window->drag_ystart;
    if (abs(diff_x) >= WINDOW_REPOSITION_THRESHOLD || abs(diff_y) >= WINDOW_REPOSITION_THRESHOLD) {
        window->reposition_threshold = true;
    }
    if (!window->reposition_threshold) return;
    
    if (window->reposition_w == 0 && window->reposition_h == 0) {
        // move, no resize
        window->repos_target_x = meta->x + diff_x;
        window->repos_target_y = meta->y + diff_y;
        window->repos_target_w = meta->width;
        window->repos_target_h = meta->height;

        // clamp
        if (window->repos_target_x < 0) window->repos_target_x = 0;
        if (window->repos_target_y < 0) window->repos_target_y = 0;
        if ((int32_t)window->repos_target_x + (int32_t)window->repos_target_w > (int32_t)window_width) window->repos_target_x = window_width - window->repos_target_w;
        if ((int32_t)window->repos_target_y + (int32_t)window->repos_target_h > (int32_t)window_height) window->repos_target_y = window_height - window->repos_target_h;
        return;
    }

    if (window->reposition_w > 0) {
        // resize x by the right edge
        window->repos_target_x = meta->x;
        window->repos_target_w = meta->width + diff_x;
        if (meta->width + diff_x < WINDOW_MIN_SIZE) window->repos_target_w = WINDOW_MIN_SIZE;
        if (window->repos_target_x + window->repos_target_w > window_width) window->repos_target_w = window_width - window->repos_target_x;
    } else if (window->reposition_w < 0) {
        // resize x by the left edge
        if (meta->width - diff_x < WINDOW_MIN_SIZE) diff_x = meta->width - WINDOW_MIN_SIZE;
        window->repos_target_x = meta->x + diff_x;
        window->repos_target_w = meta->width - diff_x;
        if (window->repos_target_x < 0) {
            window->repos_target_w += window->repos_target_x;
            window->repos_target_x = 0;
        }
    } else {
        // don't resize x
        window->repos_target_x = meta->x;
        window->repos_target_w = meta->width;
    }

    if (window->reposition_h > 0) {
        // resize y by the bottom edge
        window->repos_target_y = meta->y;
        window->repos_target_h = meta->height + diff_y;
        if (meta->height + diff_y < WINDOW_MIN_SIZE) window->repos_target_h = WINDOW_MIN_SIZE;
        if (window->repos_target_y + window->repos_target_h > window_height) window->repos_target_h = window_height - window->repos_target_y;
    } else if (window->reposition_h < 0) {
        // resize y by the top edge
        if (meta->height - diff_y < WINDOW_MIN_SIZE) diff_y = meta->height - WINDOW_MIN_SIZE;
        window->repos_target_y = meta->y + diff_y;
        window->repos_target_h = meta->height - diff_y;
        if (window->repos_target_y < 0) {
            window->repos_target_h += window->repos_target_y;
            window->repos_target_y = 0;
        }
    } else {
        // don't resize y
        window->repos_target_y = meta->y;
        window->repos_target_h = meta->height;
    }
}

static void _bolt_process_plugins(uint8_t* need_capture, uint8_t* capture_ready) {
    size_t iter = 0;
    void* item;
    while (hashmap_iter(plugins, &iter, &item)) {
        struct Plugin* plugin = *(struct Plugin**)item;
        if (plugin->is_deleted) {
            hashmap_delete(plugins, &plugin);
            _bolt_plugin_free(plugin);
            iter = 0;
            continue;
        }
        if (plugin->ext_browser_capture_count == 0) continue;
        void* item2;
        size_t iter2 = 0;
        while (hashmap_iter(plugin->external_browsers, &iter2, &item2)) {
            struct ExternalBrowser* browser = *(struct ExternalBrowser**)item2;
            if (browser->do_capture) {
                *need_capture = true;
                if (!browser->capture_ready) *capture_ready = false;
            }
        }
    }
}

static void snap_window(struct EmbeddedWindowMetadata* metadata, uint32_t window_width, uint32_t window_height, bool* did_move, bool* did_resize) {
    if (metadata->width > window_width) {
        metadata->width = window_width;
        *did_resize = true;
    }
    if (metadata->height > window_height) {
        metadata->height = window_height;
        *did_resize = true;
    }
    if (metadata->x < 0) {
        metadata->x = 0;
        *did_move = true;
    }
    if (metadata->y < 0) {
        metadata->y = 0;
        *did_move = true;
    }
    if (metadata->x + metadata->width > window_width) {
        metadata->x = window_width - metadata->width;
        *did_move = true;
    }
    if (metadata->y + metadata->height > window_height) {
        metadata->y = window_height - metadata->height;
        *did_move = true;
    }
}

static void _bolt_process_embedded_windows(uint32_t window_width, uint32_t window_height, uint8_t* need_capture, uint8_t* capture_ready) {
    _bolt_rwlock_lock_write(&windows.input_lock);
    struct WindowPendingInput inputs = windows.input;
    memset(&windows.input, 0, sizeof(windows.input));
    _bolt_rwlock_unlock_write(&windows.input_lock);

    if (inputs.mouse_motion) {
        struct MouseMotionEvent event = {.details = inputs.mouse_motion_event};
        _bolt_plugin_handle_mousemotion(&event);
    }
    if (inputs.mouse_left) {
        struct MouseButtonEvent event = {.details = inputs.mouse_left_event, .button = MBLeft};
        _bolt_plugin_handle_mousebutton(&event);
    }
    if (inputs.mouse_right) {
        struct MouseButtonEvent event = {.details = inputs.mouse_right_event, .button = MBRight};
        _bolt_plugin_handle_mousebutton(&event);
    }
    if (inputs.mouse_middle) {
        struct MouseButtonEvent event = {.details = inputs.mouse_middle_event, .button = MBMiddle};
        _bolt_plugin_handle_mousebutton(&event);
    }
    if (inputs.mouse_left_up) {
        struct MouseButtonEvent event = {.details = inputs.mouse_left_up_event, .button = MBLeft};
        _bolt_plugin_handle_mousebuttonup(&event);
    }
    if (inputs.mouse_right_up) {
        struct MouseButtonEvent event = {.details = inputs.mouse_right_up_event, .button = MBRight};
        _bolt_plugin_handle_mousebuttonup(&event);
    }
    if (inputs.mouse_middle_up) {
        struct MouseButtonEvent event = {.details = inputs.mouse_middle_up_event, .button = MBMiddle};
        _bolt_plugin_handle_mousebuttonup(&event);
    }
    if (inputs.mouse_scroll_up) {
        struct MouseScrollEvent event = {.details = inputs.mouse_scroll_up_event, .direction = 1};
        _bolt_plugin_handle_scroll(&event);
    }
    if (inputs.mouse_scroll_down) {
        struct MouseScrollEvent event = {.details = inputs.mouse_scroll_down_event, .direction = 0};
        _bolt_plugin_handle_scroll(&event);
    }

    bool any_deleted = false;
    lock_windows_for_reading();
    size_t iter = 0;
    void* item;
    while (hashmap_iter(windows.map, &iter, &item)) {
        struct EmbeddedWindow* window = *(struct EmbeddedWindow**)item;
        if (window->is_deleted) {
            any_deleted = true;
            continue;
        }

        if (window->do_capture) {
            *need_capture = true;
            if (!window->capture_ready) *capture_ready = false;
        }

        _bolt_rwlock_lock_write(&window->lock);
        bool did_move = false;
        bool did_resize = false;
        snap_window(&window->metadata, window_width, window_height, &did_move, &did_resize);
        struct EmbeddedWindowMetadata metadata = window->metadata;
        _bolt_rwlock_unlock_write(&window->lock);

        _bolt_rwlock_lock_write(&window->input_lock);
        struct WindowPendingInput inputs = window->input;
        memset(&window->input, 0, sizeof(window->input));
        _bolt_rwlock_unlock_write(&window->input_lock);

        if (did_move || did_resize) {
            if (did_resize) {
                struct PluginSurfaceUserdata* ud = window->surface_functions.userdata;
                managed_functions.surface_resize_and_clear(ud, metadata.width, metadata.height);
            }
            struct RepositionEvent event = {.x = metadata.x, .y = metadata.y, .width = metadata.width, .height = metadata.height, .did_resize = did_resize};
            _bolt_plugin_window_onreposition(window, &event);
            window->reposition_mode = false;
        }

        if (window->reposition_mode) {
            if (inputs.mouse_motion) {
                _bolt_window_calc_repos_target(window, &metadata, inputs.mouse_motion_event.x, inputs.mouse_motion_event.y, window_width, window_height);
            }
            if (inputs.mouse_left_up) {
                _bolt_window_calc_repos_target(window, &metadata, inputs.mouse_left_up_event.x, inputs.mouse_left_up_event.y, window_width, window_height);
                if (window->reposition_threshold) {
                    //did_move = (window->metadata.x != window->repos_target_x) || (window->metadata.y != window->repos_target_y);
                    did_resize = (window->metadata.width != window->repos_target_w) || (window->metadata.height != window->repos_target_h);
                    _bolt_rwlock_lock_write(&window->lock);
                    window->metadata.x = window->repos_target_x;
                    window->metadata.y = window->repos_target_y;
                    window->metadata.width = window->repos_target_w;
                    window->metadata.height = window->repos_target_h;
                    metadata = window->metadata;
                    _bolt_rwlock_unlock_write(&window->lock);
                    window->last_user_action_metadata = metadata;

                    if (did_resize) {
                        struct PluginSurfaceUserdata* ud = window->surface_functions.userdata;
                        managed_functions.surface_resize_and_clear(ud, metadata.width, metadata.height);
                    }
                } else {
                    did_resize = false;
                }
                struct RepositionEvent event = {.x = metadata.x, .y = metadata.y, .width = metadata.width, .height = metadata.height, .did_resize = did_resize};
                _bolt_plugin_window_onreposition(window, &event);
                window->reposition_mode = false;
            }
        } else {
            if (inputs.mouse_motion) {
                struct MouseMotionEvent event = {.details = inputs.mouse_motion_event};
                _bolt_plugin_window_onmousemotion(window, &event);
            }
            if (inputs.mouse_left) {
                window->drag_xstart = inputs.mouse_left_event.x;
                window->drag_ystart = inputs.mouse_left_event.y;
                struct MouseButtonEvent event = {.details = inputs.mouse_left_event, .button = MBLeft};
                _bolt_plugin_window_onmousebutton(window, &event);
            }
            if (inputs.mouse_right) {
                struct MouseButtonEvent event = {.details = inputs.mouse_right_event, .button = MBRight};
                _bolt_plugin_window_onmousebutton(window, &event);
            }
            if (inputs.mouse_middle) {
                struct MouseButtonEvent event = {.details = inputs.mouse_middle_event, .button = MBMiddle};
                _bolt_plugin_window_onmousebutton(window, &event);
            }
            if (inputs.mouse_left_up) {
                struct MouseButtonEvent event = {.details = inputs.mouse_left_up_event, .button = MBLeft};
                _bolt_plugin_window_onmousebuttonup(window, &event);
            }
            if (inputs.mouse_right_up) {
                struct MouseButtonEvent event = {.details = inputs.mouse_right_up_event, .button = MBRight};
                _bolt_plugin_window_onmousebuttonup(window, &event);
            }
            if (inputs.mouse_middle_up) {
                struct MouseButtonEvent event = {.details = inputs.mouse_middle_up_event, .button = MBMiddle};
                _bolt_plugin_window_onmousebuttonup(window, &event);
            }
            if (inputs.mouse_scroll_up) {
                struct MouseScrollEvent event = {.details = inputs.mouse_scroll_up_event, .direction = 1};
                _bolt_plugin_window_onscroll(window, &event);
            }
            if (inputs.mouse_scroll_down) {
                struct MouseScrollEvent event = {.details = inputs.mouse_scroll_down_event, .direction = 0};
                _bolt_plugin_window_onscroll(window, &event);
            }
            if (inputs.mouse_leave) {
                struct MouseMotionEvent event = {.details = inputs.mouse_motion_event};
                _bolt_plugin_window_onmouseleave(window, &event);
            }

            if (!embedded_window_meta_eq(&window->last_user_action_metadata, &metadata)) {
                // try to fit the window back to how the user last repositioned it, or as close as possible
                // did_move and did_resize aren't actually used here
                const struct EmbeddedWindowMetadata old_meta = metadata;
                metadata = window->last_user_action_metadata;                
                snap_window(&metadata, window_width, window_height, &did_move, &did_resize);
                if (!embedded_window_meta_eq(&old_meta, &metadata)) {
                    _bolt_rwlock_lock_write(&window->lock);
                    window->metadata = metadata;
                    _bolt_rwlock_unlock_write(&window->lock);
                    struct RepositionEvent event = {.x = metadata.x, .y = metadata.y, .width = metadata.width, .height = metadata.height, .did_resize = did_resize};
                    _bolt_plugin_window_onreposition(window, &event);
                }
            }
        }

        // in case it got deleted by one of the handler functions
        if (window->is_deleted) {
            any_deleted = true;
            continue;
        }

        window->surface_functions.draw_to_surface(window->surface_functions.userdata, overlay_windows.userdata, 0, 0, metadata.width, metadata.height, metadata.x, metadata.y, metadata.width, metadata.height);
        if (window->popup_shown && window->popup_initialised) {
            window->popup_surface_functions.draw_to_surface(window->popup_surface_functions.userdata, overlay_windows.userdata, 0, 0, window->popup_meta.width, window->popup_meta.height, metadata.x + window->popup_meta.x, metadata.y + window->popup_meta.y, window->popup_meta.width, window->popup_meta.height);
        }
        if (window->reposition_mode && window->reposition_threshold) {
            managed_functions.draw_region_outline(overlay_windows.userdata, window->repos_target_x, window->repos_target_y, window->repos_target_w, window->repos_target_h);
        }
    }
    unlock_windows_for_reading();

    if (any_deleted) {
        _bolt_rwlock_lock_write(&windows.lock);
        iter = 0;
        while (hashmap_iter(windows.map, &iter, &item)) {
            struct EmbeddedWindow* window = *(struct EmbeddedWindow**)item;
            if (window->is_deleted) {
                if (window->is_browser) {
                    // destroy shm object
                    _bolt_plugin_shm_close(&window->browser_shm);
                }

                // destroy the plugin registry entry
                lua_getfield(window->plugin, LUA_REGISTRYINDEX, window->is_browser ? BROWSERS_REGISTRYNAME : WINDOWS_REGISTRYNAME);
                lua_pushinteger(window->plugin, window->id);
                lua_pushnil(window->plugin);
                lua_settable(window->plugin, -3);
                lua_pop(window->plugin, 1);

                managed_functions.surface_destroy(window->surface_functions.userdata);
                if (window->is_browser && window->popup_initialised) {
                    managed_functions.surface_destroy(window->popup_surface_functions.userdata);
                }
                _bolt_rwlock_destroy(&window->lock);
                hashmap_delete(windows.map, &window);
                iter = 0;
            }
        }
        _bolt_rwlock_unlock_write(&windows.lock);
    }
}

// does a screen capture, then notifies all the browsers. don't call if any browser isn't ready yet.
static void _bolt_process_captures(uint32_t window_width, uint32_t window_height) {
    const size_t capture_bytes = window_width * window_height * 3;
    uint8_t need_remap = false;
    if (!capture_inited) {
        capture_id = _bolt_plugin_new_windowid();
        _bolt_plugin_shm_open_outbound(&capture_shm, capture_bytes, "sc", capture_id);
        capture_inited = true;
        capture_width = window_width;
        capture_height = window_height;
        need_remap = true;
    } else if (capture_width != window_width || capture_height != window_height) {
#if defined(_WIN32)
        capture_id = _bolt_plugin_new_windowid();
#endif
        _bolt_plugin_shm_resize(&capture_shm, capture_bytes, capture_id);
        capture_width = window_width;
        capture_height = window_height;
        need_remap = true;
    }
    managed_functions.read_screen_pixels(0, 0, window_width, window_height, capture_shm.file);
    
    lock_windows_for_reading();
    size_t iter = 0;
    void* item;
    while (hashmap_iter(windows.map, &iter, &item)) {
        struct EmbeddedWindow* window = *(struct EmbeddedWindow**)item;
        if (window->is_deleted) continue;
        if (window->do_capture) {
            const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_CAPTURENOTIFY_OSR;
            const struct BoltIPCCaptureNotifyHeader header = {
                .plugin_id = window->plugin_id,
                .window_id = window->id,
                .pid = getpid(),
                .capture_id = capture_id,
                .width = window_width,
                .height = window_height,
                .needs_remap = need_remap || (capture_id != window->capture_id),
            };
            _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
            _bolt_ipc_send(fd, &header, sizeof(header));
            window->capture_ready = false;
            window->capture_id = capture_id;
        }
    }
    unlock_windows_for_reading();

    iter = 0;
    while (hashmap_iter(plugins, &iter, &item)) {
        struct Plugin* plugin = *(struct Plugin**)item;
        if (plugin->is_deleted || (plugin->ext_browser_capture_count == 0)) continue;
        void* item2;
        size_t iter2 = 0;
        while (hashmap_iter(plugin->external_browsers, &iter2, &item2)) {
            struct ExternalBrowser* browser = *(struct ExternalBrowser**)item2;
            if (browser->do_capture) {
                const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_CAPTURENOTIFY_EXTERNAL;
                    const struct BoltIPCCaptureNotifyHeader header = {
                    .plugin_id = browser->plugin_id,
                    .window_id = browser->id,
                    .pid = getpid(),
                    .capture_id = capture_id,
                    .width = window_width,
                    .height = window_height,
                    .needs_remap = need_remap || (capture_id != browser->capture_id),
                };
                _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
                _bolt_ipc_send(fd, &header, sizeof(header));
                browser->capture_ready = false;
                browser->capture_id = capture_id;
            }
        }
    }
}

void _bolt_plugin_end_frame(uint32_t window_width, uint32_t window_height) {
    const uint8_t wh_valid = window_width > 1 && window_height > 1;
    if (window_width != overlay_width || window_height != overlay_height) {
        if (overlay_inited) {
            if (wh_valid) {
                managed_functions.surface_resize_and_clear(overlay.userdata, window_width, window_height);
                managed_functions.surface_resize_and_clear(overlay_windows.userdata, window_width, window_height);
            } else {
                managed_functions.surface_destroy(overlay.userdata);
                managed_functions.surface_destroy(overlay_windows.userdata);
                overlay_inited = false;
            }
        } else if (wh_valid) {
            managed_functions.surface_init(&overlay, window_width, window_height, NULL);
            managed_functions.surface_init(&overlay_windows, window_width, window_height, NULL);
            overlay_inited = true;
        }
        overlay_width = window_width;
        overlay_height = window_height;
    }

    _bolt_plugin_handle_messages();
    if (!overlay_inited) return;

    uint8_t need_capture = false;
    uint8_t capture_ready = true;
    _bolt_process_embedded_windows(window_width, window_height, &need_capture, &capture_ready);
    _bolt_process_plugins(&need_capture, &capture_ready);

    uint64_t micros = 0;
    _bolt_monotonic_microseconds(&micros);
    if (need_capture && capture_ready && (micros >= next_capture_time)) {
        _bolt_process_captures(window_width, window_height);
        next_capture_time = micros + CAPTURE_COOLDOWN_MICROS;
        if (next_capture_time < micros) next_capture_time = micros;
    } else if (!need_capture && capture_inited) {
        _bolt_plugin_shm_close(&capture_shm);
        capture_inited = false;
    }

    struct SwapBuffersEvent event;
    _bolt_plugin_handle_swapbuffers(&event);
    overlay.draw_to_screen(overlay.userdata, 0, 0, window_width, window_height, 0, 0, window_width, window_height);
    overlay_windows.draw_to_screen(overlay_windows.userdata, 0, 0, window_width, window_height, 0, 0, window_width, window_height);
    overlay.clear(overlay.userdata, 0.0, 0.0, 0.0, 0.0);
    overlay_windows.clear(overlay_windows.userdata, 0.0, 0.0, 0.0, 0.0);
}

void _bolt_plugin_close() {
    _bolt_plugin_ipc_close(fd);
    size_t iter = 0;
    void* item;
    _bolt_rwlock_lock_write(&windows.lock);
    while (hashmap_iter(windows.map, &iter, &item)) {
        struct EmbeddedWindow* window = *(struct EmbeddedWindow**)item;
        if (!window->is_deleted) {
            if (window->is_browser) {
                _bolt_plugin_shm_close(&window->browser_shm);
            }
            managed_functions.surface_destroy(window->surface_functions.userdata);
            _bolt_rwlock_destroy(&window->lock);
        }
    }
    hashmap_clear(windows.map, true);
    _bolt_rwlock_unlock_write(&windows.lock);
    iter = 0;
    while (hashmap_iter(plugins, &iter, &item)) {
        struct Plugin* plugin = *(struct Plugin**)item;
        _bolt_plugin_free(plugin);
    }
    
    hashmap_free(plugins);
    if (capture_inited) {
        _bolt_plugin_shm_close(&capture_shm);
        capture_inited = false;
    }
    inited = 0;
}

void _bolt_plugin_notify_stopped(uint64_t id) {
    const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_CLIENT_STOPPED_PLUGIN;
    const struct BoltIPCClientStoppedPluginHeader header = { .plugin_id = id };
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &header, sizeof(header));
}

struct WindowInfo* _bolt_plugin_windowinfo() {
    return &windows;
}

static void _bolt_receive_discard(uint64_t amount) {
    uint8_t buf[0x10000];
    uint64_t i = 0;
    while (amount - i > sizeof(buf)) {
        _bolt_ipc_receive(fd, buf, sizeof(buf));
        amount -= sizeof(buf);
    }
    if (i < amount) {
        _bolt_ipc_receive(fd, buf, amount - i);
    }
}

/// can return nullptr if the browser is nonexistent or deleted
static struct ExternalBrowser* get_externalbrowser(const uint64_t plugin_id, const uint64_t* window_id) {
    const struct Plugin p = {.id = plugin_id};
    const struct Plugin* pp = &p;
    struct Plugin** plugin = (struct Plugin**)hashmap_get(plugins, &pp);
    if (!plugin) return NULL;
    struct ExternalBrowser** browser = (struct ExternalBrowser**)hashmap_get((*plugin)->external_browsers, &window_id);
    if (!browser) return NULL;
    return *browser;
}

/// can return nullptr if the window is nonexistent or deleted
static struct EmbeddedWindow* get_embeddedwindow(const uint64_t* window_id) {
    lock_windows_for_reading();
    struct EmbeddedWindow** pwindow = (struct EmbeddedWindow**)hashmap_get(windows.map, &window_id);
    if (!pwindow) {
        unlock_windows_for_reading();
        return NULL;
    }
    struct EmbeddedWindow* window = *pwindow;
    unlock_windows_for_reading();
    if (window->is_deleted) return NULL;
    return window;
}

static void handle_plugin_message(lua_State* state, struct BoltIPCBrowserMessageHeader* header) {
    lua_getfield(state, LUA_REGISTRYINDEX, BROWSERS_REGISTRYNAME); /*stack: window table*/
    lua_pushinteger(state, header->window_id); /*stack: window table, window id*/
    lua_gettable(state, -2); /*stack: window table, event table*/
    lua_pushinteger(state, BROWSER_ONMESSAGE); /*stack: window table, event table, event id*/
    lua_gettable(state, -2); /*stack: window table, event table, function or nil*/
    if (lua_isfunction(state, -1)) {
        void* data = lua_newuserdata(state, header->message_size); /* window table, event table, function, message userdata*/
        _bolt_ipc_receive(fd, data, header->message_size);
        lua_pushvalue(state, -2); /* window table, event table, function, message userdata, function*/
        lua_pushlstring(state, data, header->message_size); /*stack: window table, event table, function, message userdata, function, message*/
        if (lua_pcall(state, 1, 0, 0)) { /*stack: window table, event table, function, message userdata, ?error*/
            const char* e = lua_tolstring(state, -1, 0);
            printf("plugin browser onmessage error: %s\n", e);
            lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME); /*stack: window table, event table, function, message userdata, error, plugin*/
            const struct Plugin* plugin = lua_touserdata(state, -1);
            lua_pop(state, 6); /*stack: (empty)*/
            _bolt_plugin_stop(plugin->id);
            _bolt_plugin_notify_stopped(plugin->id);
        } else {
            lua_pop(state, 4); /*stack: (empty)*/
        }
    } else {
        _bolt_receive_discard(header->message_size);
        lua_pop(state, 3); /*stack: (empty)*/
    }
}

static void handle_close_request(lua_State* state, uint64_t window_id) {
    lua_getfield(state, LUA_REGISTRYINDEX, BROWSERS_REGISTRYNAME); /*stack: window table*/
    lua_pushinteger(state, window_id); /*stack: window table, window id*/
    lua_gettable(state, -2); /*stack: window table, event table*/
    lua_pushinteger(state, BROWSER_ONCLOSEREQUEST); /*stack: window table, event table, event id*/
    lua_gettable(state, -2); /*stack: window table, event table, function or nil*/
    if (lua_isfunction(state, -1)) {
        if (lua_pcall(state, 0, 0, 0)) { /*stack: window table, event table, ?error*/
            const char* e = lua_tolstring(state, -1, 0);
            printf("plugin browser oncloserequest error: %s\n", e);
            lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME); /*stack: window table, event table, error, plugin*/
            const struct Plugin* plugin = lua_touserdata(state, -1);
            lua_pop(state, 4); /*stack: (empty)*/
            _bolt_plugin_stop(plugin->id);
            _bolt_plugin_notify_stopped(plugin->id);
        } else {
            lua_pop(state, 2); /*stack: (empty)*/
        }
    } else {
        lua_pop(state, 3); /*stack: (empty)*/
    }
}

#define IPCCASE(NAME, STRUCT) case IPC_MSG_##NAME: { \
    struct BoltIPC##STRUCT##Header header; \
    _bolt_ipc_receive(fd, &header, sizeof(header)); \
    handle_ipc_##NAME(&header); \
    break; \
}

#define IPCCASEWINDOW(NAME, STRUCT) case IPC_MSG_##NAME: { \
    struct BoltIPC##STRUCT##Header header; \
    _bolt_ipc_receive(fd, &header, sizeof(header)); \
    struct EmbeddedWindow* window = get_embeddedwindow(&header.window_id); \
    if (window && !window->is_deleted) handle_ipc_##NAME(&header, window); \
    break; \
}

#define IPCCASEWINDOWTAIL(NAME, STRUCT) case IPC_MSG_##NAME: { \
    struct BoltIPC##STRUCT##Header header; \
    _bolt_ipc_receive(fd, &header, sizeof(header)); \
    struct EmbeddedWindow* window = get_embeddedwindow(&header.window_id); \
    if (window && !window->is_deleted) handle_ipc_##NAME(&header, window); \
    else _bolt_receive_discard(get_tail_ipc_##STRUCT(&header)); \
    break; \
}

#define IPCCASEBROWSER(NAME, STRUCT) case IPC_MSG_##NAME: { \
    struct BoltIPC##STRUCT##Header header; \
    _bolt_ipc_receive(fd, &header, sizeof(header)); \
    struct ExternalBrowser* window = get_externalbrowser(header.plugin_id, &header.window_id); \
    if (window && !window->plugin->is_deleted) handle_ipc_##NAME(&header, window); \
    break; \
}

#define IPCCASEBROWSERTAIL(NAME, STRUCT) case IPC_MSG_##NAME: { \
    struct BoltIPC##STRUCT##Header header; \
    _bolt_ipc_receive(fd, &header, sizeof(header)); \
    struct ExternalBrowser* window = get_externalbrowser(header.plugin_id, &header.window_id); \
    if (window && !window->plugin->is_deleted) handle_ipc_##NAME(&header, window); \
    else _bolt_receive_discard(get_tail_ipc_##STRUCT(&header)); \
    break; \
}

static void handle_ipc_STARTPLUGIN(struct BoltIPCStartPluginHeader* header) {
    // note: incoming messages are sanitised by the UI, by replacing `\` with `/` and
    // making sure to leave a trailing slash, when initiating this type of message
    // (see PluginMenu.svelte)
    struct Plugin* plugin = malloc(sizeof(struct Plugin));
    plugin->external_browsers = hashmap_new(sizeof(struct ExternalBrowser), 8, 0, 0, _bolt_window_map_hash, _bolt_window_map_compare, NULL, NULL);
    plugin->state = luaL_newstate();
    plugin->id = header->uid;
    plugin->path = malloc(header->path_size);
    plugin->path_length = header->path_size;
    plugin->config_path = malloc(header->config_path_size);
    plugin->config_path_length = header->config_path_size;
    plugin->ext_browser_capture_count = 0;
    plugin->is_deleted = false;
    _bolt_ipc_receive(fd, plugin->path, header->path_size);
    char* full_path = lua_newuserdata(plugin->state, header->path_size + header->main_size + 1);
    memcpy(full_path, plugin->path, header->path_size);
    _bolt_ipc_receive(fd, full_path + header->path_size, header->main_size);
    _bolt_ipc_receive(fd, plugin->config_path, header->config_path_size);
    full_path[header->path_size + header->main_size] = '\0';
    if (_bolt_plugin_add(full_path, plugin)) {
        lua_pop(plugin->state, 1);
    } else {
        _bolt_plugin_notify_stopped(header->uid);
        plugin->is_deleted = true;
    }
}

static void handle_ipc_HOST_STOPPED_PLUGIN(struct BoltIPCHostStoppedPluginHeader* header) {
    _bolt_plugin_stop(header->plugin_id);
}

static size_t get_tail_ipc_OsrUpdate(const struct BoltIPCOsrUpdateHeader* header) {
    return (size_t)header->rect_count * sizeof(struct BoltIPCOsrUpdateRect);
}

static void handle_ipc_OSRUPDATE(struct BoltIPCOsrUpdateHeader* header, struct EmbeddedWindow* window) {
    struct BoltIPCOsrUpdateRect rect;
    if (header->needs_remap) {
        _bolt_plugin_shm_remap(&window->browser_shm, (size_t)header->width * (size_t)header->height * 4, header->needs_remap);
    }
    for (uint32_t i = 0; i < header->rect_count; i += 1) {
        // the backend needs contiguous pixels for the rectangle, so here we ignore
        // the width of the damage and instead update every row of pixels in the
        // damage area. that way we have contiguous pixels straight out of the shm file.
        // would it be faster to allocate and build a contiguous pixel rect and use
        // that as the subimage? probably not.
        _bolt_ipc_receive(fd, &rect, sizeof(rect));
        const void* data_ptr = (const void*)((uint8_t*)window->browser_shm.file + (header->width * rect.y * 4));
        window->surface_functions.subimage(window->surface_functions.userdata, 0, rect.y, header->width, rect.h, data_ptr, 1);
    }
    const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_OSRUPDATE_ACK;
    const struct BoltIPCOsrUpdateAckHeader ack_header = { .window_id = header->window_id, .plugin_id = window->plugin_id };
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &ack_header, sizeof(ack_header));
}

static size_t get_tail_ipc_OsrPopupContents(const struct BoltIPCOsrPopupContentsHeader* header) {
    return (size_t)header->width * (size_t)header->height * 4;
}

static void handle_ipc_OSRPOPUPCONTENTS(struct BoltIPCOsrPopupContentsHeader* header, struct EmbeddedWindow* window) {
    const size_t rgba_size = get_tail_ipc_OsrPopupContents(header);
    void* rgba = lua_newuserdata(window->plugin, rgba_size);
    struct EmbeddedWindowMetadata* meta = &window->popup_meta;
    _bolt_ipc_receive(fd, rgba, rgba_size);
    if (!window->popup_initialised) {
        managed_functions.surface_init(&window->popup_surface_functions, header->width, header->height, NULL);
        window->popup_initialised = true;
    } else if (header->width != window->popup_meta.width || header->height != window->popup_meta.height) {
        managed_functions.surface_resize_and_clear(window->popup_surface_functions.userdata, header->width, header->height);
    }
    meta->width = header->width;
    meta->height = header->height;
    window->popup_surface_functions.subimage(window->popup_surface_functions.userdata, 0, 0, header->width, header->height, rgba, true);
    lua_pop(window->plugin, 1);
}

static void handle_ipc_OSRPOPUPPOSITION(struct BoltIPCOsrPopupPositionHeader* header, struct EmbeddedWindow* window) {
    window->popup_meta.x = header->x;
    window->popup_meta.y = header->y;
}

static void handle_ipc_OSRPOPUPVISIBILITY(struct BoltIPCOsrPopupVisibilityHeader* header, struct EmbeddedWindow* window) {
    window->popup_shown = header->visible;
}

static size_t get_tail_ipc_BrowserMessage(const struct BoltIPCBrowserMessageHeader* header) {
    return header->message_size;
}

static void handle_ipc_EXTERNALBROWSERMESSAGE(struct BoltIPCBrowserMessageHeader* header, struct ExternalBrowser* browser) {
    handle_plugin_message(browser->plugin->state, header);
}

static void handle_ipc_OSRBROWSERMESSAGE(struct BoltIPCBrowserMessageHeader* header, struct EmbeddedWindow* window) {
    handle_plugin_message(window->plugin, header);
}

static void handle_ipc_OSRSTARTREPOSITION(struct BoltIPCOsrStartRepositionHeader* header, struct EmbeddedWindow* window) {
    window->reposition_mode = true;
    window->reposition_threshold = false;
    window->reposition_w = header->horizontal;
    window->reposition_h = header->vertical;
}

static void handle_ipc_OSRCANCELREPOSITION(struct BoltIPCOsrCancelRepositionHeader* header, struct EmbeddedWindow* window) {
    window->reposition_mode = false;
}

static void handle_ipc_BROWSERCLOSEREQUEST(struct BoltIPCBrowserCloseRequestHeader* header, struct ExternalBrowser* browser) {
    handle_close_request(browser->plugin->state, header->window_id);
}

static void handle_ipc_OSRCLOSEREQUEST(struct BoltIPCOsrCloseRequestHeader* header, struct EmbeddedWindow* window) {
    handle_close_request(window->plugin, header->window_id);
}

static void handle_ipc_EXTERNALCAPTUREDONE(struct BoltIPCExternalCaptureDoneHeader* header, struct ExternalBrowser* browser) {
    browser->capture_ready = true;
}

static void handle_ipc_OSRCAPTUREDONE(struct BoltIPCOsrCaptureDoneHeader* header, struct EmbeddedWindow* window) {
    window->capture_ready = true;
}

void _bolt_plugin_handle_messages() {
    enum BoltIPCMessageTypeToHost msg_type;
    while (_bolt_ipc_poll(fd)) {
        if (_bolt_ipc_receive(fd, &msg_type, sizeof(msg_type)) != 0) break;
        switch (msg_type) {
            IPCCASE(STARTPLUGIN, StartPlugin)
            IPCCASE(HOST_STOPPED_PLUGIN, HostStoppedPlugin)
            IPCCASEWINDOWTAIL(OSRUPDATE, OsrUpdate)
            IPCCASEWINDOWTAIL(OSRPOPUPCONTENTS, OsrPopupContents)
            IPCCASEWINDOW(OSRPOPUPPOSITION, OsrPopupPosition)
            IPCCASEWINDOW(OSRPOPUPVISIBILITY, OsrPopupVisibility)
            IPCCASEBROWSERTAIL(EXTERNALBROWSERMESSAGE, BrowserMessage)
            IPCCASEWINDOWTAIL(OSRBROWSERMESSAGE, BrowserMessage)
            IPCCASEWINDOW(OSRSTARTREPOSITION, OsrStartReposition)
            IPCCASEWINDOW(OSRCANCELREPOSITION, OsrCancelReposition)
            IPCCASEBROWSER(BROWSERCLOSEREQUEST, BrowserCloseRequest)
            IPCCASEWINDOW(OSRCLOSEREQUEST, OsrCloseRequest)
            IPCCASEBROWSER(EXTERNALCAPTUREDONE, ExternalCaptureDone)
            IPCCASEWINDOW(OSRCAPTUREDONE, OsrCaptureDone)
            default:
                printf("unknown message type %i\n", (int)msg_type);
                break;
        }
    }
}

static uint8_t point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return rx <= x && rx + rw > x && ry <= y && ry + rh > y;
}

uint8_t _bolt_plugin_handle_mouse_event(struct MouseEvent* event, ptrdiff_t bool_offset, ptrdiff_t event_offset, uint8_t grab_type, uint8_t* mousein_fake, uint8_t* mousein_real) {
    // IMPORTANT: this function is called from the input thread, unlike everything else in this file
    // which is called from the "main"/graphics thread, so any global variable access or function calls
    //must be done with caution. In particular, this function must not use lock_windows_for_reading().
    if (mousein_real) *mousein_real = true;
    uint8_t ret = true;
    _bolt_rwlock_lock_read(&windows.lock);

    // if the left mouse button is being held, try to route this event to the window that was clicked on
    if (event->mb_left || grab_type == GRAB_TYPE_STOP) {
        // we can only use this route if the grabbed window actually exists, otherwise fall through.
        // that will also happen if the actual game window was grabbed, since that has ID 0, which is not a valid hashmap entry.
        uint64_t* pp = &grabbed_window_id;
        struct EmbeddedWindow* const* const window = hashmap_get(windows.map, &pp);
        if (grab_type == GRAB_TYPE_STOP) grabbed_window_id = 0;
        if (window) {
            // `window` grabs this input, so route everything here then return.
            uint8_t do_mouseleave = 0;
            _bolt_rwlock_lock_read(&(*window)->lock);
            // if this input is a left-mouse-button-release, then the window has been un-grabbed, so
            // figure out if we need to send a mouseleave event to the grabbed window. do this by
            // checking if the cursor is currently in the window's rectangle.
            if (grab_type == GRAB_TYPE_STOP) {
                do_mouseleave = !point_in_rect(event->x, event->y, (*window)->metadata.x, (*window)->metadata.y, (*window)->metadata.width, (*window)->metadata.height);
            }
            // offset the x and y to be relative to the embedded window instead of the game client area
            event->x -= (*window)->metadata.x;
            event->y -= (*window)->metadata.y;
            _bolt_rwlock_unlock_read(&(*window)->lock);

            // write the relevant events to the window
            _bolt_rwlock_lock_write(&(*window)->input_lock);
            *(uint8_t*)(((uint8_t*)&(*window)->input) + bool_offset) = 1;
            *(struct MouseEvent*)(((uint8_t*)&(*window)->input) + event_offset) = *event;
            if (do_mouseleave) {
                (*window)->input.mouse_leave = 1;
                (*window)->input.mouse_leave_event = *event;
            }
            _bolt_rwlock_unlock_write(&(*window)->input_lock);

            // save this window as the most recent one to receive an event, unlock the windows mutex
            // before returning, then return 0 to indicate that this event shouldn't be forwarded to the game
            _bolt_rwlock_unlock_read(&windows.lock);
            last_mouseevent_window_id = (*window)->id;
            return false;
        }
    }

    // normal route - look through all the windows to find one that's under the cursor
    size_t iter = 0;
    void* item;
    const uint64_t* pp = &last_mouseevent_window_id;
    struct EmbeddedWindow* const* mouseleave_window = hashmap_get(windows.map, &pp);
    while (hashmap_iter(windows.map, &iter, &item)) {
        struct EmbeddedWindow* const* const window = item;

        // if the mouse is in this rect, set `ret` to false, to 0 that this event shouldn't be
        // forwarded to the game, and that this loop shouldn't continue after this iteration.
        _bolt_rwlock_lock_read(&(*window)->lock);
        if (point_in_rect(event->x, event->y, (*window)->metadata.x, (*window)->metadata.y, (*window)->metadata.width, (*window)->metadata.height)) {
            // offset the x and y to be relative to the embedded window instead of the game client area
            event->x -= (*window)->metadata.x;
            event->y -= (*window)->metadata.y;
            ret = false;
        }
        _bolt_rwlock_unlock_read(&(*window)->lock);

        if (!ret) {
            // if this is the same window as the previous one that received a mouse event, set
            // `mouseleave_window` to null, to indicate that no mouseleave event needs to be sent
            // anywhere. otherwise, leave it set, but update `last_mouseevent_window_id`.
            if (last_mouseevent_window_id == (*window)->id) mouseleave_window = NULL;
            else last_mouseevent_window_id = (*window)->id;
            if (mousein_fake) *mousein_fake = false;
            if (grab_type == GRAB_TYPE_START) grabbed_window_id = (*window)->id;

            // write the relevant event to this window
            _bolt_rwlock_lock_write(&(*window)->input_lock);
            (*window)->input.mouse_leave = 0;
            *(uint8_t*)(((uint8_t*)&(*window)->input) + bool_offset) = 1;
            *(struct MouseEvent*)(((uint8_t*)&(*window)->input) + event_offset) = *event;
            _bolt_rwlock_unlock_write(&(*window)->input_lock);
            break;
        }
    }
    // if a window needs to receive a mouseleave event, set it now before unlocking the windows mutex.
    if (mouseleave_window) {
        _bolt_rwlock_lock_write(&(*mouseleave_window)->input_lock);
        (*mouseleave_window)->input.mouse_leave = 1;
        (*mouseleave_window)->input.mouse_leave_event = *event;
        _bolt_rwlock_unlock_write(&(*mouseleave_window)->input_lock);
    }
    _bolt_rwlock_unlock_read(&windows.lock);

    // if `ret` is false, this event was consumed by an embedded window, so return 0 immediately to indicate that.
    // otherwise, set relevant events and variables for the game window, and return 1.
    if (!ret) return false;
    last_mouseevent_window_id = 0;
    if (mousein_fake) *mousein_fake = true;
    _bolt_rwlock_lock_write(&windows.input_lock);
    *(uint8_t*)(((uint8_t*)&windows.input) + bool_offset) = 1;
    *(struct MouseEvent*)(((uint8_t*)&windows.input) + event_offset) = *event;
    _bolt_rwlock_unlock_write(&windows.input_lock);
    return true;
}

DEFINE_CALLBACK_STATIC(swapbuffers, SwapBuffersEvent)
DEFINE_CALLBACK(render2d, RenderBatch2D)
DEFINE_CALLBACK(render3d, Render3D)
DEFINE_CALLBACK(renderparticles, RenderParticles)
DEFINE_CALLBACK(renderbillboard, RenderBillboard)
DEFINE_CALLBACK(rendericon, RenderIconEvent)
DEFINE_CALLBACK(renderbigicon, RenderIconEvent)
DEFINE_CALLBACK(minimapterrain, MinimapTerrainEvent)
DEFINE_CALLBACK(minimaprender2d, RenderBatch2D)
DEFINE_CALLBACK(renderminimap, RenderMinimapEvent)
DEFINE_CALLBACK(mousemotion, MouseMotionEvent)
DEFINE_CALLBACK(mousebutton, MouseButtonEvent)
DEFINE_CALLBACK(mousebuttonup, MouseButtonEvent)
DEFINE_CALLBACK(scroll, MouseScrollEvent)
DEFINE_WINDOWEVENT(reposition, REPOSITION, RepositionEvent)
DEFINE_WINDOWEVENT(mousemotion, MOUSEMOTION, MouseMotionEvent)
DEFINE_WINDOWEVENT(mousebutton, MOUSEBUTTON, MouseButtonEvent)
DEFINE_WINDOWEVENT(mousebuttonup, MOUSEBUTTONUP, MouseButtonEvent)
DEFINE_WINDOWEVENT(scroll, SCROLL, MouseScrollEvent)
DEFINE_WINDOWEVENT(mouseleave, MOUSELEAVE, MouseMotionEvent)

uint8_t _bolt_plugin_add(const char* path, struct Plugin* plugin) {
    // load the user-provided string as a lua function, putting that function on the stack
    if (luaL_loadfile(plugin->state, path)) {
        const char* e = lua_tolstring(plugin->state, -1, 0);
        printf("plugin load error: %s\n", e);
        lua_pop(plugin->state, 1);
        return 0;
    }

    // put this into our list of plugins (important to do this before lua_pcall)
    struct Plugin* const* old_plugin = hashmap_set(plugins, &plugin);
    if (hashmap_oom(plugins)) {
        printf("plugin load error: out of memory\n");
        return 0;
    }
    if (old_plugin) {
        // a plugin with this id was already running and we just overwrote it, so make sure not to leak the memory.
        // this shouldn't happen in practice because IDs are incremental, but what if someone overflows the ID to 0
        // by starting 18 quintillion plugins in one session? okay, yes, it's very unlikely.
        printf("plugin ID %llu has been overwritten by one with the same ID\n", (unsigned long long)plugin->id);
        (*old_plugin)->is_deleted = true;
    }

    // add the struct pointer to the registry
    lua_pushliteral(plugin->state, PLUGIN_REGISTRYNAME);
    lua_pushlightuserdata(plugin->state, plugin);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // Open just the specific libraries plugins are allowed to have
    lua_pushcfunction(plugin->state, luaopen_base);
    lua_call(plugin->state, 0, 0);
    lua_pushcfunction(plugin->state, luaopen_package);
    lua_call(plugin->state, 0, 0);
    lua_pushcfunction(plugin->state, luaopen_string);
    lua_call(plugin->state, 0, 0);
    lua_pushcfunction(plugin->state, luaopen_table);
    lua_call(plugin->state, 0, 0);
    lua_pushcfunction(plugin->state, luaopen_math);
    lua_call(plugin->state, 0, 0);

    // load Bolt API into package.preload, so that `require("bolt")` will find it
    lua_getfield(plugin->state, LUA_GLOBALSINDEX, "package");
    lua_getfield(plugin->state, -1, "preload");
    lua_pushliteral(plugin->state, "bolt");
    lua_pushcfunction(plugin->state, _bolt_api_init);
    lua_settable(plugin->state, -3);
    // now set package.path to the plugin's root path
    char* search_path = lua_newuserdata(plugin->state, plugin->path_length + 5);
    memcpy(search_path, plugin->path, plugin->path_length);
    memcpy(&search_path[plugin->path_length], "?.lua", 5);
    lua_pushliteral(plugin->state, "path");
    lua_pushlstring(plugin->state, search_path, plugin->path_length + 5);
    lua_settable(plugin->state, -5);
    lua_pop(plugin->state, 2);
    // finally, restrict package.loaders by removing the module searcher and all-in-one searcher,
    // because these can load .dll and .so files which are a huge security concern, and also
    // because stupid people will make windows-only plugins with it and I'm not dealing with that
    lua_getfield(plugin->state, -1, "loaders");
    lua_pushnil(plugin->state);
    lua_pushnil(plugin->state);
    lua_rawseti(plugin->state, -3, 3);
    lua_rawseti(plugin->state, -2, 4);
    lua_pop(plugin->state, 2);

    // create window table (empty)
    lua_pushliteral(plugin->state, WINDOWS_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create browsers table (empty)
    lua_pushliteral(plugin->state, BROWSERS_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create metatables
#define SETMETA(NAME) \
lua_pushliteral(plugin->state, #NAME "meta"); \
_bolt_api_push_metatable_##NAME(plugin->state); \
lua_settable(plugin->state, LUA_REGISTRYINDEX);
#define DUPEMETA(NAME, NEWNAME) \
lua_pushliteral(plugin->state, #NEWNAME "meta"); \
lua_pushliteral(plugin->state, #NAME "meta"); \
lua_gettable(plugin->state, LUA_REGISTRYINDEX); \
lua_settable(plugin->state, LUA_REGISTRYINDEX);
    SETMETA(render2d)
    SETMETA(render3d)
    SETMETA(renderparticles)
    SETMETA(renderbillboard)
    SETMETA(rendericon)
    SETMETA(minimapterrain)
    SETMETA(renderminimap)
    SETMETA(point)
    SETMETA(transform)
    SETMETA(buffer)
    SETMETA(shader)
    SETMETA(shaderprogram)
    SETMETA(shaderbuffer)
    SETMETA(swapbuffers)
    SETMETA(surface)
    SETMETA(window)
    SETMETA(browser)
    SETMETA(embeddedbrowser)
    SETMETA(reposition)
    SETMETA(mousemotion)
    SETMETA(mousebutton)
    SETMETA(scroll)
    SETMETA(mouseleave)
    DUPEMETA(mousebutton, mousebuttonup)
    DUPEMETA(render2d, minimaprender2d)
    DUPEMETA(rendericon, renderbigicon)
#undef SETMETA
#undef DUPEMETA

    // attempt to run the function
    if (lua_pcall(plugin->state, 0, 0, 0)) {
        delete_windows_by_uid(plugin->id);
        const char* e = lua_tolstring(plugin->state, -1, 0);
        printf("plugin startup error: %s\n", e);
        lua_pop(plugin->state, 1);
        plugin->is_deleted = true;
        return 0;
    } else {
        return 1;
    }
}

void _bolt_plugin_stop(uint64_t uid) {
    delete_windows_by_uid(uid);
    struct Plugin p = {.id = uid};
    struct Plugin* pp = &p;
    struct Plugin* const* plugin = hashmap_get(plugins, &pp);
    (*plugin)->is_deleted = true;
}

void _bolt_plugin_ipc_init(BoltSocketType* fd) {
#if defined(_WIN32)
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    const int olderr = errno;
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    *fd = socket(addr.sun_family, SOCK_STREAM, 0);
#if defined(_WIN32)
    const char* runtime_dir = getenv("appdata");
    snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s\\bolt-launcher\\run\\ipc-0", runtime_dir);
#else
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char* prefix = (runtime_dir && *runtime_dir) ? runtime_dir : "/tmp";
    snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s/bolt-launcher/ipc-0", prefix);
#endif
    if (connect(*fd, (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("error: IPC connect() error %i\n", errno);
        exit(1);
    }
    errno = olderr;
}

void _bolt_plugin_ipc_close(BoltSocketType fd) {
    const int olderr = errno;
    close(fd);
    errno = olderr;
}

void _bolt_plugin_draw_to_overlay(const struct SurfaceFunctions* from, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    if (overlay_inited) {
        from->draw_to_surface(from->userdata, overlay.userdata, sx, sy, sw, sh, dx, dy, dw, dh);
    }
}

void _bolt_plugin_overlay_size(int* w, int* h) {
    *w = overlay_width;
    *h = overlay_height;
}
