// The shared library's signed files: see shared_index.hpp. Own design from
// p2p_library.md; see PROVENANCE.md.
#include "library/shared_index.hpp"

#include "net/ed25519.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace tisma::library {
namespace {

using nlohmann::json;

std::span<const std::uint8_t> bytes(const std::string& s) {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}

json parse(const std::string& text, const char* en, const char* es) {
    try {
        json j = json::parse(text);
        if (j.is_object())
            return j;
    } catch (const std::exception&) {
    }
    throw SharedError(en, es);
}

std::string str(const json& o, const char* key) {
    const auto it = o.find(key);
    return it != o.end() && it->is_string() ? it->get<std::string>() : std::string();
}

long long number(const json& o, const char* key, long long fallback) {
    const auto it = o.find(key);
    return it != o.end() && it->is_number_integer() ? it->get<long long>() : fallback;
}

std::string lower(std::string s) {
    for (char& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string hex(const std::vector<std::uint8_t>& b) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (const std::uint8_t c : b)
        out += digits[c >> 4], out += digits[c & 15];
    return out;
}

void check_signature(const std::string& text, const std::vector<std::uint8_t>& signature,
                     const std::vector<std::uint8_t>& key, const char* en, const char* es) {
    if (key.size() != 32 || signature.size() != 64 || !net::ed25519_verify(key, bytes(text), signature))
        throw SharedError(en, es);
}

} // namespace

std::vector<std::uint8_t> read_library_key(const std::string& text, const std::vector<std::uint8_t>& project_key) {
    if (project_key.size() != 32)
        throw SharedError("No project key in this build", "Esta versión no trae la clave del proyecto");
    const json o = parse(text, "The library key cannot be read", "No se puede leer la clave de la biblioteca");
    const std::vector<std::uint8_t> key = net::key_bytes(str(o, "public_key"));
    const std::vector<std::uint8_t> cert = net::key_bytes(str(o, "certificate"));
    if (key.size() != 32 || cert.size() != 64)
        throw SharedError("A library key of the wrong size, or without a certificate",
                          "La clave de la biblioteca no tiene el tamaño correcto o no tiene certificado");
    std::string signed_text = "tisma-library-key/1\n";
    signed_text.append(reinterpret_cast<const char*>(key.data()), key.size());
    if (!net::ed25519_verify(project_key, bytes(signed_text), cert))
        throw SharedError("The library key's certificate does not match",
                          "El certificado de la clave de la biblioteca no coincide");
    return key;
}

bool BlockList::blocks_file(const std::string& sha256) const { return files.count(lower(sha256)) != 0; }

BlockList read_block_list(const std::string& text, const std::vector<std::uint8_t>& signature,
                          const std::vector<std::uint8_t>& library_key, long long last_sequence) {
    check_signature(text, signature, library_key, "The block list's signature does not match",
                    "La firma de la lista de bloqueo no coincide");
    const json o = parse(text, "The block list cannot be read", "No se puede leer la lista de bloqueo");
    if (str(o, "format") != "tisma-library-blocklist")
        throw SharedError("Not a block list", "No es una lista de bloqueo");
    if (number(o, "version", 0) > 1)
        throw SharedError("A newer block list format", "La lista de bloqueo es de una versión más nueva");
    BlockList list;
    list.sequence = number(o, "sequence", -1);
    if (list.sequence < 0)
        throw SharedError("The block list has no sequence", "La lista de bloqueo no tiene número de secuencia");
    if (list.sequence < last_sequence)
        throw SharedError("An older block list than the last one", "La lista de bloqueo es más vieja que la última");
    list.generated = str(o, "generated");
    list.expires = str(o, "expires");
    const auto strings = [&](const char* key) {
        std::set<std::string> out;
        if (const auto it = o.find(key); it != o.end() && it->is_array())
            for (const json& e : *it) {
                if (e.is_string())
                    out.insert(e.get<std::string>());
                else if (e.is_object() && !str(e, "id").empty())
                    out.insert(str(e, "id"));
                else if (e.is_object() && !str(e, "key").empty())
                    out.insert(str(e, "key"));
            }
        return out;
    };
    if (const auto it = o.find("files"); it != o.end() && it->is_array())
        for (const json& e : *it) {
            if (e.is_string())
                list.files[lower(e.get<std::string>())] = "";
            else if (e.is_object() && !str(e, "sha256").empty())
                list.files[lower(str(e, "sha256"))] = str(e, "reason");
        }
    list.accounts = strings("accounts");
    list.peers = strings("peers");
    for (const std::string& k : strings("revoked_keys"))
        list.revoked_keys.insert(lower(k));
    if (list.revoked_keys.count(hex(library_key)))
        throw SharedError("The library key that signed it is revoked", "La clave que firmó la lista fue revocada");
    return list;
}

bool SharedModel::has_gcode() const {
    for (const SharedFile& f : files) {
        const std::string ext = lower(f.path.substr(f.path.find_last_of('.') == std::string::npos ? f.path.size()
                                                                                                 : f.path.find_last_of('.') + 1));
        if (f.role == "gcode" || ext == "gcode" || ext == "bgcode" || ext == "gco" || ext == "g")
            return true;
    }
    return false;
}

const SharedFile* SharedModel::project() const {
    for (const SharedFile& f : files)
        if (f.role == "project")
            return &f;
    return nullptr;
}

std::string fold(const std::string& text) {
    // UTF-8 two-byte Latin letters (U+00C0 to U+017F) to their plain letter.
    static const std::map<std::string, char> plain = {
        {"á", 'a'}, {"à", 'a'}, {"â", 'a'}, {"ä", 'a'}, {"ã", 'a'}, {"å", 'a'}, {"Á", 'a'}, {"À", 'a'}, {"Â", 'a'},
        {"Ä", 'a'}, {"Ã", 'a'}, {"é", 'e'}, {"è", 'e'}, {"ê", 'e'}, {"ë", 'e'}, {"É", 'e'}, {"È", 'e'}, {"Ê", 'e'},
        {"Ë", 'e'}, {"í", 'i'}, {"ì", 'i'}, {"î", 'i'}, {"ï", 'i'}, {"Í", 'i'}, {"Ì", 'i'}, {"Î", 'i'}, {"Ï", 'i'},
        {"ó", 'o'}, {"ò", 'o'}, {"ô", 'o'}, {"ö", 'o'}, {"õ", 'o'}, {"Ó", 'o'}, {"Ò", 'o'}, {"Ô", 'o'}, {"Ö", 'o'},
        {"Õ", 'o'}, {"ú", 'u'}, {"ù", 'u'}, {"û", 'u'}, {"ü", 'u'}, {"Ú", 'u'}, {"Ù", 'u'}, {"Û", 'u'}, {"Ü", 'u'},
        {"ñ", 'n'}, {"Ñ", 'n'}, {"ç", 'c'}, {"Ç", 'c'}, {"ý", 'y'}, {"Ý", 'y'}};
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c == 0xC3 || c == 0xC5) && i + 1 < text.size()) {
            const auto it = plain.find(text.substr(i, 2));
            if (it != plain.end()) {
                out += it->second;
                ++i;
                continue;
            }
        }
        out += char(std::tolower(c));
    }
    return out;
}

