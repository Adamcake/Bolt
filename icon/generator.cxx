#define LODEPNG_NO_COMPILE_ENCODER
#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS

#include "../modules/lodepng/lodepng.h"

#include <iostream>

unsigned int output(const char* filename, const char* function_name, unsigned int size) {
    std::vector<unsigned char> image;
    unsigned int width, height;
    unsigned int error = lodepng::decode(image, width, height, filename);
    if(error) return error;
    if (width != size || height != size) return 1;
    const char* var_name = function_name + 3;
    std::cout << "constexpr unsigned char " << var_name << "[] = {";
    bool first = true;
    for (unsigned char& b: image) {
        unsigned short num = static_cast<unsigned short>(b);
        if (first) {
            first = false;
            std::cout << "0x" << num;
        } else {
            std::cout << ",0x" << num;
        }
    }
    std::cout << "};" << std::endl << "const unsigned char* Browser::Client::" << function_name << "() const { return " << var_name << "; }" << std::endl;
    return 0;
}

int main(int argc, const char** argv) {
    if (argc < 3) return 1;
    int err;
    std::cout << std::hex << "#include \"" << argv[1] << "/src/browser/client.hxx\"" << std::endl;
    size_t icon_path_len = strlen(argv[2]);
    char* filename_buf = new char[icon_path_len + strlen("/tray.png") + 1]; // longest filename, including delimiter
    memcpy(filename_buf, argv[2], icon_path_len);
    strcpy(filename_buf + icon_path_len, "/16.png");
    err = output(filename_buf, "GetIcon16", 16);
    if (err) goto exit;
    strcpy(filename_buf + icon_path_len, "/32.png");
    err = output(filename_buf, "GetIcon32", 32);
    if (err) goto exit;
    strcpy(filename_buf + icon_path_len, "/64.png");
    err = output(filename_buf, "GetIcon64", 64);
    if (err) goto exit;
    strcpy(filename_buf + icon_path_len, "/256.png");
    err = output(filename_buf, "GetIcon256", 256);
    if (err) goto exit;
    strcpy(filename_buf + icon_path_len, "/tray.png");
    err = output(filename_buf, "GetTrayIcon", 24);
    exit:
    delete[] filename_buf;
    return err;
}
