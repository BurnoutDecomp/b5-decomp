#pragma once

// Canonical RenderWare SDK home for the rw::math::vpu Vector2 operation vocabulary
// (EARenderWare rwmath 1.02.00, rw/math/vpu/vector2_operation.h -- sibling to
// vector3_operation.h and types.h).
//
// PROVENANCE (2026-08-02, final-camera wave). Added because
// BehaviourGameplayExternal::UpdateLooking takes TWO 2-lane dot products against constant
// axes, and the DecFIGS DWARF charges both of them to the SDK's own
// `rw/math/vpu/detail/vector2_operation_inline.h:108` -- i.e. they are calls into this
// header, not open-coded arithmetic in the behaviour. The X360 emits each as
//   vmulfp128 v13, <lookDir>, <axis>      ; lane-wise product
//   vspltw    v10, v13, 1                 ; broadcast lane y
//   vspltw    v13, v13, 0                 ; broadcast lane x
//   vaddfp    v0,  v13, v10               ; x + y   (the w/z lanes are never read)
// -- X360 @0x82225CC0..0x82225CD0 and @0x82225DC8..0x82225DDC, the same four-instruction
// shape twice. The z/w lanes of the SDK's Vector2 are padding (types.h keeps all four so
// the 16-byte register layout matches the console); Dot therefore reads x and y ONLY, which
// is what the two `vspltw`s attest.
//
// Same de-modelling rules as vector3_operation.h: the SDK returns a broadcast VecFloat and
// every call site here reads one lane, so this is de-modelled to a scalar `float`.
//
// gen-tool note: tools/renderware/generate_headers.py never touches rw/math/vpu/, so this
// hand-maintained header is immune to regeneration (same as types.h / vector3_operation.h).

#include "rw/math/vpu/types.h"    // rw::math::vpu::Vector2

#include <cmath>                  // std::sqrt (Magnitude)

namespace rw
{
namespace math
{
namespace vpu
{
    // SDK Dot(Vector2, Vector2) -- detail/vector2_operation_inline.h:108. x and y lanes only.
    inline float Dot(const Vector2& lLhs, const Vector2& lRhs)
    {
        return lLhs.x * lRhs.x + lLhs.y * lRhs.y;
    }

    // ADDITIVE GROW 2026-08-27 (main-map slice): the TWO-lane Magnitude, under its SDK
    // name (sibling of the Vector4 full-4 Magnitude in vector4_operation.h). The X360
    // inlines it as
    //   vmulfp128 v0, v, v          ; lane-wise square
    //   vspltw    v13, v0, 1        ; broadcast lane y
    //   vspltw    v0,  v0, 0        ; broadcast lane x
    //   vaddfp    v0,  v0, v13      ; x^2 + y^2   (z/w lanes never read)
    //   vrsqrtefp + 2x Newton-Raphson, * dot      ; == sqrt(dot)
    //   vsel <0 guard>                            ; sqrt(0) == 0 exactly
    // -- MainMapComponent::SnapToLocation @0x8245ECC4..0x8245ED0C (the console's own
    // assert text names it: "RwMathVPU::Magnitude(mv2DesiredCentre) < 1000000.0f") and
    // twice more in MainMapComponent::ApplyZoom @0x8245EFF0 / @0x8245F25C. Scalar
    // lowering per this header's convention; the zero-dot vsel guard falls out of
    // std::sqrt(0.0f) == 0.0f.
    inline float Magnitude(const Vector2& lrVector)
    {
        return std::sqrt(lrVector.x * lrVector.x + lrVector.y * lrVector.y);
    }
}
}
}
