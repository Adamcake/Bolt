#include "plugin.h"
#include "plugin_api.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <time.h>

#define API_VERSION_MAJOR 1
#define API_VERSION_MINOR 0

#define API_ADD(FUNC) lua_pushstring(state, #FUNC);lua_pushcfunction(state, api_##FUNC);lua_settable(state, -3);
#define API_ADD_SUB(FUNC, SUB) lua_pushstring(state, #FUNC);lua_pushcfunction(state, api_##SUB##_##FUNC);lua_settable(state, -3);

const char* BOLT_REGISTRYNAME = "bolt";
const char* BATCH2D_META_REGISTRYNAME = "batch2dindex";
uint64_t next_plugin_id = 1;

enum {
    ENV_CALLBACK_SWAPBUFFERS,
    ENV_CALLBACK_2D,
    ENV_CALLBACK_3D,
};

static int _bolt_api_init(lua_State* state);

lua_State* state = NULL;

void _bolt_plugin_init() {
    state = luaL_newstate();

    // Open just the specific libraries plugins are allowed to have
    lua_pushcfunction(state, luaopen_base);
    lua_call(state, 0, 0);
    lua_pushcfunction(state, luaopen_package);
    lua_call(state, 0, 0);
    lua_pushcfunction(state, luaopen_string);
    lua_call(state, 0, 0);
    lua_pushcfunction(state, luaopen_table);
    lua_call(state, 0, 0);
    lua_pushcfunction(state, luaopen_math);
    lua_call(state, 0, 0);

    // create the metatable for all RenderBatch2D objects
    lua_pushstring(state, BATCH2D_META_REGISTRYNAME);
    lua_newtable(state);
    lua_pushstring(state, "__index");

    lua_createtable(state, 0, 8);
    API_ADD_SUB(vertexcount, batch2d)
    API_ADD_SUB(verticesperimage, batch2d)
    API_ADD_SUB(isminimap, batch2d)
    API_ADD_SUB(targetsize, batch2d)
    API_ADD_SUB(vertexxy, batch2d)
    API_ADD_SUB(vertexatlasxy, batch2d)
    API_ADD_SUB(vertexatlaswh, batch2d)
    API_ADD_SUB(vertexuv, batch2d)
    API_ADD_SUB(vertexcolour, batch2d)
    lua_pushstring(state, "vertexcolor");
    lua_pushcfunction(state, api_batch2d_vertexcolour);
    lua_settable(state, -3);

    lua_settable(state, -3);
    lua_settable(state, LUA_REGISTRYINDEX);

    // create registry["bolt"] as an empty table
    lua_pushstring(state, BOLT_REGISTRYNAME);
    lua_newtable(state);
    lua_settable(state, LUA_REGISTRYINDEX);

    // load Bolt API into package.preload, so that `require("bolt")` will find it
    lua_getfield(state, LUA_GLOBALSINDEX, "package");
    lua_getfield(state, -1, "preload");
    lua_pushstring(state, BOLT_REGISTRYNAME);
    lua_pushcfunction(state, _bolt_api_init);
    lua_settable(state, -3);
    lua_pop(state, 2);
}

static int _bolt_api_init(lua_State* state) {
    lua_createtable(state, 0, 5);
    API_ADD(apiversion)
    API_ADD(checkversion)
    API_ADD(time)
    API_ADD(setcallback2d)
    API_ADD(setcallbackswapbuffers)
    return 1;
}

uint8_t _bolt_plugin_is_inited() {
    return state ? 1 : 0;
}

void _bolt_plugin_close() {
    lua_close(state);
    state = NULL;
}

