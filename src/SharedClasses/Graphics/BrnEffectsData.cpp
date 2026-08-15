#include "types.hpp"
#include "SharedClasses/Graphics/BrnEffectsData.h"

// Post-FX data blend helpers. Declaration shape from the DecFIGS DWARF
// (SharedClasses/Graphics/BrnEffectsData.h:113/:120, :161/:168, :210/:217, :260/:267,
// :314/:321): each type has a NON-STATIC `void SetToBlend(A, wA, B, wB [, C, wC, D, wD])`
// pair -- see the big banner in the header for the three witnesses that pin that order.
//
// X360 ARTIST bodies:
//   BrnEffects::BloomData::SetToBlend    (4-way) @ 0x823F34B0
//   BrnEffects::VignetteData::SetToBlend (2-way) @ 0x823F35B8
//   sub_823F3758                         (4-way VignetteData; only caller is
//                                         EvalEffectData<VignetteData> @0x823F9F6C)
//   BrnEffects::BlurData::SetToBlend     (2-way) @ 0x823F3A50
//   sub_823F3C30                         (4-way BlurData; only caller is
//                                         EvalEffectData<BlurData> @0x823FA540)
// The remaining five (BloomData 2-way, both DepthOfFieldData, both TintData2d) have no
// standalone symbol: the compiler inlined them into their single call site inside
// EvalEffectData<T>. Their bodies below are the de-optimised form of those expansions --
// BloomData 2-way from @0x823F9C0C-0x823F9CB4, DepthOfFieldData 2-way from
// @0x823FA230-0x823FA2A4 and 4-way from @0x823FA150-0x823FA22C, TintData2d 2-way from
// @0x823FA7F8-0x823FA870 and 4-way from @0x823FA700-0x823FA7F0.
//
// All of them are pure per-member linear combinations. The X360 broadcasts each weight into
// a SIMD register (vspltw) and runs a vmulfp/vmaddfp chain over the 16-byte vector members;
// the scalar members are folded in fp registers. Reconstructed store-for-store by member
// name (no raw offsets): every Vector2/Vector4 lane -- including the unused z/w lanes that
// the original 16-byte vector stores still touch -- is blended, so the result is identical.
//
// SELF-BLEND IS SAFE AND IS WHAT THE CONSOLE DOES: EvalEffectData folds each layer in with
// `lRes.SetToBlend(lRes, 1-w, lAlt, w)` (r3 == r4 at 0x823FA034). Every output member below
// is written only after that same member has been read from both sources.

