// Mixed colours with one nozzle (docs/spec/features/color_mixing.md, rules
// 5 and 7 to 10, 14 to 16). The cycle, its order and the gradient's bands
// follow the spec's rules and measurements; the shown colour is the
// Yule-Nielsen relation (Yule and Nielsen 1951). See PROVENANCE.md.

#include "slice/color_mix.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
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

std::vector<int> mix_sequence(const ColorMix& mix) {
    if (mix.gradient)
        return {};
    if (!mix.pattern.empty())
        return mix.pattern;
    std::vector<std::pair<int, double>> parts;
    for (const ColorMix::Component& c : mix.components)
        parts.push_back({c.extruder, c.share});
    return parts.empty() ? std::vector<int>{} : cycle_of(parts);
}

std::vector<int> mix_layers(const ColorMix& mix, const std::vector<Layer>& layers, const std::vector<char>& present,
                            double step) {
    const std::size_t n = layers.size();
    std::vector<int> out(n, 0);
    const auto in = [&](std::size_t l) { return l < present.size() && present[l]; };
    if (step > 0.0 && !mix.gradient && n > 0) { // by height (rule 9)
        const std::vector<int> cycle = mix_sequence(mix);
        if (cycle.empty())
            return out;
        const double base = layers.front().bottom;
        for (std::size_t l = 0; l < n; ++l)
            if (in(l)) {
                const double z = 0.5 * (layers[l].bottom + layers[l].top) - base;
                const auto k = static_cast<std::size_t>(std::max(0.0, std::floor(z / step + 1e-9)));
                out[l] = cycle[k % cycle.size()];
            }
        return out;
    }
    if (!mix.pattern.empty()) { // a written sequence, as it is (color_mixing_plus.md, rule 2)
        std::size_t first = n;
        for (std::size_t l = 0; l < n; ++l)
            if (in(l)) {
                first = std::min(first, l);
                out[l] = mix.pattern[(l - first) % mix.pattern.size()];
            }
        return out;
    }
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
    if (mix.id <= extruders || mix.components.empty() || !mix.enabled)
        return false;
    const auto real = [&](int e) { return e >= 1 && e <= extruders; };
    return std::all_of(mix.components.begin(), mix.components.end(), [&](const ColorMix::Component& c) { return real(c.extruder); }) &&
           std::all_of(mix.pattern.begin(), mix.pattern.end(), real);
}

std::vector<int> parse_mix_pattern(std::string_view text, const std::vector<int>& letters, int extruders) {
    std::vector<int> out;
    for (std::size_t i = 0; i < text.size();) {
        const char c = text[i];
        if (c == ' ' || c == ',' || c == '/' || c == '-' || c == '\t') {
            ++i;
            continue;
        }
        int filament = 0;
        if (c >= '0' && c <= '9') {
            while (i < text.size() && text[i] >= '0' && text[i] <= '9' && filament < 100000)
                filament = filament * 10 + (text[i++] - '0');
        } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            const std::size_t k = std::size_t((c | 0x20) - 'a');
            if (letters.empty())
                filament = int(k) + 1;
            else if (k < letters.size())
                filament = letters[k];
            else
                throw ColorMixError(std::string("the sequence's ") + char(c & ~0x20) + " is not one of the blend's " +
                                    std::to_string(letters.size()) + " filaments");
            ++i;
        } else {
            throw ColorMixError(std::string("the sequence has '") + c + "': only filament numbers or letters");
        }
        if (filament < 1 || filament > extruders)
            throw ColorMixError("the sequence names filament " + std::to_string(filament) + ", but there are " +
                                std::to_string(extruders));
        out.push_back(filament);
    }
    if (out.empty())
        throw ColorMixError("the sequence is empty");
    return out;
}

ColorMix pattern_mix(int id, const std::vector<int>& pattern) {
    ColorMix mix;
    mix.id = id;
    mix.pattern = pattern;
    for (const int f : pattern) {
        auto it = std::find_if(mix.components.begin(), mix.components.end(), [&](const auto& c) { return c.extruder == f; });
        if (it == mix.components.end())
            mix.components.push_back({f, 0.0}), it = mix.components.end() - 1;
        it->share += 1.0 / double(pattern.size());
    }
    return mix;
}

