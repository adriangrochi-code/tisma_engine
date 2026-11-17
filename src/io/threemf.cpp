// 3MF packages. Own implementation from the 3MF Core Specification and its
// Production Extension (3MF Consortium); see PROVENANCE.md.

#include "io/model.hpp"
#include "io/number.hpp"
#include "io/xml.hpp"
#include "io/zip.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <string>
#include <tuple>
#include <cmath>
#include <map>
#include <optional>
#include <vector>

namespace tisma {
namespace {

// An affine transform as 3MF writes it: "m00 m01 m02 m10 m11 m12 m20 m21 m22
// m30 m31 m32", a point being the row vector (x, y, z, 1) times the matrix
// whose last column is (0, 0, 0, 1).
using Transform = std::array<double, 12>;
constexpr Transform kIdentity = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};

Transform parse_transform(std::string_view text) {
    Transform t{};
    std::size_t k = 0;
    for (std::size_t i = 0; i < text.size();) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r'))
            ++i;
        if (i >= text.size())
            break;
        std::size_t j = i;
        while (j < text.size() && text[j] != ' ' && text[j] != '\t' && text[j] != '\n' && text[j] != '\r')
            ++j;
        if (k == 12 || !parse_number(text.substr(i, j - i), t[k++]))
            throw ModelError("3MF: a bad transform \"" + std::string(text) + "\"");
        i = j;
    }
    if (k != 12)
        throw ModelError("3MF: a transform needs 12 numbers: \"" + std::string(text) + "\"");
    return t;
}

// First a, then b.
Transform then(const Transform& a, const Transform& b) {
    Transform r{};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col) {
            double v = row == 3 ? b[std::size_t(9 + col)] : 0.0;
            for (int k = 0; k < 3; ++k)
                v += a[std::size_t(3 * row + k)] * b[std::size_t(3 * k + col)];
            r[std::size_t(3 * row + col)] = v;
        }
    return r;
}

Vec3f apply(const Transform& t, const Vec3f& p, double scale) {
    const double x = p.x, y = p.y, z = p.z;
    return {float(scale * (x * t[0] + y * t[3] + z * t[6] + t[9])), float(scale * (x * t[1] + y * t[4] + z * t[7] + t[10])),
            float(scale * (x * t[2] + y * t[5] + z * t[8] + t[11]))};
}

double determinant(const Transform& t) {
    return t[0] * (t[4] * t[8] - t[5] * t[7]) - t[1] * (t[3] * t[8] - t[5] * t[6]) + t[2] * (t[3] * t[7] - t[4] * t[6]);
}

double unit_scale(std::string_view unit) {
    if (unit.empty() || unit == "millimeter")
        return 1.0;
    if (unit == "micron")
        return 0.001;
    if (unit == "centimeter")
        return 10.0;
    if (unit == "inch")
        return 25.4;
    if (unit == "foot")
        return 304.8;
    if (unit == "meter")
        return 1000.0;
    throw ModelError("3MF: unknown unit \"" + std::string(unit) + "\"");
}

struct Component {
    std::string path; // the model file, empty for this one
    std::uint32_t id = 0;
    Transform transform = kIdentity;
};

struct Object {
    std::string name;
    bool printed = true; // type model or solidsupport
    std::vector<Vec3f> vertices;
    std::vector<std::array<std::uint32_t, 3>> triangles;
    std::vector<Component> components;
};

struct Item {
    Component what;
    bool printable = true;
};

struct ModelPart {
    double scale = 1.0;
    std::map<std::uint32_t, Object> objects;
    std::vector<Item> build;
};

std::uint32_t parse_index(std::string_view text, const XmlReader& xml, const char* what) {
    std::uint32_t v = 0;
    const auto [end, ec] = chars_to(text.data(), text.data() + text.size(), v);
    if (ec != std::errc() || end != text.data() + text.size())
        throw ModelError("3MF, line " + std::to_string(xml.line()) + ": a bad " + what + " \"" + std::string(text) + "\"");
    return v;
}

