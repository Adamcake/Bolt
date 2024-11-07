#include "../ipc.h"
#include "plugin_api.h"
#include "plugin.h"

#include "../../../modules/hashmap/hashmap.h"
#include "../../../modules/spng/spng/spng.h"

#include <lauxlib.h>
#include <math.h>
#include <string.h>

#if defined(_WIN32)
#define getpid _getpid
#else
#include <unistd.h>
#endif

#define API_VERSION_MAJOR 1
#define API_VERSION_MINOR 0

static void set_handler(lua_State* state, const char* regname, uint64_t window_id, int ev) {
    lua_getfield(state, LUA_REGISTRYINDEX, regname); /*stack: window table*/ \
    lua_pushinteger(state, window_id); /*stack: window table, window id*/ \
    lua_gettable(state, -2); /*stack: window table, event table*/ \
    lua_pushinteger(state, ev); /*stack: window table, event table, event id*/ \
    if (lua_isfunction(state, 2)) { \
        lua_pushvalue(state, 2); \
    } else { \
        lua_pushnil(state); \
    } /*stack: window table, event table, event id, value*/ \
    lua_settable(state, -3); /*stack: window table, event table*/ \
    lua_pop(state, 2); /*stack: (empty)*/ \
}

#define DEFINE_CALLBACK(APINAME) \
static int api_on##APINAME(lua_State* state) { \
    lua_pushliteral(state, #APINAME "cb"); \
    if (lua_isfunction(state, 1)) { \
        lua_pushvalue(state, 1); \
    } else { \
        lua_pushnil(state); \
    } \
    lua_settable(state, LUA_REGISTRYINDEX); \
    return 0; \
}

