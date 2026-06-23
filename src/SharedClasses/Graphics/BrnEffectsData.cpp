#include "types.hpp"
#include "SharedClasses/Graphics/BrnEffectsData.h"

// Post-FX data blend helpers (X360 ARTIST):
//   BrnEffects::BloomData::SetToBlend     @ 0x823F34B0  (4-way blend)
//   BrnEffects::VignetteData::SetToBlend  @ 0x823F35B8  (2-way blend)
//   BrnEffects::BlurData::SetToBlend      @ 0x823F3A50  (2-way blend)
//
// All three are pure per-member linear combinations of their source instances.
// The X360 bodies broadcast each weight into a SIMD register (vspltw) and run a
// vmaddfp chain over the 16-byte vector members; the scalar members are folded
// in fp registers. Reconstructed store-for-store by member name (no raw offsets):
// every Vector2/Vector4 lane (including the unused z/w lanes that the original
// 16-byte vector stores still touch) is blended so the result is byte-identical.

namespace BrnEffects
{

// helper: lane-wise 4-way blend of a 16-byte vector member
static inline void Blend4(Vector4& lrOut,
                          const Vector4& lA, f32 lfWa,
                          const Vector4& lB, f32 lfWb,
                          const Vector4& lC, f32 lfWc,
                          const Vector4& lD, f32 lfWd)
{
    lrOut.x = lA.x * lfWa + lB.x * lfWb + lC.x * lfWc + lD.x * lfWd;
    lrOut.y = lA.y * lfWa + lB.y * lfWb + lC.y * lfWc + lD.y * lfWd;
    lrOut.z = lA.z * lfWa + lB.z * lfWb + lC.z * lfWc + lD.z * lfWd;
    lrOut.w = lA.w * lfWa + lB.w * lfWb + lC.w * lfWc + lD.w * lfWd;
}

// helper: lane-wise 2-way blend of a 16-byte vector member (Vector2 or Vector4)
template <typename V>
static inline void Blend2(V& lrOut, const V& lA, f32 lfWa, const V& lB, f32 lfWb)
{
    lrOut.x = lA.x * lfWa + lB.x * lfWb;
    lrOut.y = lA.y * lfWa + lB.y * lfWb;
    lrOut.z = lA.z * lfWa + lB.z * lfWb;
    lrOut.w = lA.w * lfWa + lB.w * lfWb;
}

// ---- BloomData::SetToBlend @ 0x823F34B0 ----------------------------------
// scalars mfLuminance/mfThreshold folded individually; mv4Scale via the
// 4-source vmaddfp chain (stvx128 v0, r3, 16).
BloomData* BloomData::SetToBlend(BloomData* lpResult,
                                 const BloomData* lpA, f32 lfWa, f32 lfWb, f32 lfWc, f32 lfWd,
                                 const BloomData* lpB, const BloomData* lpC, const BloomData* lpD)
{
    lpResult->mfLuminance = lpA->mfLuminance * lfWa + lpB->mfLuminance * lfWb
                          + lpC->mfLuminance * lfWc + lpD->mfLuminance * lfWd;
    lpResult->mfThreshold = lpA->mfThreshold * lfWa + lpB->mfThreshold * lfWb
                          + lpC->mfThreshold * lfWc + lpD->mfThreshold * lfWd;
    Blend4(lpResult->mv4Scale,
           lpA->mv4Scale, lfWa, lpB->mv4Scale, lfWb,
           lpC->mv4Scale, lfWc, lpD->mv4Scale, lfWd);
    return lpResult;
}

// ---- VignetteData::SetToBlend @ 0x823F35B8 -------------------------------
// scalars mfAngle/mfSharpness folded individually; the four vector members
// (mv2Amount @16, mv2Centre @32, mv4InnerColour @48, mv4OuterColour @64) each
// via a 2-source vmaddfp + stvx128.
VignetteData* VignetteData::SetToBlend(VignetteData* lpResult,
                                       const VignetteData* lpA, f32 lfWa, f32 lfWb,
                                       const VignetteData* lpB)
{
    lpResult->mfAngle    = lpA->mfAngle * lfWa + lpB->mfAngle * lfWb;
    lpResult->mfSharpness = lpA->mfSharpness * lfWa + lpB->mfSharpness * lfWb;
    Blend2(lpResult->mv2Amount,      lpA->mv2Amount,      lfWa, lpB->mv2Amount,      lfWb);
    Blend2(lpResult->mv2Centre,      lpA->mv2Centre,      lfWa, lpB->mv2Centre,      lfWb);
    Blend2(lpResult->mv4InnerColour, lpA->mv4InnerColour, lfWa, lpB->mv4InnerColour, lfWb);
    Blend2(lpResult->mv4OuterColour, lpA->mv4OuterColour, lfWa, lpB->mv4OuterColour, lfWb);
    return lpResult;
}

// ---- BlurData::SetToBlend @ 0x823F3A50 -----------------------------------
// scalars mfOpacity/mfVelocity/mfSharpness/mfNoise/mfAngle folded individually
// (offsets 0..16); the four Vector2 members (mv2BlendAmount @32, mv2BlurAmount
// @48, mv2BlendCentre @64, mv2BlurCentre @80) each via a 2-source vmaddfp + stvx128.
BlurData* BlurData::SetToBlend(BlurData* lpResult,
                               const BlurData* lpA, f32 lfWa, f32 lfWb,
                               const BlurData* lpB)
{
    lpResult->mfOpacity   = lpA->mfOpacity   * lfWa + lpB->mfOpacity   * lfWb;
    lpResult->mfVelocity  = lpA->mfVelocity  * lfWa + lpB->mfVelocity  * lfWb;
    lpResult->mfSharpness = lpA->mfSharpness * lfWa + lpB->mfSharpness * lfWb;
    lpResult->mfNoise     = lpA->mfNoise     * lfWa + lpB->mfNoise     * lfWb;
    lpResult->mfAngle     = lpA->mfAngle     * lfWa + lpB->mfAngle     * lfWb;
    Blend2(lpResult->mv2BlendAmount, lpA->mv2BlendAmount, lfWa, lpB->mv2BlendAmount, lfWb);
    Blend2(lpResult->mv2BlurAmount,  lpA->mv2BlurAmount,  lfWa, lpB->mv2BlurAmount,  lfWb);
    Blend2(lpResult->mv2BlendCentre, lpA->mv2BlendCentre, lfWa, lpB->mv2BlendCentre, lfWb);
    Blend2(lpResult->mv2BlurCentre,  lpA->mv2BlurCentre,  lfWa, lpB->mv2BlurCentre,  lfWb);
    return lpResult;
}

} // namespace BrnEffects
