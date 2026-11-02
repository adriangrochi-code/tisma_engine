// A stand-in for libsecret's secret-tool in the tests (store, lookup, clear;
// the attributes after the command name one secret): it keeps them in the
// file named by TISMA_FAKE_SECRETS, one "attributes<TAB>secret" a line.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>

int main(int argc, char** argv) {
    const char* file = std::getenv("TISMA_FAKE_SECRETS");
    if (argc < 2 || !file)
        return 2;
    const std::string command = argv[1];
    std::string key;
    for (int i = 2; i < argc; ++i)
        if (std::string(argv[i]).rfind("--label=", 0) != 0)
            key += std::string(argv[i]) + " ";
    std::map<std::string, std::string> secrets;
    {
        std::ifstream in(file);
        for (std::string line; std::getline(in, line);)
            if (const std::size_t tab = line.find('\t'); tab != std::string::npos)
                secrets[line.substr(0, tab)] = line.substr(tab + 1);
    }
    if (command == "lookup") {
        const auto it = secrets.find(key);
        if (it == secrets.end())
            return 1;
        std::cout << it->second;
        return 0;
    }
    if (command == "store") {
        const std::string secret((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
        if (secret.empty())
            return 1;
        secrets[key] = secret;
    } else if (command == "clear") {
        secrets.erase(key);
    } else {
        return 2;
    }
    std::ofstream out(file, std::ios::trunc);
    for (const auto& [k, v] : secrets)
        out << k << '\t' << v << '\n';
    return 0;
}