uint64_t _bolt_plugin_add(const char* lua) {
    // load the user-provided string as a lua function, putting that function on the stack
    if (luaL_loadstring(state, lua)) {
        const char* e = lua_tolstring(state, -1, 0);
        printf("plugin load error: %s\n", e);
        lua_pop(state, 1);
        return 0;
    }

    // create a new env for this plugin, and store it as registry["bolt"][i] where `i` is a unique ID
    const uint64_t plugin_id = next_plugin_id;
    next_plugin_id += 1;
    lua_newtable(state);
    lua_getfield(state, LUA_REGISTRYINDEX, BOLT_REGISTRYNAME);
    lua_pushinteger(state, plugin_id);
    lua_pushvalue(state, -3);
    lua_settable(state, -3);
    lua_pop(state, 1);

    // allow the new env access to the outer env via __index
    lua_newtable(state);
    lua_pushstring(state, "__index");
    lua_pushvalue(state, LUA_GLOBALSINDEX);
    lua_settable(state, -3);
    lua_setmetatable(state, -2);

    // set the env of the function created by `lua_loadstring` to this new env we just made
    lua_setfenv(state, -2);

    // attempt to run the function
    if (lua_pcall(state, 0, 0, 0)) {
        const char* e = lua_tolstring(state, -1, 0);
        printf("plugin startup error: %s\n", e);
        lua_pop(state, 1);
        _bolt_plugin_stop(plugin_id);
        return 0;
    } else {
        return plugin_id;
    }
}

void _bolt_plugin_stop(uint64_t id) {
    lua_getfield(state, LUA_REGISTRYINDEX, BOLT_REGISTRYNAME);
    lua_pushinteger(state, id);
    lua_pushnil(state);
    lua_settable(state, -3);
    lua_pop(state, 1);
}

