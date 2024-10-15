#include "../modules/spng/spng/spng.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>

#define BUF_SIZE (256 * 256 * 4)

#define CHECKERR(E) if (E) { spng_ctx_free(spng); return E; } 
int output(const char* filename, const char* function_name, unsigned int size, uint8_t* rgba) {
    std::ifstream t(filename, std::ios::binary);
    if (t.fail()) return 1;
    std::stringstream ss;
    ss << t.rdbuf();
    std::string ss_str = ss.str();

    spng_ctx* spng = spng_ctx_new(0);
    if (!spng) return 1;

    int error;

    error = spng_set_png_buffer(spng, reinterpret_cast<const unsigned char*>(ss_str.c_str()), ss_str.size());
    CHECKERR(error);
    spng_ihdr ihdr;
    error = spng_get_ihdr(spng, &ihdr);
    CHECKERR(error);
    if (ihdr.width != size || ihdr.height != size) return 1;

    size_t rgba_size;
    error = spng_decoded_image_size(spng, SPNG_FMT_RGBA8, &rgba_size);
    CHECKERR(error);
    if (rgba_size > BUF_SIZE) return 1;
    error = spng_decode_image(spng, rgba, rgba_size, SPNG_FMT_RGBA8, 0);
    CHECKERR(error);
    spng_ctx_free(spng);

    const char* var_name = function_name + 3;
    std::cout << "constexpr unsigned char " << var_name << "[] = {";
    bool first = true;
    for (size_t i = 0; i < rgba_size; i += 1) {
        unsigned short num = static_cast<unsigned short>(rgba[i]);
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
    uint8_t buf[BUF_SIZE];
    if (argc < 3) return 1;
    int err;
    spng_ctx* spng = spng_ctx_new(0);
    if (!spng) return 1;
    std::cout << std::hex << "#include \"" << argv[1] << "/src/browser/client.hxx\"" << std::endl;
    size_t icon_path_len = strlen(argv[2]);
    char* filename_buf = new char[icon_path_len + strlen("/256.png") + 1]; // longest filename, including delimiter
    memcpy(filename_buf, argv[2], icon_path_len);
    strcpy(filename_buf + icon_path_len, "/16.png");
    err = output(filename_buf, "GetIcon16", 16, buf);
    if (!err) {
        strcpy(filename_buf + icon_path_len, "/64.png");
        err = output(filename_buf, "GetIcon64", 64, buf);
    }
    delete[] filename_buf;
    return err;
}
