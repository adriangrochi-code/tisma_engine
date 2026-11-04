// DXF and SVG into a sketch, and the sketch out as DXF (cad_design.md, rules
// 19 bis and 20; CD22 to CD24).

#include "cad/sketch_io.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <string>

using namespace tisma::cad;
using Catch::Matchers::WithinAbs;

namespace {

std::string dxf(const std::string& header, const std::string& entities, const std::string& blocks = "") {
    std::string s = "0\nSECTION\n2\nHEADER\n" + header + "0\nENDSEC\n";
    if (!blocks.empty())
        s += "0\nSECTION\n2\nBLOCKS\n" + blocks + "0\nENDSEC\n";
    return s + "0\nSECTION\n2\nENTITIES\n" + entities + "0\nENDSEC\n0\nEOF\n";
}

// The box round a chain's points.
std::array<double, 4> box(const Entity& e) {
    std::array<double, 4> b{1e300, 1e300, -1e300, -1e300};
    for (const Pt& p : e.points)
        b = {std::min(b[0], p.x), std::min(b[1], p.y), std::max(b[2], p.x), std::max(b[3], p.y)};
    return b;
}

} // namespace

TEST_CASE("sketch io: CD22, a DXF in inches with a circle inside a rectangle", "[cad][dxf]") {
    const std::string text = dxf("9\n$INSUNITS\n70\n1\n",
                                 "0\nLWPOLYLINE\n8\n0\n90\n4\n70\n1\n10\n0\n20\n0\n10\n40\n20\n0\n10\n40\n20\n20\n10\n0\n20\n20\n"
                                 "0\nCIRCLE\n8\n0\n10\n20\n20\n10\n30\n0\n40\n5\n");
    const SketchImport r = read_dxf(text);
    REQUIRE(r.ok());
    REQUIRE(r.entities.size() == 2);
    const Entity& rect = r.entities[0];
    CHECK(rect.kind == Entity::Kind::Chain);
    CHECK(rect.closed);
    const auto b = box(rect);
    CHECK_THAT(b[2] - b[0], WithinAbs(1016.0, 1e-9));
    CHECK_THAT(b[3] - b[1], WithinAbs(508.0, 1e-9));
    const Entity& circle = r.entities[1];
    CHECK(circle.kind == Entity::Kind::Circle);
    CHECK_THAT(2 * circle.radius, WithinAbs(254.0, 1e-9));
    CHECK_THAT(circle.points[0].x, WithinAbs(508.0, 1e-9));
    // Extruding it gives a plate with a hole: the sketch has a region.
    Sketch s;
    s.entities = r.entities;
    CHECK(s.has_region());
}

TEST_CASE("sketch io: CD23, an SVG closed outline becomes a closed profile", "[cad][svg]") {
    const std::string svg = R"svg(<?xml version="1.0"?>
<!-- a logo -->
<svg xmlns="http://www.w3.org/2000/svg" width="40mm" height="20mm" viewBox="0 0 400 200">
  <g transform="translate(10,10)">
    <path d="M0,0 L380,0 L380,180 Q190,220 0,180 Z"/>
    <circle cx="100" cy="90" r="50"/>
  </g>
  <defs><rect width="5" height="5"/></defs>
  <text x="0" y="0">ignored</text>
</svg>)svg";
    const SketchImport r = read_svg(svg);
    REQUIRE(r.ok());
    REQUIRE(r.entities.size() == 2);
    CHECK(r.entities[0].closed);
    Sketch s;
    s.entities = {r.entities[0]};
    CHECK(s.has_region());
    // 400 user units are 40 mm: 0.1 mm each; the outline's 380 are 38 mm.
    const auto b = box(r.entities[0]);
    CHECK_THAT(b[2] - b[0], WithinAbs(38.0, 1e-6));
    CHECK(r.entities[1].kind == Entity::Kind::Circle);
    CHECK_THAT(r.entities[1].radius, WithinAbs(5.0, 1e-9));
    // y up: the circle's centre (110, 100) in user units is 10 mm from the
    // drawing's bottom edge (200 - 100 = 100 units).
    CHECK_THAT(r.entities[1].points[0].y, WithinAbs(10.0, 1e-9));
    CHECK_FALSE(r.notes.empty()); // the text was left out
}

