#include "plugin.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

lua_State* state;

#define MAX_PLUGINS 8
struct BoltPlugin {
    uint8_t used;
} plugins[MAX_PLUGINS];

void _bolt_plugin_init() {
    state = luaL_newstate();
    luaopen_base(state);
    luaopen_string(state);
    luaopen_table(state);
    luaopen_math(state);
}

void _bolt_plugin_close() {
    lua_close(state);
}

uint8_t _bolt_plugin_add(const char* lua) {
    for (size_t i = 0; i < MAX_PLUGINS; i += 1) {
        if (!plugins[i].used) {
            luaL_loadstring(state, lua);
            lua_newtable(state);
            lua_pushinteger(state, i);
            lua_pushvalue(state, -2);
            lua_settable(state, LUA_REGISTRYINDEX);
            lua_newtable(state);
            lua_pushstring(state, "__index");
            lua_pushvalue(state, LUA_GLOBALSINDEX);
            lua_settable(state, -3);
            lua_setmetatable(state, -2);
            lua_setfenv(state, -2);
            lua_call(state, 0, LUA_MULTRET);
            plugins[i].used = 1;
            return 1;
        }
    }
    return 0;
}
