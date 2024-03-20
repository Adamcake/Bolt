#if defined(BOLT_PLUGINS)
#include "client.hxx"

#include <sys/socket.h>
#include <sys/un.h>

void Browser::Client::IPCBind() {
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s/ipc-0", this->runtime_dir.c_str());
    this->ipc_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    unlink(addr.sun_path);
    if (bind(this->ipc_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("[B] error: IPC bind(%i, %s) returned %i\n", this->ipc_fd, addr.sun_path, errno);
    } else if (listen(this->ipc_fd, 16) == -1) {
        printf("[B] error: IPC listen(%i) returned %i\n", this->ipc_fd, errno);
    }
}

void Browser::Client::IPCRun() {
    // todo
}

#endif
