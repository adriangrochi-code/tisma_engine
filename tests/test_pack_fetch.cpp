// Downloading the profile pack (profile_pack.md, rules 1, 19, 20; PK1, PK2,
// PK12) from a fake release server that answers the latest-release links
// with a redirect, as the real host does. Data: tests/data/pack.
#include "catalog/pack_fetch.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <httplib.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;

namespace {

namespace fs = std::filesystem;

const fs::path kPackData = fs::path(TISMA_TEST_DATA_DIR) / "pack";

std::string text_of(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

// /latest/download/<file> redirects to /files/<file>, served from the
// current release folder.
class FakeReleases {
public:
    FakeReleases() {
        server_.Get(R"(/latest/download/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard lock(mutex_);
            ++requests;
            res.status = 302;
            res.set_header("Location", "/files/" + std::string(req.matches[1]));
        });
        server_.Get(R"(/files/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard lock(mutex_);
            const fs::path file = folder_ / std::string(req.matches[1]);
            if (!fs::exists(file)) {
                res.status = 404;
                return;
            }
            res.set_content(text_of(file), "application/octet-stream");
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~FakeReleases() {
        server_.stop();
        thread_.join();
    }
    void serve(const fs::path& folder) {
        std::lock_guard lock(mutex_);
        folder_ = folder;
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_) + "/latest/download/"; }
    int requests = 0;

private:
    httplib::Server server_;
    std::thread thread_;
    std::mutex mutex_;
    fs::path folder_;
    int port_ = 0;
};

fs::path fresh_dir(const char* name) {
    const fs::path dir = fs::temp_directory_path() / name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

} // namespace

TEST_CASE("pack update: first run brings the index and the rest; vendors only when chosen (rules 1, 21, PK1)", "[pack]") {
    FakeReleases releases;
    releases.serve(kPackData / "release1");
    const fs::path pack = fresh_dir("tisma_pack_fetch1") / "pack";
    std::vector<std::string> lines;
    PackInstallReport r = pack_update(pack, releases.url(), {}, [&](const std::string& l) { lines.push_back(l); });
    CHECK(r.pack_version == "2026.11.01-aaaaaaa");
    CHECK(r.warnings.empty());
    CHECK(fs::exists(pack / "index.json"));
    CHECK(fs::exists(pack / "compatible.json"));
    CHECK(fs::exists(pack / "knowledge/glossary.json"));
    CHECK(!fs::exists(pack / "vendors"));
    CHECK(!fs::exists(fs::path(pack.string() + ".download")));
    CHECK(std::any_of(lines.begin(), lines.end(), [](const std::string& l) { return l.starts_with("downloading index.json"); }));
    // Choosing one vendor.
    r = pack_update(pack, releases.url(), PackChoice{false, {"acme"}});
    CHECK(fs::exists(pack / "vendors/acme/models/A1.json"));
    CHECK(!fs::exists(pack / "vendors/bolt"));
    CHECK(pack_newer(pack, releases.url()).empty());
}

TEST_CASE("pack update: a new version brings only the changed vendor (rules 19, 20, PK12)", "[pack]") {
    FakeReleases releases;
    releases.serve(kPackData / "release1");
    const fs::path pack = fresh_dir("tisma_pack_fetch2") / "pack";
    pack_update(pack, releases.url(), PackChoice{true, {}});
    REQUIRE(fs::exists(pack / "vendors/bolt/models/B2.json"));
    // release2 publishes a new index and Acme's archive; Bolt's is unchanged,
    // so the update must not ask for it (the server would answer 404).
    releases.serve(kPackData / "release2");
    CHECK(pack_newer(pack, releases.url()) == "2026.11.04-bbbbbbb");
    std::vector<std::string> lines;
    const PackInstallReport r = pack_update(pack, releases.url(), {}, [&](const std::string& l) { lines.push_back(l); });
    CHECK(r.pack_version == "2026.11.04-bbbbbbb");
    // release2 has index.json.xz: the index comes compressed, and index.json is not asked for.
    CHECK(std::any_of(lines.begin(), lines.end(), [](const std::string& l) { return l.starts_with("downloading index.json.xz"); }));
    CHECK(std::none_of(lines.begin(), lines.end(), [](const std::string& l) { return l.starts_with("downloading index.json ("); }));
    CHECK(std::find(r.installed.begin(), r.installed.end(), "acme.tar.xz") != r.installed.end());
    CHECK(std::find(r.kept.begin(), r.kept.end(), "bolt.tar.xz") != r.kept.end());
    CHECK(text_of(pack / "vendors/acme/models/A1.json").find("Acme One Plus") != std::string::npos);
    CHECK(fs::exists(pack / "vendors/bolt/models/B2.json"));
}

TEST_CASE("pack update: without a connection nothing changes (PK2)", "[pack]") {
    const fs::path pack = fresh_dir("tisma_pack_fetch3") / "pack";
    std::string url;
    {
        FakeReleases releases;
        releases.serve(kPackData / "release1");
        url = releases.url();
        pack_update(pack, url, {});
    } // the server is gone
    const std::string before = text_of(pack / "index.json");
    CHECK_THROWS_AS(pack_update(pack, url, {}), PackError);
    CHECK(text_of(pack / "index.json") == before);
    CHECK(pack_newer(pack, url).empty());
}
