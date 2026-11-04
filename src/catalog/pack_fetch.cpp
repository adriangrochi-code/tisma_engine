#include "catalog/pack_fetch.hpp"

#include "net/http.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <system_error>

namespace tisma {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kUnknownLimit = std::size_t(200) << 20; // a file the index gives no size for
constexpr int kRedirects = 5;

// Where a Location header points, from the URL that answered with it.
std::string resolve(const std::string& from, const std::string& location) {
    if (location.starts_with("http://") || location.starts_with("https://"))
        return location;
    net::Url u;
    if (!net::parse_url(from, u))
        return {};
    const std::string origin = std::string(u.https ? "https://" : "http://") + u.host + ":" + std::to_string(u.port);
    if (location.starts_with("/"))
        return origin + location;
    const std::string dir = u.path.substr(0, u.path.find('?'));
    return origin + dir.substr(0, dir.rfind('/') + 1) + location;
}

// Downloads one URL, following redirects; empty on success, else why.
std::string download(const std::string& first_url, std::uint64_t size, std::string& body) {
    std::string url = first_url;
    for (int hop = 0; hop <= kRedirects; ++hop) {
        net::Request req;
        req.url = url;
        req.timeout_s = 300;
        req.max_bytes = size ? std::size_t(size) : kUnknownLimit;
        const net::Response res = net::perform(req);
        if (!res.error.empty())
            return res.error;
        if (res.status >= 300 && res.status < 400 && !res.location.empty()) {
            const std::string next = resolve(url, res.location);
            if (next.empty() || (url.starts_with("https://") && !next.starts_with("https://")))
                return "redirected to an address that is not allowed";
            url = next;
            continue;
        }
        if (!res.ok())
            return "the server answered " + std::to_string(res.status);
        if (size != 0 && res.body.size() != size)
            return "size " + std::to_string(res.body.size()) + " instead of " + std::to_string(size);
        body = res.body;
        return {};
    }
    return "too many redirects";
}

} // namespace

std::vector<std::string> pack_fetch(const std::vector<PackDownload>& plan, const fs::path& staging,
                                    const std::function<void(const std::string&)>& progress) {
    std::vector<std::string> errors;
    std::error_code ec;
    fs::create_directories(staging, ec);
    for (std::size_t i = 0; i < plan.size(); ++i) {
        const PackDownload& d = plan[i];
        if (!safe_pack_path("community/" + d.name) || d.name.find('/') != std::string::npos) {
            errors.push_back(d.name + ": not a plain file name");
            continue;
        }
        if (progress)
            progress("downloading " + d.name + " (" + std::to_string(i + 1) + " of " + std::to_string(plan.size()) + ")");
        std::string body;
        const std::string why = download(d.url, d.size, body);
        if (!why.empty()) {
            errors.push_back(d.name + ": " + why);
            continue;
        }
        std::ofstream out(staging / d.name, std::ios::binary | std::ios::trunc);
        out.write(body.data(), std::streamsize(body.size()));
        if (!out)
            errors.push_back(d.name + ": cannot be written");
    }
    return errors;
}

PackInstallReport pack_update(const fs::path& pack_dir, const std::string& base_url, const PackChoice& choice,
                              const std::function<void(const std::string&)>& progress) {
    const fs::path staging = fs::path(pack_dir.string() + ".download");
    std::error_code ec;
    fs::remove_all(staging, ec);
    // The index first (rule 1).
    std::vector<std::string> errors = pack_fetch(pack_plan(nullptr, base_url, {}), staging, progress);
    if (!errors.empty()) {
        fs::remove_all(staging, ec);
        throw PackError("the profile pack cannot be downloaded: " + errors.front());
    }
    std::string text;
    {
        std::ifstream in(staging / "index.json", std::ios::binary);
        text.assign(std::istreambuf_iterator<char>(in), {});
    }
    PackIndex index;
    try {
        index = parse_pack_index(text);
    } catch (...) {
        fs::remove_all(staging, ec);
        throw;
    }
    std::optional<PackIndex> installed;
    if (fs::exists(pack_dir / "index.json")) {
        try {
            installed = pack_installed(pack_dir);
        } catch (const PackError&) {
        }
    }
    // Which vendors: all, or the chosen ones and the installed ones.
    std::set<std::string> wanted(choice.chosen.begin(), choice.chosen.end());
    if (installed)
        for (const PackVendor& v : installed->vendors)
            if (!v.archive.sha256.empty())
                wanted.insert(v.id);
    std::vector<PackDownload> plan = pack_plan(&index, base_url, {}, installed ? &*installed : nullptr);
    if (!choice.all) {
        std::set<std::string> archives;
        for (const PackVendor& v : index.vendors)
            if (!wanted.count(v.id))
                archives.insert(v.archive.name.empty() ? v.id + ".tar.xz" : v.archive.name);
        plan.erase(std::remove_if(plan.begin(), plan.end(), [&](const PackDownload& d) { return archives.count(d.name) > 0; }),
                   plan.end());
    }
    errors = pack_fetch(plan, staging, progress);
    PackInstallReport report;
    try {
        report = pack_install(pack_dir, staging, progress);
    } catch (...) {
        fs::remove_all(staging, ec);
        throw;
    }
    for (const std::string& e : errors)
        report.warnings.push_back(e); // not downloaded: the earlier one stays, the next update tries again
    fs::remove_all(staging, ec);
    return report;
}

std::string pack_newer(const fs::path& pack_dir, const std::string& base_url) {
    std::string body;
    const auto plan = pack_plan(nullptr, base_url, {});
    std::string url = plan.front().url;
    if (!download(url, 0, body).empty())
        return {};
    try {
        const PackIndex published = parse_pack_index(body);
        const PackIndex installed = pack_installed(pack_dir);
        return published.pack_version == installed.pack_version ? std::string() : published.pack_version;
    } catch (const PackError&) {
        return {};
    }
}

} // namespace tisma
