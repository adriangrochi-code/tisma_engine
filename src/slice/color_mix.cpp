// Mixed colours with one nozzle (docs/spec/features/color_mixing.md, rules
// 5 and 7 to 10, 14 to 16). The cycle, its order and the gradient's bands
// follow the spec's rules and measurements; the shown colour is the
// Yule-Nielsen relation (Yule and Nielsen 1951). See PROVENANCE.md.

#include "slice/color_mix.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

namespace tisma {

using nlohmann::json;
using nlohmann::ordered_json;

std::vector<int> mix_cycle(const std::vector<double>& shares) {
    const std::size_t k = shares.size();
    if (k <= 1)
        return std::vector<int>(k, 1);
    std::vector<int> best;
    for (int n = 2; n <= 64; ++n) {
        std::vector<int> counts(k);
        int sum = 0;
        bool close = true;
        for (std::size_t i = 0; i < k; ++i) {
            counts[i] = std::max(1, int(std::lround(shares[i] * n)));
            sum += counts[i];
            // Less than 3 points off: 7 : 93 is 1 in 11, not 1 in 10 (FS5).
            close = close && std::abs(double(counts[i]) / n - shares[i]) < 0.03 - 1e-9;
        }
        if (sum == n && close) {
            best = std::move(counts);
            break;
        }
    }
    if (best.empty()) { // none within 3 points: the nearest in 64 layers
        best.resize(k);
        for (std::size_t i = 0; i < k; ++i)
            best[i] = std::max(1, int(std::lround(shares[i] * 64)));
    }
    int g = 0;
    for (const int c : best)
        g = std::gcd(g, c);
    for (int& c : best)
        c /= std::max(g, 1);
    return best;
}

std::vector<int> mix_order(const std::vector<int>& counts) {
    const int n = std::accumulate(counts.begin(), counts.end(), 0);
    std::vector<int> order;
    std::vector<int> done(counts.size(), 0);
    for (int layer = 1; layer <= n; ++layer) {
        // Furthest behind its share by this layer (in layers of n, to stay whole).
        int pick = 0;
        long long most = 0;
        for (std::size_t i = 0; i < counts.size(); ++i) {
            const long long behind = static_cast<long long>(counts[i]) * layer - static_cast<long long>(done[i]) * n;
            if (i == 0 || behind > most)
                pick = int(i), most = behind;
        }
        ++done[std::size_t(pick)];
        order.push_back(pick);
    }
    return order;
}

namespace {

// The filament of each layer of a cycle of these components (shares above
// 0, in the blend's order), from 1.
std::vector<int> cycle_of(const std::vector<std::pair<int, double>>& parts) {
    std::vector<std::pair<int, double>> kept;
    double total = 0.0;
    for (const auto& p : parts)
        if (p.second > 0.0)
            kept.push_back(p), total += p.second;
    if (kept.empty())
        return {parts.empty() ? 1 : parts.front().first};
    std::vector<double> shares;
    for (const auto& p : kept)
        shares.push_back(p.second / total);
    std::vector<int> filaments;
    for (const int i : mix_order(mix_cycle(shares)))
        filaments.push_back(kept[std::size_t(i)].first);
    return filaments;
}

} // namespace

std::vector<int> mix_layers(const ColorMix& mix, const std::vector<Layer>& layers, const std::vector<char>& present) {
    const std::size_t n = layers.size();
    std::vector<int> out(n, 0);
    const auto in = [&](std::size_t l) { return l < present.size() && present[l]; };
    if (mix.components.empty())
        return out;
    if (!mix.gradient) {
        std::vector<std::pair<int, double>> parts;
        for (const ColorMix::Component& c : mix.components)
            parts.push_back({c.extruder, c.share});
        const std::vector<int> cycle = cycle_of(parts);
        std::size_t first = n;
        for (std::size_t l = 0; l < n; ++l)
            if (in(l)) {
                first = std::min(first, l);
                out[l] = cycle[(l - first) % cycle.size()];
            }
        return out;
    }
    // A gradient: its points by position.
    std::vector<ColorMix::Component> points = mix.components;
    std::stable_sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return a.share < b.share; });
    // Each layer's top, its height added up from the bed to the micrometre.
    std::vector<double> top(n);
    double z = n ? layers.front().bottom : 0.0;
    for (std::size_t l = 0; l < n; ++l) {
        z += std::round((layers[l].top - layers[l].bottom) * 1e6) / 1e6;
        top[l] = z;
    }
    // The ranges: the blend's own, or each run of layers it is in.
    struct Range {
        double lo, hi;
        std::vector<std::size_t> layers;
    };
    std::vector<Range> ranges;
    if (mix.min_z || mix.max_z) {
        Range r{mix.min_z.value_or(n ? layers.front().bottom : 0.0), mix.max_z.value_or(n ? layers.back().top : 0.0), {}};
        for (std::size_t l = 0; l < n; ++l)
            if (in(l)) {
                if (top[l] <= r.lo + 1e-9)
                    out[l] = points.front().extruder;
                else if (top[l] > r.hi + 1e-9)
                    out[l] = points.back().extruder;
                else
                    r.layers.push_back(l);
            }
        ranges.push_back(std::move(r));
    } else {
        for (std::size_t l = 0; l < n; ++l) {
            if (!in(l))
                continue;
            if (ranges.empty() || ranges.back().layers.back() + 1 != l)
                ranges.push_back({layers[l].bottom, layers[l].top, {}});
            ranges.back().layers.push_back(l);
            ranges.back().hi = layers[l].top;
        }
    }
    for (const Range& r : ranges) {
        if (r.layers.empty() || !(r.hi > r.lo))
            continue;
        const int bands = std::clamp(int(std::lround(double(r.layers.size()) / 10.0)), 2, 11);
        const double band_height = (r.hi - r.lo) / bands;
        int band_now = -1;
        std::vector<int> cycle;
        std::size_t start = 0;
        for (const std::size_t l : r.layers) {
            const int band = std::clamp(int(std::floor((top[l] - r.lo) / band_height)), 0, bands - 1);
            if (band != band_now) {
                band_now = band, start = l;
                // The blend at the band's middle, rounded to 10 %.
                const double at = (band + 0.5) / bands;
                if (at <= points.front().share)
                    cycle = {points.front().extruder};
                else if (at >= points.back().share)
                    cycle = {points.back().extruder};
                else {
                    std::size_t b = 1;
                    while (b + 1 < points.size() && points[b].share < at)
                        ++b;
                    const ColorMix::Component& lo = points[b - 1];
                    const ColorMix::Component& hi = points[b];
                    const double t = hi.share > lo.share ? (at - lo.share) / (hi.share - lo.share) : 1.0;
                    const double share = std::round(t * 10.0) / 10.0;
                    cycle = cycle_of({{lo.extruder, 1.0 - share}, {hi.extruder, share}});
                }
            }
            out[l] = cycle[(l - start) % cycle.size()];
        }
    }
    return out;
}