std::vector<ColorMix> auto_pair_mixes(const std::vector<std::string>& filament_colors) {
    std::vector<ColorMix> out;
    const int n = int(filament_colors.size());
    int id = n;
    for (int a = 1; a <= n; ++a)
        for (int b = a + 1; b <= n; ++b) {
            ColorMix mix;
            mix.id = ++id;
            mix.components = {{a, 0.5}, {b, 0.5}};
            mix.color = mix_color({filament_colors[std::size_t(a - 1)], filament_colors[std::size_t(b - 1)]}, {0.5, 0.5});
            out.push_back(std::move(mix));
        }
    return out;
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
        if (v.contains("enabled") && v.at("enabled").is_boolean())
            mix.enabled = v.at("enabled").get<bool>();
        if (v.contains("name") && v.at("name").is_string())
            mix.name = v.at("name").get<std::string>();
        if (v.contains("bias") && v.at("bias").is_number())
            mix.bias = std::clamp(v.at("bias").get<double>(), -1.0, 1.0);
        if (kind == "pattern") {
            if (!v.contains("pattern") || !v.at("pattern").is_array() || v.at("pattern").empty())
                fail("a pattern blend needs its pattern, a list of filaments");
            std::vector<int> pattern;
            for (const json& f : v.at("pattern")) {
                if (!f.is_number_integer() || f.get<int>() < 1 || f.get<int>() > 255)
                    fail("a pattern's filaments are numbers from 1");
                pattern.push_back(f.get<int>());
            }
            ColorMix made = pattern_mix(mix.id, pattern);
            mix.pattern = std::move(made.pattern);
            mix.components = std::move(made.components);
            if (mix.components.size() > 4)
                fail("a pattern blend has 1 to 4 filaments");
            out.push_back(std::move(mix));
            continue;
        }
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
        v["kind"] = m.gradient ? "gradient" : !m.pattern.empty() ? "pattern" : "fullspectrum";
        v["color"] = m.color;
        if (!m.pattern.empty())
            v["pattern"] = m.pattern;
        if (!m.enabled)
            v["enabled"] = false;
        if (!m.name.empty())
            v["name"] = m.name;
        if (m.bias != 0.0)
            v["bias"] = m.bias;
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

int mix_changes_estimate(const ColorMix& mix, double height, const MixChangeOptions& o) {
    if (!(height > 0.0) || !(o.layer_height > 0.0) || !(o.first_layer_height > 0.0))
        return 0;
    // Each filament with its own height (rule 8): one change a layer, with
    // the layers it makes, as slice_model lays them.
    if (o.cadence_heights && !mix.gradient && mix.pattern.empty()) {
        int two = 0;
        for (const ColorMix::Component& c : mix.components)
            two += c.share > 0.0;
        if (two == 2) {
            const double ha = o.height_a > 0.0 ? o.height_a : o.layer_height;
            const double hb = o.height_b > 0.0 ? o.height_b : o.layer_height;
            int layers = 1;
            for (double z = o.first_layer_height; z < height - 1e-9 && layers < 100000; ++layers)
                z += layers % 2 ? hb : ha;
            return layers - 1;
        }
    }
    std::vector<Layer> layers;
    for (double z = 0.0; z < height - 1e-9 && layers.size() < 100000;) {
        const double h = layers.empty() ? o.first_layer_height : o.layer_height;
        layers.push_back({z, z + h});
        z += h;
    }
    const std::vector<int> cycle = mix_layers(mix, layers, std::vector<char>(layers.size(), 1));
    // The passes of a subdivided layer, as the slicing makes them.
    std::vector<std::pair<int, double>> parts;
    double total = 0.0;
    if (!mix.gradient)
        for (const ColorMix::Component& c : mix.pattern.empty() ? mix.components : pattern_mix(mix.id, mix.pattern).components)
            if (c.share > 0.0)
                parts.push_back({c.extruder, c.share}), total += c.share;
    int changes = 0, last = 0;
    const auto use = [&](int f) {
        if (f <= 0)
            return;
        changes += last != 0 && f != last;
        last = f;
    };
    for (std::size_t l = 0; l < layers.size(); ++l) {
        const double h = layers[l].top - layers[l].bottom;
        bool split = o.subdivide && parts.size() >= 2 && total > 0.0 && (l > 0 || o.sub_first_layer);
        for (const auto& p : parts)
            split = split && p.second / total * h >= o.sub_min_height - 1e-9;
        if (!split) {
            use(l < cycle.size() ? cycle[l] : 0);
            continue;
        }
        // The filament the layer below ended with first.
        std::vector<std::pair<int, double>> order = parts;
        if (const auto it = std::find_if(order.begin(), order.end(), [&](const auto& p) { return p.first == last; });
            it != order.end())
            std::rotate(order.begin(), it, it + 1);
        for (const auto& p : order)
            use(p.first);
    }
    return changes;
}

namespace {

// sRGB "#rrggbb" to CIELAB with the D65 white (IEC 61966-2-1 and CIE 15).
std::array<double, 3> lab_of(const std::string& hex) {
    double rgb[3] = {0.5, 0.5, 0.5};
    if (hex.size() == 7 && hex[0] == '#' &&
        std::all_of(hex.begin() + 1, hex.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }))
        for (int i = 0; i < 3; ++i)
            rgb[i] = double(std::stoi(hex.substr(1 + 2 * std::size_t(i), 2), nullptr, 16)) / 255.0;
    for (double& c : rgb)
        c = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    const double x = (0.4124 * rgb[0] + 0.3576 * rgb[1] + 0.1805 * rgb[2]) / 0.95047;
    const double y = 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
    const double z = (0.0193 * rgb[0] + 0.1192 * rgb[1] + 0.9505 * rgb[2]) / 1.08883;
    const auto f = [](double t) { return t > 216.0 / 24389.0 ? std::cbrt(t) : (24389.0 / 27.0 * t + 16.0) / 116.0; };
    const double fx = f(x), fy = f(y), fz = f(z);
    return {116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

} // namespace

double purge_from_colors(const std::string& from, const std::string& to) {
    const std::array<double, 3> a = lab_of(from), b = lab_of(to);
    const double lighter = std::max(0.0, b[0] - a[0]);
    const double apart = std::hypot(b[1] - a[1], b[2] - a[2]);
    if (lighter < 1e-9 && apart < 1e-9 && std::abs(b[0] - a[0]) < 1e-9)
        return 0.0;
    return std::min(300.0, 60.0 + 1.6 * lighter + 0.6 * apart);
}

std::vector<std::vector<double>> purge_table(const std::vector<std::string>& colors) {
    std::vector<std::vector<double>> out(colors.size(), std::vector<double>(colors.size(), 0.0));
    for (std::size_t i = 0; i < colors.size(); ++i)
        for (std::size_t j = 0; j < colors.size(); ++j)
            out[i][j] = i == j ? 0.0 : purge_from_colors(colors[i], colors[j]);
    return out;
}

std::vector<MixSuggestion> mix_purge_suggestions(const ColorMix& mix, double height, const MixChangeOptions& o,
                                                 double purge, double part, double max_height) {
    std::vector<MixSuggestion> out;
    const auto share = [&](int changes) {
        const double p = std::max(0.0, purge) * changes;
        return p + part > 0.0 ? p / (p + std::max(0.0, part)) : 0.0;
    };
    const int now = mix_changes_estimate(mix, height, o);
    if (share(now) <= 0.5)
        return out;
    if (o.subdivide) {
        MixChangeOptions off = o;
        off.subdivide = false;
        const int c = mix_changes_estimate(mix, height, off);
        out.push_back({"subdivide", c, share(c)});
    }
    if (!mix.gradient && mix.pattern.empty() && mix.components.size() == 2 && o.layer_height > 0.0) {
        // The larger share's layers twice as high, the other's as they are:
        // one change a layer, with fewer layers.
        double tall = 2.0 * o.layer_height;
        if (max_height > 0.0)
            tall = std::min(tall, max_height);
        const double pair = tall + o.layer_height;
        const int layers = int(std::ceil(std::max(0.0, height - o.first_layer_height) / pair * 2.0)) + 1;
        const int c = std::max(0, layers - 1);
        if (c < now)
            out.push_back({"cadence_heights", c, share(c)});
    }
    // The same changes, the purge into the infill the shell hides (rules 13 and 33).
    out.push_back({"surface_only", now, share(now)});
    return out;
}

double ciede2000(const std::string& first, const std::string& second) { return ciede2000_lab(lab_of(first), lab_of(second)); }

double ciede2000_lab(const std::array<double, 3>& a, const std::array<double, 3>& b) {
    constexpr double pi = 3.14159265358979323846;
    const auto rad = [](double d) { return d * pi / 180.0; };
    const auto deg = [](double r) { return r * 180.0 / pi; };
    const double c1 = std::hypot(a[1], a[2]), c2 = std::hypot(b[1], b[2]);
    const double cbar = 0.5 * (c1 + c2);
    const double c7 = std::pow(cbar, 7.0);
    const double g = 0.5 * (1.0 - std::sqrt(c7 / (c7 + std::pow(25.0, 7.0))));
    const double a1 = (1.0 + g) * a[1], a2 = (1.0 + g) * b[1];
    const double cp1 = std::hypot(a1, a[2]), cp2 = std::hypot(a2, b[2]);
    const auto hue = [&](double x, double y) {
        if (x == 0.0 && y == 0.0)
            return 0.0;
        double h = deg(std::atan2(y, x));
        return h < 0.0 ? h + 360.0 : h;
    };
    const double h1 = hue(a1, a[2]), h2 = hue(a2, b[2]);
    const double dl = b[0] - a[0], dc = cp2 - cp1;
    double dh = 0.0;
    if (cp1 * cp2 != 0.0) {
        dh = h2 - h1;
        if (dh > 180.0)
            dh -= 360.0;
        else if (dh < -180.0)
            dh += 360.0;
    }
    const double dH = 2.0 * std::sqrt(cp1 * cp2) * std::sin(rad(dh / 2.0));
    const double lbar = 0.5 * (a[0] + b[0]), cpbar = 0.5 * (cp1 + cp2);
    double hbar = h1 + h2;
    if (cp1 * cp2 != 0.0) {
        if (std::abs(h1 - h2) <= 180.0)
            hbar = 0.5 * (h1 + h2);
        else
            hbar = h1 + h2 < 360.0 ? 0.5 * (h1 + h2 + 360.0) : 0.5 * (h1 + h2 - 360.0);
    }
    const double t = 1.0 - 0.17 * std::cos(rad(hbar - 30.0)) + 0.24 * std::cos(rad(2.0 * hbar)) +
                     0.32 * std::cos(rad(3.0 * hbar + 6.0)) - 0.20 * std::cos(rad(4.0 * hbar - 63.0));
    const double dtheta = 30.0 * std::exp(-std::pow((hbar - 275.0) / 25.0, 2.0));
    const double cp7 = std::pow(cpbar, 7.0);
    const double rc = 2.0 * std::sqrt(cp7 / (cp7 + std::pow(25.0, 7.0)));
    const double sl = 1.0 + 0.015 * std::pow(lbar - 50.0, 2.0) / std::sqrt(20.0 + std::pow(lbar - 50.0, 2.0));
    const double sc = 1.0 + 0.045 * cpbar, sh = 1.0 + 0.015 * cpbar * t;
    const double rt = -std::sin(rad(2.0 * dtheta)) * rc;
    return std::sqrt(std::pow(dl / sl, 2.0) + std::pow(dc / sc, 2.0) + std::pow(dH / sh, 2.0) +
                     rt * (dc / sc) * (dH / sh));
}

std::vector<MixMatch> match_color(const std::string& target, const std::vector<std::string>& colors, int id, int max_cycle,
                                  double min_part, const std::vector<double>& transmission) {
    std::vector<MixMatch> all;
    const int n = int(colors.size());
    max_cycle = std::clamp(max_cycle, 2, 64);
    const auto add = [&](const std::vector<int>& filaments, const std::vector<int>& counts) {
        const int sum = std::accumulate(counts.begin(), counts.end(), 0);
        int common = 0;
        for (const int c : counts)
            common = std::gcd(common, c);
        if (common != 1) // the same blend as a shorter cycle
            return;
        ColorMix mix;
        mix.id = id;
        std::vector<std::string> cs;
        std::vector<double> shares;
        std::vector<double> tds;
        for (std::size_t k = 0; k < filaments.size(); ++k) {
            const double share = double(counts[k]) / sum;
            if (share < min_part - 1e-12)
                return;
            mix.components.push_back({filaments[k] + 1, share});
            cs.push_back(colors[std::size_t(filaments[k])]);
            shares.push_back(share);
            const auto f = std::size_t(filaments[k]);
            tds.push_back(f < transmission.size() ? transmission[f] : 0.0);
        }
        MixMatch m{mix, transmission.empty() ? mix_color(cs, shares) : mix_color_spectral(cs, shares, tds), 0.0};
        m.distance = ciede2000(target, m.color);
        mix.color = m.color;
        m.mix.color = m.color;
        all.push_back(std::move(m));
    };
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j) {
            for (int a = 1; a < max_cycle; ++a)
                for (int b = 1; a + b <= max_cycle; ++b)
                    add({i, j}, {a, b});
            for (int k = j + 1; k < n; ++k)
                for (int a = 1; a < max_cycle; ++a)
                    for (int b = 1; a + b < max_cycle; ++b)
                        for (int c = 1; a + b + c <= max_cycle; ++c)
                            add({i, j, k}, {a, b, c});
        }
    std::stable_sort(all.begin(), all.end(), [](const MixMatch& x, const MixMatch& y) { return x.distance < y.distance; });
    if (all.size() > 4)
        all.resize(4);
    return all;
}

std::string mix_color_spectral(const std::vector<std::string>& colors, const std::vector<double>& shares,
                               const std::vector<double>& transmission) {
    const auto td = [&](std::size_t k) { return k < transmission.size() ? std::max(0.0, transmission[k]) : 0.0; };
    bool any = false;
    for (std::size_t k = 0; k < colors.size(); ++k)
        any = any || td(k) > 0.0;
    if (!any)
        return mix_color(colors, shares);
    double ks[3] = {0.0, 0.0, 0.0}, total = 0.0;
    for (std::size_t k = 0; k < colors.size() && k < shares.size(); ++k) {
        unsigned value = 0;
        const std::string& c = colors[k];
        if (c.size() != 7 || c[0] != '#' || std::sscanf(c.c_str() + 1, "%6x", &value) != 1 || !(shares[k] > 0.0))
            continue;
        const double w = shares[k] * (1.0 + td(k) / 10.0);
        for (int ch = 0; ch < 3; ++ch) {
            double r = double((value >> (16 - 8 * ch)) & 0xFFu) / 255.0;
            r = r <= 0.04045 ? r / 12.92 : std::pow((r + 0.055) / 1.055, 2.4); // linear reflectance
            r = std::clamp(r, 1e-4, 1.0);
            ks[ch] += w * (1.0 - r) * (1.0 - r) / (2.0 * r);
        }
        total += w;
    }
    if (total <= 0.0)
        return "#808080";
    char text[8];
    const auto channel = [&](int ch) {
        const double k = ks[ch] / total;
        double r = 1.0 + k - std::sqrt(k * k + 2.0 * k);
        r = r <= 0.0031308 ? 12.92 * r : 1.055 * std::pow(r, 1.0 / 2.4) - 0.055;
        return unsigned(std::lround(std::clamp(r, 0.0, 1.0) * 255.0));
    };
    std::snprintf(text, sizeof text, "#%02x%02x%02x", channel(0), channel(1), channel(2));
    return text;
}

} // namespace tisma

