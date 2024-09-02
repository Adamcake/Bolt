#ifndef _BOLT_LIBRARY_IPC_H_
#define _BOLT_LIBRARY_IPC_H_

// winsock2.h has to be included before windows.h and both before afunix.h if applicable.
// this design is evil, and can cause some really confusing linker errors if windows.h happens to be
// somewhere in your include chain before this file. so generally, this file should be #included
// right at the top of any file it's in.
#if defined(_WIN32)
#include <winsock2.h>
#include <Windows.h>
typedef SOCKET BoltSocketType;
#else
typedef int BoltSocketType;
#endif

#include <stdint.h>
#include <stddef.h>

enum BoltMessageTypeToHost {
    IPC_MSG_DUPLICATEPROCESS,
    IPC_MSG_IDENTIFY,
    IPC_MSG_CLIENT_STOPPED_PLUGINS,
    IPC_MSG_CREATEBROWSER_OSR,
    IPC_MSG_OSRUPDATE_ACK,
    IPC_MSG_EVRESIZE,
    IPC_MSG_EVMOUSEMOTION,
    IPC_MSG_EVMOUSEBUTTON,
    IPC_MSG_EVMOUSEBUTTONUP,
    IPC_MSG_EVSCROLL,
    IPC_MSG_EVMOUSELEAVE,
};

enum BoltMessageTypeToClient {
    IPC_MSG_STARTPLUGINS,
    IPC_MSG_HOST_STOPPED_PLUGINS,
    IPC_MSG_OSRUPDATE,
};

/// A generic message. The host process will always assume incoming data is an instance of this
/// struct, and may choose to handle or ignore any message based on the first parameter, `message_type`.
/// The meaning of the `items` parameter on the other hand is specific to the message type, but
/// typically indicates how much extra data there is to read from the IPC socket for this message.
///
/// Messages to the host process may originate from anywhere.
struct BoltIPCMessageToHost {
    enum BoltMessageTypeToHost message_type;
    uint32_t items;
};

/// A generic message. A client process will always assume incoming data is an instance of this
/// struct, and may choose to handle or ignore any message based on the first parameter, `message_type`.
/// The meaning of the `items` parameter on the other hand is specific to the message type, but
/// typically indicates how much extra data there is to read from the IPC socket for this message.
///
/// Messages to the host process always originate from the host.
struct BoltIPCMessageToClient {
    enum BoltMessageTypeToClient message_type;
    uint32_t items;
};

#if defined(__cplusplus)
extern "C" {
#endif

/// Sends the given bytes on the IPC channel and returns zero on success or non-zero on failure.
uint8_t _bolt_ipc_send(BoltSocketType fd, const void* data, size_t len);

/// Receives the given number of bytes from the IPC socket, blocking until the full amount has been
/// received. Use plugin_ipc_poll to check if this will block. Returns zero on success or non-zero
/// on failure.
uint8_t _bolt_ipc_receive(BoltSocketType fd, void* data, size_t len);

/// Checks whether ipc_receive would return immediately (1) or block (0) or return an error (0).
uint8_t _bolt_ipc_poll(BoltSocketType fd);

#if defined(__cplusplus)
}
#endif

#endif
