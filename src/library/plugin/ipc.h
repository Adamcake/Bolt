#ifndef _BOLT_LIBRARY_PLUGIN_IPC_H_
#define _BOLT_LIBRARY_PLUGIN_IPC_H_
#include <stddef.h>
#include <stdint.h>

/// Opens the IPC channel.
void _bolt_plugin_ipc_init();

/// Closes the IPC channel.
void _bolt_plugin_ipc_close();

/// Sends the given bytes on the IPC channel and returns zero on success or non-zero on failure.
uint8_t _bolt_plugin_ipc_send(const void* data, size_t len);

/// Receives the given number of bytes from the IPC socket, blocking until the full amount has been
/// received. Use plugin_ipc_poll to check if this will block. Returns zero on success or non-zero
/// on failure.
uint8_t _bolt_plugin_ipc_receive(void* data, size_t len);

/// Checks whether ipc_receive would return immediately (1) or block (0) or return an error (0).
uint8_t _bolt_plugin_ipc_poll();

#endif
