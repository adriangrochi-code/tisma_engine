// The seeder's HTTPS download (rule 8 octies): see shared_fetch.hpp. Own
// design; SHA-256 from Mbed TLS. See PROVENANCE.md.
#include "library/shared_fetch.hpp"

#include "net/http.hpp"

#include <mbedtls/sha256.h>
#include <nlohmann/json.hpp>

#include <array>
#include <fstream>

namespace tisma::library {
namespace {

std::string hex(const std::uint8_t* b, std::size_t n) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < n; ++i)
        out += digits[b[i] >> 4], out += digits[b[i] & 15];
    return out;
}

std::array<std::uint8_t, 32> sha256(const std::string& data) {
    std::array<std::uint8_t, 32> h{};
    mbedtls_sha256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), h.data(), 0);
    return h;
}

FetchResult failed(const std::string& code, const std::string& error) { return {false, code, error}; }

FetchResult from_status(const net::Response& r, const std::string& what) {
    if (r.status == 401)
        return failed("login", what + ": the session or the ticket is not valid");
    if (r.status == 404)
        return failed("not_available", what + ": the seeder does not have it");
    if (r.status == 403 || r.status == 451)
        return failed("blocked", what + ": blocked");
    if (r.status == 429)
        return failed("busy", what + ": too many downloads this hour");
    return failed("failed", what + ": " + (r.error.empty() ? "HTTP " + std::to_string(r.status) : r.error));
}

} // namespace

FetchResult fetch_shared_file(const std::string& server, const std::string& session, const SharedModel& model,
                              const SharedFile& file, const std::filesystem::path& out,
                              const std::function<void(double)>& progress, const std::atomic<bool>* cancel) {
    if (session.empty())
        return failed("login", "no session");
    const auto root = model.piece_roots.find(file.sha256);
    if (root == model.piece_roots.end())
        return failed("bad", "the index has no piece root for " + file.sha256);
    // The ticket (rule 8 sexies), with the account's session.
    net::Request t;
    t.method = "POST";
    t.url = server + "/v1/library/relay-ticket";
    t.headers = {{"Authorization", "Bearer " + session}, {"Content-Type", "application/json"}};
    t.body = nlohmann::json{{"sha256", file.sha256}, {"model", model.id}}.dump();
    t.max_bytes = 64 * 1024;
    const net::Response tr = net::perform(t);
    if (!tr.ok())
        return from_status(tr, "ticket");
    std::string ticket;
    try {
        ticket = nlohmann::json::parse(tr.body).value("ticket", std::string());
    } catch (const std::exception&) {
    }
    if (ticket.empty())
        return failed("failed", "ticket: no ticket in the answer");
    const auto get = [&](const std::string& path, std::size_t most) {
        net::Request q;
        q.url = server + "/v1/seed/" + file.sha256 + path;
        q.headers = {{"Authorization", "Bearer " + ticket}};
        q.max_bytes = most;
        q.timeout_s = 120;
        return net::perform(q);
    };
    // The list of pieces, checked against the signed root.
    const net::Response lr = get("/pieces", 4u << 20);
    if (!lr.ok())
        return from_status(lr, "pieces");
    std::vector<std::string> hashes;
    long long size = -1;
    try {
        const nlohmann::json j = nlohmann::json::parse(lr.body);
        size = j.value("size", -1LL);
        for (const auto& h : j.at("hashes"))
            hashes.push_back(h.get<std::string>());
    } catch (const std::exception&) {
        return failed("bad", "pieces: not a list of hashes");
    }
    constexpr long long kPiece = 1048576;
    if (size != file.size || hashes.empty() || (long long)hashes.size() != (size + kPiece - 1) / kPiece)
        return failed("bad", "pieces: the size or the count does not match the index");
    std::string raw;
    for (const std::string& h : hashes) {
        if (h.size() != 64)
            return failed("bad", "pieces: a hash of the wrong length");
        for (std::size_t i = 0; i < 64; i += 2)
            raw += char(std::stoi(h.substr(i, 2), nullptr, 16));
    }
    const auto r = sha256(raw);
    if (hex(r.data(), r.size()) != root->second)
        return failed("bad", "pieces: the list does not match the signed piece root");
    // The pieces, each checked; the file only when all match.
    const std::filesystem::path partial = out.string() + ".part";
    {
        std::ofstream f(partial, std::ios::binary | std::ios::trunc);
        if (!f)
            return failed("failed", "cannot write " + partial.string());
        for (std::size_t n = 0; n < hashes.size(); ++n) {
            if (cancel && *cancel)
                return failed("failed", "cancelled");
            const net::Response pr = get("/piece/" + std::to_string(n), std::size_t(kPiece));
            if (!pr.ok()) {
                f.close();
                std::filesystem::remove(partial);
                return from_status(pr, "piece " + std::to_string(n));
            }
            const auto h = sha256(pr.body);
            if (hex(h.data(), h.size()) != hashes[n]) {
                f.close();
                std::filesystem::remove(partial);
                return failed("bad", "piece " + std::to_string(n) + " does not match its hash");
            }
            f.write(pr.body.data(), std::streamsize(pr.body.size()));
            if (progress)
                progress(double(n + 1) / double(hashes.size()));
        }
    }
    std::ifstream in(partial, std::ios::binary);
    const std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    const auto whole = sha256(all);
    std::error_code ec;
    if (hex(whole.data(), whole.size()) != file.sha256) {
        std::filesystem::remove(partial, ec);
        return failed("bad", "the file does not match its SHA-256");
    }
    std::filesystem::rename(partial, out, ec);
    if (ec)
        return failed("failed", "cannot keep the file: " + ec.message());
    return {true, "", ""};
}

} // namespace tisma::library
