#include "fea/verdict.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace tisma {

namespace {

// The spec's table (structural_analysis.md, «Materiales»).
constexpr MaterialFamily kFamilies[] = {
    {"PLA", 3000, 2600, 0.35, 50, 30, 60, 55, 1.24, 190, 220, false},
    {"PETG", 2000, 1800, 0.38, 45, 30, 80, 70, 1.27, 220, 250, false},
    {"ABS", 2000, 1700, 0.35, 35, 20, 105, 90, 1.04, 230, 260, false},
    {"ASA", 2000, 1700, 0.35, 40, 25, 100, 90, 1.07, 240, 260, false},
    {"HIPS", 1800, 1500, 0.35, 25, 15, 100, 85, 1.04, 220, 250, false},
    {"PC", 2300, 2000, 0.37, 55, 35, 147, 115, 1.20, 260, 300, false},
    {"PA", 1500, 1200, 0.39, 45, 30, 50, 90, 1.14, 240, 280, false},
    {"PA-CF", 6000, 2500, 0.35, 70, 35, 60, 140, 1.18, 260, 290, false},
    {"PC-CF", 5000, 2300, 0.35, 65, 35, 147, 120, 1.22, 270, 300, false},
    {"PP", 1000, 800, 0.40, 25, 15, -10, 90, 0.90, 220, 250, false},
    {"POM", 2400, 2000, 0.37, 50, 25, -60, 110, 1.41, 210, 230, false},
    {"PEI", 2200, 2000, 0.36, 70, 40, 186, 150, 1.27, 350, 390, false},
    {"PEEK", 3500, 3000, 0.38, 90, 50, 143, 150, 1.30, 370, 410, false},
    {"PEKK", 3000, 2600, 0.38, 80, 45, 160, 150, 1.30, 340, 380, false},
    {"PSU", 2400, 2100, 0.37, 60, 35, 185, 170, 1.24, 340, 380, false},
    {"PVDF", 1800, 1500, 0.38, 40, 25, -35, 120, 1.78, 240, 260, false},
    {"TPU", 30, 25, 0.45, 30, 20, -40, 60, 1.21, 210, 240, true},
};

const MaterialFamily* named(std::string_view name) {
    for (const MaterialFamily& f : kFamilies)
        if (f.name == name)
            return &f;
    return nullptr;
}

} // namespace

std::span<const MaterialFamily> material_families() { return kFamilies; }

const MaterialFamily* family_of(std::string_view text) {
    // The words of the name, lower case, split at anything not a letter or
    // a digit ("bambu-pa6-cf" -> bambu, pa6, cf; "PLA+" -> pla).
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            word += char(std::tolower(static_cast<unsigned char>(c)));
        } else if (!word.empty()) {
            words.push_back(word);
            word.clear();
        }
    }
    if (!word.empty())
        words.push_back(word);
    const auto has = [&](std::initializer_list<std::string_view> any) {
        for (const std::string& w : words)
            for (const std::string_view a : any)
                if (w == a)
                    return true;
        return false;
    };
    const bool cf = has({"cf", "gf"}) || std::any_of(words.begin(), words.end(), [](const std::string& w) {
                        return w.size() > 2 && (w.ends_with("cf"));
                    });
    if (has({"tpu", "tpe"}))
        return named("TPU");
    if (has({"pa", "pa6", "pa12", "pa66", "paht", "pacf", "pa6cf", "pa12cf", "nylon"}))
        return named(cf ? "PA-CF" : "PA");
    if (has({"pc", "pccf"}))
        return named(cf ? "PC-CF" : "PC");
    if (has({"petg", "pctg", "pet", "petgcf"}))
        return named("PETG");
    if (has({"pla", "placf"}))
        return named("PLA");
    for (const char* name : {"abs", "asa", "hips", "pp", "pom", "pei", "peek", "pekk", "psu", "pvdf"})
        if (has({name})) {
            std::string upper(name);
            for (char& c : upper)
                c = char(std::toupper(static_cast<unsigned char>(c)));
            return named(upper);
        }
    if (has({"ultem"}))
        return named("PEI");
    return nullptr;
}

double temperature_factor(const MaterialFamily& family, double t) {
    if (t <= 23.0)
        return 1.0;
    const double top = family.max_service;
    if (top <= 23.0)
        return t <= top + 15.0 ? std::max(0.02, 0.5 - 0.48 * (t - top) / 15.0) : 0.02;
    if (t <= top)
        return 1.0 - 0.5 * (t - 23.0) / (top - 23.0);
    if (t <= top + 15.0)
        return 0.5 - 0.48 * (t - top) / 15.0;
    return 0.02;
}

