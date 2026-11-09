#include "io/project_import.hpp"

#include "io/xml.hpp"
#include "io/zip.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace tisma {

namespace {

std::string lower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return out;
}

bool starts_with_folder(std::string_view name, std::string_view folder) {
    if (!name.empty() && name.front() == '/')
        name.remove_prefix(1);
    return name.size() > folder.size() && lower(name.substr(0, folder.size())) == folder;
}

// The value of <metadata name="Application"> in the model, looked for where
// the metadata is: before the resources, in the first part of the file; and
// the number of its build items.
std::string application_of(const ZipArchive& zip, int& items) {
    const ZipArchive::Entry* model = zip.find("3D/3dmodel.model");
    if (!model)
        for (const ZipArchive::Entry& e : zip.entries())
            if (starts_with_folder(e.name, "3d/") && lower(e.name).ends_with(".model")) {
                model = &e;
                break;
            }
    if (!model)
        return {};
    const std::string text = zip.read(*model);
    for (std::size_t at = text.find("<item"); at != std::string::npos; at = text.find("<item", at + 5))
        if (at + 5 < text.size() && (text[at + 5] == ' ' || text[at + 5] == '\t' || text[at + 5] == '\n' || text[at + 5] == '/'))
            ++items;
    const std::string_view head = std::string_view(text).substr(0, 1 << 16);
    const std::size_t key = head.find("name=\"Application\"");
    if (key == std::string_view::npos)
        return {};
    const std::size_t open = head.find('>', key), close = head.find('<', key);
    if (open == std::string_view::npos || close == std::string_view::npos || close < open)
        return {};
    return XmlReader::decode(head.substr(open + 1, close - open - 1));
}

} // namespace

ProjectSource project_source(std::string_view package) {
    const ZipArchive zip(package);
    ProjectSource out;
    if (zip.find("Metadata/Slic3r_PE.config") || zip.find("Metadata/Slic3r_PE_model.config"))
        out.family = ProjectFamily::PrusaSlicer;
    else if (zip.find("Metadata/project_settings.config") || zip.find("Metadata/model_settings.config"))
        out.family = ProjectFamily::OrcaSlicer;
    else if (std::any_of(zip.entries().begin(), zip.entries().end(),
                         [](const ZipArchive::Entry& e) { return starts_with_folder(e.name, "cura/"); }))
        out.family = ProjectFamily::Cura;
    out.application = application_of(zip, out.items);
    // A known program's name in the metadata, as the dialog shows it.
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 9> known = {{
        {"tisma", "Tisma Slicer"},
        {"prusaslicer", "PrusaSlicer"},
        {"orcaslicer", "OrcaSlicer"},
        {"bambustudio", "Bambu Studio"},
        {"bambu studio", "Bambu Studio"},
        {"creality", "Creality Print"},
        {"elegoo", "Elegoo Slicer"},
        {"anycubic", "Anycubic Slicer Next"},
        {"cura", "Cura"},
    }};
    const std::string app = lower(out.application);
    for (const auto& [needle, name] : known)
        if (app.find(needle) != std::string::npos) {
            out.program = std::string(name);
            break;
        }
    if (out.program.empty())
        switch (out.family) {
        case ProjectFamily::PrusaSlicer: out.program = "PrusaSlicer"; break;
        case ProjectFamily::OrcaSlicer: out.program = "OrcaSlicer"; break;
        case ProjectFamily::Cura: out.program = "Cura"; break;
        case ProjectFamily::Standard: break;
        }
    return out;
}

} // namespace tisma
