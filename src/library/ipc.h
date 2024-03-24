#ifndef _BOLT_LIBRARY_IPC_H_
#define _BOLT_LIBRARY_IPC_H_
#include <stdint.h>

enum BoltMessageType {
    IPC_PLUGIN_LIST,
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

#endif
