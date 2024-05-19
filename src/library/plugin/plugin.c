#include "plugin.h"

#include "../ipc.h"
#include "plugin_api.h"
#include "../../hashmap/hashmap.h"
#include "../../../modules/spng/spng/spng.h"

#include <bits/time.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define API_VERSION_MAJOR 1
#define API_VERSION_MINOR 0

#define PUSHSTRING(STATE, STR) lua_pushlstring(STATE, STR, sizeof(STR) - sizeof(*(STR)))
#define SNPUSHSTRING(STATE, BUF, STR, ...) {int n = snprintf(BUF, sizeof(BUF), STR, __VA_ARGS__);lua_pushlstring(STATE, BUF, n <= 0 ? 0 : (n >= sizeof(BUF) ? sizeof(BUF) - 1 : n));}
#define API_ADD(FUNC) PUSHSTRING(state, #FUNC);lua_pushcfunction(state, api_##FUNC);lua_settable(state, -3);
#define API_ADD_SUB(STATE, FUNC, SUB) PUSHSTRING(STATE, #FUNC);lua_pushcfunction(STATE, api_##SUB##_##FUNC);lua_settable(STATE, -3);
#define API_ADD_SUB_ALIAS(STATE, FUNC, ALIAS, SUB) PUSHSTRING(STATE, #ALIAS);lua_pushcfunction(STATE, api_##SUB##_##FUNC);lua_settable(STATE, -3);

#define PLUGIN_REGISTRYNAME "plugin"
#define BATCH2D_META_REGISTRYNAME "batch2dmeta"
#define RENDER3D_META_REGISTRYNAME "render3dmeta"
#define MINIMAP_META_REGISTRYNAME "minimapmeta"
#define SWAPBUFFERS_META_REGISTRYNAME "swapbuffersmeta"
#define SURFACE_META_REGISTRYNAME "surfacemeta"
#define SWAPBUFFERS_CB_REGISTRYNAME "swapbufferscb"
#define BATCH2D_CB_REGISTRYNAME "batch2dcb"
#define RENDER3D_CB_REGISTRYNAME "render3dcb"
#define MINIMAP_CB_REGISTRYNAME "minimapcb"

static void (*surface_init)(struct SurfaceFunctions*, unsigned int, unsigned int, const void*);
static void (*surface_destroy)(void*);

static bool inited = false;
static int _bolt_api_init(lua_State* state);

static int fd = 0;

// a currently-running plugin.
// note strings are not null terminated, and "path" must always be converted to use '/' as path-separators
// and must always end with a trailing separator.
struct Plugin {
    lua_State* state;
    char* id;
    char* path;
    uint32_t id_length;
    uint32_t path_length;
};

void _bolt_plugin_free(struct Plugin* const* plugin) {
    lua_close((*plugin)->state);
    free((*plugin)->id);
    free(*plugin);
}

static int _bolt_plugin_map_compare(const void* a, const void* b, void* udata) {
    const struct Plugin* p1 = *(const struct Plugin* const*)a;
    const struct Plugin* p2 = *(const struct Plugin* const*)b;
    if (p1->id_length != p2->id_length) return p1->id_length;
    return strncmp(p1->id, p2->id, p1->id_length);
}

static uint64_t _bolt_plugin_map_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const struct Plugin* p = *(const struct Plugin* const*)item;
    return hashmap_sip(p->id, p->id_length, seed0, seed1);
}

static struct hashmap* plugins;

