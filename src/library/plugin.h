#ifndef _BOLT_LIBRARY_PLUGIN_H_
#define _BOLT_LIBRARY_PLUGIN_H_

#include <stdint.h>

struct RenderBatch2D {
    uint16_t* indices;
    uint8_t* texture;
    uint32_t tex_width;
    uint32_t tex_height;
    uint32_t index_count;
    uint32_t vertices_per_icon;
};

/// Init the plugin library. Call _bolt_plugin_close at the end of execution, and don't double-init.
void _bolt_plugin_init();

/// Returns true if the plugin library is initialised (i.e. init has been called more recently than
/// close), otherwise false.
uint8_t _bolt_plugin_is_inited();

/// Close the plugin library.
void _bolt_plugin_close();

/// Creates a new instance of a plugin with its own Lua environment (lua_setfenv).
/// The `lua` param will be loaded and executed in a fresh environment, then event callbacks will be
/// sent to it until it is destroyed by the plugin being stopped.
///
/// Returns a unique plugin ID on success or 0 on failure.
uint64_t _bolt_plugin_add(const char* lua);

/// Stops a plugin via its plugin ID, returned from `_bolt_plugin_add`.
void _bolt_plugin_stop(uint64_t);

/// Sends a SwapBuffers event to all plugins.
void _bolt_plugin_handle_swapbuffers(void*);

/// Sends a RenderBatch2D to all plugins.
void _bolt_plugin_handle_2d(struct RenderBatch2D*);

#endif
