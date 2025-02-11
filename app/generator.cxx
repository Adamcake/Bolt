#include "../src/mime.hxx"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string.h>
#include <vector>

struct FileTuple {
    std::string name;
    size_t size;
};

bool output_file_contents_var(std::filesystem::path& path, size_t index, size_t& size) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (file.fail()) return false;
    file.seekg(0, std::ios::end);
    size = file.tellg();
    file.seekg(0, std::ios::beg);
    char* buffer = new char[size];
    if (file.read(buffer, size)) {
        std::cout << "static constexpr unsigned char _" << index << "[] = {";
        bool first = true;
        for (size_t i = 0; i < size; i += 1) {
            unsigned short num = static_cast<unsigned short>(static_cast<unsigned char>(buffer[i]));
            if (first) {
                first = false;
            } else {
                std::cout << ",";
            }
            std::cout << num;
        }
        std::cout << "};" << std::endl;
        delete[] buffer;
        return true;
    } else {
        delete[] buffer;
        return false;
    }
}

int main(int argc, const char** argv) {
    if (argc < 3) return 1;
    std::cout << "#include \"include/cef_parser.h\"" << std::endl;
    std::cout << "#include \"" << argv[1] << "/src/file_manager/launcher.hxx\"" << std::endl;
    std::vector<FileTuple> filenames;
    size_t i;
    for (size_t arg = 3; arg < argc; arg += 1) {
        std::filesystem::path path = argv[arg];
        if (!output_file_contents_var(path, filenames.size(), i)) {
            return 2;
        }
        filenames.push_back(FileTuple { .name = path.string(), .size = i});
    }
    std::cout << "FileManager::Launcher::Launcher(): files({";
    i = 0;
    size_t argv2_len = strlen(argv[2]);
    for (FileTuple& file: filenames) {
        if (!file.name.starts_with(argv[2])) return 3;
        const char* uri_name = file.name.c_str() + argv2_len;
        if (i != 0) {
            std::cout << ",";
        }
        std::filesystem::path path(uri_name);
        const char* mime_type = GetMimeType(path);
        if (!mime_type) {
            std::cerr << "ERROR: unknown file extension \"" << path.extension().string() << "\" (" << uri_name << "), please add it to mime.cxx and rebuild" << std::endl;
            return 3;
        }
        std::cout << "{\"" << uri_name << "\", File {.contents = _" << i << ", .size = " << file.size << ", .mime_type = \"" << mime_type << "\"}}";
        i += 1;
    }
    std::cout << "}){}" << std::endl;
}
