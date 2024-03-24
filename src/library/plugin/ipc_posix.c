#include "ipc.h"

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int fd;

void _bolt_plugin_ipc_init() {
    const int olderr = errno;
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    fd = socket(addr.sun_family, SOCK_STREAM, 0);
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char* prefix = (runtime_dir && *runtime_dir) ? runtime_dir : "/tmp";
    snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s/bolt-launcher/ipc-0", prefix);
    if (connect(fd, (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("error: IPC connect() error %i\n", errno);
    }
    errno = olderr;
}

void _bolt_plugin_ipc_close() {
    const int olderr = errno;
    close(fd);
    errno = olderr;
}

uint8_t _bolt_plugin_ipc_send(const void* data, size_t len) {
    const int olderr = errno;
    size_t remaining = len;
    size_t sent = 0;
    while (remaining > 0) {
        int r = send(fd, data + sent, remaining, MSG_NOSIGNAL);
        if (r == -1) {
            printf("error: IPC send() failed, error %i\n", errno);
            errno = olderr;
            return 1;
        }
        remaining -= r;
        sent += r;
    }
    return 0;
}

uint8_t _bolt_plugin_ipc_receive(void* data, size_t len) {
    const int olderr = errno;
    size_t remaining = len;
    size_t received = 0;
    while (remaining > 0) {
        int r = recv(fd, data + received, remaining, 0);
        if (r == -1) {
            printf("error: IPC recv() failed, error %i\n", errno);
            errno = olderr;
            return 1;
        }
        remaining -= r;
        received += r;
    }
    return 0;
}

uint8_t _bolt_plugin_ipc_poll() {
    const int olderr = errno;
    struct pollfd pfd = {.events = POLLIN, .fd = fd};
    int r = poll(&pfd, 1, 0);
    if (r == -1) {
        printf("error: IPC poll() failed, error %i\n", errno);
        errno = olderr;
        return 0;
    }
    return r && (pfd.revents & POLLIN);
}
