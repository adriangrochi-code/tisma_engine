// tisma_unxz IN.xz OUT: unpacks an .xz file with the engine's own reader, so
// the tests that need the Benchy run where there is no xz program (Windows).
#include "catalog/xz.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: tisma_unxz IN.xz OUT\n");
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string out, error;
    if (data.empty() || !tisma::xz_decompress(data, out, error)) {
        std::fprintf(stderr, "error: cannot unpack %s: %s\n", argv[1], error.c_str());
        return 1;
    }
    std::ofstream(argv[2], std::ios::binary).write(out.data(), std::streamsize(out.size()));
    return 0;
}
