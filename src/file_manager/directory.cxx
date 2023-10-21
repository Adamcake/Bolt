#include "directory.hxx"

#include "include/cef_parser.h"

#include <fmt/core.h>
#include <fstream>

#if defined(__linux__)
#include <thread>
#include <sys/inotify.h>

void Watch(std::filesystem::path path, CefRefPtr<FileManager::FileManager> file_manager, int fd) {
	const struct inotify_event *event;
	char buf[256] __attribute__((aligned(__alignof__(struct inotify_event))));
	int wd = inotify_add_watch(fd, path.c_str(), IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE | IN_MOVE_SELF | IN_DELETE_SELF);
	bool run = true;
	while (run) {
		bool did_callback = false;
		size_t len = read(fd, buf, sizeof(buf));
		if (len <= 0) {
			fmt::print("[B] file monitor stopping due to inotify being interrupted\n");
			break;
		}
		for (const char* ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
			event = reinterpret_cast<const inotify_event*>(ptr);
			switch (event->mask) {
				case IN_MOVE_SELF:
				case IN_DELETE_SELF:
					fmt::print("[B] file monitor stopping due to monitored directory being moved or deleted\n");
					run = false;
					ptr = buf + len; // so we won't process any more events
					break;
				default:
					if (!did_callback) {
						did_callback = true;
						file_manager->OnFileChange();
					}
					break;
			}
		}
		
	}
}
#endif

FileManager::Directory::Directory(std::filesystem::path path): path(path) {
#if defined(__linux__)
	this->inotify_fd = inotify_init1(IN_CLOEXEC);
	this->inotify_thread = std::thread(Watch, path, this, this->inotify_fd);
#endif
}

FileManager::File FileManager::Directory::get(std::string_view uri) const {
	std::filesystem::path path = std::string(this->path) + std::string(uri);
	std::ifstream file(path, std::ios::in | std::ios::binary);
    if (file.fail()) return File { .contents = nullptr, .size = 0 };
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    char* buffer = new char[size];
    if (!file.read(buffer, size)) {
		delete[] buffer;
		return File { .contents = nullptr, .size = 0 };
	}
	return File {
		.contents = reinterpret_cast<unsigned char*>(buffer),
		.size = size,
		.mime_type = CefGetMimeType(path.string())
	};
}

void FileManager::Directory::free(File file) const {
	delete[] file.contents;
}

void FileManager::Directory::StopFileManager() {
#if defined(__linux__)
	close(this->inotify_fd);
	this->inotify_thread.join();
#endif
}