namespace BrnEffects
{

// ---- the declared-but-never-defined default constants (rung-5 "bloom lit" wave, 2026-08-15) --------
// The header declares these statics and every <Data>::Construct() reads them; nothing defined them, so
// the first TU to instantiate a Construct() (EnvironmentManager::GenerateEffects @0x827BE698's default
// arm, BrnEffectsFrame::Construct @0x822791E8) failed to link. Values are NOT invented:
//   * the scalars are the same KF_DEF_* immediates the header already carries (Hex-Rays resolves the
//     .rdata slots flt_820A3A14 = 0.98 / flt_820A3A98 = 0.77 / flt_820A3A9C = 0.0 / flt_820A3AA0 = 0.33,
//     and 0x820A3AA4..0x820A3AC7 == 0.0f x9 -- idat dump 2026-08-15, control dword_82F24240 == 1280);
//   * the vectors live in .data (unk_82FFxxxx, ALL ZERO in the image) and are written by the CRT static
//     initialisers at 0x82C60AA0..0x82C60CFC: each one lfs-loads its lanes into a 16-byte stack slot
//     (-0x10 = x, -0xC = y, -8 = z, -4 = w) and stvx128-stores it into the global. Lane sources:
//       kv4DefScale       @82FFAED0 <- flt_820A91B0 0.2627, flt_820A91AC 0.298, flt_82001C98 1.0, 1.0
//       kv2DefAmount      @82FFAE20 <- flt_820A4620 0.5,    flt_82004D00 0.6,   0, 0        (std r9=0)
//       kv2DefCentre      @82FFAEC0 <- flt_820A4620 0.5,    flt_82004C68 0.7,   0, 0
//       kv4DefInnerColour @82FFB220 <- flt_820A91B8 0.9765, flt_820A91B4 0.9882, flt_82001C98 1.0, flt_82001CC0 0.0
//       kv4DefOuterColour @82FFB1A0 <- flt_820A91C4 0.0549, flt_820A91C0 0.2078, flt_820A91BC 0.3765, flt_82001C98 1.0
//       kv2DefBlendAmount @82FFAFC0, kv2DefBlurAmount @82FFAE90, kv2DefBlendCentre @82FFB030,
//       kv2DefBlurCentre  @82FFB180, TintData2d::kv4DefaultColour @82FFB230 <- flt_82001CC0 0.0 x4
//     (they are ALSO the values BrnEffectsFrame::Construct's stvx128 stores broadcast into every frame).
const f32     BloomData::kfDefLuminance        = KF_DEF_BLOOM_LUMINANCE;             // 0.98f  flt_820A3A14
const f32     BloomData::kfDefThreshold        = KF_DEF_BLOOM_THRESHOLD;             // 0.77f  flt_820A3A98
const Vector4 BloomData::kv4DefScale           = { 0.2627f, 0.298f, 1.0f, 1.0f };    // unk_82FFAED0 (init @0x82C60AA0)
const f32     VignetteData::kfDefAngle         = KF_DEF_VIGNETTE_ANGLE;              // 0.0f   flt_820A3A9C
const f32     VignetteData::kfDefSharpness     = KF_DEF_VIGNETTE_SHARPNESS;          // 0.33f  flt_820A3AA0
const Vector2 VignetteData::kv2DefAmount       = { 0.5f, 0.6f, 0.0f, 0.0f };         // unk_82FFAE20 (init @0x82C60AE0)
const Vector2 VignetteData::kv2DefCentre       = { 0.5f, 0.7f, 0.0f, 0.0f };         // unk_82FFAEC0 (init @0x82C60B20)
const Vector4 VignetteData::kv4DefInnerColour  = { 0.9765f, 0.9882f, 1.0f, 0.0f };   // unk_82FFB220 (init @0x82C60B60)
const Vector4 VignetteData::kv4DefOuterColour  = { 0.0549f, 0.2078f, 0.3765f, 1.0f };// unk_82FFB1A0 (init @0x82C60BA8)
const f32     DepthOfFieldData::kfDefNearPlane   = KF_DEF_DEPTH_OF_FIELD_NEAR_PLANE;   // 0.0f  0x820A3AA4..
const f32     DepthOfFieldData::kfDefFocalPlane  = KF_DEF_DEPTH_OF_FIELD_FOCAL_PLANE;  // 0.0f
const f32     DepthOfFieldData::kfDefFocalPlane2 = KF_DEF_DEPTH_OF_FIELD_FOCAL_PLANE2; // 0.0f
const f32     DepthOfFieldData::kfDefFarPlane    = KF_DEF_DEPTH_OF_FIELD_FAR_PLANE;    // 0.0f
const f32     BlurData::kfDefOpacity           = KF_DEF_BLUR_OPACITY;                // 0.0f
const f32     BlurData::kfDefVelocity          = KF_DEF_BLUR_VELOCITY;               // 0.0f
const f32     BlurData::kfDefSharpness         = KF_DEF_BLUR_SHARPNESS;              // 0.0f
const f32     BlurData::kfDefNoise             = KF_DEF_BLUR_NOISE;                  // 0.0f
const f32     BlurData::kfDefAngle             = KF_DEF_BLUR_ANGLE;                  // 0.0f
const Vector2 BlurData::kv2DefBlendAmount      = { 0.0f, 0.0f, 0.0f, 0.0f };         // unk_82FFAFC0 (init @0x82C60BF0)
const Vector2 BlurData::kv2DefBlurAmount       = { 0.0f, 0.0f, 0.0f, 0.0f };         // unk_82FFAE90 (init @0x82C60C28)
const Vector2 BlurData::kv2DefBlendCentre      = { 0.0f, 0.0f, 0.0f, 0.0f };         // unk_82FFB030 (init @0x82C60C60)
const Vector2 BlurData::kv2DefBlurCentre       = { 0.0f, 0.0f, 0.0f, 0.0f };         // unk_82FFB180 (init @0x82C60C98)
const Vector4 TintData2d::kv4DefaultColour     = { 0.0f, 0.0f, 0.0f, 0.0f };         // unk_82FFB230 (init @0x82C60CD0)

// helper: lane-wise 2-way blend of a 16-byte vector member (Vector2 or Vector4)
template <typename V>
static inline void Blend2(V& lrOut, const V& lA, f32 lfWa, const V& lB, f32 lfWb)
{
    lrOut.x = lA.x * lfWa + lB.x * lfWb;
    lrOut.y = lA.y * lfWa + lB.y * lfWb;
    lrOut.z = lA.z * lfWa + lB.z * lfWb;
    lrOut.w = lA.w * lfWa + lB.w * lfWb;
}

// helper: lane-wise 4-way blend of a 16-byte vector member
template <typename V>
static inline void Blend4(V& lrOut,
                          const V& lA, f32 lfWa, const V& lB, f32 lfWb,
                          const V& lC, f32 lfWc, const V& lD, f32 lfWd)
{
    lrOut.x = lA.x * lfWa + lB.x * lfWb + lC.x * lfWc + lD.x * lfWd;
    lrOut.y = lA.y * lfWa + lB.y * lfWb + lC.y * lfWc + lD.y * lfWd;
    lrOut.z = lA.z * lfWa + lB.z * lfWb + lC.z * lfWc + lD.z * lfWd;
    lrOut.w = lA.w * lfWa + lB.w * lfWb + lC.w * lfWc + lD.w * lfWd;
}

// ---- BloomData -----------------------------------------------------------
// 2-way: inlined, from EvalEffectData<BloomData>'s two-slot arm -- `fmadds f12, f10, f13, f12`
// (lum), `fmadds f0, f9, f13, f0` (thr), `vmaddfp v0, v13, v0, v11` (mv4Scale).
void BloomData::SetToBlend(const BloomData& lA, f32 lfWa,
                           const BloomData& lB, f32 lfWb)
{
    mfLuminance = lA.mfLuminance * lfWa + lB.mfLuminance * lfWb;
    mfThreshold = lA.mfThreshold * lfWa + lB.mfThreshold * lfWb;
    Blend2(mv4Scale, lA.mv4Scale, lfWa, lB.mv4Scale, lfWb);
}

// 4-way @0x823F34B0: scalars mfLuminance/mfThreshold folded individually (lfs 0(r4)/0(r6)/
// 0(r8)/0(r10) then three fmadds); mv4Scale via the 4-source vmaddfp chain + stvx128 r3,16.
void BloomData::SetToBlend(const BloomData& lA, f32 lfWa,
                           const BloomData& lB, f32 lfWb,
                           const BloomData& lC, f32 lfWc,
                           const BloomData& lD, f32 lfWd)
{
    mfLuminance = lA.mfLuminance * lfWa + lB.mfLuminance * lfWb
                + lC.mfLuminance * lfWc + lD.mfLuminance * lfWd;
    mfThreshold = lA.mfThreshold * lfWa + lB.mfThreshold * lfWb
                + lC.mfThreshold * lfWc + lD.mfThreshold * lfWd;
    Blend4(mv4Scale, lA.mv4Scale, lfWa, lB.mv4Scale, lfWb,
                     lC.mv4Scale, lfWc, lD.mv4Scale, lfWd);
}

// ---- VignetteData --------------------------------------------------------
// 2-way @0x823F35B8: scalars mfAngle/mfSharpness folded individually; the four vector
// members (mv2Amount @0x10, mv2Centre @0x20, mv4InnerColour @0x30, mv4OuterColour @0x40 --
// the `li r30,0x10 / li r31,0x20 / li r5,0x30 / li r7,0x40` index registers) each via a
// 2-source vmulfp128 + vmaddfp + stvx128.
void VignetteData::SetToBlend(const VignetteData& lA, f32 lfWa,
                              const VignetteData& lB, f32 lfWb)
{
    mfAngle     = lA.mfAngle * lfWa + lB.mfAngle * lfWb;
    mfSharpness = lA.mfSharpness * lfWa + lB.mfSharpness * lfWb;
    Blend2(mv2Amount,      lA.mv2Amount,      lfWa, lB.mv2Amount,      lfWb);
    Blend2(mv2Centre,      lA.mv2Centre,      lfWa, lB.mv2Centre,      lfWb);
    Blend2(mv4InnerColour, lA.mv4InnerColour, lfWa, lB.mv4InnerColour, lfWb);
    Blend2(mv4OuterColour, lA.mv4OuterColour, lfWa, lB.mv4OuterColour, lfWb);
}

// 4-way sub_823F3758: identical member set, four sources (r4/r6/r8/r10, f1..f4).
void VignetteData::SetToBlend(const VignetteData& lA, f32 lfWa,
                              const VignetteData& lB, f32 lfWb,
                              const VignetteData& lC, f32 lfWc,
                              const VignetteData& lD, f32 lfWd)
{
    mfAngle     = lA.mfAngle * lfWa + lB.mfAngle * lfWb
                + lC.mfAngle * lfWc + lD.mfAngle * lfWd;
    mfSharpness = lA.mfSharpness * lfWa + lB.mfSharpness * lfWb
                + lC.mfSharpness * lfWc + lD.mfSharpness * lfWd;
    Blend4(mv2Amount,      lA.mv2Amount,      lfWa, lB.mv2Amount,      lfWb,
                           lC.mv2Amount,      lfWc, lD.mv2Amount,      lfWd);
    Blend4(mv2Centre,      lA.mv2Centre,      lfWa, lB.mv2Centre,      lfWb,
                           lC.mv2Centre,      lfWc, lD.mv2Centre,      lfWd);
    Blend4(mv4InnerColour, lA.mv4InnerColour, lfWa, lB.mv4InnerColour, lfWb,
                           lC.mv4InnerColour, lfWc, lD.mv4InnerColour, lfWd);
    Blend4(mv4OuterColour, lA.mv4OuterColour, lfWa, lB.mv4OuterColour, lfWb,
                           lC.mv4OuterColour, lfWc, lD.mv4OuterColour, lfWd);
}

// ---- DepthOfFieldData ----------------------------------------------------
// ⭐ FOUR MEMBERS ONLY. Both expansions read frame+0x90/+0x94/+0x98/+0x9C and write the
// result's +0/+4/+8/+0xC; mfDofAmount (+0x10 in the struct) is never touched by either the
// per-layer blend or the fold-in at 0x823FA31C. Adding it here would be an invention that
// silently changes the DoF amount the moment a world/fx-events layer carries weight.
void DepthOfFieldData::SetToBlend(const DepthOfFieldData& lA, f32 lfWa,
                                  const DepthOfFieldData& lB, f32 lfWb)
{
    mfNearPlane   = lA.mfNearPlane * lfWa + lB.mfNearPlane * lfWb;
    mfFocalPlane  = lA.mfFocalPlane * lfWa + lB.mfFocalPlane * lfWb;
    mfFocalPlane2 = lA.mfFocalPlane2 * lfWa + lB.mfFocalPlane2 * lfWb;
    mfFarPlane    = lA.mfFarPlane * lfWa + lB.mfFarPlane * lfWb;
}

void DepthOfFieldData::SetToBlend(const DepthOfFieldData& lA, f32 lfWa,
                                  const DepthOfFieldData& lB, f32 lfWb,
                                  const DepthOfFieldData& lC, f32 lfWc,
                                  const DepthOfFieldData& lD, f32 lfWd)
{
    mfNearPlane   = lA.mfNearPlane * lfWa + lB.mfNearPlane * lfWb
                  + lC.mfNearPlane * lfWc + lD.mfNearPlane * lfWd;
    mfFocalPlane  = lA.mfFocalPlane * lfWa + lB.mfFocalPlane * lfWb
                  + lC.mfFocalPlane * lfWc + lD.mfFocalPlane * lfWd;
    mfFocalPlane2 = lA.mfFocalPlane2 * lfWa + lB.mfFocalPlane2 * lfWb
                  + lC.mfFocalPlane2 * lfWc + lD.mfFocalPlane2 * lfWd;
    mfFarPlane    = lA.mfFarPlane * lfWa + lB.mfFarPlane * lfWb
                  + lC.mfFarPlane * lfWc + lD.mfFarPlane * lfWd;
}

// ---- BlurData ------------------------------------------------------------
// 2-way @0x823F3A50: scalars mfOpacity/mfVelocity/mfSharpness/mfNoise/mfAngle folded
// individually (offsets 0,4,8,0xC,0x10); the four Vector2 members (mv2BlendAmount @0x20,
// mv2BlurAmount @0x30, mv2BlendCentre @0x40, mv2BlurCentre @0x50) each via vmulfp128 +
// vmaddfp + stvx128.
void BlurData::SetToBlend(const BlurData& lA, f32 lfWa,
                          const BlurData& lB, f32 lfWb)
{
    mfOpacity   = lA.mfOpacity * lfWa + lB.mfOpacity * lfWb;
    mfVelocity  = lA.mfVelocity * lfWa + lB.mfVelocity * lfWb;
    mfSharpness = lA.mfSharpness * lfWa + lB.mfSharpness * lfWb;
    mfNoise     = lA.mfNoise * lfWa + lB.mfNoise * lfWb;
    mfAngle     = lA.mfAngle * lfWa + lB.mfAngle * lfWb;
    Blend2(mv2BlendAmount, lA.mv2BlendAmount, lfWa, lB.mv2BlendAmount, lfWb);
    Blend2(mv2BlurAmount,  lA.mv2BlurAmount,  lfWa, lB.mv2BlurAmount,  lfWb);
    Blend2(mv2BlendCentre, lA.mv2BlendCentre, lfWa, lB.mv2BlendCentre, lfWb);
    Blend2(mv2BlurCentre,  lA.mv2BlurCentre,  lfWa, lB.mv2BlurCentre,  lfWb);
}

// 4-way sub_823F3C30: same member set, four sources (r4/r6/r8/r10, f1..f4) -- the five
// scalar quads at 0/4/8/0xC/0x10 and the four vectors ending with `li r9, 0x50` /
// `stvx128 v0, r3, r9`.
void BlurData::SetToBlend(const BlurData& lA, f32 lfWa,
                          const BlurData& lB, f32 lfWb,
                          const BlurData& lC, f32 lfWc,
                          const BlurData& lD, f32 lfWd)
{
    mfOpacity   = lA.mfOpacity * lfWa + lB.mfOpacity * lfWb
                + lC.mfOpacity * lfWc + lD.mfOpacity * lfWd;
    mfVelocity  = lA.mfVelocity * lfWa + lB.mfVelocity * lfWb
                + lC.mfVelocity * lfWc + lD.mfVelocity * lfWd;
    mfSharpness = lA.mfSharpness * lfWa + lB.mfSharpness * lfWb
                + lC.mfSharpness * lfWc + lD.mfSharpness * lfWd;
    mfNoise     = lA.mfNoise * lfWa + lB.mfNoise * lfWb
                + lC.mfNoise * lfWc + lD.mfNoise * lfWd;
    mfAngle     = lA.mfAngle * lfWa + lB.mfAngle * lfWb
                + lC.mfAngle * lfWc + lD.mfAngle * lfWd;
    Blend4(mv2BlendAmount, lA.mv2BlendAmount, lfWa, lB.mv2BlendAmount, lfWb,
                           lC.mv2BlendAmount, lfWc, lD.mv2BlendAmount, lfWd);
    Blend4(mv2BlurAmount,  lA.mv2BlurAmount,  lfWa, lB.mv2BlurAmount,  lfWb,
                           lC.mv2BlurAmount,  lfWc, lD.mv2BlurAmount,  lfWd);
    Blend4(mv2BlendCentre, lA.mv2BlendCentre, lfWa, lB.mv2BlendCentre, lfWb,
                           lC.mv2BlendCentre, lfWc, lD.mv2BlendCentre, lfWd);
    Blend4(mv2BlurCentre,  lA.mv2BlurCentre,  lfWa, lB.mv2BlurCentre,  lfWb,
                           lC.mv2BlurCentre,  lfWc, lD.mv2BlurCentre,  lfWd);
}

// ---- TintData2d ----------------------------------------------------------
// One 16-byte member. 2-way: `vmulfp128 v0, v0, v12` + `vmaddfp v0, v13, v0, v11`
// (@0x823FA85C/0x823FA86C). 4-way: the same with three vmaddfp (@0x823FA7B0..0x823FA7EC).
void TintData2d::SetToBlend(const TintData2d& lA, f32 lfWa,
                            const TintData2d& lB, f32 lfWb)
{
    Blend2(mv4Colour, lA.mv4Colour, lfWa, lB.mv4Colour, lfWb);
}

void TintData2d::SetToBlend(const TintData2d& lA, f32 lfWa,
                            const TintData2d& lB, f32 lfWb,
                            const TintData2d& lC, f32 lfWc,
                            const TintData2d& lD, f32 lfWd)
{
    Blend4(mv4Colour, lA.mv4Colour, lfWa, lB.mv4Colour, lfWb,
                      lC.mv4Colour, lfWc, lD.mv4Colour, lfWd);
}

} // namespace BrnEffects
