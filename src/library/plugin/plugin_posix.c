#include "plugin.h"

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void _bolt_plugin_ipc_init(int* fd) {
    const int olderr = errno;
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    *fd = socket(addr.sun_family, SOCK_STREAM, 0);
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char* prefix = (runtime_dir && *runtime_dir) ? runtime_dir : "/tmp";
    snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s/bolt-launcher/ipc-0", prefix);
    if (connect(*fd, (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("error: IPC connect() error %i\n", errno);
    }
    errno = olderr;
}

void _bolt_plugin_ipc_close(int fd) {
    const int olderr = errno;
    close(fd);
    errno = olderr;
}
