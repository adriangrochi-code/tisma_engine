#pragma once

#include "ui/math.hpp"

namespace tisma::ui {

enum class StandardView { Iso, Top, Front, Back, Left, Right, Bottom };

// A camera that orbits a target point, with +Z up (the printer's axes: the
// front of the bed is -Y). Angles in radians: yaw turns around Z (0 looks from
// the front, toward +Y), pitch lifts the eye (0 level, pi/2 straight down).
// Own design; see PROVENANCE.md.
class OrbitCamera {
public:
    Vec3 target{0, 0, 0};
    float distance = 400.0f;
    float yaw = 0.0f;
    float pitch = 0.5f;
    float fov = 0.6f; // vertical field of view, radians
    // Orthographic (preview.md, rule 10), seen as through a long lens: a
    // field of view of kOrthoFov from as far as keeps the target's size, so
    // the views and shaders made for a perspective camera stay as they are.
    bool orthographic = false;
    static constexpr float kOrthoFov = 0.0174533f; // 1 degree

    // The field of view and the eye's distance the projection uses.
    float lens_fov() const { return orthographic ? kOrthoFov : fov; }
    float lens_distance() const;

    Vec3 eye() const;
    Vec3 forward() const; // from the eye toward the target
    Vec3 right() const;   // screen right
    Vec3 up() const;      // screen up

    // World to camera, and camera to clip space for a viewport of that aspect
    // (width / height). The near and far planes follow the distance.
    Mat4 view() const;
    Mat4 projection(float aspect) const;

    // Mouse moves in pixels: orbit turns around the target (pitch stays within
    // [-89, 90] degrees), pan slides the target in the screen plane so the
    // point under the cursor follows it, zoom moves the eye closer (steps > 0)
    // or farther, by 15 % per step.
    void orbit(float dx, float dy);
    void pan(float dx, float dy, float viewport_height);
    void zoom(float steps);

    // Looks at the box from the current direction so that all of it shows.
    void fit(Vec3 min, Vec3 max, float aspect);
    // Turns to a standard view (keeping the target and distance).
    void set_view(StandardView view);
};

} // namespace tisma::ui
