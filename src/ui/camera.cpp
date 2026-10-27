// Orbit camera of the 3D view. Own design; see PROVENANCE.md.

#include "ui/camera.hpp"

#include <algorithm>
#include <numbers>

namespace tisma::ui {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kMinPitch = -89.0f * kPi / 180.0f;
constexpr float kMaxPitch = 90.0f * kPi / 180.0f;
constexpr float kOrbitSpeed = 0.008f; // radians per pixel
constexpr float kZoomStep = 0.85f;
constexpr float kMinDistance = 1.0f, kMaxDistance = 20000.0f; // mm

} // namespace

float OrbitCamera::lens_distance() const {
    return orthographic ? distance * std::tan(fov * 0.5f) / std::tan(kOrthoFov * 0.5f) : distance;
}

Vec3 OrbitCamera::eye() const {
    const Vec3 back{std::cos(pitch) * std::sin(yaw), -std::cos(pitch) * std::cos(yaw), std::sin(pitch)};
    return target + back * lens_distance();
}

Vec3 OrbitCamera::forward() const {
    return Vec3{-std::cos(pitch) * std::sin(yaw), std::cos(pitch) * std::cos(yaw), -std::sin(pitch)};
}

// Right comes from the yaw alone, so looking straight down is not a special case.
Vec3 OrbitCamera::right() const { return Vec3{std::cos(yaw), std::sin(yaw), 0.0f}; }

Vec3 OrbitCamera::up() const { return cross(right(), forward()); }

Mat4 OrbitCamera::view() const {
    const Vec3 r = right(), u = up(), f = forward(), e = eye();
    return {r.x, u.x, -f.x, 0, r.y, u.y, -f.y, 0, r.z, u.z, -f.z, 0, -dot(r, e), -dot(u, e), dot(f, e), 1};
}

Mat4 OrbitCamera::projection(float aspect) const {
    float near = std::max(distance * 0.01f, 0.05f);
    float far = distance * 20.0f + 1000.0f;
    if (orthographic) {
        // The scene lies within a few distances and a bed's size of the target.
        const float d = lens_distance(), reach = 3.0f * distance + 1000.0f;
        near = std::max(d - reach, 0.05f);
        far = d + reach;
    }
    const float t = 1.0f / std::tan(lens_fov() * 0.5f);
    Mat4 m{};
    m[0] = t / std::max(aspect, 1e-6f);
    m[5] = t;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0f;
    m[14] = 2.0f * far * near / (near - far);
    return m;
}

void OrbitCamera::orbit(float dx, float dy) {
    yaw -= dx * kOrbitSpeed;
    pitch = std::clamp(pitch + dy * kOrbitSpeed, kMinPitch, kMaxPitch);
}

void OrbitCamera::pan(float dx, float dy, float viewport_height) {
    // World size of a pixel at the target's depth.
    const float per_pixel = 2.0f * distance * std::tan(fov * 0.5f) / std::max(viewport_height, 1.0f);
    target = target - right() * (dx * per_pixel) + up() * (dy * per_pixel);
}

void OrbitCamera::zoom(float steps) {
    distance = std::clamp(distance * std::pow(kZoomStep, steps), kMinDistance, kMaxDistance);
}

void OrbitCamera::fit(Vec3 min, Vec3 max, float aspect) {
    target = (min + max) * 0.5f;
    const float radius = std::max(length(max - min) * 0.5f, 1.0f);
    // The sphere around the box fits in the narrower of the two fields of view.
    const float half_v = fov * 0.5f;
    const float half_h = std::atan(std::tan(half_v) * std::max(aspect, 1e-6f));
    distance = std::clamp(radius / std::sin(std::min(half_v, half_h)) * 1.05f, kMinDistance, kMaxDistance);
}

void OrbitCamera::set_view(StandardView view) {
    switch (view) {
    case StandardView::Iso: yaw = 25.0f * kPi / 180.0f, pitch = 30.0f * kPi / 180.0f; break;
    case StandardView::Top: yaw = 0.0f, pitch = kMaxPitch; break;
    case StandardView::Front: yaw = 0.0f, pitch = 0.0f; break;
    case StandardView::Back: yaw = kPi, pitch = 0.0f; break;
    case StandardView::Left: yaw = -kPi * 0.5f, pitch = 0.0f; break;
    case StandardView::Right: yaw = kPi * 0.5f, pitch = 0.0f; break;
    case StandardView::Bottom: yaw = 0.0f, pitch = kMinPitch; break;
    }
}

} // namespace tisma::ui