std::string package_path(std::string_view raw) { return XmlReader::decode(raw); }

ModelPart parse_model_part(std::string_view text) {
    ModelPart part;
    XmlReader xml(text);
    Object* object = nullptr;
    bool in_build = false;
    const auto need = [&](std::string_view attribute) {
        const std::optional<std::string_view> v = xml.attribute(attribute);
        if (!v)
            throw ModelError("3MF, line " + std::to_string(xml.line()) + ": <" + std::string(xml.name()) + "> without " +
                             std::string(attribute));
        return *v;
    };
    const auto coordinate = [&](std::string_view attribute) {
        double v = 0.0;
        const std::string_view raw = need(attribute);
        if (!parse_number(raw, v) || !std::isfinite(v))
            throw ModelError("3MF, line " + std::to_string(xml.line()) + ": a bad coordinate \"" + std::string(raw) + "\"");
        return float(v);
    };
    for (XmlReader::Event e; (e = xml.next()) != XmlReader::Event::Done;) {
        const std::string_view name = XmlReader::local_name(xml.name());
        if (e == XmlReader::Event::End) {
            if (name == "object")
                object = nullptr;
            else if (name == "build")
                in_build = false;
            continue;
        }
        if (name == "vertex" && object) {
            object->vertices.push_back({coordinate("x"), coordinate("y"), coordinate("z")});
        } else if (name == "triangle" && object) {
            object->triangles.push_back(
                {parse_index(need("v1"), xml, "vertex index"), parse_index(need("v2"), xml, "vertex index"),
                 parse_index(need("v3"), xml, "vertex index")});
        } else if (name == "object") {
            const std::uint32_t id = parse_index(need("id"), xml, "object id");
            object = &part.objects[id];
            object->name = XmlReader::decode(xml.attribute("name").value_or(""));
            const std::string_view type = xml.attribute("type").value_or("model");
            object->printed = type == "model" || type == "solidsupport";
        } else if (name == "component" && object) {
            Component c;
            c.id = parse_index(need("objectid"), xml, "object id");
            if (const auto t = xml.attribute("transform"))
                c.transform = parse_transform(*t);
            if (const auto p = xml.attribute("path"))
                c.path = package_path(*p);
            object->components.push_back(std::move(c));
        } else if (name == "build") {
            in_build = true;
        } else if (name == "item" && in_build) {
            Item item;
            item.what.id = parse_index(need("objectid"), xml, "object id");
            if (const auto t = xml.attribute("transform"))
                item.what.transform = parse_transform(*t);
            if (const auto p = xml.attribute("path"))
                item.what.path = package_path(*p);
            item.printable = xml.attribute("printable").value_or("1") != "0";
            part.build.push_back(std::move(item));
        } else if (name == "model") {
            part.scale = unit_scale(xml.attribute("unit").value_or("millimeter"));
        }
    }
    return part;
}

