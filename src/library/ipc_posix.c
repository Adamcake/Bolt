#include "ipc.h"

#include <poll.h>
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>

uint8_t _bolt_ipc_send(int fd, const void* data, size_t len) {
    const int olderr = errno;
    size_t remaining = len;
    size_t sent = 0;
    while (remaining > 0) {
        int r = send(fd, data + sent, remaining, MSG_NOSIGNAL);
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

uint8_t _bolt_ipc_receive(int fd, void* data, size_t len) {
    const int olderr = errno;
    size_t remaining = len;
    size_t received = 0;
    while (remaining > 0) {
        int r = recv(fd, data + received, remaining, 0);
        if (r == -1) {
            printf("[IPC] error: IPC recv() failed, error %i\n", errno);
            errno = olderr;
            return 1;
        }
        remaining -= r;
        received += r;
    }
    return 0;
}

uint8_t _bolt_ipc_poll(int fd) {
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
