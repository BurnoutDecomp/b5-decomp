#pragma once

// =============================================================================
// Curves.h  (OWNING HEADER for BrnEffects::Curves::SmoothStep)
//
// Home for the effects-system curve evaluator family. Only the Vector3 +
// Vector2 Evaluate overload (the asm-attested one in this TU) is bodied here;
// the rest of the DWARF surface is declared-only so the struct shape stays
// coherent without forking the other-TU bodies.
//
// Layout (DWARF references/DecFIGS/dwarfdump/GameSource/Effects/Curves.h:29):
//   Curves.h:135  Vector3 mCurveParams;     // (x,y,z) = lower / mid / upper input thresholds
//   Curves.h:138  Vector2 mScaleFactors;    // (x,y)   = value at the lower / upper edge
//
// The bodied Evaluate is the smoothstep remap attested by
// BrnEffects::Curves::SmoothStep::Evaluate(const Vector3&, const Vector2&, f32)
// @ 0x8227D2F0. Reconstructed store-for-store from the X360 pseudocode/asm.
// =============================================================================

#include "BrnCommonTypes.h"   // Vector2, Vector3 (rw::math::vpu float lanes)

namespace BrnEffects
{
namespace Curves
{

struct SmoothStep
{
    // DWARF Curves.h:135 / :138.
    Vector3 mCurveParams;   // x: lower threshold, y: mid threshold, z: upper threshold
    Vector2 mScaleFactors;  // x: value at lower edge, y: value at upper edge

    // DWARF Curves.h:51. Stores the curve params + scale factors. Other-TU body.
    void SmoothStep_Prepare(const Vector3 &lvParams, const Vector2 &lvScale);

    // DWARF Curves.h:62. Vector2-param overload. Other-TU body.
    f32 Evaluate(const Vector2 &lvParams, const Vector2 &lvScale, f32 lfInput) const;

    // DWARF Curves.h:77 / asm @ 0x8227D2F0. The smoothstep remap.
    //
    // Two half-ranges meeting at the mid threshold mCurveParams.y:
    //   input <= params.x          -> scale.x                       (clamped low)
    //   input >= params.z          -> scale.y                       (clamped high)
    //   params.x < input <= params.y (lower half):
    //       t = (input - params.x) / (params.y - params.x)
    //       result = (t * 0.5) * (scale.y - scale.x) + scale.x
    //   params.y < input < params.z (upper half):
    //       t = (input - params.y) / (params.z - params.y)
    //       result = (t * 0.5 + 0.5) * (scale.y - scale.x) + scale.x
    //
    // The 0.5 factor (rodata flt_82001DA0) splits the [scale.x, scale.y] span
    // evenly across the two halves about the mid threshold. Defined in Curves.cpp.
    f32 Evaluate(const Vector3 &lvParams, const Vector2 &lvScale, f32 lfInput) const;

    // DWARF Curves.h:126. Instance Evaluate using the stored params. Other-TU body.
    f32 Evaluate(f32 lfInput) const;
};

} // namespace Curves
} // namespace BrnEffects