// The volumes of a slicer project's objects (Metadata/Slic3r_PE_model.config):
// for each object, the triangle ranges that are not parts.
enum class VolumeKind { Part, Negative, Modifier, Blocker, Enforcer, Other };
struct Volume {
    std::uint32_t first = 0, last = 0;
    bool part = true;
    bool negative = false; // a volume that takes material away (model_tools.md, rule 2)
    VolumeKind kind = VolumeKind::Part; // project_import.md, rule 4
    std::string name; // its "name" metadata
    int extruder = 0; // its "extruder" metadata; 0: not given
};
std::map<std::uint32_t, std::vector<Volume>> project_volumes(std::string_view text) {
    std::map<std::uint32_t, std::vector<Volume>> out;
    XmlReader xml(text);
    std::optional<std::uint32_t> object;
    Volume* volume = nullptr;
    for (XmlReader::Event e; (e = xml.next()) != XmlReader::Event::Done;) {
        const std::string_view name = xml.name();
        if (e == XmlReader::Event::End) {
            if (name == "volume")
                volume = nullptr;
            else if (name == "object")
                object.reset();
            continue;
        }
        if (name == "object") {
            if (const auto id = xml.attribute("id"))
                object = parse_index(*id, xml, "object id");
        } else if (name == "volume" && object) {
            const auto first = xml.attribute("firstid"), last = xml.attribute("lastid");
            if (!first || !last)
                continue;
            out[*object].push_back({parse_index(*first, xml, "triangle index"), parse_index(*last, xml, "triangle index"), true});
            volume = &out[*object].back();
        } else if (name == "metadata" && volume && xml.attribute("key").value_or("") == "name") {
            volume->name = std::string(xml.attribute("value").value_or(""));
        } else if (name == "metadata" && volume && xml.attribute("key").value_or("") == "extruder") {
            const std::string value(xml.attribute("value").value_or("0"));
            int e = 0;
            for (const char ch : value)
                e = ch >= '0' && ch <= '9' && e < 1000 ? e * 10 + (ch - '0') : e;
            volume->extruder = e;
        } else if (name == "metadata" && volume && xml.attribute("key").value_or("") == "volume_type") {
            const std::string_view type = xml.attribute("value").value_or("ModelPart");
            volume->part = type == "ModelPart";
            volume->negative = type == "NegativeVolume";
            volume->kind = type == "ModelPart"           ? VolumeKind::Part
                           : type == "NegativeVolume"    ? VolumeKind::Negative
                           : type == "ParameterModifier" ? VolumeKind::Modifier
                           : type == "SupportBlocker"    ? VolumeKind::Blocker
                           : type == "SupportEnforcer"   ? VolumeKind::Enforcer
                                                         : VolumeKind::Other;
        }
    }
    return out;
}

class Package {
public:
    explicit Package(std::string_view data) : zip_(data) {}

    std::string root() const {
        // The start part: the relationship of type 3D model in _rels/.rels.
        if (const ZipArchive::Entry* rels = zip_.find("_rels/.rels")) {
            const std::string text = zip_.read(*rels);
            XmlReader xml(text);
            for (XmlReader::Event e; (e = xml.next()) != XmlReader::Event::Done;)
                if (e == XmlReader::Event::Start && XmlReader::local_name(xml.name()) == "Relationship" &&
                    xml.attribute("Type").value_or("").ends_with("/3dmodel"))
                    if (const auto target = xml.attribute("Target"))
                        return package_path(*target);
        }
        return "/3D/3dmodel.model";
    }

    const ModelPart& part(const std::string& path) {
        const auto known = parts_.find(path);
        if (known != parts_.end())
            return known->second;
        const ZipArchive::Entry* entry = zip_.find(path);
        if (!entry)
            throw ModelError("3MF: the package has no " + path);
        return parts_.emplace(path, parse_model_part(zip_.read(*entry))).first->second;
    }

    std::optional<std::string> read(std::string_view name) const {
        const ZipArchive::Entry* entry = zip_.find(name);
        return entry ? std::optional<std::string>(zip_.read(*entry)) : std::nullopt;
    }

private:
    ZipArchive zip_;
    std::map<std::string, ModelPart> parts_;
};

struct Builder {
    Package& package;
    const std::map<std::uint32_t, std::vector<Volume>>& volumes; // of the root file's objects
    std::string root;
    double scale = 1.0;
    ModelFile& out;
    std::size_t item = 0; // the build item being emitted