void _bolt_plugin_handle_swapbuffers(void* event) {
    lua_pushlightuserdata(state, event);
    lua_getfield(state, LUA_REGISTRYINDEX, BATCH2D_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    int userdata_index = lua_gettop(state);
    lua_getfield(state, LUA_REGISTRYINDEX, BOLT_REGISTRYNAME);
    int key_index = lua_gettop(state);
    lua_pushnil(state);
    while (lua_next(state, key_index) != 0) {
        if (!lua_isnumber(state, -2)) {
            lua_pop(state, 1);
            continue;
        }
        lua_pushnumber(state, ENV_CALLBACK_SWAPBUFFERS);
        lua_gettable(state, -2);
        if (!lua_isfunction(state, -1)) {
            lua_pop(state, 2);
            continue;
        }
        lua_pushvalue(state, userdata_index);
        if (lua_pcall(state, 1, 0, 0)) {
            const lua_Integer plugin_id = lua_tonumber(state, -3);
            const char* e = lua_tolstring(state, -1, 0);
            printf("plugin swapbuffers error: %s\n", e);
            lua_pop(state, 2);
            _bolt_plugin_stop(plugin_id);
        } else {
            lua_pop(state, 1);
        }
    }
    lua_pop(state, 2);
}

void _bolt_plugin_handle_2d(struct RenderBatch2D* batch) {
    lua_pushlightuserdata(state, batch);
    lua_getfield(state, LUA_REGISTRYINDEX, BATCH2D_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    int userdata_index = lua_gettop(state);
    lua_getfield(state, LUA_REGISTRYINDEX, BOLT_REGISTRYNAME);
    int key_index = lua_gettop(state);
    lua_pushnil(state);
    while (lua_next(state, key_index) != 0) {
        if (!lua_isnumber(state, -2)) {
            lua_pop(state, 1);
            continue;
        }
        lua_pushnumber(state, ENV_CALLBACK_2D);
        lua_gettable(state, -2);
        if (!lua_isfunction(state, -1)) {
            lua_pop(state, 2);
            continue;
        }
        lua_pushvalue(state, userdata_index);
        if (lua_pcall(state, 1, 0, 0)) {
            const lua_Integer plugin_id = lua_tonumber(state, -3);
            const char* e = lua_tolstring(state, -1, 0);
            printf("plugin callback2d error: %s\n", e);
            lua_pop(state, 2);
            _bolt_plugin_stop(plugin_id);
        } else {
            lua_pop(state, 1);
        }
    }
    lua_pop(state, 2);
}

// Calls `error()` if arg count is incorrect
static void _bolt_check_argc(lua_State* state, int expected_argc, const char* function_name) {
    char error_buffer[256];
    const int argc = lua_gettop(state);
    if (argc != expected_argc) {
        sprintf(error_buffer, "incorrect argument count to '%s': expected %i, got %i", function_name, expected_argc, argc);
        lua_pushstring(state, error_buffer);
        lua_error(state);
    }
}

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
        sprintf(error_buffer, "checkversion major version mismatch: major version is %u, plugin expects %u", API_VERSION_MAJOR, (unsigned int)expected_major);
        lua_pushstring(state, error_buffer);
        lua_error(state);
    }
    if (expected_minor > API_VERSION_MINOR) {
        sprintf(error_buffer, "checkversion minor version mismatch: minor version is %u, plugin expects at least %u", API_VERSION_MINOR, (unsigned int)expected_minor);
        lua_pushstring(state, error_buffer);
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

static int api_setcallbackswapbuffers(lua_State* state) {
    _bolt_check_argc(state, 1, "setcallbackswapbuffers");
    lua_pushinteger(state, ENV_CALLBACK_SWAPBUFFERS);
    if (lua_isfunction(state, 1)) {
        lua_pushvalue(state, 1);
    } else {
        lua_pushnil(state);
    }
    lua_settable(state, LUA_GLOBALSINDEX);
    return 0;
}

static int api_setcallback2d(lua_State *state) {
    _bolt_check_argc(state, 1, "setcallback2d");
    lua_pushinteger(state, ENV_CALLBACK_2D);
    if (lua_isfunction(state, 1)) {
        lua_pushvalue(state, 1);
    } else {
        lua_pushnil(state);
    }
    lua_settable(state, LUA_GLOBALSINDEX);
    return 0;
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
    _bolt_check_argc(state, 1, "batch2d_verticesperimage");
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
    batch->functions.xy(batch, index - 1, batch->functions.userdata, xy);
    lua_pushinteger(state, xy[0]);
    lua_pushinteger(state, xy[1]);
    return 2;
}

static int api_batch2d_vertexatlasxy(lua_State* state) {
    _bolt_check_argc(state, 2, "batch2d_vertexatlasxy");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    int32_t xy[2];
    batch->functions.atlas_xy(batch, index - 1, batch->functions.userdata, xy);
    lua_pushinteger(state, xy[0]);
    lua_pushinteger(state, xy[1]);
    return 2;
}

static int api_batch2d_vertexatlaswh(lua_State* state) {
    _bolt_check_argc(state, 2, "batch2d_vertexatlaswh");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    int32_t wh[2];
    batch->functions.atlas_wh(batch, index - 1, batch->functions.userdata, wh);
    lua_pushinteger(state, wh[0]);
    lua_pushinteger(state, wh[1]);
    return 2;
}

static int api_batch2d_vertexuv(lua_State* state) {
    _bolt_check_argc(state, 2, "batch2d_vertexuv");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    double uv[2];
    batch->functions.uv(batch, index - 1, batch->functions.userdata, uv);
    lua_pushnumber(state, uv[0]);
    lua_pushnumber(state, uv[1]);
    return 2;
}

static int api_batch2d_vertexcolour(lua_State* state) {
    _bolt_check_argc(state, 4, "batch2d_vertexcolour");
    struct RenderBatch2D* batch = lua_touserdata(state, 1);
    const lua_Integer index = lua_tointeger(state, 2);
    double colour[4];
    batch->functions.colour(batch, index - 1, batch->functions.userdata, colour);
    lua_pushnumber(state, colour[0]);
    lua_pushnumber(state, colour[1]);
    lua_pushnumber(state, colour[2]);
    lua_pushnumber(state, colour[3]);
    return 4;
}
