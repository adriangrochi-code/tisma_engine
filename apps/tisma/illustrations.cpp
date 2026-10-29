// Drawings for the information cards and the tutorial. Own design: plain
// shapes in the box, scaled with it; see PROVENANCE.md.

#include "illustrations.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace tisma {

Picture picture_for(std::string_view key) {
    static constexpr std::pair<std::string_view, Picture> kPictures[] = {
        {"layers.height", Picture::LayerHeight},
        {"layers.first_height", Picture::FirstLayer},
        {"walls.count", Picture::Walls},
        {"walls.seam", Picture::Seam},
        {"surfaces.top_layers", Picture::TopBottom},
        {"surfaces.bottom_layers", Picture::TopBottom},
        {"surfaces.top_min_thickness", Picture::TopBottom},
        {"surfaces.bottom_min_thickness", Picture::TopBottom},
        {"surfaces.top_pattern", Picture::TopBottom},
        {"infill.density", Picture::Infill},
        {"infill.pattern", Picture::Infill},
        {"support.enabled", Picture::Support},
        {"support.automatic", Picture::Support},
        {"adhesion.skirt_loops", Picture::Skirt},
        {"adhesion.skirt_distance", Picture::Skirt},
        {"adhesion.brim_width", Picture::Brim},
        {"machine.variables.temperature", Picture::NozzleTemp},
        {"machine.variables.bed_temperature", Picture::BedTemp},
        {"cooling.fan_min", Picture::Fan},
        {"cooling.fan_max", Picture::Fan},
        {"retraction.length", Picture::Retraction},
        {"retraction.lift", Picture::Lift},
        {"machine.nozzle_diameter", Picture::Nozzle},
        {"machine.filament_diameter", Picture::Filament},
        {"machine.bed", Picture::Bed},
        {"machine.max_height", Picture::Bed},
    };
    for (const auto& [k, picture] : kPictures)
        if (k == key)
            return picture;
    return Picture::None;
}

namespace {

// A box of the drawing in its own units: 0 to 1 across, 0 to 1 down.
struct Canvas {
    ImDrawList* dl;
    ImVec2 min, size;
    float unit; // a line's width
    ImVec2 at(float x, float y) const { return ImVec2(min.x + x * size.x, min.y + y * size.y); }
    void line(float x0, float y0, float x1, float y1, ImU32 c, float w = 1.0f) const {
        dl->AddLine(at(x0, y0), at(x1, y1), c, w * unit);
    }
    void rect(float x0, float y0, float x1, float y1, ImU32 fill, float round = 0.0f) const {
        dl->AddRectFilled(at(x0, y0), at(x1, y1), fill, round * unit);
    }
    void frame(float x0, float y0, float x1, float y1, ImU32 c, float w = 1.0f, float round = 0.0f) const {
        dl->AddRect(at(x0, y0), at(x1, y1), c, round * unit, w * unit);
    }
    // An arrow from one point to the other, its head at the second.
    void arrow(float x0, float y0, float x1, float y1, ImU32 c, float w = 1.5f) const {
        const ImVec2 a = at(x0, y0), b = at(x1, y1);
        dl->AddLine(a, b, c, w * unit);
        const float dx = b.x - a.x, dy = b.y - a.y, len = std::max(1.0f, std::hypot(dx, dy));
        const float ux = dx / len, uy = dy / len, head = 6.0f * unit;
        dl->AddTriangleFilled(b, ImVec2(b.x - ux * head - uy * head * 0.6f, b.y - uy * head + ux * head * 0.6f),
                              ImVec2(b.x - ux * head + uy * head * 0.6f, b.y - uy * head - ux * head * 0.6f), c);
    }
    // The nozzle: a block and a cone whose tip is at (x, y), `w` wide.
    void nozzle(float x, float y, float w, const PictureColors& k) const {
        const float h = w * size.x / size.y;
        rect(x - w * 0.5f, y - h * 1.3f, x + w * 0.5f, y - h * 0.55f, k.dim, 2.0f);
        dl->AddTriangleFilled(at(x - w * 0.42f, y - h * 0.55f), at(x + w * 0.42f, y - h * 0.55f), at(x, y), k.line);
    }
    // Waves of heat rising from (x, y).
    void heat(float x, float y, float height, ImU32 c) const {
        for (int i = -1; i <= 1; ++i) {
            const float cx = x + float(i) * 0.035f;
            for (int k = 0; k < 6; ++k) {
                const float t0 = float(k) / 6.0f, t1 = float(k + 1) / 6.0f;
                const float s0 = std::sin(t0 * 2.0f * std::numbers::pi_v<float>) * 0.008f;
                const float s1 = std::sin(t1 * 2.0f * std::numbers::pi_v<float>) * 0.008f;
                line(cx + s0, y - t0 * height, cx + s1, y - t1 * height, c, 1.4f);
            }
        }
    }
};

// A part's outline seen from above: a rounded rectangle, grown by `grow`.
void outline(const Canvas& c, float x0, float y0, float x1, float y1, float grow, ImU32 col, float w) {
    c.frame(x0 - grow, y0 - grow * c.size.x / c.size.y, x1 + grow, y1 + grow * c.size.x / c.size.y, col, w, 10.0f);
}

} // namespace

