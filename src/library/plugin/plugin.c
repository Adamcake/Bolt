#include "plugin.h"

#include "ipc.h"
#include "plugin_api.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <time.h>

#define API_VERSION_MAJOR 1
#define API_VERSION_MINOR 0

#define API_ADD(FUNC) lua_pushstring(state, #FUNC);lua_pushcfunction(state, api_##FUNC);lua_settable(state, -3);
#define API_ADD_SUB(FUNC, SUB) lua_pushstring(state, #FUNC);lua_pushcfunction(state, api_##SUB##_##FUNC);lua_settable(state, -3);
#define API_ADD_SUB_ALIAS(FUNC, ALIAS, SUB) lua_pushstring(state, #ALIAS);lua_pushcfunction(state, api_##SUB##_##FUNC);lua_settable(state, -3);

const char* BOLT_REGISTRYNAME = "bolt";
const char* BATCH2D_META_REGISTRYNAME = "batch2dindex";
const char* RENDER3D_META_REGISTRYNAME = "batch3dindex";
const char* MINIMAP_META_REGISTRYNAME = "minimapindex";
const char* SWAPBUFFERS_META_REGISTRYNAME = "swapbuffersindex";
const char* SURFACE_META_REGISTRYNAME = "surfaceindex";
uint64_t next_plugin_id = 1;

void (*surface_init)(struct SurfaceFunctions*, unsigned int, unsigned int);
void (*surface_destroy)(void*);

enum {
    ENV_CALLBACK_SWAPBUFFERS,
    ENV_CALLBACK_2D,
    ENV_CALLBACK_3D,
    ENV_CALLBACK_MINIMAP,
};

static int _bolt_api_init(lua_State* state);

lua_State* state = NULL;

// macro for defining callback functions "_bolt_plugin_handle_*" and "api_setcallback*"
// e.g. DEFINE_CALLBACK(swapbuffers, SWAPBUFFERS_META_REGISTRYNAME, SwapBuffersEvent, ENV_CALLBACK_SWAPBUFFERS)
#define DEFINE_CALLBACK(APINAME, META_REGNAME, STRUCTNAME, ENUMNAME) \
void _bolt_plugin_handle_##APINAME(struct STRUCTNAME* e) { \
    lua_pushlightuserdata(state, e); \
    lua_getfield(state, LUA_REGISTRYINDEX, META_REGNAME); \
    lua_setmetatable(state, -2); \
    int userdata_index = lua_gettop(state); \
    lua_getfield(state, LUA_REGISTRYINDEX, BOLT_REGISTRYNAME); \
    int key_index = lua_gettop(state); \
    lua_pushnil(state); \
    while (lua_next(state, key_index) != 0) { \
        if (!lua_isnumber(state, -2)) { \
            lua_pop(state, 1); \
            continue; \
        } \
        lua_pushnumber(state, ENUMNAME); \
        lua_gettable(state, -2); \
        if (!lua_isfunction(state, -1)) { \
            lua_pop(state, 2); \
            continue; \
        } \
        lua_pushvalue(state, userdata_index); \
        if (lua_pcall(state, 1, 0, 0)) { \
            const lua_Integer plugin_id = lua_tonumber(state, -3); \
            const char* e = lua_tolstring(state, -1, 0); \
            printf("plugin callback %s error: %s\n", #APINAME, e); \
            lua_pop(state, 2); \
            _bolt_plugin_stop(plugin_id); \
        } else { \
            lua_pop(state, 1); \
        } \
    } \
    lua_pop(state, 2); \
} \
static int api_setcallback##APINAME(lua_State *state) { \
    _bolt_check_argc(state, 1, "setcallback"#APINAME); \
    lua_pushinteger(state, ENUMNAME); \
    if (lua_isfunction(state, 1)) { \
        lua_pushvalue(state, 1); \
    } else { \
        lua_pushnil(state); \
    } \
    lua_settable(state, LUA_GLOBALSINDEX); \
    return 0; \
}

static int surface_gc(lua_State* state) {
    const struct SurfaceFunctions* functions = lua_touserdata(state, 1);
    surface_destroy(functions->userdata);
    return 0;
}

