#include <lua.h>
#include <stdint.h>

#define PLUGIN_REGISTRYNAME "plugin"
#define WINDOWS_REGISTRYNAME "windows"
#define BROWSERS_REGISTRYNAME "browsers"

#define WINDOW_MIN_SIZE 60

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

struct ExternalBrowser {
    uint64_t id;
    uint64_t plugin_id;
    struct Plugin* plugin;
    uint8_t do_capture;
    uint8_t capture_ready;
    uint64_t capture_id;
};

void _bolt_api_push_bolt_table(lua_State*);
void _bolt_api_push_metatable_render2d(lua_State*);
void _bolt_api_push_metatable_render3d(lua_State*);
void _bolt_api_push_metatable_rendericon(lua_State*);
void _bolt_api_push_metatable_minimapterrain(lua_State*);
void _bolt_api_push_metatable_renderminimap(lua_State*);
void _bolt_api_push_metatable_point(lua_State*);
void _bolt_api_push_metatable_transform(lua_State*);
void _bolt_api_push_metatable_buffer(lua_State*);
void _bolt_api_push_metatable_swapbuffers(lua_State*);
void _bolt_api_push_metatable_surface(lua_State*);
void _bolt_api_push_metatable_window(lua_State*);
void _bolt_api_push_metatable_browser(lua_State*);
void _bolt_api_push_metatable_embeddedbrowser(lua_State*);
void _bolt_api_push_metatable_reposition(lua_State*);
void _bolt_api_push_metatable_mousemotion(lua_State*);
void _bolt_api_push_metatable_mousebutton(lua_State*);
void _bolt_api_push_metatable_scroll(lua_State*);
void _bolt_api_push_metatable_mouseleave(lua_State*);
