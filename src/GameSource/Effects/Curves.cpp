// =============================================================================
// Curves.cpp  (definitions for BrnEffects::Curves::SmoothStep)
//
// Out-of-line home for the asm-attested smoothstep remap. Reconstructed
// store-for-store from BrnEffects::Curves::SmoothStep::Evaluate(const Vector3&,
// const Vector2&, f32) @ 0x8227D2F0. The other Evaluate/Prepare overloads on the
// struct are owned by their own TUs and are declared-only in the header.
// =============================================================================

#include "GameSource/Effects/Curves.h"

namespace BrnEffects
{
namespace Curves
{

// @ 0x8227D2F0 -- two half-ranges meeting at the mid threshold mCurveParams.y.
//   input <= params.x  -> scale.x                  (clamped low)
//   input >= params.z  -> scale.y                  (clamped high)
//   lower half: t = (input - params.x) / (params.y - params.x)
//               result = (t * 0.5) * (scale.y - scale.x) + scale.x
//   upper half: t = (input - params.y) / (params.z - params.y)
//               result = (t * 0.5 + 0.5) * (scale.y - scale.x) + scale.x
// The 0.5 factor (rodata flt_82001DA0) splits the span evenly across the two
// halves about the mid threshold.
f32 SmoothStep::Evaluate(const Vector3 &lvParams, const Vector2 &lvScale, f32 lfInput) const
{
    if (lfInput < lvParams.x)
        return lvScale.x;

    if (lfInput > lvParams.z)
        return lvScale.y;

    const f32 lfSpan = lvScale.y - lvScale.x;

    if (lfInput > lvParams.y)
    {
        const f32 lfT = (lfInput - lvParams.y) / (lvParams.z - lvParams.y);
        return (lfT * 0.5f + 0.5f) * lfSpan + lvScale.x;
    }

    const f32 lfT = (lfInput - lvParams.x) / (lvParams.y - lvParams.x);
    return (lfT * 0.5f) * lfSpan + lvScale.x;
}

// DWARF Curves.h:126. INLINED on the console -- no standalone address (the ledger's only
// SmoothStep::Evaluate is 0x8227D2F0, the overload above). Recovered from its inlined copy in
// BrnCoronaManager::BrnSubmissionInterface::AddCorona @0x823FD270:
//   0x823FD314  addi r3, r30, 0x18   ; &mCurveParams   (BrnCoronaTypeParams record +0x18)
//   0x823FD318  addi r4, r3,  0x0C   ; &mScaleFactors  (record +0x24)
//   0x823FD32C  fmr  f1, f30         ; lfInput
//   0x823FD330  bl   BrnEffects__Curves__SmoothStep__Evaluate
// -- the stored members forwarded to the three-argument overload, and nothing else.
f32 SmoothStep::Evaluate(f32 lfInput) const
{
    return Evaluate(mCurveParams, mScaleFactors, lfInput);
}

} // namespace Curves
} // namespace BrnEffects
