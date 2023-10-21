#include "directory.hxx"

#include "include/cef_parser.h"

#include <fstream>
#include <iostream>

FileManager::Directory::Directory(std::filesystem::path path): path(path) {}

FileManager::File FileManager::Directory::get(std::string_view uri) const {
	std::filesystem::path path = std::string(this->path) + std::string(uri);
	std::cout << "[B] serving " << path.c_str() << std::endl;
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
