#include "cli/cli.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string_view> args(argv + (argc > 0 ? 1 : 0), argv + argc);
    return tisma::run_cli(args, std::cout, std::cerr);
}
