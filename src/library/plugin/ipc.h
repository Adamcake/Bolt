#ifndef _BOLT_LIBRARY_PLUGIN_IPC_H_
#define _BOLT_LIBRARY_PLUGIN_IPC_H_
#include <stddef.h>

/* no doc comments for these are they're entirely self-explanatory */

void _bolt_plugin_ipc_init();
void _bolt_plugin_ipc_close();
void _bolt_plugin_ipc_send(void* data, size_t len);

#endif
