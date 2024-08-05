#include "ipc.h"

#if defined(_WIN32)
#include <afunix.h>
#define SENDFLAGS 0
#define poll WSAPoll
#else
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#define SENDFLAGS MSG_NOSIGNAL
#endif

#include <stdio.h>

uint8_t _bolt_ipc_send(BoltSocketType fd, const void* data, size_t len) {
    const int olderr = errno;
    size_t remaining = len;
    size_t sent = 0;
    while (remaining > 0) {
        int r = send(fd, (const uint8_t*)data + sent, remaining, SENDFLAGS);
        if (r == -1) {
            printf("[IPC] error: IPC send() failed, error %i\n", errno);
            errno = olderr;
            return 1;
        }
        remaining -= r;
        sent += r;
    }
    return 0;
}

uint8_t _bolt_ipc_receive(BoltSocketType fd, void* data, size_t len) {
    const int olderr = errno;
    size_t remaining = len;
    size_t received = 0;
    while (remaining > 0) {
        int r = recv(fd, (uint8_t*)data + received, remaining, 0);
        if (r == -1) {
            printf("[IPC] error: IPC recv() failed, error %i\n", errno);
            errno = olderr;
            return 1;
        }
        if (r == 0) {
            printf("[IPC] IPC recv() got EOF\n");
            return 1;
        }
        remaining -= r;
        received += r;
    }
    return 0;
}

uint8_t _bolt_ipc_poll(BoltSocketType fd) {
    const int olderr = errno;
    struct pollfd pfd = {.events = POLLIN, .fd = fd};
    int r = poll(&pfd, 1, 0);
    if (r == -1) {
        printf("[IPC] error: IPC poll() failed, error %i\n", errno);
        errno = olderr;
        return 0;
    }
    return r && (pfd.revents & POLLIN);
}
