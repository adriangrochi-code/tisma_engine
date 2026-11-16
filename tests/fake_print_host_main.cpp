// Runs a program against a fake print host (tests/fake_print_hosts.hpp), for
// the interface's tests of the Device page from end to end:
//   tisma_fake_print_host <octoprint|moonraker|prusalink|duet|repetier> <program> [args...]
// Each "{url}" among the arguments becomes the fake's address. After the
// program ends, one line says what the fake saw («fake host: started=1
// name=...»); the exit code is the program's. Own design, as mock_cloud_main.
#include "cad/process.hpp"
#include "fake_print_hosts.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

template <class Fake>
int run(Fake& fake, int argc, char** argv, std::string (*saw)(const Fake&)) {
    // A script's «{url}» too, which is not an argument.
#ifdef _WIN32
    _putenv_s("TISMA_TEST_HOST_URL", fake.url().c_str());
#else
    setenv("TISMA_TEST_HOST_URL", fake.url().c_str(), 1);
#endif
    std::vector<std::string> args;
    for (int i = 3; i < argc; ++i) {
        std::string a = argv[i];
        for (std::size_t at = a.find("{url}"); at != std::string::npos; at = a.find("{url}"))
            a.replace(at, 5, fake.url());
        args.push_back(a);
    }
    const tisma::cad::ProcessResult r = tisma::cad::run_process(argv[2], args, 600.0);
    std::fputs(r.out.c_str(), stdout);
    std::fputs(r.err.c_str(), stderr);
    std::printf("fake host: %s\n", saw(fake).c_str());
    if (!r.started || r.timed_out) {
        std::fprintf(stderr, "tisma_fake_print_host: %s\n", r.error.c_str());
        return 1;
    }
    return r.status;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: tisma_fake_print_host <kind> <program> [args...]\n");
        return 2;
    }
    const std::string kind = argv[1];
    if (kind == "octoprint" || kind == "astrobox") { // AstroBox answers OctoPrint's calls (devices.md, rule 24)
        FakeOctoPrint fake;
        return run<FakeOctoPrint>(fake, argc, argv, [](const FakeOctoPrint& f) {
            return "started=" + std::string(f.started ? "1" : "0") + " name=" + f.uploaded_name;
        });
    }
    if (kind == "moonraker") {
        FakeMoonraker fake;
        return run<FakeMoonraker>(fake, argc, argv, [](const FakeMoonraker& f) {
            return "started=" + std::string(f.started ? "1" : "0") + " name=" + f.name;
        });
    }
    if (kind == "prusalink") {
        FakePrusaLink fake;
        return run<FakePrusaLink>(fake, argc, argv, [](const FakePrusaLink& f) {
            return "started=" + std::string(f.started ? "1" : "0") + " name=" + f.name;
        });
    }
    if (kind == "duet") {
        FakeDuet fake;
        return run<FakeDuet>(fake, argc, argv, [](const FakeDuet& f) {
            bool started = false;
            for (const std::string& g : f.codes)
                started = started || g.rfind("M32", 0) == 0;
            return "started=" + std::string(started ? "1" : "0") + " name=" + f.name;
        });
    }
    if (kind == "repetier") {
        FakeRepetier fake;
        return run<FakeRepetier>(fake, argc, argv, [](const FakeRepetier& f) {
            return "started=" + std::string(f.job != "none" ? "1" : "0") + " name=" + f.uploaded_name;
        });
    }
    std::fprintf(stderr, "tisma_fake_print_host: unknown kind %s\n", kind.c_str());
    return 2;
}
