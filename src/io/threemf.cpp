// 3MF packages. Own implementation from the 3MF Core Specification and its
// Production Extension (3MF Consortium); see PROVENANCE.md.

#include "io/model.hpp"
#include "io/number.hpp"
#include "io/xml.hpp"
#include "io/zip.hpp"

#include <algorithm>
#include <array>
#include <charconv>
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
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), v);
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
struct Volume {
    std::uint32_t first = 0, last = 0;
    bool part = true;
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
        } else if (name == "metadata" && volume && xml.attribute("key").value_or("") == "volume_type") {
            volume->part = xml.attribute("value").value_or("ModelPart") == "ModelPart";
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
        for (std::size_t t = 0; t < object.triangles.size(); ++t) {
            if (listed && std::any_of(listed->begin(), listed->end(), [&](const Volume& v) {
                    return !v.part && t >= v.first && t <= v.last;
                }))
                continue;
            const auto& tri = object.triangles[t];
            for (const std::uint32_t v : tri)
                if (v >= object.vertices.size())
                    throw ModelError("3MF: object " + std::to_string(id) + " has a triangle with vertex " +
                                     std::to_string(v) + " of " + std::to_string(object.vertices.size()));
            const Vec3f a = apply(transform, object.vertices[tri[0]], scale);
            const Vec3f b = apply(transform, object.vertices[tri[1]], scale);
            const Vec3f c = apply(transform, object.vertices[tri[2]], scale);
            if (mirrored)
                out.mesh.add_triangle(a, c, b);
            else
                out.mesh.add_triangle(a, b, c);
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

} // namespace tisma