    void emit(const std::string& path, std::uint32_t id, const Transform& transform, int depth) {
        if (depth > 32)
            throw ModelError("3MF: components nested too deep (a cycle?)");
        const ModelPart& part = package.part(path);
        const auto found = part.objects.find(id);
        if (found == part.objects.end())
            throw ModelError("3MF: no object " + std::to_string(id) + " in " + path);
        const Object& object = found->second;
        if (!object.printed)
            return;
        for (const Component& c : object.components)
            emit(c.path.empty() ? path : c.path, c.id, then(c.transform, transform), depth + 1);
        if (object.triangles.empty())
            return;
        const std::vector<Volume>* listed = nullptr;
        if (path == root)
            if (const auto v = volumes.find(id); v != volumes.end())
                listed = &v->second;
        out.parts += listed ? std::size_t(std::count_if(listed->begin(), listed->end(), [](const Volume& v) { return v.part; }))
                            : 1;
        const bool mirrored = determinant(transform) < 0.0; // keep the faces outward
        std::size_t modifiers_before = out.modifiers.size();
        if (listed)
            for (const Volume& v : *listed)
                if (v.kind == VolumeKind::Modifier)
                    out.modifiers.emplace_back();
        const Volume* piece_of = nullptr; // the part volume of the piece being filled
        bool piece_open = false;
        for (std::size_t t = 0; t < object.triangles.size(); ++t) {
            const auto in = [&](const Volume& v) { return t >= v.first && t <= v.last; };
            const Volume* volume = nullptr;
            std::size_t modifier = modifiers_before;
            if (listed)
                for (const Volume& v : *listed) {
                    if (in(v)) {
                        volume = &v;
                        break;
                    }
                    if (v.kind == VolumeKind::Modifier)
                        ++modifier;
                }
            const VolumeKind kind = volume ? volume->kind : VolumeKind::Part;
            if (kind == VolumeKind::Other)
                continue;
            TriangleMesh& to = kind == VolumeKind::Negative ? out.negative
                               : kind == VolumeKind::Modifier ? out.modifiers[modifier]
                               : kind == VolumeKind::Blocker  ? out.support_blockers
                               : kind == VolumeKind::Enforcer ? out.support_enforcers
                                                              : out.mesh;
            const auto& tri = object.triangles[t];
            for (const std::uint32_t v : tri)
                if (v >= object.vertices.size())
                    throw ModelError("3MF: object " + std::to_string(id) + " has a triangle with vertex " +
                                     std::to_string(v) + " of " + std::to_string(object.vertices.size()));
            const Vec3f a = apply(transform, object.vertices[tri[0]], scale);
            const Vec3f b = apply(transform, object.vertices[tri[1]], scale);
            const Vec3f c = apply(transform, object.vertices[tri[2]], scale);
            if (&to == &out.mesh) { // a part: its piece (FM9)
                if (!piece_open || volume != piece_of) {
                    ModelFile::Piece piece;
                    piece.name = volume && !volume->name.empty() ? volume->name : object.name;
                    piece.object = item;
                    piece.first = out.mesh.triangle_count();
                    piece.extruder = volume ? volume->extruder : 0;
                    out.pieces.push_back(std::move(piece));
                    piece_of = volume, piece_open = true;
                }
                ++out.pieces.back().count;
            }
            if (mirrored)
                to.add_triangle(a, c, b);
            else
                to.add_triangle(a, b, c);
        }
    }
};

} // namespace

ModelFile parse_3mf(std::string_view data) {
    ModelFile out;
    out.format = ModelFormat::ThreeMf;
    out.objects = 0;
    out.parts = 0;
    try {
        Package package(data);
        const std::string root = package.root();
        const ModelPart& model = package.part(root);
        std::map<std::uint32_t, std::vector<Volume>> volumes;
        if (const std::optional<std::string> config = package.read("Metadata/Slic3r_PE_model.config"))
            volumes = project_volumes(*config);
        Builder builder{package, volumes, root, model.scale, out};
        for (const Item& item : model.build) {
            if (!item.printable)
                continue;
            builder.item = out.objects;
            ++out.objects;
            builder.emit(item.what.path.empty() ? root : item.what.path, item.what.id, item.what.transform, 0);
        }
    } catch (const ZipError& e) {
        throw ModelError(std::string("3MF: ") + e.what());
    } catch (const XmlError& e) {
        throw ModelError(std::string("3MF: ") + e.what());
    }
    if (out.mesh.empty())
        throw ModelError("3MF: nothing on the build plate to print");
    return out;
}