#define DEFINE_WINDOWEVENT(APINAME, REGNAME) \
static int api_window_on##APINAME(lua_State* state) { \
    const struct EmbeddedWindow* window = require_self_userdata(state, "on" #APINAME); \
    set_handler(state, WINDOWS_REGISTRYNAME, window->id, WINDOW_ON##REGNAME); \
    return 0; \
}

#define DEFINE_BROWSEREVENT(APINAME, REGNAME) \
static int api_browser_on##APINAME(lua_State* state) { \
    const uint64_t* window_id = require_self_userdata(state, "on" #APINAME); \
    set_handler(state, BROWSERS_REGISTRYNAME, *window_id, BROWSER_ON##REGNAME); \
    return 0; \
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

static void get_binary_data(lua_State* state, int n, const void** data, size_t* size) {
    if (lua_isuserdata(state, 2)) {
        const struct FixedBuffer* buffer = lua_touserdata(state, n);
        *data = buffer->data;
        *size = buffer->size;
    } else {
        *data = luaL_checklstring(state, n, size);
    }
}

static int surface_gc(lua_State* state) {
    const struct PluginManagedFunctions* managed_functions = _bolt_plugin_managed_functions();
    const struct SurfaceFunctions* functions = lua_touserdata(state, 1);
    managed_functions->surface_destroy(functions->userdata);
    return 0;
}

static int buffer_gc(lua_State* state) {
    const struct FixedBuffer* buffer = lua_touserdata(state, 1);
    free(buffer->data);
    return 0;
}

/* API definitions start here */

DEFINE_CALLBACK(swapbuffers)
DEFINE_CALLBACK(render2d)
DEFINE_CALLBACK(render3d)
DEFINE_CALLBACK(minimap)
DEFINE_CALLBACK(mousemotion)
DEFINE_CALLBACK(mousebutton)
DEFINE_CALLBACK(mousebuttonup)
DEFINE_CALLBACK(scroll)
DEFINE_WINDOWEVENT(reposition, REPOSITION)
DEFINE_WINDOWEVENT(mousemotion, MOUSEMOTION)
DEFINE_WINDOWEVENT(mousebutton, MOUSEBUTTON)
DEFINE_WINDOWEVENT(mousebuttonup, MOUSEBUTTONUP)
DEFINE_WINDOWEVENT(scroll, SCROLL)
DEFINE_WINDOWEVENT(mouseleave, MOUSELEAVE)
DEFINE_BROWSEREVENT(closerequest, CLOSEREQUEST)
DEFINE_BROWSEREVENT(message, MESSAGE)

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
    if (_bolt_monotonic_microseconds(&micros)) {
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
    const struct PluginManagedFunctions* managed_functions = _bolt_plugin_managed_functions();
    const lua_Integer w = luaL_checkinteger(state, 1);
    const lua_Integer h = luaL_checkinteger(state, 2);
    struct SurfaceFunctions* functions = lua_newuserdata(state, sizeof(struct SurfaceFunctions));
    managed_functions->surface_init(functions, w, h, NULL);
    lua_getfield(state, LUA_REGISTRYINDEX, "surfacemeta");
    lua_setmetatable(state, -2);
    return 1;
}

static int api_createsurfacefromrgba(lua_State* state) {
    const struct PluginManagedFunctions* managed_functions = _bolt_plugin_managed_functions();
    const lua_Integer w = luaL_checkinteger(state, 1);
    const lua_Integer h = luaL_checkinteger(state, 2);
    const size_t req_length = w * h * 4;
    size_t length;
    const void* rgba;
    get_binary_data(state, 3, &rgba, &length);
    struct SurfaceFunctions* functions = lua_newuserdata(state, sizeof(struct SurfaceFunctions));
    if (length >= req_length) {
        managed_functions->surface_init(functions, w, h, rgba);
    } else {
        uint8_t* ud = lua_newuserdata(state, req_length);
        memcpy(ud, rgba, length);
        memset(ud + length, 0, req_length - length);
        managed_functions->surface_init(functions, w, h, ud);
        lua_pop(state, 1);
    }
    lua_getfield(state, LUA_REGISTRYINDEX, "surfacemeta");
    lua_setmetatable(state, -2);
    return 1;
}

static int api_createsurfacefrompng(lua_State* state) {
    const struct PluginManagedFunctions* managed_functions = _bolt_plugin_managed_functions();
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
    managed_functions->surface_init(functions, ihdr.width, ihdr.height, rgba);
    lua_getfield(state, LUA_REGISTRYINDEX, "surfacemeta");
    lua_setmetatable(state, -2);
    free(rgba);
    return 3;
}

static int api_createwindow(lua_State* state) {
    const struct PluginManagedFunctions* managed_functions = _bolt_plugin_managed_functions();
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
    window->id = _bolt_plugin_new_windowid();
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
    managed_functions->surface_init(&window->surface_functions, window->metadata.width, window->metadata.height, NULL);
    lua_getfield(state, LUA_REGISTRYINDEX, "windowmeta");
    lua_setmetatable(state, -2);

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
    struct WindowInfo* windows = _bolt_plugin_windowinfo();
    _bolt_rwlock_lock_write(&windows->lock);
    hashmap_set(windows->map, &window);
    _bolt_rwlock_unlock_write(&windows->lock);
    return 1;
}

static int api_createbrowser(lua_State* state) {
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    struct Plugin* plugin = lua_touserdata(state, -1);
    lua_pop(state, 1);

    // push a window onto the stack as the return value, then initialise it
    struct ExternalBrowser* browser = lua_newuserdata(state, sizeof(struct ExternalBrowser));
    browser->id = _bolt_plugin_new_windowid();
    browser->plugin_id = plugin->id;
    browser->plugin = plugin;
    browser->do_capture = false;
    browser->capture_id = 0;
    lua_getfield(state, LUA_REGISTRYINDEX, "browsermeta");
    lua_setmetatable(state, -2);

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
    const BoltSocketType fd = _bolt_plugin_fd();
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &header, sizeof(header));
    _bolt_ipc_send(fd, url, url_length32);

    // add to the hashmap
    hashmap_set(plugin->external_browsers, &browser);
    return 1;
}

static int api_createembeddedbrowser(lua_State* state) {
    const struct PluginManagedFunctions* managed_functions = _bolt_plugin_managed_functions();
    lua_getfield(state, LUA_REGISTRYINDEX, PLUGIN_REGISTRYNAME);
    const struct Plugin* plugin = lua_touserdata(state, -1);
    lua_pop(state, 1);

    // push a window onto the stack as the return value, then initialise it
    struct EmbeddedWindow* window = lua_newuserdata(state, sizeof(struct EmbeddedWindow));
    const uint64_t id = _bolt_plugin_new_windowid();
    if (!_bolt_plugin_shm_open_inbound(&window->browser_shm, "eb", id)) {
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
    window->id = id;
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
    managed_functions->surface_init(&window->surface_functions, window->metadata.width, window->metadata.height, NULL);
    lua_getfield(state, LUA_REGISTRYINDEX, "embeddedbrowsermeta");
    lua_setmetatable(state, -2);

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
    const BoltSocketType fd = _bolt_plugin_fd();
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &header, sizeof(header));
    _bolt_ipc_send(fd, url, url_length32);

    // set this window in the hashmap, which is accessible by backends
    struct WindowInfo* windows = _bolt_plugin_windowinfo();
    _bolt_rwlock_lock_write(&windows->lock);
    hashmap_set(windows->map, &window);
    _bolt_rwlock_unlock_write(&windows->lock);
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
    lua_getfield(state, LUA_REGISTRYINDEX, "pointmeta");
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
    lua_getfield(state, LUA_REGISTRYINDEX, "buffermeta");
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
    lua_getfield(state, LUA_REGISTRYINDEX, "pointmeta");
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
    const struct PluginManagedFunctions* managed_functions = _bolt_plugin_managed_functions();
    managed_functions->game_view_rect(&game_view_x, &game_view_y, &game_view_w, &game_view_h);
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
    lua_getfield(state, LUA_REGISTRYINDEX, "surfacemeta");
    lua_setmetatable(state, -2);
    return 1;
}

static int api_surface_drawtoscreen(lua_State* state) {
    const struct SurfaceFunctions* functions = require_self_userdata(state, "drawtoscreen");
    const int sx = luaL_checkinteger(state, 2);
    const int sy = luaL_checkinteger(state, 3);
    const int sw = luaL_checkinteger(state, 4);
    const int sh = luaL_checkinteger(state, 5);
    const int dx = luaL_checkinteger(state, 6);
    const int dy = luaL_checkinteger(state, 7);
    const int dw = luaL_checkinteger(state, 8);
    const int dh = luaL_checkinteger(state, 9);
    _bolt_plugin_draw_to_overlay(functions, sx, sy, sw, sh, dx, dy, dw, dh);
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
    lua_getfield(state, LUA_REGISTRYINDEX, "surfacemeta");
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
    lua_getfield(state, LUA_REGISTRYINDEX, "pointmeta");
    lua_setmetatable(state, -2);
    return 1;
}

static int api_render3d_modelmatrix(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "modelmatrix");
    struct Transform3D* transform = lua_newuserdata(state, sizeof(struct Transform3D));
    render->matrix_functions.model_matrix(render->matrix_functions.userdata, transform);
    lua_getfield(state, LUA_REGISTRYINDEX, "transformmeta");
    lua_setmetatable(state, -2);
    return 1;
}

static int api_render3d_viewprojmatrix(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "viewprojmatrix");
    struct Transform3D* transform = lua_newuserdata(state, sizeof(struct Transform3D));
    render->matrix_functions.viewproj_matrix(render->matrix_functions.userdata, transform);
    lua_getfield(state, LUA_REGISTRYINDEX, "transformmeta");
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
    render->vertex_functions.uv(index - 1, render->vertex_functions.userdata, uv);
    lua_pushnumber(state, uv[0]);
    lua_pushnumber(state, uv[1]);
    return 2;
}

