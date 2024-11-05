#include "../ipc.h"

#include "plugin.h"
#include "plugin_api.h"
#include "../../../modules/hashmap/hashmap.h"
#include "../../../modules/spng/spng/spng.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <afunix.h>
#define close closesocket
LARGE_INTEGER performance_frequency;
#else
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#define API_VERSION_MAJOR 1
#define API_VERSION_MINOR 0

#define API_ADD(FUNC) lua_pushliteral(state, #FUNC);lua_pushcfunction(state, api_##FUNC);lua_settable(state, -3);
#define API_ADD_SUB(STATE, FUNC, SUB) lua_pushliteral(STATE, #FUNC);lua_pushcfunction(STATE, api_##SUB##_##FUNC);lua_settable(STATE, -3);
#define API_ADD_SUB_ALIAS(STATE, FUNC, ALIAS, SUB) lua_pushliteral(STATE, #ALIAS);lua_pushcfunction(STATE, api_##SUB##_##FUNC);lua_settable(STATE, -3);

#define WINDOW_MIN_SIZE 20

#define PLUGIN_REGISTRYNAME "plugin"
#define WINDOWS_REGISTRYNAME "windows"
#define BROWSERS_REGISTRYNAME "browsers"
#define BATCH2D_META_REGISTRYNAME "batch2dmeta"
#define RENDER3D_META_REGISTRYNAME "render3dmeta"
#define MINIMAP_META_REGISTRYNAME "minimapmeta"
#define POINT_META_REGISTRYNAME "pointmeta"
#define TRANSFORM_META_REGISTRYNAME "transformmeta"
#define BUFFER_META_REGISTRYNAME "buffermeta"
#define SWAPBUFFERS_META_REGISTRYNAME "swapbuffersmeta"
#define SURFACE_META_REGISTRYNAME "surfacemeta"
#define REPOSITION_META_REGISTRYNAME "repositionmeta"
#define MOUSEMOTION_META_REGISTRYNAME "mousemotionmeta"
#define MOUSEBUTTON_META_REGISTRYNAME "mousebuttonmeta"
#define MOUSEBUTTONUP_META_REGISTRYNAME MOUSEBUTTON_META_REGISTRYNAME
#define SCROLL_META_REGISTRYNAME "scrollmeta"
#define MOUSELEAVE_META_REGISTRYNAME "mouseleavemeta"
#define WINDOW_META_REGISTRYNAME "windowmeta"
#define BROWSER_META_REGISTRYNAME "browsermeta"
#define EMBEDDEDBROWSER_META_REGISTRYNAME "embeddedbrowsermeta"
#define SWAPBUFFERS_CB_REGISTRYNAME "swapbufferscb"
#define BATCH2D_CB_REGISTRYNAME "batch2dcb"
#define RENDER3D_CB_REGISTRYNAME "render3dcb"
#define MINIMAP_CB_REGISTRYNAME "minimapcb"
#define MOUSEMOTION_CB_REGISTRYNAME "mousemotioncb"
#define MOUSEBUTTON_CB_REGISTRYNAME "mousebuttoncb"
#define MOUSEBUTTONUP_CB_REGISTRYNAME "mousebuttonupcb"
#define SCROLL_CB_REGISTRYNAME "scrollcb"
#define MESSAGE_CB_REGISTRYNAME "messagecb"

enum {
    WINDOW_USERDATA,
    WINDOW_ONREPOSITION,
    WINDOW_ONMOUSEMOTION,
    WINDOW_ONMOUSEBUTTON,
    WINDOW_ONMOUSEBUTTONUP,
    WINDOW_ONSCROLL,
    WINDOW_ONMOUSELEAVE,
    WINDOW_EVENT_ENUM_SIZE, // last member of enum
};

enum {
    BROWSER_USERDATA,
    BROWSER_ONCLOSEREQUEST,
    BROWSER_ONMESSAGE,
    BROWSER_EVENT_ENUM_SIZE, // last member of enum
};

static struct PluginManagedFunctions managed_functions;

static uint64_t next_window_id;
static struct WindowInfo windows;

static struct SurfaceFunctions overlay;
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

uint64_t _bolt_plugin_get_last_mouseevent_windowid() { return last_mouseevent_window_id; }

// a currently-running plugin.
// note "path" is not null-terminated, and must always be converted to use '/' as path-separators
// and must always end with a trailing separator by the time it's received by this process.
struct Plugin {
    lua_State* state;
    uint64_t id; // refers to this specific activation of the plugin, not the plugin in general
    struct hashmap* external_browsers;
    size_t ext_browser_capture_count;
    char* path;
    uint32_t path_length;
    char* config_path;
    uint32_t config_path_length;
    uint8_t is_deleted;
};

struct ExternalBrowser {
    uint64_t id;
    uint64_t plugin_id;
    struct Plugin* plugin;
    uint8_t do_capture;
    uint8_t capture_ready;
    uint64_t capture_id;
};

struct FixedBuffer {
    void* data;
    size_t size;
};

static void _bolt_plugin_ipc_init(BoltSocketType*);
static void _bolt_plugin_ipc_close(BoltSocketType);

static void _bolt_plugin_window_onreposition(struct EmbeddedWindow*, struct RepositionEvent*);
static void _bolt_plugin_window_onmousemotion(struct EmbeddedWindow*, struct MouseMotionEvent*);
static void _bolt_plugin_window_onmousebutton(struct EmbeddedWindow*, struct MouseButtonEvent*);
static void _bolt_plugin_window_onmousebuttonup(struct EmbeddedWindow*, struct MouseButtonEvent*);
static void _bolt_plugin_window_onscroll(struct EmbeddedWindow*, struct MouseScrollEvent*);
static void _bolt_plugin_window_onmouseleave(struct EmbeddedWindow*, struct MouseMotionEvent*);
static void _bolt_plugin_handle_mousemotion(struct MouseMotionEvent*);
static void _bolt_plugin_handle_mousebutton(struct MouseButtonEvent*);
static void _bolt_plugin_handle_mousebuttonup(struct MouseButtonEvent*);
static void _bolt_plugin_handle_scroll(struct MouseScrollEvent*);

static void _bolt_plugin_stop(uint64_t id);
static void _bolt_plugin_handle_swapbuffers(struct SwapBuffersEvent*);

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

static struct hashmap* plugins;