std::vector<ThreeMfItem> read_3mf_items(std::string_view data, std::optional<std::string>* extra, std::string_view extra_name,
                                        bool own_frame) {
    std::vector<ThreeMfItem> items;
    try {
        Package package(data);
        if (extra)
            *extra = package.read(extra_name);
        const std::string root = package.root();
        const ModelPart& model = package.part(root);
        const std::map<std::uint32_t, std::vector<Volume>> no_volumes;
        for (const Item& item : model.build) {
            if (!item.printable)
                continue;
            ModelFile one;
            Builder builder{package, no_volumes, root, model.scale, one};
            const std::string path = item.what.path.empty() ? root : item.what.path;
            const Transform& t = item.what.transform;
            builder.emit(path, item.what.id, own_frame ? kIdentity : t, 0);
            const ModelPart& part = package.part(path);
            const auto found = part.objects.find(item.what.id);
            ThreeMfItem out{found != part.objects.end() ? found->second.name : std::string(), std::move(one.mesh)};
            if (own_frame) {
                // 3MF's row vector times the matrix: bed x = x t0 + y t3 + z t6 + t9.
                for (std::size_t r = 0; r < 3; ++r) {
                    for (std::size_t c = 0; c < 3; ++c)
                        out.turn[3 * r + c] = t[3 * c + r];
                    out.offset[r] = t[9 + r] * model.scale;
                }
            }
            items.push_back(std::move(out));
        }
    } catch (const ZipError& e) {
        throw ModelError(std::string("3MF: ") + e.what());
    } catch (const XmlError& e) {
        throw ModelError(std::string("3MF: ") + e.what());
    }
    return items;
}

namespace {

std::string xml_escaped(std::string_view text) {
    std::string out;
    for (const char c : text)
        out += c == '&' ? "&amp;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '"' ? "&quot;" : std::string(1, c);
    return out;
}

// An object's <mesh>: the corners welded by their exact coordinates, each
// written with the digits that give the same float back.
void append_mesh(std::string& model, const TriangleMesh& mesh) {
    std::vector<Vec3f> vertices;
    std::vector<std::size_t> index(mesh.corners().size());
    std::map<std::tuple<float, float, float>, std::size_t> seen;
    for (std::size_t i = 0; i < mesh.corners().size(); ++i) {
        const Vec3f& v = mesh.corners()[i];
        const auto [it, added] = seen.emplace(std::make_tuple(v.x, v.y, v.z), vertices.size());
        if (added)
            vertices.push_back(v);
        index[i] = it->second;
    }
    model += "   <mesh>\n    <vertices>\n";
    char line[160];
    for (const Vec3f& v : vertices) {
        std::snprintf(line, sizeof line, "     <vertex x=\"%.9g\" y=\"%.9g\" z=\"%.9g\"/>\n", double(v.x), double(v.y), double(v.z));
        model += line;
    }
    model += "    </vertices>\n    <triangles>\n";
    for (std::size_t i = 0; i + 2 < index.size(); i += 3) {
        std::snprintf(line, sizeof line, "     <triangle v1=\"%zu\" v2=\"%zu\" v3=\"%zu\"/>\n", index[i], index[i + 1], index[i + 2]);
        model += line;
    }
    model += "    </triangles>\n   </mesh>\n";
}

} // namespace

