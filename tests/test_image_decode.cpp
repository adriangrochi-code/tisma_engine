// Tests of the JPEG and PNG reader (src/ui/image_decode.hpp).

#include "device/bambu_simulator.hpp"
#include "ui/image_decode.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

std::string from_hex(const std::string& hex) {
    std::string out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
        out.push_back(char(std::stoi(hex.substr(i, 2), nullptr, 16)));
    return out;
}

} // namespace

TEST_CASE("decode_image reads the camera's JPEG and a PNG", "[ui][image]") {
    const tisma::ui::Image jpeg = tisma::ui::decode_image(tisma::device::BambuSimulator::camera_jpeg());
    CHECK(jpeg.width == 32);
    CHECK(jpeg.height == 24);
    CHECK(jpeg.rgba.size() == std::size_t(32 * 24 * 4));

    // A 4x3 red PNG with an alpha channel.
    const tisma::ui::Image png = tisma::ui::decode_image(from_hex("89504e470d0a1a0a0000000d4948445200000004000000030806000000b4f4aec60000001549444154789c63fccfc0f09f01093031a0010c0100616b02044297ea790000000049454e44ae426082"));
    REQUIRE(png.width == 4);
    REQUIRE(png.height == 3);
    CHECK(png.rgba[0] == 255);
    CHECK(png.rgba[1] == 0);
    CHECK(png.rgba[3] == 255);

    // Not a picture, or cut short: empty.
    CHECK(tisma::ui::decode_image("not a picture").rgba.empty());
    CHECK(tisma::ui::decode_image("").rgba.empty());
    const std::string cut = tisma::device::BambuSimulator::camera_jpeg().substr(0, 20);
    CHECK(tisma::ui::decode_image(cut).width == 0);
}