// macro for defining callback functions "_bolt_plugin_handle_*" and "api_on*"
// e.g. DEFINE_CALLBACK(swapbuffers, SWAPBUFFERS, SwapBuffersEvent)
#define DEFINE_CALLBACK(APINAME, REGNAME, STRUCTNAME) \
void _bolt_plugin_handle_##APINAME(struct STRUCTNAME* e) { \
    size_t iter = 0; \
    void* item; \
    while (hashmap_iter(plugins, &iter, &item)) { \
        struct Plugin* plugin = *(struct Plugin* const*)item; \
        if (plugin->is_deleted) continue; \
        void* newud = lua_newuserdata(plugin->state, sizeof(struct STRUCTNAME)); /*stack: userdata*/ \
        memcpy(newud, e, sizeof(struct STRUCTNAME)); \
        lua_getfield(plugin->state, LUA_REGISTRYINDEX, REGNAME##_META_REGISTRYNAME); /*stack: userdata, metatable*/ \
        lua_setmetatable(plugin->state, -2); /*stack: userdata*/ \
        lua_pushliteral(plugin->state, REGNAME##_CB_REGISTRYNAME); /*stack: userdata, enumname*/ \
        lua_gettable(plugin->state, LUA_REGISTRYINDEX); /*stack: userdata, callback*/ \
        if (!lua_isfunction(plugin->state, -1)) { \
            lua_pop(plugin->state, 2); \
            continue; \
        } \
        lua_pushvalue(plugin->state, -2); /*stack: userdata, callback, userdata*/ \
        if (lua_pcall(plugin->state, 1, 0, 0)) { /*stack: userdata, ?error*/ \
            const char* e = lua_tolstring(plugin->state, -1, 0); \
            printf("plugin callback " #APINAME " error: %s\n", e); \
            lua_pop(plugin->state, 2); /*stack: (empty)*/ \
            _bolt_plugin_stop(plugin->id); \
            _bolt_plugin_notify_stopped(plugin->id); \
            break; \
        } else { \
            lua_pop(plugin->state, 1); /*stack: (empty)*/ \
        } \
    } \
} \
static int api_on##APINAME(lua_State* state) { \
    luaL_checkany(state, 1); \
    lua_pushliteral(state, REGNAME##_CB_REGISTRYNAME); \
    if (lua_isfunction(state, 1)) { \
        lua_pushvalue(state, 1); \
    } else { \
        lua_pushnil(state); \
    } \
    lua_settable(state, LUA_REGISTRYINDEX); \
    return 0; \
}

// same as DEFINE_CALLBACK except _bolt_plugin_handle_... will be defined as static
#define DEFINE_CALLBACK_STATIC(APINAME, REGNAME, STRUCTNAME) static DEFINE_CALLBACK(APINAME, REGNAME, STRUCTNAME)

// macro for defining function "api_window_on*" and "_bolt_plugin_window_on*"
// e.g. DEFINE_WINDOWEVENT(resize, RESIZE, ResizeEvent)
#define DEFINE_WINDOWEVENT(APINAME, REGNAME, EVNAME) \
static int api_window_on##APINAME(lua_State* state) { \
    const struct EmbeddedWindow* window = require_self_userdata(state, "on" #APINAME); \
    luaL_checkany(state, 2); \
    lua_getfield(state, LUA_REGISTRYINDEX, WINDOWS_REGISTRYNAME); /*stack: window table*/ \
    lua_pushinteger(state, window->id); /*stack: window table, window id*/ \
    lua_gettable(state, -2); /*stack: window table, event table*/ \
    lua_pushinteger(state, WINDOW_ON##REGNAME); /*stack: window table, event table, event id*/ \
    if (lua_isfunction(state, 2)) { \
        lua_pushvalue(state, 2); \
    } else { \
        lua_pushnil(state); \
    } /*stack: window table, event table, event id, value*/ \
    lua_settable(state, -3); /*stack: window table, event table*/ \
    lua_pop(state, 2); /*stack: (empty)*/ \
    return 0; \
} \
void _bolt_plugin_window_on##APINAME(struct EmbeddedWindow* window, struct EVNAME* event) { \
    if (window->is_deleted) return; \
    lua_State* state = window->plugin; \
    if (window->is_browser) { \
        const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_EV##REGNAME; \
        const struct BoltIPCEvHeader header = { .plugin_id = window->plugin_id, .window_id = window->id }; \
        _bolt_ipc_send(fd, &msg_type, sizeof(msg_type)); \
        _bolt_ipc_send(fd, &header, sizeof(header)); \
        _bolt_ipc_send(fd, event, sizeof(struct EVNAME)); \
        return; \
    } \
    lua_getfield(state, LUA_REGISTRYINDEX, WINDOWS_REGISTRYNAME); /*stack: window table*/ \
    lua_pushinteger(state, window->id); /*stack: window table, window id*/ \
    lua_gettable(state, -2); /*stack: window table, event table*/ \
    lua_pushinteger(state, WINDOW_ON##REGNAME); /*stack: window table, event table, event id*/ \
    lua_gettable(state, -2); /*stack: window table, event table, function or nil*/ \
    if (lua_isfunction(state, -1)) { \
        void* newud = lua_newuserdata(state, sizeof(struct EVNAME)); /*stack: window table, event table, function, event*/ \
        memcpy(newud, event, sizeof(struct EVNAME)); \
        lua_getfield(state, LUA_REGISTRYINDEX, REGNAME##_META_REGISTRYNAME); /*stack: window table, event table, function, event, event metatable*/ \
        lua_setmetatable(state, -2); /*stack: window table, event table, function, event*/ \
        if (lua_pcall(state, 1, 0, 0)) { /*stack: window table, event table, ?error*/ \
            const char* e = lua_tolstring(state, -1, 0); \
            printf("plugin window on" #APINAME " error: %s\n", e); \
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

static void* require_userdata(lua_State* state, int n, const char* apiname) {
    void* ret = lua_touserdata(state, n);
    if (!ret) {
        lua_pushfstring(state, "%s: incorrect argument at position %i, expected userdata", apiname, n);
        lua_error(state);
    }
    return ret;
}

static void* require_self_userdata(lua_State* state, const char* apiname) {
    void* ret = lua_touserdata(state, 1);
    if (!ret) {
        lua_pushfstring(state, "%s: incorrect first argument, expected userdata (did you mean to use ':' instead of '.'?)", apiname);
        lua_error(state);
    }
    return ret;
}

static int surface_gc(lua_State* state) {
    const struct SurfaceFunctions* functions = lua_touserdata(state, 1);
    managed_functions.surface_destroy(functions->userdata);
    return 0;
}

static int buffer_gc(lua_State* state) {
    const struct FixedBuffer* buffer = lua_touserdata(state, 1);
    free(buffer->data);
    return 0;
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

    managed_functions = *functions;
    _bolt_rwlock_lock_write(&windows.lock);
    next_window_id = 1;
    plugins = hashmap_new(sizeof(struct Plugin*), 8, 0, 0, _bolt_plugin_map_hash, _bolt_plugin_map_compare, NULL, NULL);
    inited = 1;
    _bolt_rwlock_unlock_write(&windows.lock);
}

static int _bolt_api_init(lua_State* state) {
    lua_createtable(state, 0, 22);
    API_ADD(apiversion)
    API_ADD(checkversion)
    API_ADD(close)
    API_ADD(time)
    API_ADD(datetime)
    API_ADD(weekday)
    API_ADD(loadfile)
    API_ADD(loadconfig)
    API_ADD(saveconfig)
    API_ADD(onrender2d)
    API_ADD(onrender3d)
    API_ADD(onminimap)
    API_ADD(onswapbuffers)
    API_ADD(onmousemotion)
    API_ADD(onmousebutton)
    API_ADD(onmousebuttonup)
    API_ADD(onscroll)
    API_ADD(createsurface)
    API_ADD(createsurfacefromrgba)
    API_ADD(createsurfacefrompng)
    API_ADD(createwindow)
    API_ADD(createbrowser)
    API_ADD(createembeddedbrowser)
    API_ADD(point)
    API_ADD(createbuffer)
    return 1;
}

uint8_t _bolt_plugin_is_inited() {
    return inited;
}

// true on success
static uint8_t monotonic_microseconds(uint64_t* microseconds) {
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
    _bolt_rwlock_lock_read(&windows.lock);
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
        if (window->metadata.width > window_width) {
            window->metadata.width = window_width;
            did_resize = true;
        }
        if (window->metadata.height > window_height) {
            window->metadata.height = window_height;
            did_resize = true;
        }
        if (window->metadata.x < 0) {
            window->metadata.x = 0;
            did_move = true;
        }
        if (window->metadata.y < 0) {
            window->metadata.y = 0;
            did_move = true;
        }
        if (window->metadata.x + window->metadata.width > window_width) {
            window->metadata.x = window_width - window->metadata.width;
            did_move = true;
        }
        if (window->metadata.y + window->metadata.height > window_height) {
            window->metadata.y = window_height - window->metadata.height;
            did_move = true;
        }
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
        }

        // in case it got deleted by one of the handler functions
        if (window->is_deleted) {
            any_deleted = true;
            continue;
        }

        window->surface_functions.draw_to_surface(window->surface_functions.userdata, overlay.userdata, 0, 0, metadata.width, metadata.height, metadata.x, metadata.y, metadata.width, metadata.height);
        if (window->popup_shown && window->popup_initialised) {
            window->popup_surface_functions.draw_to_surface(window->popup_surface_functions.userdata, overlay.userdata, 0, 0, window->popup_meta.width, window->popup_meta.height, metadata.x + window->popup_meta.x, metadata.y + window->popup_meta.y, window->popup_meta.width, window->popup_meta.height);
        }
        if (window->reposition_mode && window->reposition_threshold) {
            managed_functions.draw_region_outline(overlay.userdata, window->repos_target_x, window->repos_target_y, window->repos_target_w, window->repos_target_h);
        }
    }
    _bolt_rwlock_unlock_read(&windows.lock);

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
        _bolt_plugin_shm_open_outbound(&capture_shm, capture_bytes, "sc", next_window_id);
        capture_inited = true;
        capture_id = next_window_id;
        capture_width = window_width;
        capture_height = window_height;
        need_remap = true;
        next_window_id += 1;
    } else if (capture_width != window_width || capture_height != window_height) {
        _bolt_plugin_shm_resize(&capture_shm, capture_bytes, next_window_id);
#if defined(_WIN32)
        capture_id = next_window_id;
        next_window_id += 1;
#endif
        capture_width = window_width;
        capture_height = window_height;
        need_remap = true;
    }
    managed_functions.read_screen_pixels(window_width, window_height, capture_shm.file);
    
    _bolt_rwlock_lock_read(&windows.lock);
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
    _bolt_rwlock_unlock_read(&windows.lock);

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
    if (window_width != overlay_width || window_height != overlay_height) {
        if (overlay_inited) {
            managed_functions.surface_resize_and_clear(overlay.userdata, window_width, window_height);
        } else {
            managed_functions.surface_init(&overlay, window_width, window_height, NULL);
            overlay_inited = true;
        }
        overlay_width = window_width;
        overlay_height = window_height;
    }

    uint8_t need_capture = false;
    uint8_t capture_ready = true;
    _bolt_plugin_handle_messages();
    _bolt_process_embedded_windows(window_width, window_height, &need_capture, &capture_ready);
    _bolt_process_plugins(&need_capture, &capture_ready);

    uint64_t micros = 0;
    monotonic_microseconds(&micros);
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
    overlay.clear(overlay.userdata, 0.0, 0.0, 0.0, 0.0);
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

static void _bolt_plugin_notify_stopped(uint64_t id) {
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
    _bolt_rwlock_lock_read(&windows.lock);
    struct EmbeddedWindow** pwindow = (struct EmbeddedWindow**)hashmap_get(windows.map, &window_id);
    if (!pwindow) {
        _bolt_rwlock_unlock_read(&windows.lock);
        return NULL;
    }
    struct EmbeddedWindow* window = *pwindow;
    _bolt_rwlock_unlock_read(&windows.lock);
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
        void* data = malloc(header->message_size);
        if (!data) {
            lua_pushfstring(state, "onmessage: heap error, failed to allocate %i bytes", header->message_size);
            lua_error(state);
        }
        _bolt_ipc_receive(fd, data, header->message_size);
        struct FixedBuffer* buffer = lua_newuserdata(state, sizeof(struct FixedBuffer)); /*stack: window table, event table, function, message*/
        buffer->data = data;
        buffer->size = header->message_size;
        if (lua_pcall(state, 1, 0, 0)) { /*stack: window table, event table, ?error*/
            const char* e = lua_tolstring(state, -1, 0);
            printf("plugin browser onmessage error: %s\n", e);
            lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME); /*stack: window table, event table, error, plugin*/
            const struct Plugin* plugin = lua_touserdata(state, -1);
            lua_pop(state, 4); /*stack: (empty)*/
            _bolt_plugin_stop(plugin->id);
            _bolt_plugin_notify_stopped(plugin->id);
        } else {
            lua_pop(state, 2); /*stack: (empty)*/
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
    if (window) handle_ipc_##NAME(&header, window); \
    break; \
}

#define IPCCASEWINDOWTAIL(NAME, STRUCT) case IPC_MSG_##NAME: { \
    struct BoltIPC##STRUCT##Header header; \
    _bolt_ipc_receive(fd, &header, sizeof(header)); \
    struct EmbeddedWindow* window = get_embeddedwindow(&header.window_id); \
    if (window) handle_ipc_##NAME(&header, window); \
    else _bolt_receive_discard(get_tail_ipc_##STRUCT(&header)); \
    break; \
}

#define IPCCASEBROWSER(NAME, STRUCT) case IPC_MSG_##NAME: { \
    struct BoltIPC##STRUCT##Header header; \
    _bolt_ipc_receive(fd, &header, sizeof(header)); \
    struct ExternalBrowser* window = get_externalbrowser(header.plugin_id, &header.window_id); \
    if (window) handle_ipc_##NAME(&header, window); \
    break; \
}

#define IPCCASEBROWSERTAIL(NAME, STRUCT) case IPC_MSG_##NAME: { \
    struct BoltIPC##STRUCT##Header header; \
    _bolt_ipc_receive(fd, &header, sizeof(header)); \
    struct ExternalBrowser* window = get_externalbrowser(header.plugin_id, &header.window_id); \
    if (window) handle_ipc_##NAME(&header, window); \
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
    return (size_t)header->width * (size_t)header->height * 4;
}

static void handle_ipc_OSRUPDATE(struct BoltIPCOsrUpdateHeader* header, struct EmbeddedWindow* window) {
    struct BoltIPCOsrUpdateRect rect;
    if (header->needs_remap) {
        _bolt_plugin_shm_remap(&window->browser_shm, get_tail_ipc_OsrUpdate(header), header->needs_remap);
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
    if (mousein_real) *mousein_real = true;
    _bolt_rwlock_lock_write(&windows.input_lock);
    *(uint8_t*)(((uint8_t*)&windows.input) + bool_offset) = 1;
    *(struct MouseEvent*)(((uint8_t*)&windows.input) + event_offset) = *event;
    _bolt_rwlock_unlock_write(&windows.input_lock);
    return true;
}

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

    // create the metatable for all RenderBatch2D objects
    lua_pushliteral(plugin->state, BATCH2D_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 14);
    API_ADD_SUB(plugin->state, vertexcount, batch2d)
    API_ADD_SUB(plugin->state, verticesperimage, batch2d)
    API_ADD_SUB(plugin->state, isminimap, batch2d)
    API_ADD_SUB(plugin->state, targetsize, batch2d)
    API_ADD_SUB(plugin->state, vertexxy, batch2d)
    API_ADD_SUB(plugin->state, vertexatlasxy, batch2d)
    API_ADD_SUB(plugin->state, vertexatlaswh, batch2d)
    API_ADD_SUB(plugin->state, vertexuv, batch2d)
    API_ADD_SUB(plugin->state, vertexcolour, batch2d)
    API_ADD_SUB(plugin->state, textureid, batch2d)
    API_ADD_SUB(plugin->state, texturesize, batch2d)
    API_ADD_SUB(plugin->state, texturecompare, batch2d)
    API_ADD_SUB(plugin->state, texturedata, batch2d)
    API_ADD_SUB_ALIAS(plugin->state, vertexcolour, vertexcolor, batch2d)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all Render3D objects
    lua_pushliteral(plugin->state, RENDER3D_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 16);
    API_ADD_SUB(plugin->state, vertexcount, render3d)
    API_ADD_SUB(plugin->state, vertexxyz, render3d)
    API_ADD_SUB(plugin->state, modelmatrix, render3d)
    API_ADD_SUB(plugin->state, viewprojmatrix, render3d)
    API_ADD_SUB(plugin->state, vertexmeta, render3d)
    API_ADD_SUB(plugin->state, atlasxywh, render3d)
    API_ADD_SUB(plugin->state, vertexuv, render3d)
    API_ADD_SUB(plugin->state, vertexcolour, render3d)
    API_ADD_SUB(plugin->state, textureid, render3d)
    API_ADD_SUB(plugin->state, texturesize, render3d)
    API_ADD_SUB(plugin->state, texturecompare, render3d)
    API_ADD_SUB(plugin->state, texturedata, render3d)
    API_ADD_SUB(plugin->state, vertexbone, render3d)
    API_ADD_SUB(plugin->state, boneanimation, render3d)
    API_ADD_SUB(plugin->state, animated, render3d)
    API_ADD_SUB_ALIAS(plugin->state, vertexcolour, vertexcolor, render3d)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all RenderMinimap objects
    lua_pushliteral(plugin->state, MINIMAP_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 3);
    API_ADD_SUB(plugin->state, angle, minimap)
    API_ADD_SUB(plugin->state, scale, minimap)
    API_ADD_SUB(plugin->state, position, minimap)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all Point objects
    lua_pushliteral(plugin->state, POINT_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 3);
    API_ADD_SUB(plugin->state, transform, point)
    API_ADD_SUB(plugin->state, get, point)
    API_ADD_SUB(plugin->state, aspixels, point)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all Transform objects
    lua_pushliteral(plugin->state, TRANSFORM_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 2);
    API_ADD_SUB(plugin->state, decompose, transform)
    API_ADD_SUB(plugin->state, get, transform)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all Buffer objects
    lua_pushliteral(plugin->state, BUFFER_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 3);
    API_ADD_SUB(plugin->state, writeinteger, buffer)
    API_ADD_SUB(plugin->state, writenumber, buffer)
    API_ADD_SUB(plugin->state, writestring, buffer)
    lua_settable(plugin->state, -3);
    lua_pushliteral(plugin->state, "__gc");
    lua_pushcfunction(plugin->state, buffer_gc);
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all SwapBuffers objects
    lua_pushliteral(plugin->state, SWAPBUFFERS_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 0);
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all Surface objects
    lua_pushliteral(plugin->state, SURFACE_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 5);
    API_ADD_SUB(plugin->state, clear, surface)
    API_ADD_SUB(plugin->state, subimage, surface)
    API_ADD_SUB(plugin->state, drawtoscreen, surface)
    API_ADD_SUB(plugin->state, drawtosurface, surface)
    API_ADD_SUB(plugin->state, drawtowindow, surface)
    lua_settable(plugin->state, -3);
    lua_pushliteral(plugin->state, "__gc");
    lua_pushcfunction(plugin->state, surface_gc);
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all Window objects
    lua_pushliteral(plugin->state, WINDOW_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 12);
    API_ADD_SUB(plugin->state, close, window)
    API_ADD_SUB(plugin->state, id, window)
    API_ADD_SUB(plugin->state, size, window)
    API_ADD_SUB(plugin->state, clear, window)
    API_ADD_SUB(plugin->state, subimage, window)
    API_ADD_SUB(plugin->state, startreposition, window)
    API_ADD_SUB(plugin->state, cancelreposition, window)
    API_ADD_SUB(plugin->state, onreposition, window)
    API_ADD_SUB(plugin->state, onmousemotion, window)
    API_ADD_SUB(plugin->state, onmousebutton, window)
    API_ADD_SUB(plugin->state, onmousebuttonup, window)
    API_ADD_SUB(plugin->state, onscroll, window)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all Browser objects
    lua_pushliteral(plugin->state, BROWSER_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 8);
    API_ADD_SUB(plugin->state, close, browser)
    API_ADD_SUB(plugin->state, sendmessage, browser)
    API_ADD_SUB(plugin->state, enablecapture, browser)
    API_ADD_SUB(plugin->state, disablecapture, browser)
    API_ADD_SUB(plugin->state, startreposition, window)
    API_ADD_SUB(plugin->state, cancelreposition, window)
    API_ADD_SUB(plugin->state, oncloserequest, browser)
    API_ADD_SUB(plugin->state, onmessage, browser)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all EmbeddedBrowser objects
    lua_pushliteral(plugin->state, EMBEDDEDBROWSER_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 4);
    API_ADD_SUB(plugin->state, close, embeddedbrowser)
    API_ADD_SUB(plugin->state, sendmessage, embeddedbrowser)
    API_ADD_SUB(plugin->state, enablecapture, embeddedbrowser)
    API_ADD_SUB(plugin->state, disablecapture, embeddedbrowser)
    API_ADD_SUB(plugin->state, oncloserequest, browser)
    API_ADD_SUB(plugin->state, onmessage, browser)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all RepositionEvent objects
    lua_pushliteral(plugin->state, REPOSITION_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 2);
    API_ADD_SUB(plugin->state, xywh, repositionevent)
    API_ADD_SUB(plugin->state, didresize, repositionevent)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all MouseMotionEvent objects
    lua_pushliteral(plugin->state, MOUSEMOTION_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 8);
    API_ADD_SUB(plugin->state, xy, mouseevent)
    API_ADD_SUB(plugin->state, ctrl, mouseevent);
    API_ADD_SUB(plugin->state, shift, mouseevent);
    API_ADD_SUB(plugin->state, meta, mouseevent);
    API_ADD_SUB(plugin->state, alt, mouseevent);
    API_ADD_SUB(plugin->state, capslock, mouseevent);
    API_ADD_SUB(plugin->state, numlock, mouseevent);
    API_ADD_SUB(plugin->state, mousebuttons, mouseevent);
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all MouseButtonEvent objects
    lua_pushliteral(plugin->state, MOUSEBUTTON_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 9);
    API_ADD_SUB(plugin->state, xy, mouseevent)
    API_ADD_SUB(plugin->state, ctrl, mouseevent);
    API_ADD_SUB(plugin->state, shift, mouseevent);
    API_ADD_SUB(plugin->state, meta, mouseevent);
    API_ADD_SUB(plugin->state, alt, mouseevent);
    API_ADD_SUB(plugin->state, capslock, mouseevent);
    API_ADD_SUB(plugin->state, numlock, mouseevent);
    API_ADD_SUB(plugin->state, mousebuttons, mouseevent);
    API_ADD_SUB(plugin->state, button, mousebutton);
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all MouseScrollEvent objects
    lua_pushliteral(plugin->state, SCROLL_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 9);
    API_ADD_SUB(plugin->state, xy, mouseevent)
    API_ADD_SUB(plugin->state, ctrl, mouseevent);
    API_ADD_SUB(plugin->state, shift, mouseevent);
    API_ADD_SUB(plugin->state, meta, mouseevent);
    API_ADD_SUB(plugin->state, alt, mouseevent);
    API_ADD_SUB(plugin->state, capslock, mouseevent);
    API_ADD_SUB(plugin->state, numlock, mouseevent);
    API_ADD_SUB(plugin->state, mousebuttons, mouseevent);
    API_ADD_SUB(plugin->state, direction, scroll);
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all MouseLeaveEvent objects
    lua_pushliteral(plugin->state, MOUSEMOTION_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    lua_pushliteral(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 8);
    API_ADD_SUB(plugin->state, xy, mouseevent)
    API_ADD_SUB(plugin->state, ctrl, mouseevent);
    API_ADD_SUB(plugin->state, shift, mouseevent);
    API_ADD_SUB(plugin->state, meta, mouseevent);
    API_ADD_SUB(plugin->state, alt, mouseevent);
    API_ADD_SUB(plugin->state, capslock, mouseevent);
    API_ADD_SUB(plugin->state, numlock, mouseevent);
    API_ADD_SUB(plugin->state, mousebuttons, mouseevent);
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // attempt to run the function
    if (lua_pcall(plugin->state, 0, 0, 0)) {
        _bolt_rwlock_lock_read(&windows.lock);
        size_t iter = 0;
        void* item;
        while (hashmap_iter(windows.map, &iter, &item)) {
            struct EmbeddedWindow* window = *(struct EmbeddedWindow**)item;
            window->is_deleted = true;
        }
        _bolt_rwlock_unlock_read(&windows.lock);

        const char* e = lua_tolstring(plugin->state, -1, 0);
        printf("plugin startup error: %s\n", e);
        lua_pop(plugin->state, 1);
        plugin->is_deleted = true;
        return 0;
    } else {
        return 1;
    }
}

static void _bolt_plugin_stop(uint64_t uid) {
    size_t iter = 0;
    void* item;
    _bolt_rwlock_lock_read(&windows.lock);
    while (hashmap_iter(windows.map, &iter, &item)) {
        struct EmbeddedWindow* window = *(struct EmbeddedWindow**)item;
        if (window->plugin_id == uid) window->is_deleted = true;
    }
    _bolt_rwlock_unlock_read(&windows.lock);

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

void get_binary_data(lua_State* state, int n, const void** data, size_t* size) {
    if (lua_isuserdata(state, 2)) {
        const struct FixedBuffer* buffer = lua_touserdata(state, n);
        *data = buffer->data;
        *size = buffer->size;
    } else {
        *data = luaL_checklstring(state, n, size);
    }
}

DEFINE_CALLBACK_STATIC(swapbuffers, SWAPBUFFERS, SwapBuffersEvent)
DEFINE_CALLBACK(render2d, BATCH2D, RenderBatch2D)
DEFINE_CALLBACK(render3d, RENDER3D, Render3D)
DEFINE_CALLBACK(minimap, MINIMAP, RenderMinimapEvent)
DEFINE_CALLBACK(mousemotion, MOUSEMOTION, MouseMotionEvent)
DEFINE_CALLBACK(mousebutton, MOUSEBUTTON, MouseButtonEvent)
DEFINE_CALLBACK(mousebuttonup, MOUSEBUTTONUP, MouseButtonEvent)
DEFINE_CALLBACK(scroll, SCROLL, MouseScrollEvent)
DEFINE_WINDOWEVENT(reposition, REPOSITION, RepositionEvent)
DEFINE_WINDOWEVENT(mousemotion, MOUSEMOTION, MouseMotionEvent)
DEFINE_WINDOWEVENT(mousebutton, MOUSEBUTTON, MouseButtonEvent)
DEFINE_WINDOWEVENT(mousebuttonup, MOUSEBUTTONUP, MouseButtonEvent)
DEFINE_WINDOWEVENT(scroll, SCROLL, MouseScrollEvent)
DEFINE_WINDOWEVENT(mouseleave, MOUSELEAVE, MouseMotionEvent)

static int api_apiversion(lua_State* state) {
    lua_pushnumber(state, API_VERSION_MAJOR);
    lua_pushnumber(state, API_VERSION_MINOR);
    return 2;
}

static int api_checkversion(lua_State* state) {
    lua_Integer expected_major = luaL_checkinteger(state, 1);
    lua_Integer expected_minor = luaL_checkinteger(state, 2);
    if (expected_major != API_VERSION_MAJOR) {
        lua_pushfstring(state, "checkversion major version mismatch: major version is %u, plugin expects %u", API_VERSION_MAJOR, (unsigned int)expected_major);
        lua_error(state);
    }
    if (expected_minor > API_VERSION_MINOR) {
        lua_pushfstring(state, "checkversion minor version mismatch: minor version is %u, plugin expects at least %u", API_VERSION_MINOR, (unsigned int)expected_minor);
        lua_error(state);
    }
    return 2;
}

static int api_close(lua_State* state) {
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    const struct Plugin* plugin = lua_touserdata(state, -1);
    const uint64_t id = plugin->id;
    lua_pop(state, 1);
    _bolt_plugin_stop(id);
    _bolt_plugin_notify_stopped(id);
    return 0;
}

static int api_time(lua_State* state) {
    uint64_t micros;
    if (monotonic_microseconds(&micros)) {
        lua_pushinteger(state, micros);
    } else {
        lua_pushnil(state);
    }
    return 1;
}

static int api_datetime(lua_State* state) {
    const time_t t = time(NULL);
    const struct tm* time = gmtime(&t);
    lua_pushinteger(state, time->tm_year + 1900);
    lua_pushinteger(state, time->tm_mon + 1);
    lua_pushinteger(state, time->tm_mday);
    lua_pushinteger(state, time->tm_hour);
    lua_pushinteger(state, time->tm_min);
    lua_pushinteger(state, time->tm_sec);
    return 6;
}

static int api_weekday(lua_State* state) {
    const time_t t = time(NULL);
    const struct tm* time = gmtime(&t);
    lua_pushinteger(state, time->tm_wday + 1);
    return 1;
}

static int api_loadfile(lua_State* state) {
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    const struct Plugin* plugin = lua_touserdata(state, -1);
    size_t path_length;
    const char* path = luaL_checklstring(state, 1, &path_length);
    const size_t full_path_length = plugin->path_length + path_length + 1;
    char* full_path = lua_newuserdata(state, full_path_length);
    memcpy(full_path, plugin->path, plugin->path_length);
    memcpy(full_path + plugin->path_length, path, path_length + 1);
    for (char* c = full_path + plugin->path_length; *c; c += 1) {
        if (*c == '\\') *c = '/';
    }

    FILE* f = fopen(full_path, "rb");
    if (!f) {
        lua_pop(state, 2);
        lua_pushnil(state);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    const long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* content = lua_newuserdata(state, file_size);
    if (fread(content, 1, file_size, f) < file_size) {
        lua_pop(state, 3);
        lua_pushnil(state);
        fclose(f);
        return 1;
    }
    fclose(f);
    lua_pop(state, 3);
    lua_pushlstring(state, content, file_size);
    return 1;
}

static int api_loadconfig(lua_State* state) {
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    const struct Plugin* plugin = lua_touserdata(state, -1);
    size_t path_length;
    const char* path = luaL_checklstring(state, 1, &path_length);
    const size_t full_path_length = plugin->config_path_length + path_length + 1;
    char* full_path = lua_newuserdata(state, full_path_length);
    memcpy(full_path, plugin->config_path, plugin->config_path_length);
    memcpy(full_path + plugin->config_path_length, path, path_length + 1);
    for (char* c = full_path + plugin->config_path_length; *c; c += 1) {
        if (*c == '\\') *c = '/';
    }

    FILE* f = fopen(full_path, "rb");
    if (!f) {
        lua_pop(state, 2);
        lua_pushnil(state);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    const long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* content = lua_newuserdata(state, file_size);
    if (fread(content, 1, file_size, f) < file_size) {
        lua_pop(state, 3);
        lua_pushnil(state);
        fclose(f);
        return 1;
    }
    fclose(f);
    lua_pop(state, 3);
    lua_pushlstring(state, content, file_size);
    return 1;
}

static int api_saveconfig(lua_State* state) {
    size_t path_length, content_length;
    const void* content;
    const char* path = luaL_checklstring(state, 1, &path_length);
    get_binary_data(state, 2, &content, &content_length);
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    const struct Plugin* plugin = lua_touserdata(state, -1);
    const size_t full_path_length = plugin->config_path_length + path_length + 1;
    char* full_path = lua_newuserdata(state, full_path_length);
    memcpy(full_path, plugin->config_path, plugin->config_path_length);
    memcpy(full_path + plugin->config_path_length, path, path_length + 1);
    for (char* c = full_path + plugin->config_path_length; *c; c += 1) {
        if (*c == '\\') *c = '/';
    }

    FILE* f = fopen(full_path, "wb");
    if (!f) {
        lua_pop(state, 2);
        lua_pushboolean(state, false);
        return 1;
    }
    if (fwrite(content, 1, content_length, f) < content_length) {
        lua_pop(state, 2);
        lua_pushboolean(state, false);
        fclose(f);
        return 1;
    }
    fclose(f);
    lua_pop(state, 2);
    lua_pushboolean(state, true);
    return 1;
}

static int api_createsurface(lua_State* state) {
    const lua_Integer w = luaL_checkinteger(state, 1);
    const lua_Integer h = luaL_checkinteger(state, 2);
    struct SurfaceFunctions* functions = lua_newuserdata(state, sizeof(struct SurfaceFunctions));
    managed_functions.surface_init(functions, w, h, NULL);
    lua_getfield(state, LUA_REGISTRYINDEX, SURFACE_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_createsurfacefromrgba(lua_State* state) {
    const lua_Integer w = luaL_checkinteger(state, 1);
    const lua_Integer h = luaL_checkinteger(state, 2);
    const size_t req_length = w * h * 4;
    size_t length;
    const void* rgba;
    get_binary_data(state, 3, &rgba, &length);
    struct SurfaceFunctions* functions = lua_newuserdata(state, sizeof(struct SurfaceFunctions));
    if (length >= req_length) {
        managed_functions.surface_init(functions, w, h, rgba);
    } else {
        uint8_t* ud = lua_newuserdata(state, req_length);
        memcpy(ud, rgba, length);
        memset(ud + length, 0, req_length - length);
        managed_functions.surface_init(functions, w, h, ud);
        lua_pop(state, 1);
    }
    lua_getfield(state, LUA_REGISTRYINDEX, SURFACE_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_createsurfacefrompng(lua_State* state) {
    const char extension[] = ".png";
    size_t rgba_size, path_length;
    const char* path = luaL_checklstring(state, 1, &path_length);
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    const struct Plugin* plugin = lua_touserdata(state, -1);
    const size_t full_path_length = plugin->path_length + path_length + sizeof(extension);
    char* full_path = lua_newuserdata(state, full_path_length);
    memcpy(full_path, plugin->path, plugin->path_length);
    memcpy(full_path + plugin->path_length, path, path_length + 1);
    for (char* c = full_path + plugin->path_length; *c; c += 1) {
        if (*c == '.') *c = '/';
    }
    memcpy(full_path + plugin->path_length + path_length, extension, sizeof(extension));
    FILE* f = fopen(full_path, "rb");
    if (!f) {
        lua_pushfstring(state, "createsurfacefrompng: error opening file '%s'", (char*)full_path);
        lua_error(state);
    }
    fseek(f, 0, SEEK_END);
    const long png_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* png = lua_newuserdata(state, png_size);
    if (fread(png, 1, png_size, f) < png_size) {
        fclose(f);
        lua_pushfstring(state, "createsurfacefrompng: error reading file '%s'", (char*)full_path);
        lua_error(state);
    }
    fclose(f);

#define CALL_SPNG(FUNC, ...) err = FUNC(__VA_ARGS__); if(err){free(rgba);lua_pushfstring(state,"createsurfacefrompng: error decoding file '%s': " #FUNC " returned %i",(char*)full_path,err);lua_error(state);}
    void* rgba = NULL;
    int err;
    spng_ctx* spng = spng_ctx_new(0);
    CALL_SPNG(spng_set_png_buffer, spng, png, png_size);
    struct spng_ihdr ihdr;
    CALL_SPNG(spng_get_ihdr, spng, &ihdr)
    CALL_SPNG(spng_decoded_image_size, spng, SPNG_FMT_RGBA8, &rgba_size)
    rgba = malloc(rgba_size);
    CALL_SPNG(spng_decode_image, spng, rgba, rgba_size, SPNG_FMT_RGBA8, 0)
    spng_ctx_free(spng);
    lua_pop(state, 3);
#undef CALL_SPNG

    lua_pushinteger(state, ihdr.width);
    lua_pushinteger(state, ihdr.height);
    struct SurfaceFunctions* functions = lua_newuserdata(state, sizeof(struct SurfaceFunctions));
    managed_functions.surface_init(functions, ihdr.width, ihdr.height, rgba);
    lua_getfield(state, LUA_REGISTRYINDEX, SURFACE_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    free(rgba);
    return 3;
}

static int api_createwindow(lua_State* state) {
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    const struct Plugin* plugin = lua_touserdata(state, -1);
    lua_pop(state, 1);
    
    // push a window onto the stack as the return value, then initialise it
    struct EmbeddedWindow* window = lua_newuserdata(state, sizeof(struct EmbeddedWindow));
    window->is_deleted = false;
    window->is_browser = false;
    window->popup_shown = false;
    window->do_capture = false;
    window->reposition_mode = false;
    window->id = next_window_id;
    window->plugin_id = plugin->id;
    window->plugin = state;
    _bolt_rwlock_init(&window->lock);
    _bolt_rwlock_init(&window->input_lock);
    window->metadata.x = luaL_checkinteger(state, 1);
    window->metadata.y = luaL_checkinteger(state, 2);
    window->metadata.width = luaL_checkinteger(state, 3);
    window->metadata.height = luaL_checkinteger(state, 4);
    if (window->metadata.width < WINDOW_MIN_SIZE) window->metadata.width = WINDOW_MIN_SIZE;
    if (window->metadata.height < WINDOW_MIN_SIZE) window->metadata.height = WINDOW_MIN_SIZE;
    memset(&window->input, 0, sizeof(window->input));
    managed_functions.surface_init(&window->surface_functions, window->metadata.width, window->metadata.height, NULL);
    lua_getfield(state, LUA_REGISTRYINDEX, WINDOW_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    next_window_id += 1;

    // create an event table in the registry for this window
    lua_getfield(state, LUA_REGISTRYINDEX, WINDOWS_REGISTRYNAME);
    lua_pushinteger(state, window->id);
    lua_createtable(state, WINDOW_EVENT_ENUM_SIZE, 0);
    lua_pushinteger(state, WINDOW_USERDATA);
    lua_pushvalue(state, -5);
    lua_settable(state, -3);
    lua_settable(state, -3);
    lua_pop(state, 1);

    // set this window in the hashmap, which is accessible by backends
    _bolt_rwlock_lock_write(&windows.lock);
    hashmap_set(windows.map, &window);
    _bolt_rwlock_unlock_write(&windows.lock);
    return 1;
}

static int api_createbrowser(lua_State* state) {
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    struct Plugin* plugin = lua_touserdata(state, -1);
    lua_pop(state, 1);

    // push a window onto the stack as the return value, then initialise it
    struct ExternalBrowser* browser = lua_newuserdata(state, sizeof(struct ExternalBrowser));
    browser->id = next_window_id;
    browser->plugin_id = plugin->id;
    browser->plugin = plugin;
    browser->do_capture = false;
    browser->capture_id = 0;
    lua_getfield(state, LUA_REGISTRYINDEX, BROWSER_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    next_window_id += 1;

    // create an empty event table in the registry for this window
    lua_getfield(state, LUA_REGISTRYINDEX, BROWSERS_REGISTRYNAME);
    lua_pushinteger(state, browser->id);
    lua_createtable(state, BROWSER_EVENT_ENUM_SIZE, 0);
    lua_settable(state, -3);
    lua_pop(state, 1);

    // send an IPC message to the host to create a browser
    const int pid = getpid();
    size_t url_length;
    const char* url = luaL_checklstring(state, 3, &url_length);
    const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_CREATEBROWSER_EXTERNAL;
    const struct BoltIPCCreateBrowserHeader header = {
        .plugin_id = plugin->id,
        .window_id = browser->id,
        .url_length = url_length,
        .pid = pid,
        .w = luaL_checkinteger(state, 1),
        .h = luaL_checkinteger(state, 2),
    };
    const uint32_t url_length32 = (uint32_t)url_length;
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &header, sizeof(header));
    _bolt_ipc_send(fd, url, url_length32);

    // add to the hashmap
    hashmap_set(plugin->external_browsers, &browser);
    return 1;
}

static int api_createembeddedbrowser(lua_State* state) {
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    const struct Plugin* plugin = lua_touserdata(state, -1);
    lua_pop(state, 1);

    // push a window onto the stack as the return value, then initialise it
    struct EmbeddedWindow* window = lua_newuserdata(state, sizeof(struct EmbeddedWindow));
    if (!_bolt_plugin_shm_open_inbound(&window->browser_shm, "eb", next_window_id)) {
        lua_pushliteral(state, "createembeddedbrowser: failed to create shm object");
        lua_error(state);
    }
    window->is_deleted = false;
    window->is_browser = true;
    window->popup_shown = false;
    window->do_capture = false;
    window->capture_id = 0;
    window->popup_initialised = false;
    window->popup_meta.x = 0;
    window->popup_meta.y = 0;
    window->reposition_mode = false;
    window->id = next_window_id;
    window->plugin_id = plugin->id;
    window->plugin = state;
    _bolt_rwlock_init(&window->lock);
    _bolt_rwlock_init(&window->input_lock);
    window->metadata.x = luaL_checkinteger(state, 1);
    window->metadata.y = luaL_checkinteger(state, 2);
    window->metadata.width = luaL_checkinteger(state, 3);
    window->metadata.height = luaL_checkinteger(state, 4);
    if (window->metadata.width < WINDOW_MIN_SIZE) window->metadata.width = WINDOW_MIN_SIZE;
    if (window->metadata.height < WINDOW_MIN_SIZE) window->metadata.height = WINDOW_MIN_SIZE;
    memset(&window->input, 0, sizeof(window->input));
    managed_functions.surface_init(&window->surface_functions, window->metadata.width, window->metadata.height, NULL);
    lua_getfield(state, LUA_REGISTRYINDEX, EMBEDDEDBROWSER_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    next_window_id += 1;

    // create an event table in the registry for this window
    lua_getfield(state, LUA_REGISTRYINDEX, BROWSERS_REGISTRYNAME);
    lua_pushinteger(state, window->id);
    lua_createtable(state, BROWSER_EVENT_ENUM_SIZE, 0);
    lua_pushinteger(state, BROWSER_USERDATA);
    lua_pushvalue(state, -5);
    lua_settable(state, -3);
    lua_settable(state, -3);
    lua_pop(state, 1);

    // send an IPC message to the host to create an OSR browser
    const int pid = getpid();
    size_t url_length;
    const char* url = luaL_checklstring(state, 5, &url_length);
    const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_CREATEBROWSER_OSR;
    const struct BoltIPCCreateBrowserHeader header = {
        .plugin_id = plugin->id,
        .window_id = window->id,
        .url_length = url_length,
        .pid = pid,
        .w = window->metadata.width,
        .h = window->metadata.height,
    };
    const uint32_t url_length32 = (uint32_t)url_length;
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &header, sizeof(header));
    _bolt_ipc_send(fd, url, url_length32);

    // set this window in the hashmap, which is accessible by backends
    _bolt_rwlock_lock_write(&windows.lock);
    hashmap_set(windows.map, &window);
    _bolt_rwlock_unlock_write(&windows.lock);
    return 1;
}

static int api_point(lua_State* state) {
    struct Point3D* point = lua_newuserdata(state, sizeof(struct Point3D));
    point->xyzh.floats[0] = luaL_checknumber(state, 1);
    point->xyzh.floats[1] = luaL_checknumber(state, 2);
    point->xyzh.floats[2] = luaL_checknumber(state, 3);
    point->xyzh.floats[3] = 1.0;
    point->integer = false;
    point->homogenous = false;
    lua_getfield(state, LUA_REGISTRYINDEX, POINT_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_createbuffer(lua_State* state) {
    const long size = luaL_checklong(state, 1);
    struct FixedBuffer* buffer = lua_newuserdata(state, sizeof(struct FixedBuffer));
    buffer->data = malloc(size);
    if (!buffer->data) {
        lua_pushfstring(state, "createbuffer: heap error, failed to allocate %i bytes", size);
        lua_error(state);
    }
    buffer->size = size;
    lua_getfield(state, LUA_REGISTRYINDEX, BUFFER_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_batch2d_vertexcount(lua_State* state) {
    const struct RenderBatch2D* batch = require_self_userdata(state, "vertexcount");
    lua_pushinteger(state, batch->index_count);
    return 1;
}

static int api_batch2d_verticesperimage(lua_State* state) {
    const struct RenderBatch2D* batch = require_self_userdata(state, "verticesperimage");
    lua_pushinteger(state, batch->vertices_per_icon);
    return 1;
}

static int api_batch2d_isminimap(lua_State* state) {
    const struct RenderBatch2D* batch = require_self_userdata(state, "isminimap");
    lua_pushboolean(state, batch->is_minimap);
    return 1;
}

static int api_batch2d_targetsize(lua_State* state) {
    const struct RenderBatch2D* batch = require_self_userdata(state, "targetsize");
    lua_pushinteger(state, batch->screen_width);
    lua_pushinteger(state, batch->screen_height);
    return 2;
}

static int api_batch2d_vertexxy(lua_State* state) {
    const struct RenderBatch2D* batch = require_self_userdata(state, "vertexxy");
    const lua_Integer index = luaL_checkinteger(state, 2);
    int32_t xy[2];
    batch->vertex_functions.xy(index - 1, batch->vertex_functions.userdata, xy);
    lua_pushinteger(state, xy[0]);
    lua_pushinteger(state, xy[1]);
    return 2;
}

static int api_batch2d_vertexatlasxy(lua_State* state) {
    const struct RenderBatch2D* batch = require_self_userdata(state, "vertexatlasxy");
    const lua_Integer index = luaL_checkinteger(state, 2);
    int32_t xy[2];
    batch->vertex_functions.atlas_xy(index - 1, batch->vertex_functions.userdata, xy);
    lua_pushinteger(state, xy[0]);
    lua_pushinteger(state, xy[1]);
    return 2;
}

static int api_batch2d_vertexatlaswh(lua_State* state) {
    const struct RenderBatch2D* batch = require_self_userdata(state, "vertexatlaswh");
    const lua_Integer index = luaL_checkinteger(state, 2);
    int32_t wh[2];
    batch->vertex_functions.atlas_wh(index - 1, batch->vertex_functions.userdata, wh);
    lua_pushinteger(state, wh[0]);
    lua_pushinteger(state, wh[1]);
    return 2;
}

static int api_batch2d_vertexuv(lua_State* state) {
    const struct RenderBatch2D* batch = require_self_userdata(state, "vertexuv");
    const lua_Integer index = lua_tointeger(state, 2);
    double uv[2];
    batch->vertex_functions.uv(index - 1, batch->vertex_functions.userdata, uv);
    lua_pushnumber(state, uv[0]);
    lua_pushnumber(state, uv[1]);
    return 2;
}

static int api_batch2d_vertexcolour(lua_State* state) {
    const struct RenderBatch2D* batch = require_self_userdata(state, "vertexcolour");
    const lua_Integer index = luaL_checkinteger(state, 2);
    double colour[4];
    batch->vertex_functions.colour(index - 1, batch->vertex_functions.userdata, colour);
    lua_pushnumber(state, colour[0]);
    lua_pushnumber(state, colour[1]);
    lua_pushnumber(state, colour[2]);
    lua_pushnumber(state, colour[3]);
    return 4;
}

static int api_batch2d_textureid(lua_State* state) {
    const struct RenderBatch2D* render = require_self_userdata(state, "textureid");
    const size_t id = render->texture_functions.id(render->texture_functions.userdata);
    lua_pushinteger(state, id);
    return 1;
}

static int api_batch2d_texturesize(lua_State* state) {
    const struct RenderBatch2D* render = require_self_userdata(state, "texturesize");
    size_t size[2];
    render->texture_functions.size(render->texture_functions.userdata, size);
    lua_pushinteger(state, size[0]);
    lua_pushinteger(state, size[1]);
    return 2;
}

static int api_batch2d_texturecompare(lua_State* state) {
    const struct RenderBatch2D* render = require_self_userdata(state, "texturecompare");
    const size_t x = luaL_checkinteger(state, 2);
    const size_t y = luaL_checkinteger(state, 3);
    size_t data_len;
    const void* data;
    get_binary_data(state, 4, &data, &data_len);
    const uint8_t match = render->texture_functions.compare(render->texture_functions.userdata, x, y, data_len, (const unsigned char*)data);
    lua_pushboolean(state, match);
    return 1;
}

static int api_batch2d_texturedata(lua_State* state) {
    const struct RenderBatch2D* render = require_self_userdata(state, "texturedata");
    const size_t x = luaL_checkinteger(state, 2);
    const size_t y = luaL_checkinteger(state, 3);
    const size_t len = luaL_checkinteger(state, 4);
    const uint8_t* ret = render->texture_functions.data(render->texture_functions.userdata, x, y);
    lua_pushlstring(state, (const char*)ret, len);
    return 1;
}

static int api_minimap_angle(lua_State* state) {
    const struct RenderMinimapEvent* render = require_self_userdata(state, "angle");
    lua_pushnumber(state, render->angle);
    return 1;
}

static int api_minimap_scale(lua_State* state) {
    const struct RenderMinimapEvent* render = require_self_userdata(state, "scale");
    lua_pushnumber(state, render->scale);
    return 1;
}

static int api_minimap_position(lua_State* state) {
    const struct RenderMinimapEvent* render = require_self_userdata(state, "position");
    lua_pushnumber(state, render->x);
    lua_pushnumber(state, render->y);
    return 2;
}

static int api_point_transform(lua_State* state) {
    const struct Point3D* point = require_self_userdata(state, "transform");
    const struct Transform3D* transform = require_userdata(state, 2, "transform");
    double x, y, z, h;
    if (point->integer) {
        x = (double)point->xyzh.ints[0];
        y = (double)point->xyzh.ints[1];
        z = (double)point->xyzh.ints[2];
        h = 1.0;
    } else {
        x = point->xyzh.floats[0];
        y = point->xyzh.floats[1];
        z = point->xyzh.floats[2];
        h = point->xyzh.floats[3];
    }
    struct Point3D* out = lua_newuserdata(state, sizeof(struct Point3D));
    out->xyzh.floats[0] = (x * transform->matrix[0]) + (y * transform->matrix[4]) + (z * transform->matrix[8]) + (h * transform->matrix[12]);
    out->xyzh.floats[1] = (x * transform->matrix[1]) + (y * transform->matrix[5]) + (z * transform->matrix[9]) + (h * transform->matrix[13]);
    out->xyzh.floats[2] = (x * transform->matrix[2]) + (y * transform->matrix[6]) + (z * transform->matrix[10]) + (h * transform->matrix[14]);
    out->xyzh.floats[3] = (x * transform->matrix[3]) + (y * transform->matrix[7]) + (z * transform->matrix[11]) + (h * transform->matrix[15]);
    out->integer = false;
    out->homogenous = true;
    lua_getfield(state, LUA_REGISTRYINDEX, POINT_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_point_get(lua_State* state) {
    const struct Point3D* point = require_self_userdata(state, "get");
    if (point->integer) {
        lua_pushinteger(state, point->xyzh.ints[0]);
        lua_pushinteger(state, point->xyzh.ints[1]);
        lua_pushinteger(state, point->xyzh.ints[2]);
    } else if (!point->homogenous) {
        lua_pushnumber(state, point->xyzh.floats[0]);
        lua_pushnumber(state, point->xyzh.floats[1]);
        lua_pushnumber(state, point->xyzh.floats[2]);
    } else {
        lua_pushnumber(state, point->xyzh.floats[0] / point->xyzh.floats[3]);
        lua_pushnumber(state, point->xyzh.floats[1] / point->xyzh.floats[3]);
        lua_pushnumber(state, point->xyzh.floats[2] / point->xyzh.floats[3]);
    }
    return 3;
}

static int api_point_aspixels(lua_State* state) {
    const struct Point3D* point = require_self_userdata(state, "aspixels");
    double x, y;
    if (point->integer) {
        x = (double)point->xyzh.ints[0];
        y = (double)point->xyzh.ints[1];
    } else if (!point->homogenous) {
        x = point->xyzh.floats[0];
        y = point->xyzh.floats[1];
    } else {
        x = point->xyzh.floats[0] / point->xyzh.floats[3];
        y = point->xyzh.floats[1] / point->xyzh.floats[3];
    }
    int game_view_x, game_view_y, game_view_w, game_view_h;
    managed_functions.game_view_rect(&game_view_x, &game_view_y, &game_view_w, &game_view_h);
    lua_pushnumber(state, ((x  + 1.0) * game_view_w / 2.0) + (double)game_view_x);
    lua_pushnumber(state, ((-y + 1.0) * game_view_h / 2.0) + (double)game_view_y);
    return 2;
}

#define LENGTH(N1, N2, N3) sqrt((N1 * N1) + (N2 * N2) + (N3 * N3))
static int api_transform_decompose(lua_State* state) {
    const struct Transform3D* transform = require_self_userdata(state, "decompose");
    lua_pushnumber(state, transform->matrix[12]);
    lua_pushnumber(state, transform->matrix[13]);
    lua_pushnumber(state, transform->matrix[14]);
    const double scale_x = LENGTH(transform->matrix[0], transform->matrix[1], transform->matrix[2]);
    const double scale_y = LENGTH(transform->matrix[4], transform->matrix[5], transform->matrix[6]);
    const double scale_z = LENGTH(transform->matrix[8], transform->matrix[9], transform->matrix[10]);
    lua_pushnumber(state, scale_x);
    lua_pushnumber(state, scale_y);
    lua_pushnumber(state, scale_z);
    if (scale_x == 0.0 || scale_y == 0.0 || scale_z == 0.0) {
        // scale=0 means we can't calculate angles because there would be a division by zero,
        // and angle is meaningless for a zero-scale model anyway, so just put 0 for all the angles
        lua_pushnumber(state, 0.0);
        lua_pushnumber(state, 0.0);
        lua_pushnumber(state, 0.0);
    } else {
        // see https://stackoverflow.com/questions/39128589/decomposing-rotation-matrix-x-y-z-cartesian-angles
        const double yaw = atan2(-(transform->matrix[8] / scale_z), sqrt((transform->matrix[0] * transform->matrix[0] / (scale_x * scale_x)) + (transform->matrix[4] * transform->matrix[4] / (scale_y * scale_y))));
        const double pitch = atan2((transform->matrix[9] / scale_z) / cos(yaw), (transform->matrix[10] / scale_z) / cos(yaw));
        const double roll = atan2((transform->matrix[4] / scale_y) / cos(yaw), (transform->matrix[0] / scale_x) / cos(yaw));
        lua_pushnumber(state, yaw);
        lua_pushnumber(state, pitch);
        lua_pushnumber(state, roll);
    }
    return 9;
}
#undef LENGTH

static int api_transform_get(lua_State* state) {
    const struct Transform3D* transform = require_self_userdata(state, "get");
    for (size_t i = 0; i < 16; i += 1) {
        lua_pushnumber(state, transform->matrix[i]);
    }
    return 16;
}

static int api_surface_clear(lua_State* state) {
    const struct SurfaceFunctions* functions = require_self_userdata(state, "clear");
    const int argc = lua_gettop(state);
    const double r = (argc >= 2) ? luaL_checknumber(state, 2) : 0.0;
    const double g = (argc >= 3) ? luaL_checknumber(state, 3) : r;
    const double b = (argc >= 4) ? luaL_checknumber(state, 4) : r;
    const double a = (argc >= 5) ? luaL_checknumber(state, 5) : ((argc > 1) ? 1.0 : 0.0);
    functions->clear(functions->userdata, r, g, b, a);
    return 0;
}

static int api_surface_subimage(lua_State* state) {
    const struct SurfaceFunctions* functions = require_self_userdata(state, "subimage");
    const lua_Integer x = luaL_checkinteger(state, 2);
    const lua_Integer y = luaL_checkinteger(state, 3);
    const lua_Integer w = luaL_checkinteger(state, 4);
    const lua_Integer h = luaL_checkinteger(state, 5);
    const size_t req_length = w * h * 4;
    size_t length;
    const void* rgba;
    get_binary_data(state, 6, &rgba, &length);
    if (length >= req_length) {
        functions->subimage(functions->userdata, x, y, w, h, rgba, 0);
    } else {
        uint8_t* ud = lua_newuserdata(state, req_length);
        memcpy(ud, rgba, length);
        memset(ud + length, 0, req_length - length);
        functions->subimage(functions->userdata, x, y, w, h, (const void*)ud, 0);
        lua_pop(state, 1);
    }
    lua_getfield(state, LUA_REGISTRYINDEX, SURFACE_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_surface_drawtoscreen(lua_State* state) {
    if (!overlay_inited) return 0;
    const struct SurfaceFunctions* functions = require_self_userdata(state, "drawtoscreen");
    const int sx = luaL_checkinteger(state, 2);
    const int sy = luaL_checkinteger(state, 3);
    const int sw = luaL_checkinteger(state, 4);
    const int sh = luaL_checkinteger(state, 5);
    const int dx = luaL_checkinteger(state, 6);
    const int dy = luaL_checkinteger(state, 7);
    const int dw = luaL_checkinteger(state, 8);
    const int dh = luaL_checkinteger(state, 9);
    functions->draw_to_surface(functions->userdata, overlay.userdata, sx, sy, sw, sh, dx, dy, dw, dh);
    return 0;
}

static int api_surface_drawtosurface(lua_State* state) {
    const struct SurfaceFunctions* functions = require_self_userdata(state, "drawtosurface");
    const struct SurfaceFunctions* target = require_userdata(state, 2, "drawtosurface");
    const int sx = luaL_checkinteger(state, 3);
    const int sy = luaL_checkinteger(state, 4);
    const int sw = luaL_checkinteger(state, 5);
    const int sh = luaL_checkinteger(state, 6);
    const int dx = luaL_checkinteger(state, 7);
    const int dy = luaL_checkinteger(state, 8);
    const int dw = luaL_checkinteger(state, 9);
    const int dh = luaL_checkinteger(state, 10);
    functions->draw_to_surface(functions->userdata, target->userdata, sx, sy, sw, sh, dx, dy, dw, dh);
    return 0;
}

static int api_surface_drawtowindow(lua_State* state) {
    const struct SurfaceFunctions* functions = require_self_userdata(state, "drawtowindow");
    const struct EmbeddedWindow* target = require_userdata(state, 2, "drawtowindow");
    const int sx = luaL_checkinteger(state, 3);
    const int sy = luaL_checkinteger(state, 4);
    const int sw = luaL_checkinteger(state, 5);
    const int sh = luaL_checkinteger(state, 6);
    const int dx = luaL_checkinteger(state, 7);
    const int dy = luaL_checkinteger(state, 8);
    const int dw = luaL_checkinteger(state, 9);
    const int dh = luaL_checkinteger(state, 10);
    functions->draw_to_surface(functions->userdata, target->surface_functions.userdata, sx, sy, sw, sh, dx, dy, dw, dh);
    return 0;
}

static int api_window_close(lua_State* state) {
    struct EmbeddedWindow* window = require_self_userdata(state, "state");
    window->is_deleted = true;
    return 0;
}

static int api_window_clear(lua_State* state) {
    const struct EmbeddedWindow* window = require_self_userdata(state, "clear");
    const int argc = lua_gettop(state);
    const double r = (argc >= 2) ? luaL_checknumber(state, 2) : 0.0;
    const double g = (argc >= 3) ? luaL_checknumber(state, 3) : r;
    const double b = (argc >= 4) ? luaL_checknumber(state, 4) : r;
    const double a = (argc >= 5) ? luaL_checknumber(state, 5) : ((argc > 1) ? 1.0 : 0.0);
    window->surface_functions.clear(window->surface_functions.userdata, r, g, b, a);
    return 0;
}

static int api_window_id(lua_State* state) {
    const struct EmbeddedWindow* window = require_self_userdata(state, "id");
    lua_pushinteger(state, window->id);
    return 1;
}

static int api_window_size(lua_State* state) {
    struct EmbeddedWindow* window = require_self_userdata(state, "size");
    _bolt_rwlock_lock_read(&window->lock);
    lua_pushinteger(state, window->metadata.width);
    lua_pushinteger(state, window->metadata.height);
    _bolt_rwlock_unlock_read(&window->lock);
    return 2;
}

static int api_window_subimage(lua_State* state) {
    const struct EmbeddedWindow* window = require_self_userdata(state, "subimage");
    const lua_Integer x = luaL_checkinteger(state, 2);
    const lua_Integer y = luaL_checkinteger(state, 3);
    const lua_Integer w = luaL_checkinteger(state, 4);
    const lua_Integer h = luaL_checkinteger(state, 5);
    const size_t req_length = w * h * 4;
    size_t length;
    const void* rgba;
    get_binary_data(state, 6, &rgba, &length);
    if (length >= req_length) {
        window->surface_functions.subimage(window->surface_functions.userdata, x, y, w, h, rgba, 0);
    } else {
        uint8_t* ud = lua_newuserdata(state, req_length);
        memcpy(ud, rgba, length);
        memset(ud + length, 0, req_length - length);
        window->surface_functions.subimage(window->surface_functions.userdata, x, y, w, h, (const void*)ud, 0);
        lua_pop(state, 1);
    }
    lua_getfield(state, LUA_REGISTRYINDEX, SURFACE_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_window_startreposition(lua_State* state) {
    struct EmbeddedWindow* window = require_self_userdata(state, "startreposition");
    const lua_Integer xscale = luaL_checkinteger(state, 2);
    const lua_Integer yscale = luaL_checkinteger(state, 3);
    window->reposition_mode = true;
    window->reposition_threshold = false;
    window->reposition_w = (xscale == 0) ? 0 : ((xscale > 0) ? 1 : -1);
    window->reposition_h = (yscale == 0) ? 0 : ((yscale > 0) ? 1 : -1);
    return 0;
}

static int api_window_cancelreposition(lua_State* state) {
    struct EmbeddedWindow* window = require_self_userdata(state, "cancelreposition");
    window->reposition_mode = false;
    return 0;
}

static int api_render3d_vertexcount(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "vertexcount");
    lua_pushinteger(state, render->vertex_count);
    return 1;
}

static int api_render3d_vertexxyz(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "vertexxyz");
    const lua_Integer index = lua_tointeger(state, 2);
    struct Point3D* point = lua_newuserdata(state, sizeof(struct Point3D));
    render->vertex_functions.xyz(index - 1, render->vertex_functions.userdata, point);
    lua_getfield(state, LUA_REGISTRYINDEX, POINT_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_render3d_modelmatrix(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "modelmatrix");
    struct Transform3D* transform = lua_newuserdata(state, sizeof(struct Transform3D));
    render->matrix_functions.model_matrix(render->matrix_functions.userdata, transform);
    lua_getfield(state, LUA_REGISTRYINDEX, TRANSFORM_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_render3d_viewprojmatrix(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "viewprojmatrix");
    struct Transform3D* transform = lua_newuserdata(state, sizeof(struct Transform3D));
    render->matrix_functions.viewproj_matrix(render->matrix_functions.userdata, transform);
    lua_getfield(state, LUA_REGISTRYINDEX, TRANSFORM_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_render3d_vertexmeta(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "vertexmeta");
    const lua_Integer index = luaL_checkinteger(state, 2);
    size_t meta = render->vertex_functions.atlas_meta(index - 1, render->vertex_functions.userdata);
    lua_pushinteger(state, meta);
    return 1;
}

static int api_render3d_atlasxywh(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "atlasxywh");
    const lua_Integer meta = luaL_checkinteger(state, 2);
    int32_t xywh[4];
    render->vertex_functions.atlas_xywh(meta, render->vertex_functions.userdata, xywh);
    lua_pushinteger(state, xywh[0]);
    lua_pushinteger(state, xywh[1]);
    lua_pushinteger(state, xywh[2]);
    lua_pushinteger(state, xywh[3]);
    return 4;
}

static int api_render3d_vertexuv(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "vertexuv");
    const lua_Integer index = luaL_checkinteger(state, 2);
    double uv[4];
    render->vertex_functions.uv(index, render->vertex_functions.userdata, uv);
    lua_pushnumber(state, uv[0]);
    lua_pushnumber(state, uv[1]);
    return 2;
}

static int api_render3d_vertexcolour(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "vertexcolour");
    const lua_Integer index = luaL_checkinteger(state, 2);
    double col[4];
    render->vertex_functions.colour(index, render->vertex_functions.userdata, col);
    lua_pushnumber(state, col[0]);
    lua_pushnumber(state, col[1]);
    lua_pushnumber(state, col[2]);
    lua_pushnumber(state, col[3]);
    return 2;
}

static int api_render3d_textureid(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "textureid");
    const size_t id = render->texture_functions.id(render->texture_functions.userdata);
    lua_pushinteger(state, id);
    return 1;
}

static int api_render3d_texturesize(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "texturesize");
    size_t size[2];
    render->texture_functions.size(render->texture_functions.userdata, size);
    lua_pushinteger(state, size[0]);
    lua_pushinteger(state, size[1]);
    return 2;
}

static int api_render3d_texturecompare(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "texturecompare");
    const size_t x = luaL_checkinteger(state, 2);
    const size_t y = luaL_checkinteger(state, 3);
    size_t data_len;
    const void* data;
    get_binary_data(state, 4, &data, &data_len);
    const uint8_t match = render->texture_functions.compare(render->texture_functions.userdata, x, y, data_len, (const unsigned char*)data);
    lua_pushboolean(state, match);
    return 1;
}

static int api_render3d_texturedata(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "texturedata");
    const size_t x = luaL_checkinteger(state, 2);
    const size_t y = luaL_checkinteger(state, 3);
    const size_t len = luaL_checkinteger(state, 4);
    const uint8_t* ret = render->texture_functions.data(render->texture_functions.userdata, x, y);
    lua_pushlstring(state, (const char*)ret, len);
    return 1;
}

static int api_render3d_vertexbone(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "vertexbone");
    const int index = luaL_checkinteger(state, 2);
    uint8_t ret = render->vertex_functions.bone_id(index - 1, render->vertex_functions.userdata);
    lua_pushinteger(state, ret);
    return 1;
}

static int api_render3d_boneanimation(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "boneanimation");
    const lua_Integer bone_id = luaL_checkinteger(state, 2);

    if (!render->is_animated) {
        lua_pushliteral(state, "boneanimation: cannot get bone transforms for non-animated model");
        lua_error(state);
    }

    // not a mistake - game's shader supports values from 0 to 128, inclusive
    if (bone_id < 0 || bone_id > 128) {
        lua_pushliteral(state, "boneanimation: invalid bone ID");
        lua_error(state);
    }

    struct Transform3D* transform = lua_newuserdata(state, sizeof(struct Transform3D));
    render->vertex_functions.bone_transform(bone_id, render->vertex_functions.userdata, transform);
    lua_getfield(state, LUA_REGISTRYINDEX, TRANSFORM_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_render3d_animated(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "animated");
    lua_pushboolean(state, render->is_animated);
    return 1;
}

static int api_repositionevent_xywh(lua_State* state) {
    const struct RepositionEvent* event = require_self_userdata(state, "xywh");
    lua_pushinteger(state, event->x);
    lua_pushinteger(state, event->y);
    lua_pushinteger(state, event->width);
    lua_pushinteger(state, event->height);
    return 4;
}

static int api_repositionevent_didresize(lua_State* state) {
    const struct RepositionEvent* event = require_self_userdata(state, "didresize");
    lua_pushboolean(state, event->did_resize);
    return 1;
}

static int api_mouseevent_xy(lua_State* state) {
    const struct MouseEvent* event = require_self_userdata(state, "xy");
    lua_pushinteger(state, event->x);
    lua_pushinteger(state, event->y);
    return 2;
}

static int api_mouseevent_ctrl(lua_State* state) {
    const struct MouseEvent* event = require_self_userdata(state, "ctrl");
    lua_pushboolean(state, event->ctrl);
    return 1;
}

static int api_mouseevent_shift(lua_State* state) {
    const struct MouseEvent* event = require_self_userdata(state, "shift");
    lua_pushboolean(state, event->shift);
    return 1;
}

static int api_mouseevent_meta(lua_State* state) {
    const struct MouseEvent* event = require_self_userdata(state, "meta");
    lua_pushboolean(state, event->meta);
    return 1;
}

static int api_mouseevent_alt(lua_State* state) {
    const struct MouseEvent* event = require_self_userdata(state, "meta");
    lua_pushboolean(state, event->alt);
    return 1;
}

static int api_mouseevent_capslock(lua_State* state) {
    const struct MouseEvent* event = require_self_userdata(state, "capslock");
    lua_pushboolean(state, event->capslock);
    return 1;
}

static int api_mouseevent_numlock(lua_State* state) {
    const struct MouseEvent* event = require_self_userdata(state, "numlock");
    lua_pushboolean(state, event->numlock);
    return 1;
}

static int api_mouseevent_mousebuttons(lua_State* state) {
    const struct MouseEvent* event = require_self_userdata(state, "mousebuttons");
    lua_pushboolean(state, event->mb_left);
    lua_pushboolean(state, event->mb_right);
    lua_pushboolean(state, event->mb_middle);
    return 3;
}

static int api_mousebutton_button(lua_State* state) {
    const struct MouseButtonEvent* event = require_self_userdata(state, "button");
    lua_pushinteger(state, event->button);
    return 1;
}

static int api_scroll_direction(lua_State* state) {
    const struct MouseScrollEvent* event = require_self_userdata(state, "direction");
    lua_pushinteger(state, event->direction);
    return 1;
}

static int api_browser_close(lua_State* state) {
    const struct ExternalBrowser* browser = require_self_userdata(state, "close");
    if (browser->do_capture) browser->plugin->ext_browser_capture_count += 1;
    hashmap_delete(browser->plugin->external_browsers, &browser);

    const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_CLOSEBROWSER_EXTERNAL;
    const struct BoltIPCCloseBrowserHeader header = { .plugin_id = browser->plugin_id, .window_id = browser->id };
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &header, sizeof(header));

    return 0;
}

static int api_browser_sendmessage(lua_State* state) {
    const struct ExternalBrowser* window = require_self_userdata(state, "sendmessage");
    const void* str;
    size_t len;
    get_binary_data(state, 2, &str, &len);
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    const struct Plugin* plugin = lua_touserdata(state, -1);
    const uint64_t id = plugin->id;
    lua_pop(state, 1);

    const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_PLUGINMESSAGE;
    const struct BoltIPCPluginMessageHeader header = { .plugin_id = id, .window_id = window->id, .message_size = len };
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &header, sizeof(header));
    _bolt_ipc_send(fd, str, len);
    return 0;
}

static int api_browser_enablecapture(lua_State* state) {
    struct ExternalBrowser* window = require_self_userdata(state, "enablecapture");
    if (!window->do_capture) {
        window->plugin->ext_browser_capture_count += 1;
        window->do_capture = true;
        window->capture_ready = true;
    }
    return 0;
}

static int api_browser_disablecapture(lua_State* state) {
    struct ExternalBrowser* window = require_self_userdata(state, "disablecapture");
    if (window->do_capture) window->plugin->ext_browser_capture_count -= 1;
    window->do_capture = false;
    return 0;
}

static int api_browser_oncloserequest(lua_State* state) {
    const uint64_t* window_id = require_self_userdata(state, "oncloserequest");
    luaL_checkany(state, 2);
    lua_getfield(state, LUA_REGISTRYINDEX, BROWSERS_REGISTRYNAME); /*stack: window table*/
    lua_pushinteger(state, *window_id); /*stack: window table, window id*/
    lua_gettable(state, -2); /*stack: window table, event table*/
    lua_pushinteger(state, BROWSER_ONCLOSEREQUEST); /*stack: window table, event table, event id*/
    if (lua_isfunction(state, 2)) {
        lua_pushvalue(state, 2);
    } else {
        lua_pushnil(state);
    } /*stack: window table, event table, event id, value*/
    lua_settable(state, -3); /*stack: window table, event table*/
    lua_pop(state, 2); /*stack: (empty)*/
    return 0;
}

static int api_browser_onmessage(lua_State* state) {
    const uint64_t* window_id = require_self_userdata(state, "onmessage");
    luaL_checkany(state, 2);
    lua_getfield(state, LUA_REGISTRYINDEX, BROWSERS_REGISTRYNAME); /*stack: window table*/
    lua_pushinteger(state, *window_id); /*stack: window table, window id*/
    lua_gettable(state, -2); /*stack: window table, event table*/
    lua_pushinteger(state, BROWSER_ONMESSAGE); /*stack: window table, event table, event id*/
    if (lua_isfunction(state, 2)) {
        lua_pushvalue(state, 2);
    } else {
        lua_pushnil(state);
    } /*stack: window table, event table, event id, value*/
    lua_settable(state, -3); /*stack: window table, event table*/
    lua_pop(state, 2); /*stack: (empty)*/
    return 0;
}

static int api_embeddedbrowser_close(lua_State* state) {
    struct EmbeddedWindow* window = require_self_userdata(state, "close");
    window->is_deleted = true;

    const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_CLOSEBROWSER_OSR;
    const struct BoltIPCCloseBrowserHeader header = { .plugin_id = window->plugin_id, .window_id = window->id };
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &header, sizeof(header));

    return 0;
}

static int api_embeddedbrowser_sendmessage(lua_State* state) {
    const struct EmbeddedWindow* window = require_self_userdata(state, "sendmessage");
    const void* str;
    size_t len;
    get_binary_data(state, 2, &str, &len);
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    const struct Plugin* plugin = lua_touserdata(state, -1);
    const uint64_t id = plugin->id;
    lua_pop(state, 1);

    const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_OSRPLUGINMESSAGE;
    const struct BoltIPCPluginMessageHeader header = { .plugin_id = id, .window_id = window->id, .message_size = len };
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &header, sizeof(header));
    _bolt_ipc_send(fd, str, len);
    return 0;
}

static int api_embeddedbrowser_enablecapture(lua_State* state) {
    struct EmbeddedWindow* window = require_self_userdata(state, "enablecapture");
    if (!window->do_capture) {
        next_window_id += 1;
        window->do_capture = true;
        window->capture_ready = true;
    }
    return 0;
}

static int api_embeddedbrowser_disablecapture(lua_State* state) {
    struct EmbeddedWindow* window = require_self_userdata(state, "disablecapture");
    if (window->do_capture) {
        window->do_capture = false;
    }
    return 0;
}

static int api_buffer_writeinteger(lua_State* state) {
    const struct FixedBuffer* buffer = require_self_userdata(state, "writeinteger");
    const uint64_t value = (uint64_t)luaL_checklong(state, 2);
    const long offset = luaL_checklong(state, 3);
    const int width = luaL_checkint(state, 4);
    for (size_t i = 0; i < width; i += 1) {
        const uint8_t b = (uint8_t)((value >> (8 * i)) & 0xFF);
        *((uint8_t*)buffer->data + offset + i) = b;
    }
    return 0;
}

static int api_buffer_writenumber(lua_State* state) {
    const struct FixedBuffer* buffer = require_self_userdata(state, "writenumber");
    const double value = (double)luaL_checknumber(state, 2);
    const long offset = luaL_checklong(state, 3);
    memcpy((uint8_t*)buffer->data + offset, &value, sizeof(value));
    return 0;
}

static int api_buffer_writestring(lua_State* state) {
    const struct FixedBuffer* buffer = require_self_userdata(state, "writestring");
    size_t size;
    const void* data = luaL_checklstring(state, 2, &size);
    const long offset = luaL_checklong(state, 3);
    memcpy((uint8_t*)buffer->data + offset, data, size);
    return 0;
}