std::vector<std::size_t> SharedIndex::search(const std::string& query, const BlockList* list) const {
    std::vector<std::string> words;
    std::string word;
    for (const char c : fold(query) + " ") {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!word.empty())
                words.push_back(word), word.clear();
        } else {
            word += c;
        }
    }
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < models.size(); ++i) {
        const SharedModel& m = models[i];
        if (m.has_gcode())
            continue;
        if (list && (list->blocks_account(m.author_account) ||
                     std::any_of(m.files.begin(), m.files.end(), [&](const SharedFile& f) { return list->blocks_file(f.sha256); })))
            continue;
        std::string hay = fold(m.name) + "\n" + fold(m.author) + "\n" + fold(m.license);
        for (const std::string& t : m.tags)
            hay += "\n" + fold(t);
        if (std::all_of(words.begin(), words.end(), [&](const std::string& w) { return hay.find(w) != std::string::npos; }))
            out.push_back(i);
    }
    return out;
}

SharedIndex read_index(const std::string& text, const std::vector<std::uint8_t>& signature,
                       const std::vector<std::uint8_t>& library_key, long long last_sequence) {
    if (text.size() > kMaxIndexBytes)
        throw SharedError("The library is too large for this version", "La biblioteca es demasiado grande para esta versión; actualizá la app");
    check_signature(text, signature, library_key, "The index's signature does not match",
                    "La firma del índice de la biblioteca no coincide");
    const json o = parse(text, "The index cannot be read", "No se puede leer el índice de la biblioteca");
    if (str(o, "format") != "tisma-library-index")
        throw SharedError("Not a library index", "No es un índice de la biblioteca");
    if (number(o, "version", 0) > 1)
        throw SharedError("A newer index format", "El índice es de una versión más nueva");
    SharedIndex index;
    index.sequence = number(o, "sequence", -1);
    if (index.sequence < 0)
        throw SharedError("The index has no sequence", "El índice no tiene número de secuencia");
    if (index.sequence < last_sequence)
        throw SharedError("An older index than the last one", "El índice es más viejo que el último");
    index.generated = str(o, "generated");
    if (const auto it = o.find("models"); it != o.end() && it->is_array())
        for (const json& m : *it) {
            if (!m.is_object())
                continue;
            SharedModel model;
            model.id = str(m, "id");
            model.revision = int(number(m, "revision", 1));
            model.published = str(m, "published");
            model.name = str(m, "name");
            model.description = str(m, "description");
            model.license = str(m, "license");
            if (const auto a = m.find("author"); a != m.end() && a->is_object())
                model.author = str(*a, "name"), model.author_account = str(*a, "account");
            model.thumbnail = str(m, "thumbnail");
            model.size = number(m, "size", 0);
            model.checked = m.value("checked", false);
            if (const auto t = m.find("tags"); t != m.end() && t->is_array())
                for (const json& tag : *t)
                    if (tag.is_string())
                        model.tags.push_back(tag.get<std::string>());
            if (const auto f = m.find("files"); f != m.end() && f->is_array())
                for (const json& file : *f)
                    if (file.is_object())
                        model.files.push_back({str(file, "path"), str(file, "role"), lower(str(file, "sha256")),
                                               number(file, "size", 0)});
            index.models.push_back(std::move(model));
        }
    return index;
}

} // namespace tisma::library
