#if defined(BOLT_PLUGINS)
#include "client.hxx"

#if defined(_WIN32)
#include <afunix.h>
#define poll WSAPoll
#define close closesocket
#define SHUT_RDWR SD_BOTH
#define OSPATH_PRINTF_STR "%ls"
#else
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#define OSPATH_PRINTF_STR "%s"
#endif

#include <algorithm>
#include <fmt/core.h>

void Browser::Client::IPCBind() {
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, OSPATH_PRINTF_STR "/ipc-0", this->runtime_dir.c_str());
	this->ipc_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	unlink(addr.sun_path);
	if (bind(this->ipc_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		fmt::print("[B] error: IPC bind({}, {}) returned {}\n", this->ipc_fd, addr.sun_path, errno);
	} else if (listen(this->ipc_fd, 16) == -1) {
		fmt::print("[B] error: IPC listen({}) returned {}\n", this->ipc_fd, errno);
	}
}

void Browser::Client::IPCRun() {
	std::vector<pollfd> pfds;
	pfds.reserve(8);
	// pfds[0] is the IPC socket itself, it's special and won't move or be removed
	pfds.push_back({.fd = this->ipc_fd, .events = POLLIN});
	while (true) {
		int ready = poll(pfds.data(), pfds.size(), -1);
		// ready=0 indicates a timeout which will probably never actually happen
		if (ready == 0) continue;
		if (ready == -1) {
			fmt::print("[I] IPC thread exiting due to poll error {}\n", errno);
			break;
		}

		// check pfds[0] for new client connections
		if (pfds[0].revents == POLLIN) {
			BoltSocketType client_fd = 
#if defined(_WIN32)
				accept(this->ipc_fd, nullptr, nullptr);
#else
				accept4(this->ipc_fd, nullptr, nullptr, SOCK_CLOEXEC);
#endif
			if (client_fd == -1) {
				fmt::print("[I] IPC thread exiting due to accept error {}\n", errno);
				break;
			}
			pfds.push_back({.fd = client_fd, .events = POLLIN});
			this->IPCHandleNewClient(client_fd);
			this->IPCHandleClientListUpdate();
		} else if (pfds[0].revents != 0) {
			fmt::print("[I] IPC thread exiting due to poll event {}\n", pfds[0].revents);
			break;
		}

		// check the rest of the pollfds for incoming data
		for (auto i = pfds.begin() + 1; i != pfds.end(); i += 1) {
			if (i->revents != 0) {
				uint8_t byte;
				if (i->revents & POLLIN) {
					if (!this->IPCHandleMessage(i->fd)) {
						fmt::print("[I] dropping client fd {} due to read error or eof\n", i->fd);
						close(i->fd);
						this->IPCHandleClosed(i->fd);
						this->IPCHandleClientListUpdate();
						i->fd = 0;
					}
				} else {
					fmt::print("[I] dropping client fd {} due to poll event {}\n", i->fd, i->revents);
					close(i->fd);
					this->IPCHandleClosed(i->fd);
					this->IPCHandleClientListUpdate();
					i->fd = 0;
				}
			}
		}
		pfds.erase(std::remove_if(pfds.begin(), pfds.end(), [](const pollfd& pfd) { return pfd.fd == 0; }), pfds.end());
		if (pfds.size() == 1) {
			// only the incoming IPC socket remains
			this->IPCHandleNoMoreClients();
		}
	}

	// between us closing our last FD and IPCStop() possibly being called, there might have been
	// new connections, so we need to handle those by sending eof and closing them
	for (auto i = pfds.begin() + 1; i != pfds.end(); i += 1) {
		shutdown(i->fd, SHUT_RDWR);
		close(i->fd);
	}
	close(pfds[0].fd);
}

void Browser::Client::IPCStop() {
	shutdown(this->ipc_fd, SHUT_RDWR);
	this->ipc_thread.join();
	close(this->ipc_fd);
}

#endif
