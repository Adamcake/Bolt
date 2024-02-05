#ifndef _BOLT_LIBRARY_PLUGIN_H_
#define _BOLT_LIBRARY_PLUGIN_H_

#include <stdint.h>

/// Init the plugin library. Call _bolt_plugin_close at the end of execution, and don't double-init.
void _bolt_plugin_init();

/// Close the plugin library.
void _bolt_plugin_close();

/// Creates a new instance of a plugin with its own Lua environment (lua_setfenv).
/// The `lua` param will be loaded and executed in a fresh environment, then event callbacks will be
/// sent to it until it is destroyed by the plugin being stopped.
/// Returns true if the plugin was added successfully, false otherwise.
uint8_t _bolt_plugin_add(const char* lua);

#endif
