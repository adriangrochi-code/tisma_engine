// Installing the profile pack from a folder of downloaded files
// (profile_pack.md, rules 4, 5, 7, 8, 20 and 21). Own design.
#include "catalog/pack.hpp"
#include "catalog/xz.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

namespace tisma {

namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t kMaxEntry = 50ull << 20;   // rule 5: a file over 50 MB discards the archive
constexpr std::size_t kMaxArchive = std::size_t(1) << 30; // what one archive may unpack to

bool read_all(const fs::path& path, std::string& data) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream s;
    s << in.rdbuf();
    data = s.str();
    return bool(in) || in.eof();
}

bool write_all(const fs::path& path, std::string_view data) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(data.data(), std::streamsize(data.size()));
    return bool(out);
}

// Whether a downloaded file is the one the index names (rule 4).
std::string check_file(const std::string& data, const PackFile& expected) {
    if (expected.size != 0 && data.size() != expected.size)
        return "size " + std::to_string(data.size()) + " instead of " + std::to_string(expected.size);
    if (!expected.sha256.empty()) {
        std::string want = expected.sha256;
        std::transform(want.begin(), want.end(), want.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (sha256_hex(data) != want)
            return "SHA-256 does not match the index";
    }
    return {};
}

// Unpacks one archive in memory and checks every entry before anything is
// written (rule 5): all paths safe and under `prefix`, no links, none over
// 50 MB. Empty on success, else why the whole archive is refused.
std::string unpack(const std::string& packed, const std::string& prefix, std::vector<TarEntry>& files) {
    std::string tar, error;
    if (!xz_decompress(packed, tar, error, kMaxArchive))
        return error;
    std::vector<std::string> skipped;
    if (!tar_read(tar, files, skipped, error))
        return error;
    if (!skipped.empty())
        return "a link or special file inside: " + skipped.front();
    for (const TarEntry& f : files) {
        if (!safe_pack_path(f.path) || !f.path.starts_with(prefix))
            return "an unsafe path inside: " + f.path;
        if (f.data.size() > kMaxEntry)
            return "a file over 50 MB inside: " + f.path;
    }
    return {};
}

// The archives installed and their SHA-256 (installed.json), so an update
// downloads again the ones that were refused (rule 20).
std::map<std::string, std::string> read_installed(const fs::path& pack_dir) {
    std::map<std::string, std::string> out;
    std::string text;
    if (!read_all(pack_dir / "installed.json", text))
        return out;
    try {
        const nlohmann::json j = nlohmann::json::parse(text);
        if (j.contains("archives") && j.at("archives").is_object())
            for (const auto& [name, sha] : j.at("archives").items())
                if (sha.is_string())
                    out[name] = sha.get<std::string>();
    } catch (const nlohmann::json::exception&) {
    }
    return out;
}

} // namespace

PackIndex pack_installed(const fs::path& pack_dir) {
    std::string text;
    if (!read_all(pack_dir / "index.json", text))
        throw PackError("no profile pack installed in " + pack_dir.string());
    PackIndex index = parse_pack_index(text);
    const auto archives = read_installed(pack_dir);
    for (PackVendor& v : index.vendors) {
        const std::string name = v.archive.name.empty() ? v.id + ".tar.xz" : v.archive.name;
        const auto it = archives.find(name);
        v.archive.sha256 = it == archives.end() ? std::string() : it->second;
    }
    return index;
}

