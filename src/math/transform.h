#pragma once

#include "vec3.h"


// * Frames
// ? Three frames are in play, and mixing them up is the easiest way to fly the
// ? drone into something:
// ?
// ?   CAMERA  - OpenCV convention. x right, y DOWN, z forward along the
// ?             optical axis. This is what Perception reports.
// ?   BODY    - PX4 convention (FRD). x forward, y right, z DOWN.
// ?   WORLD   - PX4 convention (NED). x north, y east, z DOWN.
// ?
// ? CAMERA -> BODY is a FIXED rotation, because the camera is bolted to the
// ? airframe:
// ?     body.x = camera.z    (forward)
// ?     body.y = camera.x    (right)
// ?     body.z = camera.y    (down)
// ?
// ? BODY -> WORLD is the drone's attitude, which PX4 reports as a quaternion
// ? in vehicle_attitude.q (w, x, y, z), body FRD -> world NED.
// ?
// ! The camera->body rotation below assumes the camera points FORWARD and is
// ! mounted upright. A downward-facing or tilted mount needs its own rotation -
// ! that is the mount extrinsic tracked by [#32].

namespace cogidrone {

// * Transform
// ? A rigid transform: rotate, then translate. `rotation` maps child -> parent,
// ? `translation` is the child's origin expressed in the parent frame.
// ? `Transform{}` is the identity: Quat defaults to (1,0,0,0), Vec3 to zero.
struct Transform {
    Quat rotation;                                                                      // child -> parent
    Vec3 translation;                                                                   // child origin, in parent frame
};


// * Camera <-> body
// ? 120 degrees about the (1,1,1) axis. w = cos(60) = 0.5,
// ? xyz = sin(60)/sqrt(3) = 0.5 each.
[[nodiscard]] inline constexpr Quat cameraToBodyRotation() noexcept {
    return Quat{0.5f, 0.5f, 0.5f, 0.5f};
}

// ? Written out explicitly rather than as a quaternion rotation: it is a pure
// ? axis permutation, so the component form is both faster and impossible to
// ? misread.
[[nodiscard]] inline constexpr Vec3 cameraToBody(const Vec3& camera) noexcept {
    return Vec3{camera.z, camera.x, camera.y};
}

[[nodiscard]] inline constexpr Vec3 bodyToCamera(const Vec3& body) noexcept {
    return Vec3{body.y, body.z, body.x};
}


// * Composition
[[nodiscard]] inline constexpr Vec3 apply(const Transform& transform, const Vec3& point) noexcept {
    return rotate(transform.rotation, point) + transform.translation;
}

// ? `child` is expressed in `parent`'s frame. The result maps grandchild ->
// ? grandparent.
[[nodiscard]] inline constexpr Transform compose(const Transform& parent, const Transform& child) noexcept {
    return Transform{
        .rotation = parent.rotation * child.rotation,
        .translation = apply(parent, child.translation)
    };
}


// * Camera -> world
// ? The one function Navigation needs. Everything else in this file exists so
// ? that a gimbal or a mount offset can be added later without touching the
// ? call site.
// ?
// ?   world = dronePosition + R_bodyToWorld * R_cameraToBody * cameraPoint
[[nodiscard]] inline constexpr Vec3 cameraToWorld(
    const Vec3& cameraPoint,
    const Quat& bodyToWorld,                                                            // vehicle_attitude.q
    const Vec3& bodyPositionWorld                                                       // VehicleLocalPosition x, y, z
) noexcept {
    return bodyPositionWorld + rotate(bodyToWorld, cameraToBody(cameraPoint));
}


// * Compile-time proof
// ? These are the two facts the whole camera->world chain rests on. If either
// ? breaks, every detection is placed in the wrong frame and the drone follows
// ? a ghost. A static_assert catches that at build time instead of in the air.
// ?
// ?   camera +z (forward)  ->  body +x (forward)
// ?   camera +x (right)    ->  body +y (right)
static_assert([] {
    constexpr Vec3 FORWARD = cameraToWorld(
        Vec3{0.0f, 0.0f, 1.0f},                                                     // camera: 1 m forward
        Quat{},                                                                     // identity attitude
        Vec3{}                                                                      // at the origin
    );
    return FORWARD.x == 1.0f && FORWARD.y == 0.0f && FORWARD.z == 0.0f;
}(), "camera +z must map to body +x (forward)");

static_assert([] {
    constexpr Vec3 RIGHT = cameraToWorld(
        Vec3{1.0f, 0.0f, 0.0f},                                                     // camera: 1 m right
        Quat{},
        Vec3{}
    );
    return RIGHT.x == 0.0f && RIGHT.y == 1.0f && RIGHT.z == 0.0f;
}(), "camera +x must map to body +y (right)");

} // namespace cogidrone