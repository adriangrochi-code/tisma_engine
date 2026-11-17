#pragma once
// The shared library's signed files (p2p_library.md, rules 3 bis to 3
// quinquies and 4): the library key with its certificate from the project's
// key, the block list and the index, each checked before use, and the search
// in the index (rule 3 ter bis: words anywhere in the name, tags, author or
// licence, without telling accents or capitals apart). The same rules as the
// phone apps. Own design; see PROVENANCE.md.
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <map>
#include <vector>

namespace tisma::library {

// Why a file was refused, in English (for the log) and in Spanish.
class SharedError : public std::runtime_error {
public:
    SharedError(const std::string& en, std::string es) : std::runtime_error(en), es_(std::move(es)) {}
    const std::string& spanish() const { return es_; }

private:
    std::string es_;
};

// GET /v1/library/key: {"public_key", "certificate"} (hex or base64). The
// key is used only when the project's key signed «tisma-library-key/1\n» and
// its 32 bytes. Its 32 bytes, or SharedError.
std::vector<std::uint8_t> read_library_key(const std::string& json, const std::vector<std::uint8_t>& project_key);

struct BlockList {
    long long sequence = 0;
    std::string generated, expires;          // as the server wrote them (RFC 3339)
    std::map<std::string, std::string> files; // sha256 (lower case) and the reason
    std::set<std::string> accounts, peers, revoked_keys;
    bool blocks_file(const std::string& sha256) const;
    bool blocks_account(const std::string& account) const { return accounts.count(account) != 0; }
};

// The list (rule 3 quater), signed by `library_key`; one with a lower
// sequence than `last_sequence` is refused (it cannot be replayed), and so is
// one that names the signing key as revoked.
BlockList read_block_list(const std::string& json, const std::vector<std::uint8_t>& signature,
                          const std::vector<std::uint8_t>& library_key, long long last_sequence = -1);

struct SharedFile {
    std::string path, role, sha256; // role: project, profile, analysis or photo
    long long size = 0;
};

struct SharedModel {
    std::string id, name, description, license, author, author_account, thumbnail, published;
    int revision = 1;
    std::vector<std::string> tags;
    long long size = 0;
    std::vector<SharedFile> files;
    // swarm.piece_roots: each file's SHA-256 to the SHA-256 of its pieces'
    // hashes (rule 3 ter), what its pieces are checked against.
    std::map<std::string, std::string> piece_roots;
    bool checked = false;
    // Rule 3 sexies: never G-code; a model that lists one is not offered.
    bool has_gcode() const;
    // The project to open on the plate: the first file with role «project».
    const SharedFile* project() const;
};

struct SharedIndex {
    long long sequence = 0;
    std::string generated;
    std::vector<SharedModel> models;
    // Rule 4 and the search: the models not blocked (by file or account) and
    // without G-code whose name, tags, author or licence hold every word of
    // `query` (accents and capitals aside). Their places in `models`.
    std::vector<std::size_t> search(const std::string& query, const BlockList* list) const;
};

// The largest index the program takes (rule 3 ter bis): bigger, SharedError
// «La biblioteca es demasiado grande para esta versión; actualizá la app».
inline constexpr std::size_t kMaxIndexBytes = std::size_t(256) << 20;

// The index (rule 3 ter), signed by `library_key`; a lower sequence than
// `last_sequence` is refused.
SharedIndex read_index(const std::string& json, const std::vector<std::uint8_t>& signature,
                       const std::vector<std::uint8_t>& library_key, long long last_sequence = -1);

// Lower case without the accents of Spanish and the other Latin letters
// (á→a, ñ→n, ü→u…), for the search.
std::string fold(const std::string& text);

} // namespace tisma::library