PackInstallReport pack_install(const fs::path& pack_dir, const fs::path& staging,
                               const std::function<void(const std::string&)>& progress) {
    const auto say = [&](const std::string& line) {
        if (progress)
            progress(line);
    };
    std::string index_text;
    if (!read_all(staging / "index.json", index_text))
        throw PackError("no index.json in " + staging.string());
    const PackIndex index = parse_pack_index(index_text); // PF1: a newer version stops here, nothing changed

    PackInstallReport report;
    report.pack_version = index.pack_version;
    const fs::path fresh = fs::path(pack_dir.string() + ".new");
    const fs::path old = fs::path(pack_dir.string() + ".old");
    std::error_code ec;
    fs::remove_all(fresh, ec);
    fs::create_directories(fresh, ec);
    if (ec)
        throw PackError("cannot write " + fresh.string() + ": " + ec.message());
    const bool had = fs::exists(pack_dir / "index.json");
    const auto was = had ? read_installed(pack_dir) : std::map<std::string, std::string>();
    std::map<std::string, std::string> now;

    const auto refuse = [&](const std::string& name, const std::string& why) {
        report.discarded.push_back(name);
        report.warnings.push_back(name + ": " + why);
        say("discarded " + name + ": " + why);
    };
    // Keeps what the earlier install had for `name` (its files under `sub`).
    const auto keep = [&](const std::string& name, const fs::path& sub) {
        if (!had || !fs::exists(pack_dir / sub))
            return;
        fs::create_directories((fresh / sub).parent_path(), ec);
        fs::copy(pack_dir / sub, fresh / sub, fs::copy_options::recursive, ec);
        if (ec) {
            report.warnings.push_back(name + ": the earlier files could not be kept: " + ec.message());
            return;
        }
        report.kept.push_back(name);
        const auto it = was.find(name);
        if (it != was.end())
            now[name] = it->second;
    };

    // compatible.json
    {
        const std::string name = index.compatible.name.empty() ? "compatible.json" : index.compatible.name;
        std::string data;
        if (read_all(staging / name, data)) {
            std::string why = check_file(data, index.compatible);
            if (why.empty()) {
                try {
                    (void)parse_pack_compatible(data);
                } catch (const std::exception& e) {
                    why = std::string("not valid: ") + e.what();
                }
            }
            if (why.empty() && write_all(fresh / "compatible.json", data)) {
                report.installed.push_back(name);
                now[name] = sha256_hex(data);
                say("installed " + name);
            } else {
                refuse(name, why.empty() ? "cannot be written" : why);
                keep(name, "compatible.json");
            }
        } else {
            keep(name, "compatible.json");
        }
    }

    // The archives: community, knowledge and each vendor.
    struct Archive {
        std::string name;
        PackFile expected;
        std::string prefix;
    };
    std::vector<Archive> archives;
    archives.push_back({index.community.name.empty() ? "community.tar.xz" : index.community.name, index.community, "community/"});
    archives.push_back({"knowledge.tar.xz", PackFile{}, "knowledge/"}); // rule 21: not in the index
    for (const PackVendor& v : index.vendors)
        archives.push_back({v.archive.name.empty() ? v.id + ".tar.xz" : v.archive.name, v.archive, "vendors/" + v.id + "/"});
    for (const Archive& a : archives) {
        const fs::path sub = fs::path(a.prefix.substr(0, a.prefix.size() - 1));
        std::string data;
        if (!read_all(staging / a.name, data)) {
            keep(a.name, sub); // not downloaded this time: unchanged or not chosen
            continue;
        }
        std::string why = check_file(data, a.expected);
        std::vector<TarEntry> files;
        if (why.empty())
            why = unpack(data, a.prefix, files);
        if (why.empty()) {
            for (const TarEntry& f : files)
                if (!write_all(fresh / fs::path(f.path), f.data)) {
                    why = "cannot write " + f.path;
                    break;
                }
        }
        if (!why.empty()) {
            fs::remove_all(fresh / sub, ec);
            refuse(a.name, why);
            keep(a.name, sub);
            continue;
        }
        report.installed.push_back(a.name);
        now[a.name] = sha256_hex(data);
        say("installed " + a.name + " (" + std::to_string(files.size()) + " files)");
    }

    // The index as published, and what was really installed.
    nlohmann::json state = {{"pack_version", index.pack_version}, {"archives", nlohmann::json::object()}};
    for (const auto& [name, sha] : now)
        state["archives"][name] = sha;
    if (!write_all(fresh / "index.json", index_text) || !write_all(fresh / "installed.json", state.dump(1))) {
        fs::remove_all(fresh, ec);
        throw PackError("cannot write " + fresh.string());
    }

    // Rule 7: one switch, so a cut install leaves the earlier pack whole.
    fs::remove_all(old, ec);
    if (fs::exists(pack_dir)) {
        fs::rename(pack_dir, old, ec);
        if (ec) {
            fs::remove_all(fresh, ec);
            throw PackError("cannot replace " + pack_dir.string() + ": " + ec.message());
        }
    }
    fs::rename(fresh, pack_dir, ec);
    if (ec) {
        std::error_code back;
        fs::rename(old, pack_dir, back);
        throw PackError("cannot replace " + pack_dir.string() + ": " + ec.message());
    }
    fs::remove_all(old, ec);
    say("pack " + index.pack_version + " ready");
    return report;
}

} // namespace tisma