void draw_picture(ImDrawList* dl, ImVec2 min, ImVec2 size, Picture picture, const PictureColors& k) {
    const Canvas c{dl, min, size, std::max(1.0f, size.y / 120.0f)};
    const float bed_y = 0.88f;
    const auto bed = [&](float x0 = 0.04f, float x1 = 0.96f) { c.rect(x0, bed_y, x1, bed_y + 0.05f, k.dim, 2.0f); };
    switch (picture) {
    case Picture::None: break;
    case Picture::Slicing: {
        // A part (a rounded hill), then the same cut into layers, then lines.
        for (int i = 0; i < 24; ++i) {
            const float t = float(i) / 24.0f, half = 0.13f * std::sqrt(1.0f - t * t);
            c.rect(0.18f - half, bed_y - (t + 1.0f / 24.0f) * 0.7f, 0.18f + half, bed_y - t * 0.7f, k.soft);
        }
        c.arrow(0.34f, 0.5f, 0.42f, 0.5f, k.accent);
        for (int i = 0; i < 8; ++i) {
            const float t = float(i) / 8.0f, half = 0.13f * std::sqrt(1.0f - t * t);
            c.rect(0.58f - half, bed_y - (t + 0.1f) * 0.7f, 0.58f + half, bed_y - (t + 0.1f) * 0.7f + 0.055f, k.accent, 3.0f);
        }
        c.arrow(0.74f, 0.5f, 0.82f, 0.5f, k.accent);
        for (int i = 0; i < 6; ++i)
            c.line(0.85f, 0.22f + float(i) * 0.1f, 0.85f + (i % 2 ? 0.08f : 0.11f), 0.22f + float(i) * 0.1f, k.line, 1.6f);
        bed(0.04f, 0.74f);
        break;
    }
    case Picture::LayerHeight: {
        // The same height in thin layers (left) and in thick ones (right).
        for (int i = 0; i < 10; ++i)
            c.rect(0.12f, bed_y - (float(i) + 1.0f) * 0.065f, 0.42f, bed_y - float(i) * 0.065f - 0.008f, k.accent, 3.0f);
        for (int i = 0; i < 4; ++i)
            c.rect(0.58f, bed_y - (float(i) + 1.0f) * 0.1625f, 0.88f, bed_y - float(i) * 0.1625f - 0.012f, k.soft, 6.0f);
        c.nozzle(0.27f, bed_y - 0.67f, 0.05f, k);
        c.nozzle(0.73f, bed_y - 0.67f, 0.05f, k);
        bed();
        break;
    }
    case Picture::FirstLayer: {
        for (int i = 0; i < 5; ++i)
            c.rect(0.25f, bed_y - 0.16f - (float(i) + 1.0f) * 0.11f, 0.75f, bed_y - 0.16f - float(i) * 0.11f - 0.012f, k.soft, 5.0f);
        c.rect(0.25f, bed_y - 0.16f, 0.75f, bed_y - 0.005f, k.accent, 6.0f); // the first, thicker and pressed
        c.arrow(0.84f, bed_y - 0.4f, 0.77f, bed_y - 0.08f, k.accent);
        bed();
        break;
    }
    case Picture::Walls: {
        // From above: the outer wall in violet, the inner ones lighter, infill inside.
        outline(c, 0.3f, 0.2f, 0.7f, 0.8f, 0.0f, k.accent, 3.0f);
        outline(c, 0.3f, 0.2f, 0.7f, 0.8f, -0.025f, k.soft, 3.0f);
        outline(c, 0.3f, 0.2f, 0.7f, 0.8f, -0.05f, k.soft, 3.0f);
        for (int i = 0; i < 5; ++i) {
            const float x = 0.39f + float(i) * 0.055f;
            c.line(x, 0.36f, x, 0.64f, k.dim, 1.0f);
        }
        break;
    }
    case Picture::TopBottom: {
        // A cut through the part: solid layers on top and at the bottom, sparse infill between.
        c.frame(0.2f, 0.12f, 0.8f, bed_y, k.line, 2.0f, 4.0f);
        for (int i = 0; i < 3; ++i) {
            c.rect(0.21f, 0.13f + float(i) * 0.06f, 0.79f, 0.17f + float(i) * 0.06f, k.accent);
            c.rect(0.21f, bed_y - 0.05f - float(i) * 0.06f, 0.79f, bed_y - 0.01f - float(i) * 0.06f, k.accent);
        }
        for (int i = 0; i < 7; ++i) {
            const float x = 0.24f + float(i) * 0.08f;
            c.line(x, 0.32f, x + 0.04f, 0.5f, k.dim, 1.4f);
            c.line(x + 0.04f, 0.5f, x, 0.68f, k.dim, 1.4f);
        }
        bed();
        break;
    }
    case Picture::Infill: {
        // Three squares from above: a little, some and much infill.
        const float xs[3] = {0.08f, 0.38f, 0.68f};
        const int lines[3] = {2, 5, 10};
        for (int s = 0; s < 3; ++s) {
            const float x0 = xs[s], x1 = x0 + 0.24f, y0 = 0.2f, y1 = 0.8f;
            c.frame(x0, y0, x1, y1, k.accent, 3.0f, 4.0f);
            for (int i = 1; i <= lines[s]; ++i) {
                const float t = float(i) / float(lines[s] + 1);
                c.line(x0 + t * (x1 - x0), y0 + 0.02f, x0 + t * (x1 - x0), y1 - 0.02f, k.dim, 1.2f);
                c.line(x0 + 0.01f, y0 + t * (y1 - y0), x1 - 0.01f, y0 + t * (y1 - y0), k.dim, 1.2f);
            }
        }
        break;
    }
    case Picture::Seam: {
        // Layers seen from the side, each starting at the same point: a seam.
        for (int i = 0; i < 9; ++i) {
            const float y = bed_y - (float(i) + 1.0f) * 0.085f;
            c.rect(0.2f, y, 0.8f, y + 0.07f, k.soft, 4.0f);
            c.dl->AddCircleFilled(c.at(0.62f, y + 0.035f), 3.2f * c.unit, k.accent);
        }
        c.line(0.62f, bed_y - 0.78f, 0.62f, bed_y - 0.02f, k.accent, 1.0f);
        bed();
        break;
    }
    case Picture::Support: {
        // A shelf that hangs in the air, held by supports from the bed.
        c.rect(0.2f, 0.3f, 0.34f, bed_y, k.soft, 4.0f);
        c.rect(0.2f, 0.18f, 0.8f, 0.3f, k.soft, 4.0f);
        for (int i = 0; i < 7; ++i) {
            const float x = 0.4f + float(i) * 0.06f;
            c.line(x, 0.31f, x, bed_y, k.accent, 2.0f);
        }
        c.line(0.38f, 0.32f, 0.79f, 0.32f, k.accent, 1.2f);
        bed();
        break;
    }
    case Picture::Skirt:
    case Picture::Brim: {
        // From above: the part's foot and, around it, loops apart (skirt) or touching (brim).
        c.rect(0.38f, 0.32f, 0.62f, 0.68f, k.soft, 6.0f);
        if (picture == Picture::Skirt) {
            outline(c, 0.38f, 0.32f, 0.62f, 0.68f, 0.09f, k.accent, 2.5f);
        } else {
            for (int i = 1; i <= 5; ++i)
                outline(c, 0.38f, 0.32f, 0.62f, 0.68f, 0.012f * float(i), k.accent, 2.0f);
        }
        break;
    }
    case Picture::NozzleTemp: {
        c.rect(0.4f, 0.04f, 0.46f, 0.3f, k.dim, 2.0f); // the filament going in
        c.nozzle(0.43f, 0.72f, 0.14f, k);
        c.heat(0.3f, 0.68f, 0.25f, k.hot);
        c.heat(0.56f, 0.68f, 0.25f, k.hot);
        c.rect(0.4f, 0.74f, 0.46f, bed_y, k.accent, 2.0f); // the melted line coming out
        // A thermometer.
        c.frame(0.78f, 0.16f, 0.82f, 0.7f, k.line, 1.5f, 6.0f);
        c.rect(0.785f, 0.38f, 0.815f, 0.7f, k.hot, 4.0f);
        c.dl->AddCircleFilled(c.at(0.8f, 0.76f), 0.055f * c.size.y, k.hot);
        break;
    }
    case Picture::BedTemp: {
        c.rect(0.3f, bed_y - 0.3f, 0.7f, bed_y, k.soft, 4.0f);
        c.rect(0.3f, bed_y - 0.06f, 0.7f, bed_y, k.accent, 2.0f);
        c.rect(0.04f, bed_y, 0.96f, bed_y + 0.08f, k.hot, 2.0f);
        c.heat(0.15f, bed_y - 0.02f, 0.4f, k.hot);
        c.heat(0.85f, bed_y - 0.02f, 0.4f, k.hot);
        break;
    }
    case Picture::Fan: {
        // The fan blowing on the line just laid.
        const ImVec2 o = c.at(0.25f, 0.4f);
        const float r = 0.28f * c.size.y;
        c.dl->AddCircle(o, r, k.line, 32, 2.0f * c.unit);
        for (int b = 0; b < 4; ++b) {
            const float a = float(b) * std::numbers::pi_v<float> * 0.5f;
            c.dl->AddTriangleFilled(o, ImVec2(o.x + std::cos(a) * r * 0.9f, o.y + std::sin(a) * r * 0.9f),
                                    ImVec2(o.x + std::cos(a + 0.6f) * r * 0.9f, o.y + std::sin(a + 0.6f) * r * 0.9f), k.dim);
        }
        for (int i = 0; i < 3; ++i)
            c.arrow(0.42f, 0.32f + float(i) * 0.1f, 0.6f, 0.5f + float(i) * 0.1f, k.cold, 1.5f);
        c.rect(0.55f, bed_y - 0.3f, 0.92f, bed_y, k.soft, 4.0f);
        c.rect(0.55f, bed_y - 0.36f, 0.92f, bed_y - 0.3f, k.accent, 3.0f);
        c.nozzle(0.85f, bed_y - 0.37f, 0.06f, k);
        bed();
        break;
    }
    case Picture::Retraction: {
        // Before a travel the filament goes back up: no strings between the parts.
        c.rect(0.08f, bed_y - 0.3f, 0.3f, bed_y, k.soft, 4.0f);
        c.rect(0.7f, bed_y - 0.3f, 0.92f, bed_y, k.soft, 4.0f);
        c.nozzle(0.3f, bed_y - 0.36f, 0.07f, k);
        c.arrow(0.3f, bed_y - 0.55f, 0.3f, bed_y - 0.85f, k.accent, 2.0f);
        for (int i = 0; i < 9; ++i)
            c.line(0.34f + float(i) * 0.035f, bed_y - 0.36f, 0.355f + float(i) * 0.035f, bed_y - 0.36f, k.dim, 1.5f);
        c.arrow(0.66f, bed_y - 0.36f, 0.69f, bed_y - 0.36f, k.dim, 1.5f);
        bed();
        break;
    }
    case Picture::Lift: {
        c.rect(0.08f, bed_y - 0.3f, 0.3f, bed_y, k.soft, 4.0f);
        c.rect(0.4f, bed_y - 0.36f, 0.6f, bed_y, k.soft, 4.0f);
        c.rect(0.7f, bed_y - 0.3f, 0.92f, bed_y, k.soft, 4.0f);
        c.line(0.3f, bed_y - 0.32f, 0.33f, bed_y - 0.5f, k.accent, 2.0f);
        c.line(0.33f, bed_y - 0.5f, 0.67f, bed_y - 0.5f, k.accent, 2.0f);
        c.arrow(0.67f, bed_y - 0.5f, 0.7f, bed_y - 0.33f, k.accent, 2.0f);
        c.nozzle(0.33f, bed_y - 0.52f, 0.06f, k);
        bed();
        break;
    }
    case Picture::Nozzle: {
        c.nozzle(0.5f, 0.66f, 0.3f, k);
        c.rect(0.475f, 0.66f, 0.525f, bed_y, k.accent, 2.0f);
        c.arrow(0.4f, 0.8f, 0.47f, 0.8f, k.line, 1.2f);
        c.arrow(0.6f, 0.8f, 0.53f, 0.8f, k.line, 1.2f);
        break;
    }
    case Picture::Filament: {
        const ImVec2 o = c.at(0.3f, 0.48f);
        c.dl->AddCircle(o, 0.36f * c.size.y, k.line, 40, 2.0f * c.unit);
        c.dl->AddCircleFilled(o, 0.3f * c.size.y, k.soft, 40);
        c.dl->AddCircleFilled(o, 0.1f * c.size.y, k.dim, 24);
        c.line(0.3f, 0.48f - 0.3f, 0.86f, 0.18f, k.accent, 4.0f);
        c.arrow(0.8f, 0.36f, 0.8f, 0.22f, k.line, 1.2f);
        c.arrow(0.8f, 0.02f, 0.8f, 0.14f, k.line, 1.2f);
        break;
    }
    case Picture::Bed: {
        // The bed seen at an angle, its two sides and the height above it.
        const ImVec2 a = c.at(0.18f, 0.82f), b = c.at(0.7f, 0.82f), d = c.at(0.86f, 0.56f), e = c.at(0.34f, 0.56f);
        c.dl->AddQuadFilled(a, b, d, e, k.soft);
        c.dl->AddQuad(a, b, d, e, k.line, 1.5f * c.unit);
        c.arrow(0.18f, 0.92f, 0.7f, 0.92f, k.accent);
        c.arrow(0.74f, 0.86f, 0.9f, 0.6f, k.accent);
        c.arrow(0.34f, 0.54f, 0.34f, 0.08f, k.accent);
        break;
    }
    }
}

} // namespace tisma
