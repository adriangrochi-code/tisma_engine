// Model files: OBJ, and the choice between STL, OBJ and 3MF. Own design from
// the Wavefront OBJ format description; see PROVENANCE.md.

#include "io/model.hpp"

#include "io/number.hpp"
#include "io/stl.hpp"
#include "io/zip.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <vector>

namespace tisma {
namespace {

bool blank(char c) { return c == ' ' || c == '\t' || c == '\r'; }

// The next word of a line, or empty at its end.
std::string_view word(std::string_view line, std::size_t& at) {
    while (at < line.size() && blank(line[at]))
        ++at;
    const std::size_t start = at;
    while (at < line.size() && !blank(line[at]))
        ++at;
    return line.substr(start, at - start);
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return out;
}

} // namespace

ModelFile parse_obj(std::string_view text) {
    ModelFile out;
    out.format = ModelFormat::Obj;
    std::vector<Vec3f> vertices;
    std::vector<std::size_t> face;
    std::string joined; // a line continued with '\'
    std::size_t number = 0;
    for (std::size_t pos = 0; pos < text.size();) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos)
            end = text.size();
        std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        ++number;
        if (const std::size_t hash = line.find('#'); hash != std::string_view::npos)
            line = line.substr(0, hash);
        while (!line.empty() && blank(line.back()))
            line.remove_suffix(1);
        if (!line.empty() && line.back() == '\\') {
            joined.append(line.substr(0, line.size() - 1)).push_back(' ');
            continue;
        }
        if (!joined.empty()) {
            joined.append(line);
            line = joined;
        }
        const auto fail = [&](const std::string& message) {
            throw ModelError("OBJ, line " + std::to_string(number) + ": " + message);
        };
        std::size_t at = 0;
        const std::string_view keyword = word(line, at);
        if (keyword == "v") {
            double c[3] = {0.0, 0.0, 0.0};
            for (double& v : c) {
                const std::string_view w = word(line, at);
                if (!parse_number(w, v) || !std::isfinite(v))
                    fail("expected a coordinate, found '" + std::string(w) + "'");
            }
            vertices.push_back({float(c[0]), float(c[1]), float(c[2])});
        } else if (keyword == "f") {
            face.clear();
            for (std::string_view w = word(line, at); !w.empty(); w = word(line, at)) {
                // "v", "v/vt", "v//vn" or "v/vt/vn": the vertex is the first.
                const std::string_view index = w.substr(0, w.find('/'));
                long long i = 0;
                const auto [p, ec] = std::from_chars(index.data(), index.data() + index.size(), i);
                if (ec != std::errc() || p != index.data() + index.size() || i == 0)
                    fail("a bad vertex index '" + std::string(w) + "'");
                const long long n = static_cast<long long>(vertices.size());
                const long long k = i > 0 ? i - 1 : n + i;
                if (k < 0 || k >= n)
                    fail("vertex " + std::string(index) + " is not defined yet (" + std::to_string(n) + " so far)");
                face.push_back(std::size_t(k));
            }
            if (face.size() < 3)
                fail("a face needs three vertices");
            for (std::size_t k = 1; k + 1 < face.size(); ++k)
                out.mesh.add_triangle(vertices[face[0]], vertices[face[k]], vertices[face[k + 1]]);
        }
        joined.clear();
    }
    if (out.mesh.empty())
        throw ModelError("OBJ: no faces");
    return out;
}

ModelFile parse_model(std::string_view data, std::string_view extension) {
    if (looks_like_zip(data))
        return parse_3mf(data);
    const std::string ext = lower(extension);
    if (ext == ".3mf")
        throw ModelError("3MF: not a ZIP package");
    if (ext == ".obj")
        return parse_obj(data);
    try {
        StlFile stl = parse_stl(data);
        ModelFile out;
        out.mesh = std::move(stl.mesh);
        out.format = stl.format == StlFormat::Binary ? ModelFormat::StlBinary : ModelFormat::StlAscii;
        out.name = std::move(stl.name);
        return out;
    } catch (const StlError& e) {
        throw ModelError(e.what());
    }
}

ModelFile read_model(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw ModelError("cannot open '" + path.string() + "'");
    const std::streamoff size = in.tellg();
    std::string data(size > 0 ? static_cast<std::size_t>(size) : 0, '\0');
    in.seekg(0);
    if (size < 0 || !in.read(data.data(), size))
        throw ModelError("cannot read '" + path.string() + "'");
    return parse_model(data, path.extension().string());
}

const char* format_name(ModelFormat format) {
    switch (format) {
    case ModelFormat::StlBinary:
        return "binary STL";
    case ModelFormat::StlAscii:
        return "ASCII STL";
    case ModelFormat::Obj:
        return "OBJ";
    case ModelFormat::ThreeMf:
        return "3MF";
    }
    return "?";
}

} // namespace tisma