static int api_render3d_vertexcolour(lua_State* state) {
    const struct Render3D* render = require_self_userdata(state, "vertexcolour");
    const lua_Integer index = luaL_checkinteger(state, 2);
    double col[4];
    render->vertex_functions.colour(index - 1, render->vertex_functions.userdata, col);
    lua_pushnumber(state, col[0]);
    lua_pushnumber(state, col[1]);
    lua_pushnumber(state, col[2]);
    lua_pushnumber(state, col[3]);
    return 4;
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
    lua_getfield(state, LUA_REGISTRYINDEX, "transformmeta");
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
    const BoltSocketType fd = _bolt_plugin_fd();
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
    const BoltSocketType fd = _bolt_plugin_fd();
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

static int api_embeddedbrowser_close(lua_State* state) {
    struct EmbeddedWindow* window = require_self_userdata(state, "close");
    window->is_deleted = true;

    const enum BoltIPCMessageTypeToHost msg_type = IPC_MSG_CLOSEBROWSER_OSR;
    const struct BoltIPCCloseBrowserHeader header = { .plugin_id = window->plugin_id, .window_id = window->id };
    const BoltSocketType fd = _bolt_plugin_fd();
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
    const BoltSocketType fd = _bolt_plugin_fd();
    _bolt_ipc_send(fd, &msg_type, sizeof(msg_type));
    _bolt_ipc_send(fd, &header, sizeof(header));
    _bolt_ipc_send(fd, str, len);
    return 0;
}

static int api_embeddedbrowser_enablecapture(lua_State* state) {
    struct EmbeddedWindow* window = require_self_userdata(state, "enablecapture");
    if (!window->do_capture) {
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

static int api_buffer_writebuffer(lua_State* state) {
    const struct FixedBuffer* buffer = require_self_userdata(state, "writebuffer");
    const struct FixedBuffer* source = require_userdata(state, 2, "writebuffer");
    const long offset = luaL_checklong(state, 3);
    memcpy((uint8_t*)buffer->data + offset, source->data, source->size);
    return 0;
}

/* below here is API function-list builders, declared in plugin_api.h */

struct ApiFuncTemplate {
    int (*func)(lua_State*);
    const char* name;
    size_t size;
};

static void populate_table(lua_State* state, const struct ApiFuncTemplate* functions, size_t count) {
    for (size_t i = 0; i < count; i += 1) {
        const struct ApiFuncTemplate* function = &functions[i];
        lua_pushlstring(state, function->name, function->size);
        lua_pushcfunction(state, function->func);
        lua_settable(state, -3);
    }
}

static void push_metatable(lua_State* state, const struct ApiFuncTemplate* functions, size_t count) {
    lua_newtable(state);
    lua_pushliteral(state, "__index");
    lua_createtable(state, 0, count);
    populate_table(state, functions, count);
    lua_settable(state, -3);
}

static void push_metatable_with_gc(lua_State* state, const struct ApiFuncTemplate* functions, size_t count, int(*gc)(lua_State*)) {
    push_metatable(state, functions, count);
    lua_pushliteral(state, "__gc");
    lua_pushcfunction(state, gc);
    lua_settable(state, -3);
}

#define BOLTFUNC(NAME) {.func=api_##NAME, .name=#NAME, .size=sizeof #NAME - sizeof *#NAME}
static struct ApiFuncTemplate bolt_functions[] = {
    BOLTFUNC(apiversion),
    BOLTFUNC(checkversion),
    BOLTFUNC(close),
    BOLTFUNC(time),
    BOLTFUNC(datetime),
    BOLTFUNC(weekday),
    BOLTFUNC(loadfile),
    BOLTFUNC(loadconfig),
    BOLTFUNC(saveconfig),
    BOLTFUNC(onrender2d),
    BOLTFUNC(onrender3d),
    BOLTFUNC(onminimap),
    BOLTFUNC(onswapbuffers),
    BOLTFUNC(onmousemotion),
    BOLTFUNC(onmousebutton),
    BOLTFUNC(onmousebuttonup),
    BOLTFUNC(onscroll),
    BOLTFUNC(createsurface),
    BOLTFUNC(createsurfacefromrgba),
    BOLTFUNC(createsurfacefrompng),
    BOLTFUNC(createwindow),
    BOLTFUNC(createbrowser),
    BOLTFUNC(createembeddedbrowser),
    BOLTFUNC(point),
    BOLTFUNC(createbuffer),
};
#undef BOLTFUNC

#define BOLTFUNC(NAME, OBJECT) {.func=api_##OBJECT##_##NAME, .name=#NAME, .size=sizeof(#NAME)-sizeof(*#NAME)}
#define BOLTALIAS(REALNAME, ALIAS, OBJECT) {.func=api_##OBJECT##_##REALNAME, .name=#ALIAS, .size=sizeof(#ALIAS)-sizeof(*#ALIAS)}

static struct ApiFuncTemplate batch2d_functions[] = {
    BOLTFUNC(vertexcount, batch2d),
    BOLTFUNC(verticesperimage, batch2d),
    BOLTFUNC(isminimap, batch2d),
    BOLTFUNC(targetsize, batch2d),
    BOLTFUNC(vertexxy, batch2d),
    BOLTFUNC(vertexatlasxy, batch2d),
    BOLTFUNC(vertexatlaswh, batch2d),
    BOLTFUNC(vertexuv, batch2d),
    BOLTFUNC(vertexcolour, batch2d),
    BOLTFUNC(textureid, batch2d),
    BOLTFUNC(texturesize, batch2d),
    BOLTFUNC(texturecompare, batch2d),
    BOLTFUNC(texturedata, batch2d),
    BOLTALIAS(vertexcolour, vertexcolor, batch2d),
};

static struct ApiFuncTemplate render3d_functions[] = {
    BOLTFUNC(vertexcount, render3d),
    BOLTFUNC(vertexxyz, render3d),
    BOLTFUNC(modelmatrix, render3d),
    BOLTFUNC(viewprojmatrix, render3d),
    BOLTFUNC(vertexmeta, render3d),
    BOLTFUNC(atlasxywh, render3d),
    BOLTFUNC(vertexuv, render3d),
    BOLTFUNC(vertexcolour, render3d),
    BOLTFUNC(textureid, render3d),
    BOLTFUNC(texturesize, render3d),
    BOLTFUNC(texturecompare, render3d),
    BOLTFUNC(texturedata, render3d),
    BOLTFUNC(vertexbone, render3d),
    BOLTFUNC(boneanimation, render3d),
    BOLTFUNC(animated, render3d),
    BOLTALIAS(vertexcolour, vertexcolor, render3d),
};

static struct ApiFuncTemplate minimap_functions[] = {
    BOLTFUNC(angle, minimap),
    BOLTFUNC(scale, minimap),
    BOLTFUNC(position, minimap),
};

static struct ApiFuncTemplate point_functions[] = {
    BOLTFUNC(transform, point),
    BOLTFUNC(get, point),
    BOLTFUNC(aspixels, point),
};

static struct ApiFuncTemplate transform_functions[] = {
    BOLTFUNC(decompose, transform),
    BOLTFUNC(get, transform),
};

static struct ApiFuncTemplate buffer_functions[] = {
    BOLTFUNC(writeinteger, buffer),
    BOLTFUNC(writenumber, buffer),
    BOLTFUNC(writestring, buffer),
    BOLTFUNC(writebuffer, buffer),
};

static struct ApiFuncTemplate swapbuffers_functions[] = {

};

static struct ApiFuncTemplate surface_functions[] = {
    BOLTFUNC(clear, surface),
    BOLTFUNC(subimage, surface),
    BOLTFUNC(drawtoscreen, surface),
    BOLTFUNC(drawtosurface, surface),
    BOLTFUNC(drawtowindow, surface),
};

static struct ApiFuncTemplate window_functions[] = {
    BOLTFUNC(close, window),
    BOLTFUNC(id, window),
    BOLTFUNC(size, window),
    BOLTFUNC(clear, window),
    BOLTFUNC(subimage, window),
    BOLTFUNC(startreposition, window),
    BOLTFUNC(cancelreposition, window),
    BOLTFUNC(onreposition, window),
    BOLTFUNC(onmousemotion, window),
    BOLTFUNC(onmousebutton, window),
    BOLTFUNC(onmousebuttonup, window),
    BOLTFUNC(onscroll, window),
};

static struct ApiFuncTemplate browser_functions[] = {
    BOLTFUNC(close, browser),
    BOLTFUNC(sendmessage, browser),
    BOLTFUNC(enablecapture, browser),
    BOLTFUNC(disablecapture, browser),
    BOLTFUNC(startreposition, window),
    BOLTFUNC(cancelreposition, window),
    BOLTFUNC(oncloserequest, browser),
    BOLTFUNC(onmessage, browser),
};

static struct ApiFuncTemplate embeddedbrowser_functions[] = {
    BOLTFUNC(close, embeddedbrowser),
    BOLTFUNC(sendmessage, embeddedbrowser),
    BOLTFUNC(enablecapture, embeddedbrowser),
    BOLTFUNC(disablecapture, embeddedbrowser),
    BOLTFUNC(oncloserequest, browser),
    BOLTFUNC(onmessage, browser),
};

static struct ApiFuncTemplate reposition_functions[] = {
    BOLTFUNC(xywh, repositionevent),
    BOLTFUNC(didresize, repositionevent)
};

static struct ApiFuncTemplate mousemotion_functions[] = {
    BOLTFUNC(xy, mouseevent),
    BOLTFUNC(ctrl, mouseevent),
    BOLTFUNC(shift, mouseevent),
    BOLTFUNC(meta, mouseevent),
    BOLTFUNC(alt, mouseevent),
    BOLTFUNC(capslock, mouseevent),
    BOLTFUNC(numlock, mouseevent),
    BOLTFUNC(mousebuttons, mouseevent),
};

static struct ApiFuncTemplate mousebutton_functions[] = {
    BOLTFUNC(xy, mouseevent),
    BOLTFUNC(ctrl, mouseevent),
    BOLTFUNC(shift, mouseevent),
    BOLTFUNC(meta, mouseevent),
    BOLTFUNC(alt, mouseevent),
    BOLTFUNC(capslock, mouseevent),
    BOLTFUNC(numlock, mouseevent),
    BOLTFUNC(mousebuttons, mouseevent),
    BOLTFUNC(button, mousebutton),
};

static struct ApiFuncTemplate scroll_functions[] = {
    BOLTFUNC(xy, mouseevent),
    BOLTFUNC(ctrl, mouseevent),
    BOLTFUNC(shift, mouseevent),
    BOLTFUNC(meta, mouseevent),
    BOLTFUNC(alt, mouseevent),
    BOLTFUNC(capslock, mouseevent),
    BOLTFUNC(numlock, mouseevent),
    BOLTFUNC(mousebuttons, mouseevent),
    BOLTFUNC(direction, scroll),
};

static struct ApiFuncTemplate mouseleave_functions[] = {
    BOLTFUNC(xy, mouseevent),
    BOLTFUNC(ctrl, mouseevent),
    BOLTFUNC(shift, mouseevent),
    BOLTFUNC(meta, mouseevent),
    BOLTFUNC(alt, mouseevent),
    BOLTFUNC(capslock, mouseevent),
    BOLTFUNC(numlock, mouseevent),
    BOLTFUNC(mousebuttons, mouseevent),
};

#undef BOLTFUNC
#undef BOLTALIAS

void _bolt_api_push_bolt_table(lua_State* state) {
    const size_t count = sizeof bolt_functions / sizeof *bolt_functions;
    lua_createtable(state, 0, count);
    populate_table(state, bolt_functions, count);
}

#define DEFPUSHMETA(NAME) \
void _bolt_api_push_metatable_##NAME(lua_State* state) { \
    push_metatable(state, NAME##_functions, sizeof NAME##_functions / sizeof *NAME##_functions); \
}
#define DEFPUSHMETAGC(NAME) \
void _bolt_api_push_metatable_##NAME(lua_State* state) { \
    push_metatable_with_gc(state, NAME##_functions, sizeof NAME##_functions / sizeof *NAME##_functions, NAME##_gc); \
}

DEFPUSHMETA(batch2d)
DEFPUSHMETA(render3d)
DEFPUSHMETA(minimap)
DEFPUSHMETA(point)
DEFPUSHMETA(transform)
DEFPUSHMETAGC(buffer)
DEFPUSHMETA(swapbuffers)
DEFPUSHMETAGC(surface)
DEFPUSHMETA(window)
DEFPUSHMETA(browser)
DEFPUSHMETA(embeddedbrowser)
DEFPUSHMETA(reposition)
DEFPUSHMETA(mousemotion)
DEFPUSHMETA(mousebutton)
DEFPUSHMETA(scroll)
DEFPUSHMETA(mouseleave)
