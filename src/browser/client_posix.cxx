#if defined(BOLT_PLUGINS)
#include "client.hxx"

#include <algorithm>
#include <fmt/core.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

void Browser::Client::IPCBind() {
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s/ipc-0", this->runtime_dir.c_str());
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
			int client_fd = accept4(this->ipc_fd, nullptr, nullptr, SOCK_CLOEXEC);
			if (client_fd == -1) {
				fmt::print("[I] IPC thread exiting due to accept error {}\n", errno);
				break;
			}
			pfds.push_back({.fd = client_fd, .events = POLLIN});
			fmt::print("[I] new client fd {}\n", client_fd);
		} else if (pfds[0].revents != 0) {
			fmt::print("[I] IPC thread exiting due to poll event {}\n", pfds[0].revents);
			break;
		}

		// check the rest of the pollfds for incoming data
		for (auto i = pfds.begin() + 1; i != pfds.end(); i += 1) {
			if (i->revents != 0) {
				uint8_t byte;
				bool is_pollin = !!(i->revents & POLLIN);
				size_t read_size;
				if (is_pollin && (read_size = read(i->fd, &byte, 1)) > 0) {
					fmt::print("[I] fd {} received byte {}\n", i->fd, (int)byte);
				} else {
					if (is_pollin) {
						if (read_size == 0) {
							fmt::print("[I] dropping client fd {} due to read eof\n", i->fd);
						} else {
							fmt::print("[I] dropping client fd {} due to read error {}\n", i->fd, errno);
						}
					} else {
						fmt::print("[I] dropping client fd {} due to poll event {}\n", i->fd, i->revents);
					}
					close(i->fd);
					i->fd = 0;
				}
			}
		}
		pfds.erase(std::remove_if(pfds.begin(), pfds.end(), [](const pollfd& pfd) { return pfd.fd == 0; }), pfds.end());
	}

	// there should probably not be any fds left when we reach this point, but for good measure...
	for (auto i = pfds.begin() + 1; i != pfds.end(); i += 1) {
		close(i->fd);
	}
}

void Browser::Client::IPCStop() {
	shutdown(this->ipc_fd, SHUT_RDWR);
	this->ipc_thread.join();
	close(this->ipc_fd);
}

#endif