std::string write_3mf_objects(const std::vector<ThreeMfObject>& objects, std::vector<ZipEntryToWrite> extra) {
    std::string model = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<model unit=\"millimeter\" xml:lang=\"en-US\" "
                        "xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n"
                        " <metadata name=\"Application\">Tisma</metadata>\n <resources>\n";
    for (std::size_t i = 0; i < objects.size(); ++i) {
        model += "  <object id=\"" + std::to_string(i + 1) + "\" type=\"model\" name=\"" + xml_escaped(objects[i].name) + "\">\n";
        append_mesh(model, *objects[i].mesh);
        model += "  </object>\n";
    }
    model += " </resources>\n <build>\n";
    char line[512];
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const auto& o = objects[i].offset;
        const auto& a = objects[i].turn; // by rows; 3MF wants its transpose (row vectors)
        if (o == std::array<double, 3>{0.0, 0.0, 0.0} && a == std::array<double, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1})
            std::snprintf(line, sizeof line, "  <item objectid=\"%zu\"/>\n", i + 1);
        else
            std::snprintf(line, sizeof line,
                          "  <item objectid=\"%zu\" transform=\"%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g "
                          "%.9g %.9g %.9g\"/>\n",
                          i + 1, a[0], a[3], a[6], a[1], a[4], a[7], a[2], a[5], a[8], o[0], o[1], o[2]);
        model += line;
    }
    model += " </build>\n</model>\n";
    std::string types = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
                        " <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
                        " <Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>\n"
                        " <Default Extension=\"json\" ContentType=\"application/json\"/>\n"
                        "</Types>\n";
    const std::string rels = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                             "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
                             " <Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
                             "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>\n"
                             "</Relationships>\n";
    std::vector<ZipEntryToWrite> entries{{"[Content_Types].xml", std::move(types)}, {"_rels/.rels", rels}, {"3D/3dmodel.model", std::move(model)}};
    for (ZipEntryToWrite& e : extra)
        entries.push_back(std::move(e));
    return write_zip_stored(entries);
}

std::string write_3mf(const TriangleMesh& mesh, std::string_view name) {
    // The corners welded by their exact coordinates into a vertex list.
    std::vector<Vec3f> vertices;
    std::vector<std::size_t> index(mesh.corners().size());
    {
        std::map<std::tuple<float, float, float>, std::size_t> seen;
        for (std::size_t i = 0; i < mesh.corners().size(); ++i) {
            const Vec3f& v = mesh.corners()[i];
            const auto [it, added] = seen.emplace(std::make_tuple(v.x, v.y, v.z), vertices.size());
            if (added)
                vertices.push_back(v);
            index[i] = it->second;
        }
    }
    std::string escaped;
    for (const char c : name)
        escaped += c == '&' ? "&amp;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '"' ? "&quot;" : std::string(1, c);
    std::string model = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<model unit=\"millimeter\" xml:lang=\"en-US\" "
                        "xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n"
                        " <resources>\n  <object id=\"1\" type=\"model\" name=\"" + escaped + "\">\n   <mesh>\n    <vertices>\n";
    char line[128];
    for (const Vec3f& v : vertices) {
        std::snprintf(line, sizeof line, "     <vertex x=\"%.9g\" y=\"%.9g\" z=\"%.9g\"/>\n", double(v.x), double(v.y), double(v.z));
        model += line;
    }
    model += "    </vertices>\n    <triangles>\n";
    for (std::size_t i = 0; i + 2 < index.size(); i += 3) {
        std::snprintf(line, sizeof line, "     <triangle v1=\"%zu\" v2=\"%zu\" v3=\"%zu\"/>\n", index[i], index[i + 1], index[i + 2]);
        model += line;
    }
    model += "    </triangles>\n   </mesh>\n  </object>\n </resources>\n <build>\n  <item objectid=\"1\"/>\n </build>\n</model>\n";
    const std::string types = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                              "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
                              " <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
                              " <Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>\n"
                              "</Types>\n";
    const std::string rels = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                             "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
                             " <Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
                             "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>\n"
                             "</Relationships>\n";
    return write_zip_stored({{"[Content_Types].xml", types}, {"_rels/.rels", rels}, {"3D/3dmodel.model", model}});
}

} // namespace tisma