const char* verdict_name(Verdict v) {
    switch (v) {
    case Verdict::Holds: return "holds";
    case Verdict::LowMargin: return "low_margin";
    case Verdict::Overloaded: return "overloaded";
    case Verdict::TooFlexible: return "too_flexible";
    case Verdict::OverTemperature: return "over_temperature";
    case Verdict::NotApplicable: return "not_applicable";
    }
    return "";
}

Verdict structural_verdict(const VerdictInput& in) {
    if (in.family && in.family->elastomer)
        return Verdict::NotApplicable;
    if (in.family && in.working_temperature > in.family->max_service)
        return Verdict::OverTemperature;
    if (in.min_safety < 1.0)
        return Verdict::Overloaded;
    if (in.min_safety < in.required_safety)
        return Verdict::LowMargin;
    if (in.limit_exceeded)
        return Verdict::TooFlexible;
    return Verdict::Holds;
}

std::vector<MaterialSuggestion> suggest_materials(const VerdictInput& in) {
    std::vector<MaterialSuggestion> out;
    if (!in.family || in.family->elastomer || !std::isfinite(in.min_safety))
        return out;
    const MaterialFamily& now = *in.family;
    const double f_now = std::max(temperature_factor(now, in.working_temperature), 1e-6);
    for (const MaterialFamily& f : kFamilies) {
        if (&f == &now || f.elastomer || f.max_service < in.working_temperature)
            continue;
        // Stresses stay as they are (the loads do not change); the strength
        // scales, the weaker of its two directions ruling.
        const double ratio = std::min(f.s_xy / now.s_xy, f.s_z / now.s_z);
        const double safety = in.min_safety * ratio * temperature_factor(f, in.working_temperature) / f_now;
        if (safety >= in.required_safety)
            out.push_back({&f, safety});
    }
    std::sort(out.begin(), out.end(), [](const MaterialSuggestion& a, const MaterialSuggestion& b) { return a.safety > b.safety; });
    return out;
}

double voxel_mass_grams(const VoxelGrid& grid, double density) {
    double volume = 0.0; // mm³
    for (int k = 0; k < grid.nz; ++k) {
        std::size_t n = 0;
        for (int j = 0; j < grid.ny; ++j)
            for (int i = 0; i < grid.nx; ++i)
                n += grid.solid[grid.index(i, j, k)] != 0;
        volume += double(n) * grid.hx * grid.hy * grid.hz(k);
    }
    return volume * density / 1000.0;
}

VoxelSurface voxel_surface(const VoxelGrid& grid, const std::vector<double>& per_voxel) {
    VoxelSurface out;
    if (grid.solid.size() != std::size_t(grid.nx) * std::size_t(grid.ny) * std::size_t(grid.nz) ||
        per_voxel.size() != grid.solid.size())
        return out;
    // The six faces: the neighbour's offset, and the face's corners as
    // (x, y, z) picks of the voxel's low (0) and high (1) sides, counter-
    // clockwise seen from outside.
    struct Face {
        int di, dj, dk;
        int c[4][3];
    };
    static constexpr Face kFaces[6] = {
        {-1, 0, 0, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}},
        {1, 0, 0, {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}},
        {0, -1, 0, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}},
        {0, 1, 0, {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}}},
        {0, 0, -1, {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}},
        {0, 0, 1, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}},
    };
    for (int k = 0; k < grid.nz; ++k)
        for (int j = 0; j < grid.ny; ++j)
            for (int i = 0; i < grid.nx; ++i) {
                if (!grid.at(i, j, k))
                    continue;
                const double v = per_voxel[grid.index(i, j, k)];
                const double xs[2] = {grid.x0 + i * grid.hx, grid.x0 + (i + 1) * grid.hx};
                const double ys[2] = {grid.y0 + j * grid.hy, grid.y0 + (j + 1) * grid.hy};
                const double zs[2] = {grid.z[std::size_t(k)], grid.z[std::size_t(k) + 1]};
                for (const Face& f : kFaces) {
                    if (grid.at(i + f.di, j + f.dj, k + f.dk))
                        continue;
                    Vec3f q[4];
                    for (int n = 0; n < 4; ++n)
                        q[n] = Vec3f{float(xs[f.c[n][0]]), float(ys[f.c[n][1]]), float(zs[f.c[n][2]])};
                    out.corners.insert(out.corners.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
                    out.value.insert(out.value.end(), {v, v});
                }
            }
    return out;
}

} // namespace tisma