namespace tisma {

ColorName color_name(const std::string& hex) {
    const auto lab = lab_of(hex);
    const double l = lab[0], chroma = std::hypot(lab[1], lab[2]);
    if (chroma < 10.0) {
        if (l > 90.0)
            return {"White", "Blanco"};
        if (l > 65.0)
            return {"Light grey", "Gris claro"};
        if (l > 40.0)
            return {"Grey", "Gris"};
        if (l > 20.0)
            return {"Dark grey", "Gris oscuro"};
        return {"Black", "Negro"};
    }
    double h = std::atan2(lab[2], lab[1]) * 180.0 / 3.14159265358979323846;
    if (h < 0.0)
        h += 360.0;
    struct Band {
        double from;
        const char *en, *es;
        bool warm; // dark ones are brown
    };
    static const Band bands[] = {{25.0, "red", "rojo", true},       {50.0, "orange", "naranja", true},
                                 {75.0, "yellow", "amarillo", true}, {110.0, "green", "verde", false},
                                 {165.0, "cyan", "cian", false},     {230.0, "blue", "azul", false},
                                 {315.0, "purple", "violeta", false}, {340.0, "pink", "rosa", false}};
    const Band* band = &bands[7]; // from 340 round to 25
    for (const Band& b : bands)
        if (h >= b.from)
            band = &b;
    if (band->warm && l < 45.0)
        return {l < 25.0 ? "Dark brown" : "Brown", l < 25.0 ? "Marrón oscuro" : "Marrón"};
    std::string en = band->en, es = band->es;
    if (l > 70.0)
        return {"Light " + en, es + " claro"};
    if (l < 35.0)
        return {"Dark " + en, es + " oscuro"};
    en[0] = char(std::toupper(static_cast<unsigned char>(en[0])));
    es[0] = char(std::toupper(static_cast<unsigned char>(es[0])));
    return {en, es};
}

std::vector<PaletteSample> quick_palette(const std::vector<std::string>& colors, int max_cycle, int first_id,
                                         const std::vector<double>& transmission) {
    std::vector<PaletteSample> out;
    const int n = int(colors.size());
    max_cycle = std::clamp(max_cycle, 1, 64);
    int id = first_id;
    const auto add = [&](const std::vector<int>& filaments, const std::vector<int>& counts) {
        int common = 0, sum = 0;
        for (const int c : counts) {
            common = std::gcd(common, c);
            sum += c;
        }
        if (common != 1 || sum > max_cycle)
            return;
        PaletteSample s;
        s.mix.id = id++;
        std::vector<std::string> cs;
        std::vector<double> shares, tds;
        for (std::size_t k = 0; k < filaments.size(); ++k) {
            const auto f = std::size_t(filaments[k]);
            s.mix.components.push_back({filaments[k] + 1, double(counts[k]) / sum});
            cs.push_back(colors[f]);
            shares.push_back(double(counts[k]) / sum);
            tds.push_back(f < transmission.size() ? transmission[f] : 0.0);
        }
        s.color = transmission.empty() ? mix_color(cs, shares) : mix_color_spectral(cs, shares, tds);
        s.mix.color = s.color;
        s.name = color_name(s.color);
        s.mix.name = s.name.en;
        out.push_back(std::move(s));
    };
    for (int i = 0; i < n; ++i)
        add({i}, {1});
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            for (int a = 1; a < max_cycle; ++a)
                for (int b = 1; a + b <= max_cycle; ++b)
                    add({i, j}, {a, b});
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            for (int k = j + 1; k < n; ++k)
                for (int a = 1; a < max_cycle; ++a)
                    for (int b = 1; a + b < max_cycle; ++b)
                        for (int c = 1; a + b + c <= max_cycle; ++c)
                            add({i, j, k}, {a, b, c});
    return out;
}

} // namespace tisma
