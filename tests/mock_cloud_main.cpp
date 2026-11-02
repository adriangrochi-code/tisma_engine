// Runs a program against a mock Tisma Cloud server (tests/mock_cloud.hpp):
//   tisma_mock_cloud <program> [args...]
// Each "{url}" among the arguments becomes the server's address. The exit
// code is the program's; what it printed is printed after it ends.

#include "cad/process.hpp"
#include "mock_cloud.hpp"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: tisma_mock_cloud <program> [args...]\n");
        return 2;
    }
    tisma::test::MockCloud mock;
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        for (std::size_t at = a.find("{url}"); at != std::string::npos; at = a.find("{url}"))
            a.replace(at, 5, mock.url());
        args.push_back(a);
    }
    const tisma::cad::ProcessResult r = tisma::cad::run_process(argv[1], args, 600.0);
    std::fputs(r.out.c_str(), stdout);
    std::fputs(r.err.c_str(), stderr);
    if (!r.started || r.timed_out) {
        std::fprintf(stderr, "tisma_mock_cloud: %s\n", r.error.c_str());
        return 1;
    }
    return r.status;
}