// macro for defining callback functions "_bolt_plugin_handle_*" and "api_setcallback*"
// e.g. DEFINE_CALLBACK(swapbuffers, SWAPBUFFERS, SwapBuffersEvent)
#define DEFINE_CALLBACK(APINAME, REGNAME, STRUCTNAME) \
void _bolt_plugin_handle_##APINAME(struct STRUCTNAME* e) { \
    size_t iter = 0; \
    void* item; \
    while (hashmap_iter(plugins, &iter, &item)) { \
        struct Plugin* plugin = *(struct Plugin* const*)item; \
        lua_pushlightuserdata(plugin->state, e); /*stack: userdata*/ \
        lua_getfield(plugin->state, LUA_REGISTRYINDEX, REGNAME##_META_REGISTRYNAME); /*stack: userdata, metatable*/ \
        lua_setmetatable(plugin->state, -2); /*stack: userdata*/ \
        PUSHSTRING(plugin->state, REGNAME##_CB_REGISTRYNAME); /*stack: userdata, enumname*/ \
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
            _bolt_plugin_stop(plugin->id, plugin->id_length); \
            break; \
        } else { \
            lua_pop(plugin->state, 1); /*stack: (empty)*/ \
        } \
    } \
} \
static int api_setcallback##APINAME(lua_State* state) { \
    _bolt_check_argc(state, 1, "setcallback" #APINAME); \
    PUSHSTRING(state, REGNAME##_CB_REGISTRYNAME); \
    if (lua_isfunction(state, 1)) { \
        lua_pushvalue(state, 1); \
    } else { \
        lua_pushnil(state); \
    } \
    lua_settable(state, LUA_REGISTRYINDEX); \
    return 0; \
}

static int surface_gc(lua_State* state) {
    const struct SurfaceFunctions* functions = lua_touserdata(state, 1);
    surface_destroy(functions->userdata);
    return 0;
}

void _bolt_plugin_init(void (*_surface_init)(struct SurfaceFunctions*, unsigned int, unsigned int, const void*), void (*_surface_destroy)(void*)) {
    _bolt_plugin_ipc_init(&fd);

    const char* display_name = getenv("JX_DISPLAY_NAME");
    if (display_name && *display_name) {
        size_t name_len = strlen(display_name);
        struct BoltIPCMessageToHost message = {.message_type = IPC_MSG_IDENTIFY, .items = name_len};
        _bolt_ipc_send(fd, &message, sizeof(message));
        _bolt_ipc_send(fd, display_name, name_len);
    }

    surface_init = _surface_init;
    surface_destroy = _surface_destroy;
    plugins = hashmap_new(sizeof(struct Plugin*), 8, 0, 0, _bolt_plugin_map_hash, _bolt_plugin_map_compare, NULL, NULL);
    inited = 1;
}

static int _bolt_api_init(lua_State* state) {
    lua_createtable(state, 0, 12);
    API_ADD(apiversion)
    API_ADD(checkversion)
    API_ADD(time)
    API_ADD(datetime)
    API_ADD(weekday)
    API_ADD(setcallback2d)
    API_ADD(setcallback3d)
    API_ADD(setcallbackminimap)
    API_ADD(setcallbackswapbuffers)
    API_ADD(createsurface)
    API_ADD(createsurfacefromrgba)
    API_ADD(createsurfacefrompng)
    return 1;
}

uint8_t _bolt_plugin_is_inited() {
    return inited;
}

void _bolt_plugin_close() {
    _bolt_plugin_ipc_close(fd);
    size_t iter = 0;
    void* item;
    while (hashmap_iter(plugins, &iter, &item)) {
        struct Plugin** plugin = item;
        _bolt_plugin_free(plugin);
    }
    hashmap_free(plugins);
    inited = 0;
}

void _bolt_plugin_handle_messages() {
    struct BoltIPCMessageToHost message;
    while (_bolt_ipc_poll(fd)) {
        if (_bolt_ipc_receive(fd, &message, sizeof(message)) != 0) break;
        switch (message.message_type) {
            case IPC_MSG_STARTPLUGINS: {
                // note: incoming messages are sanitised by the UI, by replacing `\` with `/` and
                // making sure to leave a trailing slash, when initiating this type of message
                // (see PluginMenu.svelte)
                for (size_t i = 0; i < message.items; i += 1) {
                    struct Plugin* plugin = malloc(sizeof(struct Plugin));
                    plugin->state = luaL_newstate();
                    uint32_t main_length;
                    _bolt_ipc_receive(fd, &plugin->id_length, sizeof(uint32_t));
                    _bolt_ipc_receive(fd, &plugin->path_length, sizeof(uint32_t));
                    _bolt_ipc_receive(fd, &main_length, sizeof(uint32_t));
                    plugin->id = malloc(plugin->id_length);
                    plugin->path = malloc(plugin->path_length);
                    _bolt_ipc_receive(fd, plugin->id, plugin->id_length);
                    _bolt_ipc_receive(fd, plugin->path, plugin->path_length);
                    char* full_path = lua_newuserdata(plugin->state, plugin->path_length + main_length + 1);
                    memcpy(full_path, plugin->path, plugin->path_length);
                    _bolt_ipc_receive(fd, full_path + plugin->path_length, main_length);
                    full_path[plugin->path_length + main_length] = '\0';
                    if (_bolt_plugin_add(full_path, plugin)) {
                        lua_pop(plugin->state, 1);
                    } else {
                        _bolt_plugin_free(&plugin);
                    }
                }
                break;
            }
            default:
                printf("unknown message type %u\n", message.message_type);
                break;
        }
    }
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
        _bolt_plugin_free(&plugin);
        return 0;
    }
    if (old_plugin) {
        // a plugin with this id was already running and we just overwrote it, so make sure not to leak the memory
        _bolt_plugin_free(old_plugin);
    }

    // add the struct pointer to the registry
    PUSHSTRING(plugin->state, PLUGIN_REGISTRYNAME);
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
    PUSHSTRING(plugin->state, "bolt");
    lua_pushcfunction(plugin->state, _bolt_api_init);
    lua_settable(plugin->state, -3);
    // now set package.path to the plugin's root path
    char* search_path = lua_newuserdata(plugin->state, plugin->path_length + 5);
    memcpy(search_path, plugin->path, plugin->path_length);
    memcpy(&search_path[plugin->path_length], "?.lua", 5);
    PUSHSTRING(plugin->state, "path");
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

    // create the metatable for all RenderBatch2D objects
    PUSHSTRING(plugin->state, BATCH2D_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    PUSHSTRING(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 12);
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
    PUSHSTRING(plugin->state, RENDER3D_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    PUSHSTRING(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 13);
    API_ADD_SUB(plugin->state, vertexcount, render3d)
    API_ADD_SUB(plugin->state, vertexxyz, render3d)
    API_ADD_SUB(plugin->state, vertexmeta, render3d)
    API_ADD_SUB(plugin->state, atlasxywh, render3d)
    API_ADD_SUB(plugin->state, vertexuv, render3d)
    API_ADD_SUB(plugin->state, vertexcolour, render3d)
    API_ADD_SUB(plugin->state, textureid, render3d)
    API_ADD_SUB(plugin->state, texturesize, render3d)
    API_ADD_SUB(plugin->state, texturecompare, render3d)
    API_ADD_SUB(plugin->state, texturedata, render3d)
    API_ADD_SUB(plugin->state, toworldspace, render3d)
    API_ADD_SUB(plugin->state, toscreenspace, render3d)
    API_ADD_SUB(plugin->state, worldposition, render3d)
    API_ADD_SUB_ALIAS(plugin->state, vertexcolour, vertexcolor, render3d)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all RenderMinimap objects
    PUSHSTRING(plugin->state, MINIMAP_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    PUSHSTRING(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 3);
    API_ADD_SUB(plugin->state, angle, minimap)
    API_ADD_SUB(plugin->state, scale, minimap)
    API_ADD_SUB(plugin->state, position, minimap)
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all SwapBuffers objects
    PUSHSTRING(plugin->state, SWAPBUFFERS_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    PUSHSTRING(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 0);
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // create the metatable for all Surface objects
    PUSHSTRING(plugin->state, SURFACE_META_REGISTRYNAME);
    lua_newtable(plugin->state);
    PUSHSTRING(plugin->state, "__index");
    lua_createtable(plugin->state, 0, 3);
    API_ADD_SUB(plugin->state, clear, surface)
    API_ADD_SUB(plugin->state, drawtoscreen, surface)
    API_ADD_SUB(plugin->state, drawtosurface, surface)
    lua_settable(plugin->state, -3);
    PUSHSTRING(plugin->state, "__gc");
    lua_pushcfunction(plugin->state, surface_gc);
    lua_settable(plugin->state, -3);
    lua_settable(plugin->state, LUA_REGISTRYINDEX);

    // attempt to run the function
    if (lua_pcall(plugin->state, 0, 0, 0)) {
        const char* e = lua_tolstring(plugin->state, -1, 0);
        printf("plugin startup error: %s\n", e);
        lua_pop(plugin->state, 1);
        hashmap_delete(plugins, &plugin);
        return 0;
    } else {
        return 1;
    }
}

void _bolt_plugin_stop(char* id, uint32_t id_length) {
    struct Plugin* const* plugin = hashmap_delete(plugins, &(struct Plugin){.id = id, .id_length = id_length});
    _bolt_plugin_free(plugin);
}

// Calls `error()` if arg count is incorrect
static void _bolt_check_argc(lua_State* state, int expected_argc, const char* function_name) {
    char error_buffer[256];
    const int argc = lua_gettop(state);
    if (argc != expected_argc) {
        SNPUSHSTRING(state, "incorrect argument count to '%s': expected %i, got %i", function_name, expected_argc, argc);
        lua_error(state);
    }
}

DEFINE_CALLBACK(swapbuffers, SWAPBUFFERS, SwapBuffersEvent)
DEFINE_CALLBACK(2d, BATCH2D, RenderBatch2D)
DEFINE_CALLBACK(3d, RENDER3D, Render3D)
DEFINE_CALLBACK(minimap, MINIMAP, RenderMinimapEvent)

static int api_apiversion(lua_State* state) {
    _bolt_check_argc(state, 0, "apiversion");
    lua_pushnumber(state, API_VERSION_MAJOR);
    lua_pushnumber(state, API_VERSION_MINOR);
    return 2;
}

static int api_checkversion(lua_State* state) {
    _bolt_check_argc(state, 2, "checkversion");
    char error_buffer[256];
    lua_Integer expected_major = lua_tonumber(state, 1);
    lua_Integer expected_minor = lua_tonumber(state, 2);
    if (expected_major != API_VERSION_MAJOR) {
        SNPUSHSTRING(state, error_buffer, "checkversion major version mismatch: major version is %u, plugin expects %u", API_VERSION_MAJOR, (unsigned int)expected_major);
        lua_error(state);
    }
    if (expected_minor > API_VERSION_MINOR) {
        SNPUSHSTRING(state, error_buffer, "checkversion minor version mismatch: minor version is %u, plugin expects at least %u", API_VERSION_MINOR, (unsigned int)expected_minor);
        lua_error(state);
    }
    return 2;
}

static int api_time(lua_State* state) {
    _bolt_check_argc(state, 0, "time");
    struct timespec s;
    clock_gettime(CLOCK_MONOTONIC, &s);
    const uint64_t microseconds = (s.tv_sec * 1000000) + (s.tv_nsec / 1000);
    lua_pushinteger(state, microseconds);
    return 1;
}

static int api_datetime(lua_State* state) {
    _bolt_check_argc(state, 0, "datetime");
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
    _bolt_check_argc(state, 0, "weekday");
    const time_t t = time(NULL);
    const struct tm* time = gmtime(&t);
    lua_pushinteger(state, time->tm_wday + 1);
    return 1;
}

static int api_createsurface(lua_State* state) {
    _bolt_check_argc(state, 2, "createsurface");
    const lua_Integer w = lua_tointeger(state, 1);
    const lua_Integer h = lua_tointeger(state, 2);
    struct SurfaceFunctions* functions = lua_newuserdata(state, sizeof(struct SurfaceFunctions));
    surface_init(functions, w, h, NULL);
    lua_getfield(state, LUA_REGISTRYINDEX, SURFACE_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_createsurfacefromrgba(lua_State* state) {
    _bolt_check_argc(state, 3, "createsurfacefromrgba");
    const lua_Integer w = lua_tointeger(state, 1);
    const lua_Integer h = lua_tointeger(state, 2);
    const size_t req_length = w * h * 4;
    size_t length;
    const void* rgba = lua_tolstring(state, 3, &length);
    struct SurfaceFunctions* functions = lua_newuserdata(state, sizeof(struct SurfaceFunctions));
    if (length >= req_length) {
        surface_init(functions, w, h, rgba);
    } else {
        void* ud = lua_newuserdata(state, req_length);
        memcpy(ud, rgba, length);
        memset(ud + length, 0, req_length - length);
        surface_init(functions, w, h, ud);
        lua_pop(state, 1);
    }
    lua_getfield(state, LUA_REGISTRYINDEX, SURFACE_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
}

static int api_createsurfacefrompng(lua_State* state) {
    _bolt_check_argc(state, 1, "createsurfacefrompng");
    const char extension[] = ".png";
    size_t rgba_size, path_length;
    const char* path = lua_tolstring(state, 1, &path_length);
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
        char error_buffer[65536];
        SNPUSHSTRING(state, error_buffer, "createsurfacefrompng: error opening file '%s'", (char*)full_path);
        lua_error(state);
    }
    fseek(f, 0, SEEK_END);
    const long png_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* png = lua_newuserdata(state, png_size);
    if (fread(png, 1, png_size, f) < png_size) {
        char error_buffer[65536];
        fclose(f);
        SNPUSHSTRING(state, error_buffer, "createsurfacefrompng: error reading file '%s'", (char*)full_path);
        lua_error(state);
    }
    fclose(f);

#define CALL_SPNG(FUNC, ...) err = FUNC(__VA_ARGS__); if(err){char b[65536];free(rgba);SNPUSHSTRING(state,b,"createsurfacefrompng: error decoding file '%s': " #FUNC " returned %i",(char*)full_path,err);lua_error(state);}
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
    surface_init(functions, ihdr.width, ihdr.height, rgba);
    lua_getfield(state, LUA_REGISTRYINDEX, SURFACE_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    free(rgba);
    return 3;
}

static int api_batch2d_vertexcount(lua_State* state) {
    _bolt_check_argc(state, 1, "batch2d_vertexcount");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    lua_pushinteger(state, batch->index_count);
    return 1;
}

static int api_batch2d_verticesperimage(lua_State* state) {
    _bolt_check_argc(state, 1, "batch2d_verticesperimage");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    lua_pushinteger(state, batch->vertices_per_icon);
    return 1;
}

static int api_batch2d_isminimap(lua_State* state) {
    _bolt_check_argc(state, 1, "batch2d_isminimap");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    lua_pushboolean(state, batch->is_minimap);
    return 1;
}

static int api_batch2d_targetsize(lua_State* state) {
    _bolt_check_argc(state, 1, "batch2d_targetsize");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    lua_pushinteger(state, batch->screen_width);
    lua_pushinteger(state, batch->screen_height);
    return 2;
}

static int api_batch2d_vertexxy(lua_State* state) {
    _bolt_check_argc(state, 2, "batch2d_vertexxy");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    int32_t xy[2];
    batch->vertex_functions.xy(index - 1, batch->vertex_functions.userdata, xy);
    lua_pushinteger(state, xy[0]);
    lua_pushinteger(state, xy[1]);
    return 2;
}

static int api_batch2d_vertexatlasxy(lua_State* state) {
    _bolt_check_argc(state, 2, "batch2d_vertexatlasxy");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    int32_t xy[2];
    batch->vertex_functions.atlas_xy(index - 1, batch->vertex_functions.userdata, xy);
    lua_pushinteger(state, xy[0]);
    lua_pushinteger(state, xy[1]);
    return 2;
}

static int api_batch2d_vertexatlaswh(lua_State* state) {
    _bolt_check_argc(state, 2, "batch2d_vertexatlaswh");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    int32_t wh[2];
    batch->vertex_functions.atlas_wh(index - 1, batch->vertex_functions.userdata, wh);
    lua_pushinteger(state, wh[0]);
    lua_pushinteger(state, wh[1]);
    return 2;
}

static int api_batch2d_vertexuv(lua_State* state) {
    _bolt_check_argc(state, 2, "batch2d_vertexuv");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    double uv[2];
    batch->vertex_functions.uv(index - 1, batch->vertex_functions.userdata, uv);
    lua_pushnumber(state, uv[0]);
    lua_pushnumber(state, uv[1]);
    return 2;
}

static int api_batch2d_vertexcolour(lua_State* state) {
    _bolt_check_argc(state, 4, "batch2d_vertexcolour");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    double colour[4];
    batch->vertex_functions.colour(index - 1, batch->vertex_functions.userdata, colour);
    lua_pushnumber(state, colour[0]);
    lua_pushnumber(state, colour[1]);
    lua_pushnumber(state, colour[2]);
    lua_pushnumber(state, colour[3]);
    return 4;
}

static int api_batch2d_textureid(lua_State* state) {
    _bolt_check_argc(state, 1, "batch2d_textureid");
    struct RenderBatch2D* render = lua_touserdata(state, 1);
    const size_t id = render->texture_functions.id(render->texture_functions.userdata);
    lua_pushinteger(state, id);
    return 1;
}

static int api_batch2d_texturesize(lua_State* state) {
    _bolt_check_argc(state, 1, "batch2d_texturesize");
    struct RenderBatch2D* render = lua_touserdata(state, 1);
    size_t size[2];
    render->texture_functions.size(render->texture_functions.userdata, size);
    lua_pushinteger(state, size[0]);
    lua_pushinteger(state, size[1]);
    return 2;
}

static int api_batch2d_texturecompare(lua_State* state) {
    _bolt_check_argc(state, 4, "batch2d_texturecompare");
    struct RenderBatch2D* render = lua_touserdata(state, 1);
    const size_t x = lua_tointeger(state, 2);
    const size_t y = lua_tointeger(state, 3);
    size_t data_len;
    const unsigned char* data = (const unsigned char*)lua_tolstring(state, 4, &data_len);
    const uint8_t match = render->texture_functions.compare(render->texture_functions.userdata, x, y, data_len, data);
    lua_pushboolean(state, match);
    return 1;
}

static int api_batch2d_texturedata(lua_State* state) {
    _bolt_check_argc(state, 4, "batch2d_texturedata");
    struct RenderBatch2D* render = lua_touserdata(state, 1);
    const size_t x = lua_tointeger(state, 2);
    const size_t y = lua_tointeger(state, 3);
    const size_t len = lua_tointeger(state, 4);
    const uint8_t* ret = render->texture_functions.data(render->texture_functions.userdata, x, y);
    lua_pushlstring(state, (const char*)ret, len);
    return 1;
}

static int api_minimap_angle(lua_State* state) {
    _bolt_check_argc(state, 1, "minimap_angle");
    struct RenderMinimapEvent* render = lua_touserdata(state, 1);
    lua_pushnumber(state, render->angle);
    return 1;
}

static int api_minimap_scale(lua_State* state) {
    _bolt_check_argc(state, 1, "minimap_scale");
    struct RenderMinimapEvent* render = lua_touserdata(state, 1);
    lua_pushnumber(state, render->scale);
    return 1;
}

static int api_minimap_position(lua_State* state) {
    _bolt_check_argc(state, 1, "minimap_position");
    struct RenderMinimapEvent* render = lua_touserdata(state, 1);
    lua_pushnumber(state, render->x);
    lua_pushnumber(state, render->y);
    return 2;
}

static int api_surface_clear(lua_State* state) {
    char error_buffer[256];
    const int argc = lua_gettop(state);
    switch (argc) {
        case 1: {
            const struct SurfaceFunctions* functions = lua_touserdata(state, 1);
            functions->clear(functions->userdata, 0.0, 0.0, 0.0, 0.0);
            break;
        }
        case 4: {
            const struct SurfaceFunctions* functions = lua_touserdata(state, 1);
            const double r = lua_tonumber(state, 2);
            const double g = lua_tonumber(state, 3);
            const double b = lua_tonumber(state, 4);
            functions->clear(functions->userdata, r, g, b, 1.0);
            break;
        }
        case 5: {
            const struct SurfaceFunctions* functions = lua_touserdata(state, 1);
            const double r = lua_tonumber(state, 2);
            const double g = lua_tonumber(state, 3);
            const double b = lua_tonumber(state, 4);
            const double a = lua_tonumber(state, 5);
            functions->clear(functions->userdata, r, g, b, a);
            break;
        }
        default: {
            SNPUSHSTRING(state, error_buffer, "incorrect argument count to 'surface_clear': expected 1, 4, or 5, but got %i", argc);
            lua_error(state);
        }
    }
    return 0;
}

static int api_surface_drawtoscreen(lua_State* state) {
    _bolt_check_argc(state, 9, "surface_drawtoscreen");
    const struct SurfaceFunctions* functions = lua_touserdata(state, 1);
    const int sx = lua_tointeger(state, 2);
    const int sy = lua_tointeger(state, 3);
    const int sw = lua_tointeger(state, 4);
    const int sh = lua_tointeger(state, 5);
    const int dx = lua_tointeger(state, 6);
    const int dy = lua_tointeger(state, 7);
    const int dw = lua_tointeger(state, 8);
    const int dh = lua_tointeger(state, 9);
    functions->draw_to_screen(functions->userdata, sx, sy, sw, sh, dx, dy, dw, dh);
    return 0;
}

static int api_surface_drawtosurface(lua_State* state) {
    _bolt_check_argc(state, 10, "surface_drawtosurface");
    const struct SurfaceFunctions* functions = lua_touserdata(state, 1);
    const struct SurfaceFunctions* target = lua_touserdata(state, 2);
    const int sx = lua_tointeger(state, 3);
    const int sy = lua_tointeger(state, 4);
    const int sw = lua_tointeger(state, 5);
    const int sh = lua_tointeger(state, 6);
    const int dx = lua_tointeger(state, 7);
    const int dy = lua_tointeger(state, 8);
    const int dw = lua_tointeger(state, 9);
    const int dh = lua_tointeger(state, 10);
    functions->draw_to_surface(functions->userdata, target->userdata, sx, sy, sw, sh, dx, dy, dw, dh);
    return 0;
}

static int api_render3d_vertexcount(lua_State* state) {
    _bolt_check_argc(state, 1, "render3d_vertexcount");
    const struct Render3D* render = lua_touserdata(state, 1);
    lua_pushinteger(state, render->vertex_count);
    return 1;
}

static int api_render3d_vertexxyz(lua_State* state) {
    _bolt_check_argc(state, 2, "render3d_vertexxyz");
    const struct Render3D* render = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    int32_t xyz[3];
    render->vertex_functions.xyz(index - 1, render->vertex_functions.userdata, xyz);
    lua_pushinteger(state, xyz[0]);
    lua_pushinteger(state, xyz[1]);
    lua_pushinteger(state, xyz[2]);
    return 3;
}

static int api_render3d_vertexmeta(lua_State* state) {
    _bolt_check_argc(state, 2, "render3d_vertexmeta");
    const struct Render3D* render = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    size_t meta = render->vertex_functions.atlas_meta(index - 1, render->vertex_functions.userdata);
    lua_pushinteger(state, meta);
    return 1;
}

static int api_render3d_atlasxywh(lua_State* state) {
    _bolt_check_argc(state, 2, "render3d_atlasxywh");
    const struct Render3D* render = lua_touserdata(state, 1);
    const lua_Integer meta = lua_tointeger(state, 2);
    int32_t xywh[4];
    render->vertex_functions.atlas_xywh(meta, render->vertex_functions.userdata, xywh);
    lua_pushinteger(state, xywh[0]);
    lua_pushinteger(state, xywh[1]);
    lua_pushinteger(state, xywh[2]);
    lua_pushinteger(state, xywh[3]);
    return 4;
}

static int api_render3d_vertexuv(lua_State* state) {
    _bolt_check_argc(state, 2, "render3d_vertexuv");
    const struct Render3D* render = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    double uv[4];
    render->vertex_functions.uv(index, render->vertex_functions.userdata, uv);
    lua_pushnumber(state, uv[0]);
    lua_pushnumber(state, uv[1]);
    return 2;
}

static int api_render3d_vertexcolour(lua_State* state) {
    _bolt_check_argc(state, 2, "render3d_vertexcolour");
    const struct Render3D* render = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    double col[4];
    render->vertex_functions.colour(index, render->vertex_functions.userdata, col);
    lua_pushnumber(state, col[0]);
    lua_pushnumber(state, col[1]);
    lua_pushnumber(state, col[2]);
    lua_pushnumber(state, col[3]);
    return 2;
}

static int api_render3d_textureid(lua_State* state) {
    _bolt_check_argc(state, 1, "render3d_textureid");
    struct Render3D* render = lua_touserdata(state, 1);
    const size_t id = render->texture_functions.id(render->texture_functions.userdata);
    lua_pushinteger(state, id);
    return 1;
}

static int api_render3d_texturesize(lua_State* state) {
    _bolt_check_argc(state, 1, "render3d_texturesize");
    struct Render3D* render = lua_touserdata(state, 1);
    size_t size[2];
    render->texture_functions.size(render->texture_functions.userdata, size);
    lua_pushinteger(state, size[0]);
    lua_pushinteger(state, size[1]);
    return 2;
}

static int api_render3d_texturecompare(lua_State* state) {
    _bolt_check_argc(state, 4, "render3d_texturecompare");
    struct Render3D* render = lua_touserdata(state, 1);
    const size_t x = lua_tointeger(state, 2);
    const size_t y = lua_tointeger(state, 3);
    size_t data_len;
    const unsigned char* data = (const unsigned char*)lua_tolstring(state, 4, &data_len);
    const uint8_t match = render->texture_functions.compare(render->texture_functions.userdata, x, y, data_len, data);
    lua_pushboolean(state, match);
    return 1;
}

static int api_render3d_texturedata(lua_State* state) {
    _bolt_check_argc(state, 4, "render3d_texturedata");
    struct Render3D* render = lua_touserdata(state, 1);
    const size_t x = lua_tointeger(state, 2);
    const size_t y = lua_tointeger(state, 3);
    const size_t len = lua_tointeger(state, 4);
    const uint8_t* ret = render->texture_functions.data(render->texture_functions.userdata, x, y);
    lua_pushlstring(state, (const char*)ret, len);
    return 1;
}

static int api_render3d_toworldspace(lua_State* state) {
    _bolt_check_argc(state, 4, "render3d_toworldspace");
    struct Render3D* render = lua_touserdata(state, 1);
    const int x = lua_tointeger(state, 2);
    const int y = lua_tointeger(state, 3);
    const int z = lua_tointeger(state, 4);
    double out[3];
    render->matrix_functions.to_world_space(x, y, z, render->matrix_functions.userdata, out);
    lua_pushnumber(state, out[0]);
    lua_pushnumber(state, out[1]);
    lua_pushnumber(state, out[2]);
    return 3;
}

static int api_render3d_toscreenspace(lua_State* state) {
    _bolt_check_argc(state, 4, "render3d_toscreenspace");
    struct Render3D* render = lua_touserdata(state, 1);
    const int x = lua_tointeger(state, 2);
    const int y = lua_tointeger(state, 3);
    const int z = lua_tointeger(state, 4);
    double out[2];
    render->matrix_functions.to_screen_space(x, y, z, render->matrix_functions.userdata, out);
    lua_pushnumber(state, out[0]);
    lua_pushnumber(state, out[1]);
    return 2;
}

static int api_render3d_worldposition(lua_State* state) {
    _bolt_check_argc(state, 1, "render3d_worldposition");
    struct Render3D* render = lua_touserdata(state, 1);
    double out[3];
    render->matrix_functions.world_pos(render->matrix_functions.userdata, out);
    lua_pushnumber(state, out[0]);
    lua_pushnumber(state, out[1]);
    lua_pushnumber(state, out[2]);
    return 3;
}