bool mix_usable(const ColorMix& mix, int extruders) {
    if (mix.id <= extruders || mix.components.empty())
        return false;
    return std::all_of(mix.components.begin(), mix.components.end(),
                       [&](const ColorMix::Component& c) { return c.extruder >= 1 && c.extruder <= extruders; });
}

std::vector<ColorMix> parse_color_mixes(std::string_view text) {
    json root;
    try {
        root = json::parse(text);
    } catch (const json::exception& e) {
        throw ColorMixError(std::string("color mixes: not valid JSON: ") + e.what());
    }
    const auto fail = [](const std::string& what) { throw ColorMixError("color mixes: " + what); };
    if (!root.is_object())
        fail("the top level must be an object");
    std::vector<ColorMix> out;
    if (!root.contains("virtual_extruders"))
        return out;
    const json& list = root.at("virtual_extruders");
    if (!list.is_array())
        fail("virtual_extruders must be a list");
    for (const json& v : list) {
        if (!v.is_object() || !v.contains("id") || !v.at("id").is_number_integer() || !v.contains("components") ||
            !v.at("components").is_array())
            fail("each virtual extruder needs an id and its components");
        ColorMix mix;
        mix.id = v.at("id").get<int>();
        if (mix.id < 2 || mix.id > 255)
            fail("a virtual extruder's id must be from 2 to 255");
        if (v.contains("color") && v.at("color").is_string())
            mix.color = v.at("color").get<std::string>();
        const std::string kind = v.contains("kind") && v.at("kind").is_string() ? v.at("kind").get<std::string>() : "fullspectrum";
        mix.gradient = kind == "gradient";
        for (const char* key : {"minz", "maxz"})
            if (v.contains(key) && v.at(key).is_number())
                (std::string_view(key) == "minz" ? mix.min_z : mix.max_z) = v.at(key).get<double>();
        const char* share = mix.gradient ? "position" : "ratio";
        double total = 0.0;
        for (const json& c : v.at("components")) {
            if (!c.is_object() || !c.contains("extruder") || !c.at("extruder").is_number_integer() || !c.contains(share) ||
                !c.at(share).is_number() || c.at(share).get<double>() < 0.0)
                fail(std::string("each component needs its extruder and its ") + share + ", not negative");
            mix.components.push_back({c.at("extruder").get<int>(), c.at(share).get<double>()});
            total += mix.components.back().share;
        }
        if (mix.components.empty() || mix.components.size() > 3)
            fail("a blend has 1 to 3 filaments");
        if (!mix.gradient && total > 0.0) // in proportion: 1 and 3 are 0.25 and 0.75 (rule 16)
            for (ColorMix::Component& c : mix.components)
                c.share /= total;
        out.push_back(std::move(mix));
    }
    return out;
}

