#ifndef _BOLT_LIBRARY_IPC_H_
#define _BOLT_LIBRARY_IPC_H_
#include <stdint.h>
#include <stddef.h>

enum BoltMessageType {
    IPC_MSG_NEWPLUGINS,
};

/// A generic message. The host process and game clients will always assume incoming data is an
/// instance of this struct, and they may choose to handle or ignore any message based on the first
/// parameter, `message_type`, which is a value in the enum BoltMessageType. The meaning of the
/// `items` parameter on the other hand is specific to the message type, but typically indicates
/// how much extra data pertinent to this message there is to read from the IPC socket.
///
/// Messages to a game client will always originate from the host process, whereas message to the
/// host process may originate from anywhere.
struct BoltIPCMessage {
    uint32_t message_type;
    uint32_t items;
};

#if defined(__cplusplus)
extern "C" {
#endif

/// Sends the given bytes on the IPC channel and returns zero on success or non-zero on failure.
uint8_t _bolt_ipc_send(int fd, const void* data, size_t len);

/// Receives the given number of bytes from the IPC socket, blocking until the full amount has been
/// received. Use plugin_ipc_poll to check if this will block. Returns zero on success or non-zero
/// on failure.
uint8_t _bolt_ipc_receive(int fd, void* data, size_t len);

/// Checks whether ipc_receive would return immediately (1) or block (0) or return an error (0).
uint8_t _bolt_ipc_poll(int fd);

#if defined(__cplusplus)
}
#endif

#endif
