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
}
}
}
