// The command line through the C API (include/tisma/tisma_engine.h,
// tisma_run_cli): for platforms that cannot start a process. Own design.

#include "cli/cli.hpp"

#include <tisma/tisma_engine.h>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

// A malloc'd NUL-terminated copy, so C callers free it with tisma_free.
void give(const std::string& text, char** to, size_t* size) {
    if (size)
        *size = text.size();
    if (!to)
        return;
    *to = static_cast<char*>(std::malloc(text.size() + 1));
    if (*to) {
        std::memcpy(*to, text.data(), text.size());
        (*to)[text.size()] = '\0';
    } else if (size) {
        *size = 0;
    }
}

} // namespace

extern "C" int tisma_run_cli(int argc, const char* const* argv, char** out, size_t* out_size, char** err,
                             size_t* err_size) {
    std::ostringstream o, e;
    int code = tisma::kExitInternal;
    try {
        std::vector<std::string_view> args;
        for (int i = 1; argv && i < argc; ++i)
            args.emplace_back(argv[i] ? argv[i] : "");
        code = tisma::run_cli(args, o, e);
    } catch (const std::exception& x) {
        e << "error: " << x.what() << "\n";
        code = tisma::kExitInternal;
    } catch (...) {
        e << "error: unknown\n";
        code = tisma::kExitInternal;
    }
    give(o.str(), out, out_size);
    give(e.str(), err, err_size);
    return code;
}

extern "C" void tisma_free(void* buffer) { std::free(buffer); }