std::string color_mixes_json(const std::vector<ColorMix>& mixes, const std::vector<std::string>& filament_colors) {
    ordered_json root;
    root["version"] = 1;
    ordered_json physical = ordered_json::array();
    for (std::size_t k = 0; k < filament_colors.size(); ++k)
        physical.push_back({{"id", int(k + 1)}, {"color", filament_colors[k]}});
    root["physical_extruders"] = std::move(physical);
    ordered_json virtuals = ordered_json::array();
    for (const ColorMix& m : mixes) {
        ordered_json v;
        v["id"] = m.id;
        v["kind"] = m.gradient ? "gradient" : "fullspectrum";
        v["color"] = m.color;
        if (m.min_z)
            v["minz"] = *m.min_z;
        if (m.max_z)
            v["maxz"] = *m.max_z;
        ordered_json components = ordered_json::array();
        for (const ColorMix::Component& c : m.components)
            components.push_back({{"extruder", c.extruder}, {m.gradient ? "position" : "ratio", c.share}});
        v["components"] = std::move(components);
        virtuals.push_back(std::move(v));
    }
    root["virtual_extruders"] = std::move(virtuals);
    return root.dump(2) + "\n";
}

std::string mix_color(const std::vector<std::string>& colors, const std::vector<double>& shares) {
    double rgb[3] = {0.0, 0.0, 0.0}, total = 0.0;
    for (std::size_t k = 0; k < colors.size() && k < shares.size(); ++k) {
        unsigned value = 0;
        const std::string& c = colors[k];
        if (c.size() != 7 || c[0] != '#' || std::sscanf(c.c_str() + 1, "%6x", &value) != 1)
            continue;
        for (int ch = 0; ch < 3; ++ch) // reflectances mixed as their square roots (n = 2)
            rgb[ch] += shares[k] * std::sqrt(double((value >> (16 - 8 * ch)) & 0xFFu) / 255.0);
        total += shares[k];
    }
    if (total <= 0.0)
        return "#808080";
    char text[8];
    const auto channel = [&](int ch) { return unsigned(std::lround(std::pow(rgb[ch] / total, 2.0) * 255.0)); };
    std::snprintf(text, sizeof text, "#%02x%02x%02x", channel(0), channel(1), channel(2));
    return text;
}

} // namespace tisma
