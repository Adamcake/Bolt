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

enum BoltIPCMessageTypeToHost {
    IPC_MSG_DUPLICATEPROCESS,
    IPC_MSG_IDENTIFY,
    IPC_MSG_CLIENT_STOPPED_PLUGIN,
    IPC_MSG_CREATEBROWSER_EXTERNAL,
    IPC_MSG_CREATEBROWSER_OSR,
    IPC_MSG_CLOSEBROWSER_EXTERNAL,
    IPC_MSG_CLOSEBROWSER_OSR,
    IPC_MSG_OSRUPDATE_ACK,
    IPC_MSG_EVREPOSITION,
    IPC_MSG_EVMOUSEMOTION,
    IPC_MSG_EVMOUSEBUTTON,
    IPC_MSG_EVMOUSEBUTTONUP,
    IPC_MSG_EVSCROLL,
    IPC_MSG_EVMOUSELEAVE,
    IPC_MSG_PLUGINMESSAGE,
    IPC_MSG_OSRPLUGINMESSAGE,
    IPC_MSG_CAPTURENOTIFY_EXTERNAL,
    IPC_MSG_CAPTURENOTIFY_OSR,
};

enum BoltIPCMessageTypeToClient {
    IPC_MSG_STARTPLUGIN,
    IPC_MSG_HOST_STOPPED_PLUGIN,
    IPC_MSG_OSRUPDATE,
    IPC_MSG_OSRPOPUPCONTENTS,
    IPC_MSG_OSRPOPUPPOSITION,
    IPC_MSG_OSRPOPUPVISIBILITY,
    IPC_MSG_EXTERNALBROWSERMESSAGE,
    IPC_MSG_OSRBROWSERMESSAGE,
    IPC_MSG_OSRSTARTREPOSITION,
    IPC_MSG_OSRCANCELREPOSITION,
    IPC_MSG_BROWSERCLOSEREQUEST,
    IPC_MSG_OSRCLOSEREQUEST,
    IPC_MSG_EXTERNALCAPTUREDONE,
    IPC_MSG_OSRCAPTUREDONE,
};

/// Header for BoltIPCMessageTypeToHost::IPC_MSG_IDENTIFY
struct BoltIPCIdentifyHeader {
    uint8_t name_length;
};

/// Header for BoltIPCMessageTypeToHost::IPC_MSG_CLIENT_STOPPED_PLUGINS
struct BoltIPCClientStoppedPluginHeader {
    uint64_t plugin_id;
};

/// Header for BoltIPCMessageTypeToHost::IPC_MSG_CREATEBROWSER_*
struct BoltIPCCreateBrowserHeader {
    uint64_t plugin_id;
    uint64_t window_id;
    uint32_t url_length;
    int pid;
    int w;
    int h;
};

/// Header for BoltIPCMessageTypeToHost::IPC_MSG_CLOSEBROWSER_*
struct BoltIPCCloseBrowserHeader {
    uint64_t plugin_id;
    uint64_t window_id;
};

/// Header for BoltIPCMessageTypeToHost::IPC_MSG_OSRUPDATE_ACK
struct BoltIPCOsrUpdateAckHeader {
    uint64_t plugin_id;
    uint64_t window_id;
};

/// Header for BoltIPCMessageTypeToHost::IPC_MSG_EV*
struct BoltIPCEvHeader {
    uint64_t plugin_id;
    uint64_t window_id;
};

/// Header for BoltMessageTypeToHost::IPC_MSG_PLUGINMESSAGE and BoltMessageTypeToHost::IPC_MSG_OSRPLUGINMESSAGE
struct BoltIPCPluginMessageHeader {
    uint64_t plugin_id;
    uint64_t window_id;
    size_t message_size;
};

/// Header for BoltMessageTypeToHost::IPC_MSG_CAPTURENOTIFY_*
struct BoltIPCCaptureNotifyHeader {
    uint64_t plugin_id;
    uint64_t window_id;
    uint64_t pid;
    uint64_t capture_id;
    uint32_t width;
    uint32_t height;
    uint8_t needs_remap;
};

/// Header for BoltIPCMessageTypeToClient::IPC_MSG_STARTPLUGIN
struct BoltIPCStartPluginHeader {
    uint64_t uid;
    uint32_t path_size;
    uint32_t main_size;
};

/// Header for BoltIPCMessageTypeToClient::IPC_MSG_HOST_STOPPED_PLUGIN
struct BoltIPCHostStoppedPluginHeader {
    uint64_t plugin_id;
};

/// Header for BoltIPCMessageTypeToClient::IPC_MSG_OSRUPDATE
struct BoltIPCOsrUpdateHeader {
    uint32_t rect_count;
    uint64_t window_id;
    void* needs_remap; // 1 or 0 on POSIX-compliant platforms, HANDLE or nullptr on Windows
    int width;
    int height;
};

/// Rectangle sent after IPC_MSG_OSRUPDATE; header.rect_count indicates the number of rects sent
struct BoltIPCOsrUpdateRect {
    int x;
    int y;
    int w;
    int h;
};

/// Header for BoltIPCMessageTypeToClient::IPC_MSG_OSRPOPUPCONTENTS
struct BoltIPCOsrPopupContentsHeader {
    uint64_t window_id;
    int width;
    int height;
};

/// Header for BoltIPCMessageTypeToClient::IPC_MSG_OSRPOPUPPOSITION
struct BoltIPCOsrPopupPositionHeader {
    uint64_t window_id;
    int x;
    int y;
};

/// Header for BoltIPCMessageTypeToClient::IPC_MSG_OSRPOPUPVISIBILITY
struct BoltIPCOsrPopupVisibilityHeader {
    uint64_t window_id;
    uint8_t visible;
};

/// Header for BoltMessageTypeToClient::IPC_MSG_EXTERNALBROWSERMESSAGE and BoltMessageTypeToClient::IPC_MSG_OSRBROWSERMESSAGE
struct BoltIPCBrowserMessageHeader {
    uint64_t window_id;
    uint64_t plugin_id;
    size_t message_size;
};

/// Header for BoltMessageTypeToClient::IPC_MSG_OSRSTARTREPOSITION
struct BoltIPCOsrStartRepositionHeader {
    uint64_t window_id;
    int8_t horizontal; // -1 for left edge, 1 for right edge
    int8_t vertical; // -1 for top edge, 1 for bottom edge
};

/// Header for BoltMessageTypeToClient::IPC_MSG_OSRCANCELREPOSITION
struct BoltIPCOsrCancelRepositionHeader {
    uint64_t window_id;
};

/// Header for BoltMessageTypeToClient::IPC_MSG_BROWSERCLOSEREQUEST
struct BoltIPCBrowserCloseRequestHeader {
    uint64_t window_id;
    uint64_t plugin_id;
};

/// Header for BoltMessageTypeToClient::IPC_MSG_OSRCLOSEREQUEST
struct BoltIPCOsrCloseRequestHeader {
    uint64_t window_id;
};

/// Header for BoltMessageTypeToClient::IPC_MSG_EXTERNALCAPTUREDONE
struct BoltIPCExternalCaptureDoneHeader {
    uint64_t window_id;
    uint64_t plugin_id;
};

/// Header for BoltMessageTypeToClient::IPC_MSG_OSRCAPTUREDONE
struct BoltIPCOsrCaptureDoneHeader {
    uint64_t window_id;
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