void _bolt_plugin_init(void (*_surface_init)(struct SurfaceFunctions*, unsigned int, unsigned int), void (*_surface_destroy)(void*)) {
    _bolt_plugin_ipc_init();

    surface_init = _surface_init;
    surface_destroy = _surface_destroy;
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
    lua_createtable(state, 0, 12);
    API_ADD_SUB(vertexcount, batch2d)
    API_ADD_SUB(verticesperimage, batch2d)
    API_ADD_SUB(isminimap, batch2d)
    API_ADD_SUB(targetsize, batch2d)
    API_ADD_SUB(vertexxy, batch2d)
    API_ADD_SUB(vertexatlasxy, batch2d)
    API_ADD_SUB(vertexatlaswh, batch2d)
    API_ADD_SUB(vertexuv, batch2d)
    API_ADD_SUB(vertexcolour, batch2d)
    API_ADD_SUB(textureid, batch2d)
    API_ADD_SUB(texturesize, batch2d)
    API_ADD_SUB(texturecompare, batch2d)
    API_ADD_SUB(texturedata, batch2d)
    API_ADD_SUB_ALIAS(vertexcolour, vertexcolor, batch2d)
    lua_settable(state, -3);
    lua_settable(state, LUA_REGISTRYINDEX);

    // create the metatable for all Render3D objects
    lua_pushstring(state, RENDER3D_META_REGISTRYNAME);
    lua_newtable(state);
    lua_pushstring(state, "__index");
    lua_createtable(state, 0, 13);
    API_ADD_SUB(vertexcount, render3d)
    API_ADD_SUB(vertexxyz, render3d)
    API_ADD_SUB(vertexmeta, render3d)
    API_ADD_SUB(atlasxywh, render3d)
    API_ADD_SUB(vertexuv, render3d)
    API_ADD_SUB(vertexcolour, render3d)
    API_ADD_SUB(textureid, render3d)
    API_ADD_SUB(texturesize, render3d)
    API_ADD_SUB(texturecompare, render3d)
    API_ADD_SUB(texturedata, render3d)
    API_ADD_SUB(toworldspace, render3d)
    API_ADD_SUB(toscreenspace, render3d)
    API_ADD_SUB(worldposition, render3d)
    API_ADD_SUB_ALIAS(vertexcolour, vertexcolor, render3d)
    lua_settable(state, -3);
    lua_settable(state, LUA_REGISTRYINDEX);

    // create the metatable for all RenderMinimap objects
    lua_pushstring(state, MINIMAP_META_REGISTRYNAME);
    lua_newtable(state);
    lua_pushstring(state, "__index");
    lua_createtable(state, 0, 3);
    API_ADD_SUB(angle, minimap)
    API_ADD_SUB(scale, minimap)
    API_ADD_SUB(position, minimap)
    lua_settable(state, -3);
    lua_settable(state, LUA_REGISTRYINDEX);

    // create the metatable for all SwapBuffers objects
    lua_pushstring(state, SWAPBUFFERS_META_REGISTRYNAME);
    lua_newtable(state);
    lua_pushstring(state, "__index");
    lua_createtable(state, 0, 0);
    lua_settable(state, -3);
    lua_settable(state, LUA_REGISTRYINDEX);

    // create the metatable for all Surface objects
    lua_pushstring(state, SURFACE_META_REGISTRYNAME);
    lua_newtable(state);
    lua_pushstring(state, "__index");
    lua_createtable(state, 0, 2);
    API_ADD_SUB(clear, surface)
    API_ADD_SUB(drawtoscreen, surface)
    lua_settable(state, -3);
    lua_pushstring(state, "__gc");
    lua_pushcfunction(state, surface_gc);
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
    API_ADD(setcallback3d)
    API_ADD(setcallbackminimap)
    API_ADD(setcallbackswapbuffers)
    API_ADD(createsurface)
    return 1;
}

uint8_t _bolt_plugin_is_inited() {
    return state ? 1 : 0;
}

void _bolt_plugin_close() {
    lua_close(state);
    state = NULL;
    _bolt_plugin_ipc_close();
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

DEFINE_CALLBACK(swapbuffers, SWAPBUFFERS_META_REGISTRYNAME, SwapBuffersEvent, ENV_CALLBACK_SWAPBUFFERS)
DEFINE_CALLBACK(2d, BATCH2D_META_REGISTRYNAME, RenderBatch2D, ENV_CALLBACK_2D)
DEFINE_CALLBACK(3d, RENDER3D_META_REGISTRYNAME, Render3D, ENV_CALLBACK_3D)
DEFINE_CALLBACK(minimap, MINIMAP_META_REGISTRYNAME, RenderMinimapEvent, ENV_CALLBACK_MINIMAP)

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

static int api_createsurface(lua_State* state) {
    _bolt_check_argc(state, 2, "createsurface");
    const lua_Integer w = lua_tointeger(state, 1);
    const lua_Integer h = lua_tointeger(state, 2);
    struct SurfaceFunctions* functions = lua_newuserdata(state, sizeof(struct SurfaceFunctions));
    surface_init(functions, w, h);
    lua_getfield(state, LUA_REGISTRYINDEX, SURFACE_META_REGISTRYNAME);
    lua_setmetatable(state, -2);
    return 1;
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
            sprintf(error_buffer, "incorrect argument count to 'surface_clear': expected 1, 4, or 5, but got %i", argc);
            lua_pushstring(state, error_buffer);
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