TEST_CASE("sketch io: CD24, a sketch out as DXF and back", "[cad][dxf]") {
    Sketch s;
    Entity line;
    line.kind = Entity::Kind::Chain;
    line.points = {{0, 0}, {40, 0}};
    Entity arc;
    arc.kind = Entity::Kind::Arc;
    arc.points = {{20, 10}, {30, 10}, {20, 20}}; // a quarter, counter-clockwise
    arc.radius = 10;
    Entity circle;
    circle.kind = Entity::Kind::Circle;
    circle.points = {{5.123456, 7.5}};
    circle.radius = 2.25;
    Entity rect;
    rect.kind = Entity::Kind::Rectangle;
    rect.points = {{-10, -10}, {-2, -4}};
    rect.construction = true;
    s.entities = {line, arc, circle, rect};
    const std::string text = write_dxf(s);
    CHECK(text.find("$INSUNITS") != std::string::npos);
    const SketchImport r = read_dxf(text);
    REQUIRE(r.ok());
    REQUIRE(r.entities.size() == 4);
    CHECK_THAT(r.entities[0].points[1].x, WithinAbs(40.0, 1e-3));
    const Entity& a = r.entities[1];
    REQUIRE(a.kind == Entity::Kind::Arc);
    CHECK_THAT(a.radius, WithinAbs(10.0, 1e-3));
    CHECK_THAT(a.points[1].x, WithinAbs(30.0, 1e-3));
    CHECK_THAT(a.points[2].y, WithinAbs(20.0, 1e-3));
    CHECK_THAT(r.entities[2].points[0].x, WithinAbs(5.123456, 1e-3));
    CHECK_THAT(r.entities[2].radius, WithinAbs(2.25, 1e-3));
    CHECK(r.entities[3].closed);
    CHECK(r.entities[3].points.size() == 4);
    // A clockwise arc goes out counter-clockwise from its end: the same curve.
    Sketch cw;
    Entity back = arc;
    back.points = {{20, 10}, {20, 20}, {30, 10}};
    back.clockwise = true;
    cw.entities = {back};
    const SketchImport rc = read_dxf(write_dxf(cw));
    REQUIRE(rc.ok());
    CHECK_THAT(rc.entities[0].points[1].x, WithinAbs(30.0, 1e-3));
}

TEST_CASE("sketch io: DXF bulges, blocks with arrays, paper space and bad files", "[cad][dxf]") {
    // A slot: two lines and two half circles by bulge 1.
    const std::string slot = dxf("",
                                 "0\nLWPOLYLINE\n90\n4\n70\n1\n10\n0\n20\n0\n10\n10\n20\n0\n42\n1\n10\n10\n20\n4\n10\n0\n20\n4\n42\n1\n");
    const SketchImport s = read_dxf(slot);
    REQUIRE(s.ok());
    int arcs = 0;
    for (const Entity& e : s.entities)
        if (e.kind == Entity::Kind::Arc) {
            ++arcs;
            CHECK_THAT(e.radius, WithinAbs(2.0, 1e-9));
        }
    CHECK(arcs == 2);
    // A block of one circle, inserted twice the size, turned 90° and in a 2 by 3 array.
    const std::string blocks = "0\nBLOCK\n2\nHOLE\n10\n1\n20\n0\n0\nCIRCLE\n10\n1\n20\n0\n40\n1\n0\nENDBLK\n";
    const std::string inserted = dxf("", "0\nINSERT\n2\nHOLE\n10\n100\n20\n0\n41\n2\n42\n2\n50\n90\n70\n2\n71\n3\n44\n10\n45\n5\n"
                                         "0\nLINE\n67\n1\n10\n0\n20\n0\n11\n1\n21\n1\n",
                                     blocks);
    const SketchImport b = read_dxf(inserted);
    REQUIRE(b.ok());
    REQUIRE(b.entities.size() == 6);
    for (const Entity& e : b.entities)
        CHECK_THAT(e.radius, WithinAbs(2.0, 1e-9));
    // Column 1, row 0: 10 along the turned x axis (now +y); the spacing is in
    // drawing units, not scaled with the block.
    bool found = false;
    for (const Entity& e : b.entities)
        found |= std::abs(e.points[0].x - 100.0) < 1e-9 && std::abs(e.points[0].y - 10.0) < 1e-9;
    CHECK(found);
    CHECK_FALSE(b.notes.empty()); // the paper space line
    // Not DXF, empty, nothing in model space.
    CHECK_FALSE(read_dxf("hello\nworld\n").ok());
    CHECK_FALSE(read_dxf("").ok());
    CHECK_FALSE(read_dxf(dxf("", "0\nTEXT\n1\nhi\n")).ok());
    CHECK_FALSE(read_svg("<html></html>").ok());
    CHECK_FALSE(read_svg("<svg><path d=").ok());
}

TEST_CASE("sketch io: SVG arcs, curves and transforms", "[cad][svg]") {
    // A half circle by an arc command, scaled 2: radius 20 user units, 96 dpi.
    const SketchImport r = read_svg(R"svg(<svg><path transform="scale(2)" d="M0 0 A10 10 0 0 1 20 0 Z"/></svg>)svg");
    REQUIRE(r.ok());
    REQUIRE(r.entities.size() == 1);
    const auto b = box(r.entities[0]);
    const double unit = 25.4 / 96.0;
    CHECK_THAT(b[2] - b[0], WithinAbs(40 * unit, 1e-6));
    CHECK_THAT(b[3] - b[1], WithinAbs(20 * unit, 0.01));
    // Relative commands and implicit separators.
    const SketchImport rel = read_svg(R"svg(<svg width="10mm" viewBox="0 0 10 10"><path d="m1,1h8v8h-8z"/></svg>)svg");
    REQUIRE(rel.ok());
    const auto rb = box(rel.entities[0]);
    CHECK_THAT(rb[2] - rb[0], WithinAbs(8.0, 1e-9));
    CHECK(rel.entities[0].closed);
}
