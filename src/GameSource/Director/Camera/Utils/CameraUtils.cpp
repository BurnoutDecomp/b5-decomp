#include "GameSource/Director/Camera/Utils/CameraUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/fpu/scalar_operation.h"            // Tan / IsZero
#include "rw/math/vpu/vector3_operation.h"           // Subtract / Normalize / IsZero
#include "rw/math/vpu/matrix44affine_operation.h"    // Mult / TransformPoint /
                                                     //   InverseOfMatrixWithOrthonormal3x3

#include <cmath>   // std::atan / std::acos / std::asin / std::sin / std::cos / std::fabs /
                   //   std::copysign

// BrnDirector::Camera::Utils -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, class:BrnDirector::Camera::Utils::TransitionSmoother):
//   TransitionSmoother::Set @0x821F22A0
//
// Bodied here (5 ledger functions, class:BrnDirector::Camera::Utils -- FOV/zoom/pitch
// scalar math whose stores are cleanly recoverable):
//   GetZoomFromFOVDegs        @0x821F23E8
//   ConvertFOVDegsToLensLength@0x821F2530
//   Cycle                     @0x821F2378
//   GetFOVDegsFromZoom        @0x821F2490
//   GetPitchAboutPointRads    @0x82205A60
//
// Bodied here (2 ledger functions, class:BrnDirector::Camera::Utils -- the look-at basis
// builder, both overloads; see the KV_AXIS_* block below for the rodata attestation):
//   CreateLookAt(Vector3, Vector3)          @0x8220C4F8   (CameraUtils.cpp:704)
//   CreateLookAt(Vector3, Vector3, Vector3) @0x8220C960   (CameraUtils.cpp:767)
//
// Bodied here (3 ledger functions, class:BrnDirector::Camera::Utils -- the zoom/screen-fit
// trio the car-select orbit camera and Looker::Zoom both drive; ADDED 2026-08-01, all three
// were declaration-only and all three are on the live path):
//   GetSizeOnScreen                    @0x82221918   (360 asm lines)
//   GetFOVDegsToFitObjectToScreenSize  @0x8220C258   (80 asm lines)
//   CreateAdjustedLookAt               @0x82221EB8   (106 asm lines)
//
// Bodied here (1 ledger function, class:BrnDirector::Camera::Utils -- the Euler decomposition
// that feeds the fly-by camera's BANK; see the block comment at its definition for the
// branch-by-branch attestation):
//   EulerAnglesZXYFromMatrix44Affine        @0x82222180   (CameraUtils.cpp:453)
//
// Bodied here (1 ledger function, class:BrnDirector::Camera::Utils -- the ZXY Euler
// COMPOSER, i.e. the exact inverse of the decomposition above; ADDED 2026-08-02, it was
// declaration-only and it gated the whole chase-camera helper cluster plus BrnCameraShake.cpp):
//   RotateMatrix44AffineByEulerAnglesZXY    @0x82204F98   (CameraUtils.h:423)
//
// Bodied here (3 ledger functions, class:BrnDirector::Camera::Utils -- the scalar response
// curves BehaviourGameplayExternal::ApplySlideyEffects drives; ADDED 2026-08-02, and NONE of
// the three had a declaration anywhere in the tree before this. SineLerp alone is named as a
// blocker by four other committed camera TUs):
//   PositiveValueTendToLimit  @0x821F8B78 / PS3 @0x1B410   (CameraUtils.cpp:616)
//   TendToLimits(f32 x5)      NO X360 SYMBOL / PS3 @0x1B53C (CameraUtils.cpp:660) -- the
//                             X360 inlines it into every caller; see its own banner
//   SineLerp                  @0x8220CCB0 / PS3 @0x20AE4   (CameraUtils.cpp:808)
//
// DECLARATION-ONLY + FLAGGED (declared in CameraUtils.h, bodies not reconstructed --
// each is an inline VMX minimax / corner lattice / permute over UNATTESTED raw rodata
// coefficient constants; bodying them store-for-store would fabricate those
// tables, so they are left unbodied per the no-fabrication rule):
//   ApplyPitchAboutPointRads          @0x822183E0  (Sin/Cos minimax, rodata 82000BD0..82000C60)
//       ⛔ ITS REASON IS THE ONE THAT JUST EXPIRED FOR ITS NEIGHBOUR -- DO NOT RE-QUOTE IT.
//       RotateMatrix44AffineByEulerAnglesZXY sat on this same list, citing this same rodata
//       range, and the coefficients turned out to be an implementation detail of sin and cos
//       that this file de-optimises to libm as a matter of course (see its definition below).
//       0x82000C60 is DUMPED as { pi, 2pi, 1/pi, 1/(2pi) } -- the range-reduction row, not a
//       coefficient table. What actually has to be recovered here is the pivot/compose order,
//       and nobody has tried. It stays on this list only because it has not been done, NOT
//       because it is blocked.
//   CalcNearClipCorners               @0x821F25B8  (XMVectorTan + VMX corner lattice)
//   FindNonParallelNormalisedVectorTo @0x822171B0  (the SECOND of its two returned constants
//                                                   is still unattested; unk_82181510 is now
//                                                   pinned -- see KV_AXIS_Y below)
//   GetFOVDegsToFitObjectToScreenArea @0x8220C398  (VMX rsqrt/reciprocal Newton-refine + vsel)
//
// ⛔ CORRECTED 2026-08-01 -- CreateAdjustedLookAt and GetFOVDegsToFitObjectToScreenSize used
//   to sit in the list above, described as unbodiable "vrefp128 + vperm128 mask" and "two
//   vrefp128 Newton-refine reciprocals" blocks. THAT WAS A MISREADING OF WHAT THE VMX IS
//   DOING: neither reads a coefficient table. `vrefp128` + Newton-Raphson is a RECIPROCAL
//   (i.e. a divide, which the PC writes as `/`), and the `vperm` against unk_82CDA350 is the
//   tree's already-documented `Vector3(x, y, z)` construction mask, not data. There was
//   nothing to fabricate in either, and both are now bodied below.
//   THE LESSON GENERALISES: "unattested rodata" and "an addressing mode I have not decoded"
//   are different findings, and only the first one blocks a reconstruction. Anything still
//   on the list above should be re-read with that distinction in mind before it is trusted.

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

namespace
{
    // Half a degree in radians (pi/360). The FOV<->zoom conversions evaluate tan/atan of
    // half the FOV; pinned from the asm immediate 0.0087266462 (matches BrnLooker.cpp's
    // KF_HALF_DEG_TO_RADS in this same directory).
    const f32 KF_HALF_DEGS_TO_RADS = 0.0087266462f;

    // Two radians in degrees (2 * 180/pi == 360/pi). atan() returns half the FOV in radians;
    // the console scales it back by this to reach the full FOV in degrees.
    const f32 KF_TWO_RADS_TO_DEGS = 114.591559f;

    // ConvertFOVDegsToLensLength floors the half-FOV tangent at 1e-4 before dividing, to
    // guard the reciprocal against a near-zero (very wide) FOV.
    const f32 KF_TAN_FLOOR = 1.0e-4f;

    // pi/2 -- GetPitchAboutPointRads returns (pi/2 - acos(dir.y)) == asin(dir.y).
    // Also the near-vertical gate and the den==0 arm of the Euler decomposition below;
    // DUMPED as flt_82001754 == 1.570796371 (and its negative flt_82005560).
    const f32 KF_HALF_PI = 1.5707964f;

    // pi -- the den<0 arm of the Euler decomposition's atan2. DUMPED as
    // flt_8200174C == 3.141592741.
    const f32 KF_PI = 3.1415927f;

    // The +/-1 clamp band EulerAnglesZXYFromMatrix44Affine puts every source row through.
    // DUMPED as flt_820037C8 == -1 and flt_82001C98 == +1; the console's vmaxfp/vminfp
    // operands carry 0 in the fourth lane, which is why the w lane is pinned to 0 here.
    const Vector3 KV_MINUS_ONE = { -1.0f, -1.0f, -1.0f, 0.0f };
    const Vector3 KV_PLUS_ONE  = {  1.0f,  1.0f,  1.0f, 0.0f };

    // ---- the identity-basis rodata block, DUMPED from the shipped image -----------------
    // Headless IDA 9.3 over BURNOUT_X360_ARTIST.XEX read 0x82181500..0x8218153F as four
    // consecutive 16-byte rows; only the first carries a symbol, and the other three are the
    // `unk_8218151x` sentinels several TUs in this tree still describe as "unattested":
    //
    //   0x82181500  rw::math::vpu::detail::gIVector   3F800000 00000000 00000000 00000000  {1,0,0,0}
    //   0x82181510  unk_82181510                      00000000 3F800000 00000000 00000000  {0,1,0,0}
    //   0x82181520  unk_82181520                      00000000 00000000 3F800000 00000000  {0,0,1,0}
    //   0x82181530  unk_82181530                      00000000 00000000 00000000 3F800000  {0,0,0,1}
    //
    // CreateLookAt reads exactly two of them: unk_82181510 lands where the three-argument
    // overload takes its explicit lUpVector (so it IS the default up axis), and unk_82181520
    // is the Z axis substituted when the eye->target direction is degenerate.
    const Vector3 KV_AXIS_X = { 1.0f, 0.0f, 0.0f, 0.0f };   // gIVector      @0x82181500
    const Vector3 KV_AXIS_Y = { 0.0f, 1.0f, 0.0f, 0.0f };   // unk_82181510  @0x82181510
    const Vector3 KV_AXIS_Z = { 0.0f, 0.0f, 1.0f, 0.0f };   // unk_82181520  @0x82181520

    // The tolerance every rw::math::vpu IsZero in this family is called with: the shipped
    // splat source flt_82001770 == 0x34000000 == 2^-23 == FLT_EPSILON. (The vendor header's
    // own default parameter is a looser 1e-6 placeholder, so the calls below pass this
    // explicitly rather than silently widening the degenerate band by ten times.)
    const f32 KF_VPU_EPSILON = 1.1920929e-07f;

    // ---- the console's inlined vector atan2 --------------------------------------------
    // EulerAnglesZXYFromMatrix44Affine evaluates two quadrant-correct arctangents inline;
    // both expand to the SAME nine-instruction pattern (X360 0x82222374..0x822223E0 and
    // 0x82222404..0x82222438, and again in both degenerate arms at 0x82222534..0x8222255C).
    // The DecFIGS PS3 build attributes the identical block to the SDK's own vector arctan
    // (bits/atanf4.h + rw/math/vpu/detail/ps3/ppu/trig_operation_inline.h), so this is the
    // SDK helper, not hand-written code. Transcribed arm for arm:
    //
    //   base = XMVectorATan(num * Reciprocal(den))
    //   vcmpgtfp(0, den) -> vsel :  if (den <  0)  base += copysign(pi,   num)
    //   vcmpeqfp(0, den) -> vsel :  if (den == 0)  base  = copysign(pi/2, num)
    //
    // (the sign transplant is the literal `vand` of num against the 0x80000000 mask followed
    // by `vor` into the constant -- i.e. copysign, including for a negative zero numerator.)
    //
    // ⚠️ NOT std::atan2: for den == 0 the console hands back +/-pi/2 even when the numerator
    // is also zero, where std::atan2(0,0) is 0. That divergence is preserved deliberately --
    // it is the console's own degenerate answer.
    // FLAG (PC-platform, numeric): the console's `Reciprocal` is a vrefp estimate refined by
    // one Newton-Raphson step (`t = 1 - e*d ; e*t + e`, asm 0x82222364/0x82222368) and its
    // XMVectorATan is a minimax polynomial. Both are de-optimised here to the exact divide and
    // std::atan -- the standing convention of this tree (see rw/math/vpu/vector3_operation.h's
    // Normalize). Tighter than the console, never looser.
    f32 ATan2(f32 lfNumerator, f32 lfDenominator)
    {
        if (lfDenominator == 0.0f)
            return std::copysign(KF_HALF_PI, lfNumerator);

        const f32 lfAngle = std::atan(lfNumerator / lfDenominator);

        if (lfDenominator < 0.0f)
            return lfAngle + std::copysign(KF_PI, lfNumerator);

        return lfAngle;
    }
} // namespace

// @ 0x821F22A0 -- CameraUtils.h:157/:160 range tripwires (both non-gating; the
// stores land in the asm order: data/target first, then the ideal amount and the
// zeroed live amount between the two guards).
void TransitionSmoother::Set(f32 lfValue, f32 lfLerpAmount0to1,
                             f32 lfLerpAmountLerpAmount0to1,
                             f32 lfSimilarityToleranceScale)
{
    mfData   = lfValue;
    mfTarget = lfValue;

    CGS_ASSERT(lfLerpAmount0to1 >= 0.0f && lfLerpAmount0to1 <= 1.0f,
               "lfLerpAmount0to1 >= 0.0f && lfLerpAmount0to1 <= 1.0f");   // :157

    mfIdealLerpAmount = lfLerpAmount0to1;
    mfLerpAmount      = 0.0f;

    CGS_ASSERT(lfLerpAmountLerpAmount0to1 >= 0.0f && lfLerpAmountLerpAmount0to1 <= 1.0f,
               "lfLerpAmountLerpAmount0to1 >= 0.0f && lfLerpAmountLerpAmount0to1 <= 1.0f");   // :160

    mfLerpAmountLerpAmount     = lfLerpAmountLerpAmount0to1;
    mfSimilarityToleranceScale = lfSimilarityToleranceScale;
}

// CameraUtils.h:130 -- aim the smoother at a new value WITHOUT snapping to it (Set() above is
// the snapping form: it assigns mfData as well). One store; the console inlines it everywhere,
// so there is no standalone symbol and nothing to transcribe. `Update(dt)` is what walks
// mfData toward mfTarget and is still its own (declared-only) ledger function -- until it
// lands, a smoother that has only been given a target holds its Set() value, which is the
// conservative direction (a target with no chase never overshoots).
void TransitionSmoother::SetTarget(f32 lfTarget)
{
    mfTarget = lfTarget;
}

// @ 0x821F2378 -- wrap a value into [lo, hi) after adding a step (unsigned modular cycle).
// Guards lo < hi (CameraUtils.h:340). The __twllei on (hi-lo)==0 is the compiler-inserted
// divide-by-zero trap for the modulo below, not a separate assert.
u32 Cycle(u32 luValue, u32 luInclusiveLowerBound, u32 luExclusiveUpperBound, u32 luStep)
{
    CGS_ASSERT(luInclusiveLowerBound < luExclusiveUpperBound,
               "luInclusiveLowerBound < luExclusiveUpperBound");

    return (luValue - luInclusiveLowerBound + luStep)
               % (luExclusiveUpperBound - luInclusiveLowerBound)
           + luInclusiveLowerBound;
}

// @ 0x821F23E8 -- map a field-of-view (degrees) to a zoom scalar: zoom = 1 / tan(fov*pi/360).
// Asserts the tangent is non-zero before dividing (CameraUtils.h:559). The console evaluates
// tan twice (once for the guard, once for the reciprocal); one value here is identical.
f32 GetZoomFromFOVDegs(f32 lfFOVDegs)
{
    const f32 lfTan = rw::math::fpu::Tan(lfFOVDegs * KF_HALF_DEGS_TO_RADS);

    CGS_ASSERT(!rw::math::fpu::IsZero(lfTan),
               "!rw::math::fpu::IsZero(rw::math::fpu::Tan(lfFOVDegs * rw::math::fpu::DEGREES_TO_RADIANS * 0.5f))");

    return 1.0f / lfTan;
}

// @ 0x821F2490 -- map a zoom scalar to a field-of-view in degrees: fov = atan(1/zoom) * (2*180/pi).
// Asserts the zoom is non-zero (CameraUtils.h:577).
f32 GetFOVDegsFromZoom(f32 lfZoom)
{
    CGS_ASSERT(!rw::math::fpu::IsZero(lfZoom), "!rw::math::fpu::IsZero(lfZoom)");

    return std::atan(1.0f / lfZoom) * KF_TWO_RADS_TO_DEGS;
}

// @ 0x821F2530 -- convert a field-of-view (degrees) to a 35mm-equivalent lens length.
//   lensLength = (filmSize * 12) / (2 * tan(fovDegs * pi/360)), with the tangent floored at
//   1e-4 to guard the divide. Used by BrnDirector::DebugComponent::RenderHUD.
f32 ConvertFOVDegsToLensLength(f32 lfFOVDegs, f32 lfFilmSize)
{
    f32 lfHalfTan = rw::math::fpu::Tan(lfFOVDegs * KF_HALF_DEGS_TO_RADS);
    if (lfHalfTan <= 0.0f)
        lfHalfTan = KF_TAN_FLOOR;

    return (lfFilmSize * 12.0f) / (lfHalfTan * 2.0f);
}

// @ 0x82205A60 -- the pitch angle (radians) of the centre->point direction.
//   dir = Normalize(lPoint - lCentre); pitch = pi/2 - acos(dir.y)  ( == asin(dir.y) ).
// Asserts the direction is non-zero (CameraUtils.h:1205). std::acos stands in for the
// external XMVectorACos call (asm 0x82205B24).
f32 GetPitchAboutPointRads(Vector3 lCentre, Vector3 lPoint)
{
    const Vector3 lCentreToPoint = rw::math::vpu::Subtract(lPoint, lCentre);

    CGS_ASSERT(!rw::math::vpu::IsZero(lCentreToPoint), "!IsZero(lCentreToPoint)");

    const Vector3 lDir = rw::math::vpu::Normalize(lCentreToPoint);
    return KF_HALF_PI - std::acos(lDir.y);
}

// ----------------------------------------------------------------------------------------
// @ 0x8220C4F8 -- CameraUtils.cpp:704. Build the world look-at frame for an eye looking at a
// target, using the world up axis. THE single gate inside
// TrafficLaneTruck::CalcTransformFromLanePosition, and the basis builder ~30 director call
// sites share (xrefs_to lists BehaviourRig / BehaviourDebugFlyWorld / BehaviourGyroCam /
// TrafficLaneTruck::Update + MoveAlongTrafficLane{Forwards,Backwards} / ArbStatePostEvent /
// RaceCarEntityModule::SpawnRaceCar ...).
//
// The console body is one straight-line VMX pipeline; it is transcribed here operation for
// operation from the raw instruction stream, not from the fused pseudocode:
//
//   v123 = lEyePosition (v1)      v127 = lTargetPosition (v2)      r28 = the sret pointer
//   0x8220C51C..0x8220C5A4  IsValid(lEyePosition)      -- 3x vspltw128+vcmpeqfp. (x==x NaN test)
//   0x8220C5A8..0x8220C628  IsValid(lTargetPosition)
//   0x8220C630  vsubfp128 v13, v127, v123              -- lDirection = target - eye
//   0x8220C648..0x8220C674  IsZero(lDirection, flt_82001770)
//                             vandc against vslw128(-1,-1)==0x80000000  == per-lane fabs;
//                             vrlimi128 v11,v0,1,1 copies lane x into lane w so the w lane
//                             cannot poison the vcmpgtfp. "none set" (CR6 bit 2) test;
//                           -> if none of |x|,|y|,|z| exceeds the tolerance, jump to the
//                              fallback at 0x8220C720.
//   0x8220C678..0x8220C6B8  vmsum3fp128 dot3 + vrsqrtefp + TWO Newton-Raphson refinements
//                             (vnmsubfp t = 1 - d*e^2 ; vmaddfp e' = e + 0.5*e*t), then
//                             vmulfp128 v127 = lDirection * e''      == Normalize(lDirection)
//   0x8220C6BC..0x8220C71C  IsValid(the normalised Z) -- and if it is NOT valid, fall through
//                             into the SAME fallback (this overload substitutes silently; the
//                             three-argument one below asserts instead).
//   0x8220C720  v127 = unk_82181520 == {0,0,1,0}
//   0x8220C72C..0x8220C750  lXaxis = Cross(unk_82181510 /*{0,1,0,0}*/, lZaxis)
//                             the SDK permute form: yzx(a*yzx(b) - yzx(a)*b).
//   0x8220C754..0x8220C7B0  IsZero -> gIVector {1,0,0,0}, else the same rsqrt+2NR Normalize.
//   0x8220C82C  assert IsValid(lXaxis)                                        (:741)
//   0x8220C848..0x8220C860  lYaxis = Cross(lZaxis, lXaxis)  -- NOT normalised: Z and X are
//                             already orthonormal, so their cross is unit by construction.
//   0x8220C8C8  assert IsValid(lYaxis)                                        (:745)
//   0x8220C914  assert !IsZero(lYaxis)                                        (:746)
//   0x8220C934..0x8220C94C  stvx128 rows: [+0x00]=lXaxis [+0x10]=lYaxis [+0x20]=lZaxis
//                                         [+0x30]=lEyePosition
//
// FLAG (PC-platform, numeric): the console's reciprocal square root is a vrsqrtefp estimate
// refined by exactly two Newton-Raphson steps; the vendor Normalize this calls is the
// de-optimised exact 1/sqrt (the standing convention of vendor/renderware/include/rw/math/vpu/
// vector3_operation.h). The result is a touch TIGHTER than the console's, never looser.
// FLAG: Cross() clears the w lane, where the console leaves the permute residue there; every
// consumer reads xyz only (the affine rows are direction rows).
Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition)
{
    CGS_ASSERT(rw::math::vpu::IsValid(lEyePosition), "IsValid(lEyePosition)");
    CGS_ASSERT(rw::math::vpu::IsValid(lTargetPosition), "IsValid(lTargetPosition)");

    const Vector3 lDirection = rw::math::vpu::Subtract(lTargetPosition, lEyePosition);

    Vector3 lZaxis = KV_AXIS_Z;
    if (!rw::math::vpu::IsZero(lDirection, KF_VPU_EPSILON))
    {
        const Vector3 lNormalisedDirection = rw::math::vpu::Normalize(lDirection);
        if (rw::math::vpu::IsValid(lNormalisedDirection))
            lZaxis = lNormalisedDirection;
    }

    Vector3 lXaxis = rw::math::vpu::Cross(KV_AXIS_Y, lZaxis);
    if (rw::math::vpu::IsZero(lXaxis, KF_VPU_EPSILON))
        lXaxis = KV_AXIS_X;
    else
        lXaxis = rw::math::vpu::Normalize(lXaxis);

    CGS_ASSERT(rw::math::vpu::IsValid(lXaxis), "IsValid(lXaxis)");

    const Vector3 lYaxis = rw::math::vpu::Cross(lZaxis, lXaxis);

    CGS_ASSERT(rw::math::vpu::IsValid(lYaxis), "IsValid(lYaxis)");
    CGS_ASSERT(!rw::math::vpu::IsZero(lYaxis, KF_VPU_EPSILON), "!IsZero(lYaxis)");

    Matrix44Affine lLookAt;
    lLookAt.xAxis = lXaxis;
    lLookAt.yAxis = lYaxis;
    lLookAt.zAxis = lZaxis;
    lLookAt.wAxis = lEyePosition;
    return lLookAt;
}

// ----------------------------------------------------------------------------------------
// @ 0x8220C960 -- CameraUtils.cpp:767. The same builder with an explicit up axis (the exported
// symbol is `sub_8220C960`; it is this overload, pinned by its assert line numbers 0x30B/0x319/
// 0x31D == 779/793/797 and by the fact that its v3 argument occupies exactly the slot the
// two-argument form fills with unk_82181510).
//
//   v122 = lEyePosition (v1)   v2 = lTargetPosition   v123 = lUpVector (v3)   r25 = sret
//   0x8220C990  vsubfp128 v127, v2, v122            -- lDirection = target - eye
//   0x8220C998..0x8220C9C0  IsZero(lDirection) -> the unk_82181520 fallback at 0x8220CA08
//   0x8220C9C4..0x8220CA00  the rsqrt + 2x Newton-Raphson Normalize -> v126 = lZaxis
//   0x8220CA14..0x8220CAFC  assert IsValid(lZaxis) -- and unlike the two-argument overload
//                             there is NO silent substitution here: the failure path builds a
//                             CgsDev message stream, appends the PRE-normalised direction
//                             (`sub_82203F70` with v1 = v127) and fires at line :779.
//   0x8220CB00..0x8220CB24  lXaxis = Cross(lUpVector, lZaxis)
//   0x8220CB28..0x8220CB94  IsZero -> gIVector, else Normalize
//   0x8220CC00  assert IsValid(lXaxis)                                        (:793)
//   0x8220CC1C..0x8220CC3C  lYaxis = Cross(lZaxis, lXaxis)
//   0x8220CC64  assert !IsZero(lYaxis)                                        (:797)
//   0x8220CC84..0x8220CC9C  rows [+0x00]=lXaxis [+0x10]=lYaxis [+0x20]=lZaxis [+0x30]=eye
//
// (this overload does NOT re-check IsValid(lYaxis) -- only !IsZero.)
// FLAG: the :779 assert's console text is streamed ("Invalid Z Axis after normalisation,
// pre-normalised: " followed by the pre-normalise direction). CGS_ASSERT takes a plain literal,
// so the streamed vector is dropped from the message; the predicate is unchanged.
Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition, Vector3 lUpVector)
{
    const Vector3 lDirection = rw::math::vpu::Subtract(lTargetPosition, lEyePosition);

    Vector3 lZaxis = KV_AXIS_Z;
    if (!rw::math::vpu::IsZero(lDirection, KF_VPU_EPSILON))
        lZaxis = rw::math::vpu::Normalize(lDirection);

    CGS_ASSERT(rw::math::vpu::IsValid(lZaxis),
               "Invalid Z Axis after normalisation, pre-normalised: ");

    Vector3 lXaxis = rw::math::vpu::Cross(lUpVector, lZaxis);
    if (rw::math::vpu::IsZero(lXaxis, KF_VPU_EPSILON))
        lXaxis = KV_AXIS_X;
    else
        lXaxis = rw::math::vpu::Normalize(lXaxis);

    CGS_ASSERT(rw::math::vpu::IsValid(lXaxis), "IsValid(lXaxis)");

    const Vector3 lYaxis = rw::math::vpu::Cross(lZaxis, lXaxis);

    CGS_ASSERT(!rw::math::vpu::IsZero(lYaxis, KF_VPU_EPSILON), "!IsZero(lYaxis)");

    Matrix44Affine lLookAt;
    lLookAt.xAxis = lXaxis;
    lLookAt.yAxis = lYaxis;
    lLookAt.zAxis = lZaxis;
    lLookAt.wAxis = lEyePosition;
    return lLookAt;
}

// ----------------------------------------------------------------------------------------
// ⭐ @0x82222180 -- CameraUtils.cpp:453. The ZXY Euler angles (radians) of an affine's
// rotation. THE gate that was holding the fly-by's camera BANK: TrafficLaneTruck::Update
// feeds it the frame-to-frame relative rotation and the road-runner behaviour rolls the
// camera by the resulting yaw rate.
//
// Read off the raw instruction stream (the pseudocode fuses the two degenerate arms):
//
//   0x822221A4..0x82222218  the three ROTATION rows are each clamped lane-wise into
//                             [-1, +1]: vmaxfp against {-1,-1,-1,0} (flt_820037C8) then
//                             vminfp against {+1,+1,+1,0} (flt_82001C98). The translation
//                             row is never loaded.
//   0x8222221C..0x82222264  the "keep last frame" arm: only when lpLastAngles is non-null
//                             AND | |zAxis.y| - 1 | < lfVerticalComparisonEpsilon, i.e. the
//                             frame is within epsilon of straight up/down, where the yaw and
//                             roll are not separable. Returns *lpLastAngles VERBATIM.
//   0x82222268..0x82222294  pitch = XMVectorASin(-zAxis.y)   -> lane x  (vrlimi128 mask 8)
//   0x822222BC / 0x822222FC the two-sided gate on that pitch: pi/2 > pitch > -pi/2
//                             (flt_82001754 / flt_82005560, both dumped).
//   0x8222232C..0x822223E8    normal arm, yaw : atan2(zAxis.x, zAxis.z) -> lane y (mask 4/3)
//   0x822223EC..0x8222243C    normal arm, roll: atan2(xAxis.y, yAxis.y) -> lane z (mask 2/2)
//   0x82222440 / 0x822224B0   the two DEGENERATE arms -- the compiler duplicated one body
//                             into two stack-slot allocations; both compute the identical
//                             pair: yaw = atan2(xAxis.z, xAxis.x) and roll = 0
//                             (flt_82001CC0). At the poles the roll is folded into the yaw,
//                             which is exactly what that substitution says.
//
// ⚠️ VMX128 OPERAND-ORDER NOTE (a correction to the rule this tree recorded with SLerp):
// IDA prints the PLAIN VMX `vnmsubfp` in architectural order, so 0x82222470's
// `vnmsubfp v13, v0, v13, v12` really is `vB - vA*vC`. The VMX128 forms are NOT the same
// shape -- `vnmsubfp128 vD, vA, vB` is `vD -= vA*vB` and `vmaddcfp128 vD, vA, vB` is
// `vD = vA*vD + vB`, with IDA printing the implied vD as an extra operand. Both are pinned
// by the reciprocal idiom at 0x82222364/0x8222236C, which only reads as `t = 1 - e*d` and
// `num * (1/den)` under that reading.
//
// FLAG (PC-platform, numeric): XMVectorASin is the console's minimax polynomial; std::asin
// is the exact form, the same de-optimisation the rest of this file already applies. The
// clamp above guarantees the argument is in range, so there is no domain risk.
// FLAG (lane w): the console never writes the fourth lane -- it loads the result register
// from an uninitialised stack slot and vrlimi128s only lanes x/y/z into it, so the w lane is
// whatever residue was there. Pinned to 0 here rather than propagating indeterminate bits;
// every consumer reads xyz only (IsValid / GetLocalAngularVelocity().y).
Vector3 EulerAnglesZXYFromMatrix44Affine(Matrix44Affine lIn, Vector3* lpLastAngles,
                                         f32 lfVerticalComparisonEpsilon)
{
    const Vector3 lXaxis = rw::math::vpu::Min(rw::math::vpu::Max(lIn.xAxis, KV_MINUS_ONE),
                                              KV_PLUS_ONE);
    const Vector3 lYaxis = rw::math::vpu::Min(rw::math::vpu::Max(lIn.yAxis, KV_MINUS_ONE),
                                              KV_PLUS_ONE);
    const Vector3 lZaxis = rw::math::vpu::Min(rw::math::vpu::Max(lIn.zAxis, KV_MINUS_ONE),
                                              KV_PLUS_ONE);

    if (lpLastAngles != 0
        && std::fabs(std::fabs(lZaxis.y) - 1.0f) < lfVerticalComparisonEpsilon)
    {
        return *lpLastAngles;
    }

    Vector3 lEulerAngles;
    lEulerAngles.x = std::asin(-lZaxis.y);
    lEulerAngles.w = 0.0f;

    if (KF_HALF_PI > lEulerAngles.x && lEulerAngles.x > -KF_HALF_PI)
    {
        lEulerAngles.y = ATan2(lZaxis.x, lZaxis.z);
        lEulerAngles.z = ATan2(lXaxis.y, lYaxis.y);
    }
    else
    {
        lEulerAngles.y = ATan2(lXaxis.z, lXaxis.x);
        lEulerAngles.z = 0.0f;
    }

    return lEulerAngles;
}

// ----------------------------------------------------------------------------
// RotateMatrix44AffineByEulerAnglesZXY  @0x82204F98 / PS3 @0xA9780   (368 asm lines)
//
// BODIED 2026-08-02 (rotate-helper wave). It was DECLARATION-ONLY and it was the highest-
// leverage symbol left in the chase-camera cluster: BehaviourGameplayExternal's
// CalculateCameraTransform needs it twice and ApplyJumpEffects once, BrnCameraShake.cpp
// cannot be mounted without it, and BrnPerlinShakeController / BrnBehaviourDebugFlyWorld /
// BrnLooker already call it.
//
// ⛔⛔ ITS COMMITTED BLOCKING REASON WAS STALE, AND IT IS THE SAME STALE SHAPE THIS FILE'S
// OWN BANNER ALREADY WARNS ABOUT. DirectorLinkStubs.cpp said the body is "almost entirely an
// inlined XMVectorSinCos minimax polynomial whose coefficient table has not been dumped".
// The first clause is true and the second is true, AND NEITHER IS A REASON: the coefficients
// are an implementation detail OF sin and cos, and de-optimising a console minimax to the
// exact libm form is the standing convention of this very file -- already applied to
// XMVectorASin and XMVectorATan inside EulerAnglesZXYFromMatrix44Affine directly above, and
// to vrsqrtefp/vrefp in Normalize and OrthoNormalize3x3. There was never anything to
// fabricate. (`ApplyPitchAboutPointRads @0x822183E0` is still on the banner's FLAG list
// citing the SAME rodata range 82000BD0..82000C60 for the SAME reason -- re-read it before
// trusting it.)
//
// WHAT THE ASM ACTUALLY IS, and how each claim below was settled:
//
//   * THE ANGLE LANES. Three SinCos evaluations, in source order Y, X, Z -- the console
//     splats lane 1 first (0x82204F9C), lane 0 second (0x82205134), lane 2 third
//     (0x82205328). So lEulerAngles is {x = pitch, y = yaw, z = roll}, which is also what
//     the two DebugFlyWorld call sites pass ({0, mfYaw, 0} and {mfPitch, mfYaw, mfRoll}).
//
//   * THE RANGE REDUCTION IS DUMPED, not inferred: v30/v29 are lanes 3 and 1 of the 16-byte
//     row at 0x82000C60, read out of the shipped image as
//         {3.1415927, 6.2831855, 0.31830987, 0.15915494} == {pi, 2pi, 1/pi, 1/(2pi)}
//     and the idiom at 0x82204FD0..0x82205024 is `x - 2pi * vrfin(x * (1/(2pi)))`. std::sin
//     and std::cos do their own (better) argument reduction, so this drops out.
//     ⚠️ Only the reduction row is attested; 0x82000BD0..0x82000C50 (the minimax
//     coefficients themselves) read as UNMAPPED in the .id1 -- which is exactly why they are
//     not transcribed and exactly why they do not need to be.
//
//   * THE THREE ELEMENTARY MATRICES, read off the vperm/vrlimi128 packing store-for-store:
//         Ry rows: ( cosY, 0, -sinY ) ( 0, 1, 0 ) ( sinY, 0, cosY )    0x82205190..0x822051B8
//         Rx rows: ( 1, 0, 0 ) ( 0, cosX, sinX ) ( 0, -sinX, cosX )    0x822052B0..0x822052F0
//         Rz rows: ( cosZ, sinZ, 0 ) ( -sinZ, cosZ, 0 ) ( 0, 0, 1 )    0x82205454..0x8220549C
//     Every one of the three has a ZERO fourth row (each is built by vperm-ing the zero
//     register), which is why the composed rotation contributes no translation below.
//
//   * THE COMPOSITION ORDER, which is the only thing in here that was ever real work:
//         lRotation = Mult(Rz, Mult(Rx, Ry))          [row-major, i.e. Z applied first]
//         lrMatrix  = Mult(lrMatrix, lRotation)
//     The inner product is at 0x822052EC..0x82205378 (Rx's rows broadcast against Ry's rows)
//     and the outer at 0x8220547C..0x822054E8 (Rz's rows against that product).
//
//   * ⭐ AND IT ROUND-TRIPS. EulerAnglesZXYFromMatrix44Affine directly above is this
//     function's exact inverse, so the order is TESTABLE rather than arguable. Composing the
//     three rows above in this order gives
//         zAxis = ( cosX*sinY, -sinX, cosX*cosY )
//         xAxis.y = sinZ*cosX ,  yAxis.y = cosZ*cosX
//     and the inverse reads back exactly pitch = asin(-zAxis.y), yaw = atan2(zAxis.x,
//     zAxis.z), roll = atan2(xAxis.y, yAxis.y) -- which is, line for line, what the
//     committed decomposition does. No other ordering of the three closes that loop.
//
// ⚠️ THE TRANSLATION ROW IS ROTATED TOO, AND THAT IS NOT A TRANSCRIPTION SLIP. The console
//   loads all FOUR rows (r3, r3+0x10, r3+0x20, r3+0x30 at 0x82205440..0x822054F4) and stores
//   all four back (0x822054F8..0x8220554C); the w row goes through the same
//   `row.x*R0 + row.y*R1 + row.z*R2 + R3` cascade as the other three, with R3 == 0. So this
//   is the plain affine product `lrMatrix * lRotation`, and a matrix with a non-zero
//   position has that position swung about the WORLD ORIGIN. Callers that want an in-place
//   re-orientation must hand it a matrix whose Pos() is zero (DebugFlyWorld does exactly
//   that: it SetIdentity()s a scratch frame first). Preserved deliberately -- it is the
//   console's own behaviour, and Looker's fixed-look-offset path depends on whatever it
//   produces.
//
// FLAG (PC-platform, numeric): the console's SinCos is a shared minimax polynomial over a
//   2pi-reduced argument; std::sin / std::cos are the exact forms. Tighter than the console,
//   never looser -- the same de-optimisation the rest of this file already applies.
// ----------------------------------------------------------------------------
void RotateMatrix44AffineByEulerAnglesZXY(Matrix44Affine& lrMatrix, Vector3 lEulerAngles)
{
    // The console evaluates SinCos in the order Y, X, Z and hands each pair to the SDK's own
    // elementary builder (matrix44affine_operation_platform_inline.h :253 / :239-:240 / :269
    // respectively -- see the note at MakeRotationX in matrix44affine_operation.h for how
    // those three line numbers were pinned to their axes).
    const Matrix44Affine lRotateAboutY = rw::math::vpu::MakeRotationY(lEulerAngles.y);
    const Matrix44Affine lRotateAboutX = rw::math::vpu::MakeRotationX(lEulerAngles.x);
    const Matrix44Affine lRotateAboutZ = rw::math::vpu::MakeRotationZ(lEulerAngles.z);

    const Matrix44Affine lRotation =
        rw::math::vpu::Mult(lRotateAboutZ,
                            rw::math::vpu::Mult(lRotateAboutX, lRotateAboutY));

    lrMatrix = rw::math::vpu::Mult(lrMatrix, lRotation);
}

// ============================================================================
// THE ZOOM / SCREEN-FIT TRIO -- BODIED 2026-08-01 (orbit-camera wave).
//
// All three were DECLARATION-ONLY, and all three are on the live car-select path:
// BehaviourRotateAboutVehicle::Update calls GetSizeOnScreen and
// GetFOVDegsToFitObjectToScreenSize twice each and CreateAdjustedLookAt once, per frame.
// (Looker::Zoom is the console's other caller of the first two.)
// ============================================================================

// ----------------------------------------------------------------------------
// GetSmallestDifferenceBetweenDegsAngles @0x821F8868   (71 asm lines)
//
// The signed shortest arc (degrees) from lfFromDegs to lfToDegs, wrapped into [-180, 180].
// BODIED 2026-08-01 (orbit-camera wave): it was declaration-only, and it is the last callee
// standing between CameraSphericalRotationController::Update (the car-select free-look stick)
// and a clean link.
//
//   0x821F8884  fsubs f31, f2, f1                       -- the raw delta, TO minus FROM
//   0x821F8894  the |delta| >= 360 gate; inside it, `* (1/360)` (flt_82004920) then
//               fctiwz/fcfid/frsp -- a TRUNCATION toward zero, not a round -- then
//               `fnmsubs f31, f0, f30, f31` == delta - trunc(delta/360) * 360
//   0x821F88D8  assert "(lrAngle < 360.0f) && (lrAngle > -360.0f)"   CameraUtils.cpp:535
//   0x821F890C  if (delta >  180) delta -= 360;  else if (delta < -180) delta += 360;
//   0x821F8938  assert "(lrAngle <= 180.0f) && (lrAngle >= -180.0f)" CameraUtils.cpp:546
//
// ⚠️ THE TWO ASSERTS ARE NOT REDUNDANT AND ARE NOT THE SAME TEST: the first is STRICT on both
//   ends (< 360 / > -360) and the second is INCLUSIVE (<= 180 / >= -180). Reproduced exactly.
// ⚠️ The half-open wrap is asymmetric on purpose: a delta of exactly +180 is left alone
//   (the test is `> 180`) while exactly -180 is also left alone (`< -180`), so both ends of
//   the band survive and the second assert passes either way.
// ----------------------------------------------------------------------------
f32 GetSmallestDifferenceBetweenDegsAngles(f32 lfFromDegs, f32 lfToDegs)
{
    const f32 KF_FULL_TURN_DEGS     = 360.0f;    // flt_82004928
    const f32 KF_NEG_FULL_TURN_DEGS = -360.0f;   // flt_82004924
    const f32 KF_RECIP_FULL_TURN    = 0.0027777778f;  // flt_82004920 == 1/360
    const f32 KF_HALF_TURN_DEGS     = 180.0f;    // flt_820025FC
    const f32 KF_NEG_HALF_TURN_DEGS = -180.0f;   // flt_820048B4

    f32 lfAngle = lfToDegs - lfFromDegs;

    if (lfAngle >= KF_FULL_TURN_DEGS || lfAngle <= KF_NEG_FULL_TURN_DEGS)
    {
        // `fctiwz` truncates toward zero, so this is the sign-preserving remainder.
        const f32 lfTurns = static_cast<f32>(static_cast<s32>(lfAngle * KF_RECIP_FULL_TURN));
        lfAngle -= lfTurns * KF_FULL_TURN_DEGS;
    }

    CGS_ASSERT(lfAngle < KF_FULL_TURN_DEGS && lfAngle > KF_NEG_FULL_TURN_DEGS,
               "(lrAngle < 360.0f) && (lrAngle > -360.0f)");            // .cpp:535

    if (lfAngle > KF_HALF_TURN_DEGS)
    {
        lfAngle -= KF_FULL_TURN_DEGS;
    }
    else if (lfAngle < KF_NEG_HALF_TURN_DEGS)
    {
        lfAngle += KF_FULL_TURN_DEGS;
    }

    CGS_ASSERT(lfAngle <= KF_HALF_TURN_DEGS && lfAngle >= KF_NEG_HALF_TURN_DEGS,
               "(lrAngle <= 180.0f) && (lrAngle >= -180.0f)");          // .cpp:546

    return lfAngle;
}

// ----------------------------------------------------------------------------
// GetSmallestDifferenceBetweenRadAngles(f32, f32) @0x821F8988 / PS3 @0x37EA4  (75 asm lines)
//
// ⭐⭐ ADDED 2026-08-02 (chase-camera helper wave). This was named by the predecessor wave as
// THE one link dependency of BehaviourGameplayExternal::Update that does not live in that
// file -- Update calls the Vector3 overload at BehaviourGameplayExternal.cpp:337 and neither
// overload was declared anywhere in the tree. The only sibling that existed was the DEGREES
// scalar above.
//
// It is that sibling, term for term, with 2*PI where it has 360 and PI where it has 180 --
// which is itself the cross-check, since the two were transcribed from different asm four
// days apart and land on the same shape:
//   0x821F89B8  fsubs f31, f29, f30                      -- the raw delta, TO minus FROM
//   0x821F89BC  the |delta| >= 2*PI gate (>= flt_82001C94 == +6.2831855, or
//               <= flt_82004980 == -6.2831855)
//   0x821F89D8  `* flt_82001C90` (== 0.15915494 == 1/(2*PI)) then fctiwz/fcfid/frsp -- a
//               TRUNCATION toward zero, not a round -- then
//               `fnmsubs f31, f13, f0, f31` == delta - trunc(delta/2PI) * 2PI
//   0x821F8A00  the first assert, CameraUtils.cpp:578 (0x242). ⚠️ ITS TEXT IS NOT A LITERAL:
//               the console STREAMS it ("Angle: " << angle << " From: " << from << " To: "
//               << to << "\n") through gpcMessageBuffer and passes the built buffer to
//               FireAssert. The condition is the strict band, exactly as in the degs sibling.
//   0x821F8B04  if (delta >  PI /*flt_8200174C*/) delta -= 2PI;
//               else if (delta < -PI /*flt_82004964*/) delta += 2PI;
//   0x821F8B30  assert "(lrAngle <= rw::math::PI) && (lrAngle >= -rw::math::PI)"
//               CameraUtils.cpp:589 (0x24D) -- this one IS a literal, and it names the
//               constant, which is how we know flt_8200174C is rw::math::PI and not a
//               coincidental 3.14159.
// Every constant above was read out of the image with scratchpad\afw_id1b.py, not inferred.
//
// ⚠️ THE TWO ASSERTS ARE NOT THE SAME TEST (same trap as the degs sibling): the first is
// STRICT on both ends, the second INCLUSIVE. Reproduced exactly. Both non-gating.
// ----------------------------------------------------------------------------
f32 GetSmallestDifferenceBetweenRadAngles(f32 lfFromRads, f32 lfToRads)
{
    const f32 KF_FULL_TURN_RADS     =  6.2831855f;    // flt_82001C94 == 2*PI
    const f32 KF_NEG_FULL_TURN_RADS = -6.2831855f;    // flt_82004980
    const f32 KF_RECIP_FULL_TURN    =  0.15915494f;   // flt_82001C90 == 1/(2*PI)
    const f32 KF_PI                 =  3.1415927f;    // flt_8200174C == rw::math::PI
    const f32 KF_NEG_PI             = -3.1415927f;    // flt_82004964

    f32 lfAngle = lfToRads - lfFromRads;

    if (lfAngle >= KF_FULL_TURN_RADS || lfAngle <= KF_NEG_FULL_TURN_RADS)
    {
        // `fctiwz` truncates toward zero, so this is the sign-preserving remainder.
        const f32 lfTurns = static_cast<f32>(static_cast<s32>(lfAngle * KF_RECIP_FULL_TURN));
        lfAngle -= lfTurns * KF_FULL_TURN_RADS;
    }

    // .cpp:578 -- streamed message, strict band. Non-gating (the console's next instruction
    // reads lfAngle regardless).
    CGS_ASSERT(lfAngle < KF_FULL_TURN_RADS && lfAngle > KF_NEG_FULL_TURN_RADS, "Angle");

    if (lfAngle > KF_PI)
    {
        lfAngle -= KF_FULL_TURN_RADS;
    }
    else if (lfAngle < KF_NEG_PI)
    {
        lfAngle += KF_FULL_TURN_RADS;
    }

    CGS_ASSERT(lfAngle <= KF_PI && lfAngle >= KF_NEG_PI,
               "(lrAngle <= rw::math::PI) && (lrAngle >= -rw::math::PI)");   // .cpp:589

    return lfAngle;
}

// ----------------------------------------------------------------------------
// GetSmallestDifferenceBetweenRadAngles(Vector3, Vector3) PS3 @0x382E0   (60 asm lines)
//
// Three calls to the scalar overload, one per lane, permuted back into a vector.
// Store-for-store from the PS3 asm (this overload is fully inlined away in the X360 build,
// so DecFIGS is the only witness -- and it is an unambiguous one, because every one of the
// three `bl`s targets the scalar overload by name):
//   0x38348..0x38378  lane X: f1 = lFromRads.x, f2 = lToRads.x  -> call
//   0x38384..0x383EC  lane Y: same pair, vperm'd into result lane 1
//                             (VectorPermuteConstant<4,1,2,3> then <0,4,2,3>)
//   0x383F8..0x38464  lane Z: same pair, vperm'd into result lane 2
//                             (VectorPermuteConstant<0,1,4,3>)
//
// ⚠️ THE W LANE IS NEVER WRITTEN. The console `lvx v31, 0, r29` loads the (uninitialised)
// sret buffer and only ever vperms lanes 0/1/2 into it -- the three permute constants above
// each keep source lane 3. Reproduced by leaving lResult.w alone rather than zeroing it,
// which would be an invention. (Every caller in this cluster consumes X/Y/Z only.)
// ----------------------------------------------------------------------------
Vector3 GetSmallestDifferenceBetweenRadAngles(Vector3 lFromRads, Vector3 lToRads)
{
    Vector3 lResult;
    lResult.x = GetSmallestDifferenceBetweenRadAngles(lFromRads.x, lToRads.x);
    lResult.y = GetSmallestDifferenceBetweenRadAngles(lFromRads.y, lToRads.y);
    lResult.z = GetSmallestDifferenceBetweenRadAngles(lFromRads.z, lToRads.z);
    return lResult;
}

// ----------------------------------------------------------------------------
// PositiveValueTendToLimit @0x821F8B78 / PS3 @0x1B410   (DWARF CameraUtils.cpp:616)
// ADDED 2026-08-02 (final-helpers wave). ~30 asm lines, of which ~20 are the three asserts.
//
// The saturating response curve the chase camera's slide/drift offsets are shaped by. It is
// a Michaelis-Menten / hyperbola: strictly increasing, asymptotic to lrLimit, and reaching
// exactly HALF of lrLimit at lrValueIn == lrValueForHalfway -- which is the whole point of
// the second parameter's name and the check that this decode is the right one.
//
// asm walk (f1 = lrValueIn, f2 = lrValueForHalfway, f3 = lrLimit):
//   0x821F8BAC  assert lrValueIn >= 0.0f                                 (.cpp:618)
//   0x821F8BD0  assert lrValueForHalfway > 0.0f                          (.cpp:643)
//   0x821F8BF4  fdivs f30, f31, f30            -- lrX = in / halfway
//   0x821F8C04  fadds f31, f30, 1.0f           -- lrX + 1
//   0x821F8C0C  the IsZero band test on that sum, then                   (.cpp:645)
//               assert "!rw::math::fpu::IsZero(lrX+1.0f)"
//   0x821F8C54  fdivs f0, f30, f31 ; fmuls f1, f0, f28
//
// ⚠️ THE THIRD ASSERT'S BAND *IS* rw::math::fpu::IsZero, not a hand-rolled epsilon: the two
// bounds it compares against are flt_82001770 == +1.1920929e-07 and flt_82002514 ==
// -1.1920929e-07, both DUMPED, and that is exactly the vendor header's KF_IS_ZERO_TOLERANCE.
// Spelled as the call the assert text names rather than as two literals.
// ⚠️ All three asserts are NON-GATING (the console falls straight through into the divide).
// ⚠️ PARAMETER NAMES are the DWARF's own (the PS3 export carries them on f1/f2/f3).
// ----------------------------------------------------------------------------
f32 PositiveValueTendToLimit(f32 lrValueIn, f32 lrValueForHalfway, f32 lrLimit)
{
    CGS_ASSERT(lrValueIn >= 0.0f, "lrValueIn >= 0.0f");                    // .cpp:618
    CGS_ASSERT(lrValueForHalfway > 0.0f, "lrValueForHalfway > 0.0f");      // .cpp:643

    const f32 lrX = lrValueIn / lrValueForHalfway;

    CGS_ASSERT(!rw::math::fpu::IsZero(lrX + 1.0f),                         // .cpp:645
               "!rw::math::fpu::IsZero(lrX+1.0f)");

    return (lrX / (lrX + 1.0f)) * lrLimit;
}

// ----------------------------------------------------------------------------
// TendToLimits(f32 x5) PS3 @0x1B53C   (DWARF CameraUtils.cpp:660)
// ADDED 2026-08-02 (final-helpers wave). SIX instructions on PS3, both arms a TAIL BRANCH
// into PositiveValueTendToLimit -- so the whole function is "fold the sign away, then pick
// which of the two authored response pairs applies".
//
//   0x1B540  fcmpu cr7, lrValueIn, 0.0 ; cror eq = gt|eq ; bne -> the negative arm
//   0x1B54C  (>= 0)  fmr f2, f4 ; fmr f3, f5  -- swap the POS pair into the callee's slots
//   0x1B558  (<  0)  fneg f1, f1              -- and keep the NEG pair already in place
//
// ⚠️ NO X360 SYMBOL AT ALL: the X360 ARTIST build inlines this into every caller. That is
// not a gap, it is the corroboration -- BehaviourGameplayExternal::ApplySlideyEffects
// contains two verbatim expansions of exactly this shape (@0x822262E4 and @0x822263EC,
// each an `fcmpu`/`blt` around an `fneg` and two `fmr`/`lfs` pairs feeding one
// PositiveValueTendToLimit call), and reading those is how that helper's arguments were
// separated. Two builds, one function.
// ⚠️ THE NEG PAIR COMES FIRST in the signature and the two are easy to transpose. The
// console's own `fmr f2, f4` / `fmr f3, f5` in the >= 0 arm is what fixes it: f2/f3 are the
// slots PositiveValueTendToLimit reads, so f2/f3 must be the NEGATIVE pair and f4/f5 the
// POSITIVE one. DecFIGS names all five.
// ----------------------------------------------------------------------------
f32 TendToLimits(f32 lrValueIn,
                 f32 lrNegValueForHalfway, f32 lrNegLimit,
                 f32 lrPosValueForHalfway, f32 lrPosLimit)
{
    if (lrValueIn >= 0.0f)
    {
        return PositiveValueTendToLimit(lrValueIn, lrPosValueForHalfway, lrPosLimit);
    }

    return PositiveValueTendToLimit(-lrValueIn, lrNegValueForHalfway, lrNegLimit);
}

// ----------------------------------------------------------------------------
// SineLerp @0x8220CCB0 / PS3 @0x20AE4   (DWARF CameraUtils.cpp:808)
// ADDED 2026-08-02 (final-helpers wave). ~25 asm lines.
//
// A cosine-eased lerp: ordinary Lerp(from, to, t) with the parameter first pushed through
// the classic smooth-step-by-cosine remap, so the blend starts and ends with zero slope.
//
// asm walk (f1 = lfFrom, f2 = lfTo, f3 = lfParameter):
//   0x8220CCE0  assert lfParameter >= 0.0f && lfParameter <= 1.0f     (.cpp:810)
//   0x8220CD14  lfs flt_8200174C (PI, DUMPED 3.1415927) ; fmuls f1, f30, f0
//   0x8220CD1C  bl cos                       <- a REAL libm call in the shipped X360 image,
//                                               not an inlined XMVectorSinCos minimax
//   0x8220CD24  lfs flt_82001DA0 == 0.5      ; fsubs f13, f28, f29  == lfTo - lfFrom
//   0x8220CD30  fadds f12, f12, 1.0f
//   0x8220CD34  fnmsubs f0, f12, 0.5f, 1.0f  == 1 - (cos + 1) * 0.5
//   0x8220CD38  fmadds f1, f0, f13, f29      == lfFrom + t * (lfTo - lfFrom)
//
// ⭐ THE REMAP IS PINNED BY ITS ENDPOINTS, not by pattern-matching: p=0 -> 1-(1+1)/2 = 0,
// p=1 -> 1-(-1+1)/2 = 1, p=0.5 -> 1-(0+1)/2 = 0.5. Any sign slip breaks one of the three.
// ⚠️ The assert is NON-GATING, and its bound is INCLUSIVE at both ends.
// ⚠️ NOTE FOR CALLERS: an out-of-range parameter does not clamp -- it extrapolates through
// the cosine, which is why the assert exists at all.
// ----------------------------------------------------------------------------
f32 SineLerp(f32 lfFrom, f32 lfTo, f32 lfParameter)
{
    const f32 KF_PI = 3.1415927f;   // flt_8200174C == rw::math::PI

    CGS_ASSERT(lfParameter >= 0.0f && lfParameter <= 1.0f,                 // .cpp:810
               "lfParameter >= 0.0f && lfParameter <= 1.0f");

    const f32 lfEased = 1.0f - (std::cos(lfParameter * KF_PI) + 1.0f) * 0.5f;

    return lfFrom + lfEased * (lfTo - lfFrom);
}

// ----------------------------------------------------------------------------
// GetSizeOnScreen @0x82221918   (360 asm lines)
//
// The on-screen footprint of an oriented AABB, in NORMALISED SCREEN FRACTIONS: project all
// eight corners through the camera and return the width/height of their screen-space bounds.
//
// ---- asm walk (r30 = &lCameraTransform, r29 = &lTargetTransform, r5..r8 = the AABB) ----
//   0x82221970..0x82221998  lvFOV * (pi/180) * 0.5 -> XMVectorTan   == tan(half FOV)
//   0x8222199C..0x82221A40  vrefp128 + 2x Newton on lvAspect, times that tangent
//                             == tan(half FOV) / aspect, the vertical half-extent at unit depth
//   0x822219C8..0x82221AE0  six vmrghw/vmrglw (the 3x3 transpose) + `vsubfp v12, 0, camPos`
//                             + two vmaddfp cascades == the target transform expressed in
//                             CAMERA space, i.e. Mult(target, inverse(camera))
//   0x82221AD0..0x82221BB4  four vperm (mask @0x82CDA350 == {A.x, B.y, A.x, A.x}) each followed
//                             by a `vrlimi128 …, 2, 0` that drops in the z lane -- the eight
//                             Vector3(x, y, z) min/max combinations, stored to an 8x16 stack array
//   0x82221BF8..0x82221CA0  each corner run through the camera-space transform (vmaddfp cascade)
//   0x82221CB4..0x82221E90  the r9 = 8 loop: reject |z| <= FLT_EPSILON, perspective-divide,
//                             scale by 0.5, and fold into a running min/max
//   0x82221E94              `vsubfp v1, v29, v31` == max - min, returned in v1
//
// ⚠️⚠️ THE MAX ACCUMULATOR IS SEEDED WITH FLT_MIN (1.1754944e-38, `flt_82001738`), NOT
//   -FLT_MAX. That is the smallest positive normal, so an object whose entire screen
//   footprint lies at negative x (or negative y) never updates that axis's max and the
//   returned size is wrong -- for a centred subject it cannot happen (the bounds straddle
//   zero), which is why it shipped. REPRODUCED AS-IS: it is the console's behaviour, it is
//   benign for every caller in the image, and "fixing" it would break parity. The min
//   accumulator's FLT_MAX seed is correct.
// ⚠️ The `> FLT_EPSILON` test is on |z|, so a corner BEHIND the camera (negative z) is NOT
//   rejected -- it is projected with a negative divisor and lands mirrored through the
//   origin. Also the console's own behaviour; also reproduced.
// ----------------------------------------------------------------------------
Vector2 GetSizeOnScreen(Matrix44Affine lCameraTransform,
                        VecFloat lvFOV,
                        VecFloat lvAspect,
                        Matrix44Affine lTargetTransform,
                        AABBox lAABB)
{
    // The frustum half-extents at unit depth.
    const f32 lfTanHalfFOV        = rw::math::fpu::Tan(lvFOV * KF_HALF_DEGS_TO_RADS);
    const f32 lfTanHalfFOVPerAspect = lfTanHalfFOV / lvAspect;

    // The subject's frame, expressed in the camera's. The console open-codes the transpose
    // and the negated-position cascade; that is exactly this composition, and the rw helper
    // documents the same X360 shape.
    const Matrix44Affine lTargetInCameraSpace =
        rw::math::vpu::Mult(lTargetTransform,
                            rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lCameraTransform));

    // The eight corners. The console emits them in the order
    // (max,max,max) (min,max,max) (max,min,max) (min,min,max) (max,max,min) (min,max,min)
    // (max,min,min) (min,min,min); the order cannot matter to a min/max reduction, so it is
    // written here as the readable three-bit enumeration.
    Vector3 laCorners[8];
    for (s32 liCorner = 0; liCorner < 8; ++liCorner)
    {
        laCorners[liCorner].x = ((liCorner & 1) != 0) ? lAABB.mMax.x : lAABB.mMin.x;
        laCorners[liCorner].y = ((liCorner & 2) != 0) ? lAABB.mMax.y : lAABB.mMin.y;
        laCorners[liCorner].z = ((liCorner & 4) != 0) ? lAABB.mMax.z : lAABB.mMin.z;
        laCorners[liCorner].w = 0.0f;
    }

    // The screen-space bounds. See the ⚠️⚠️ banner for the max seed.
    const f32 KF_FLT_MAX     = 3.4028235e+38f;   // flt_8200173C
    const f32 KF_FLT_MIN     = 1.1754944e-38f;   // flt_82001738
    const f32 KF_FLT_EPSILON = 1.1920929e-07f;   // flt_82001770

    f32 lfMinX = KF_FLT_MAX;
    f32 lfMinY = KF_FLT_MAX;
    f32 lfMaxX = KF_FLT_MIN;
    f32 lfMaxY = KF_FLT_MIN;

    for (s32 liCorner = 0; liCorner < 8; ++liCorner)
    {
        const Vector3 lCornerInCameraSpace =
            rw::math::vpu::TransformPoint(lTargetInCameraSpace, laCorners[liCorner]);

        if (!(std::fabs(lCornerInCameraSpace.z) > KF_FLT_EPSILON))
        {
            continue;
        }

        // The perspective divide, then the console's 0.5 scale: the result is the fraction of
        // the screen the corner sits at, so the returned size is directly comparable with the
        // authored mfTargetSubjectXSize/YSize (both are 0..1 screen fractions).
        const f32 lfScreenX =
            (lCornerInCameraSpace.x / (lCornerInCameraSpace.z * lfTanHalfFOV)) * 0.5f;
        const f32 lfScreenY =
            (lCornerInCameraSpace.y / (lCornerInCameraSpace.z * lfTanHalfFOVPerAspect)) * 0.5f;

        if (lfMinX > lfScreenX) { lfMinX = lfScreenX; }
        if (lfMinY > lfScreenY) { lfMinY = lfScreenY; }
        if (lfMaxX < lfScreenX) { lfMaxX = lfScreenX; }
        if (lfMaxY < lfScreenY) { lfMaxY = lfScreenY; }
    }

    Vector2 lSize;
    lSize.x = lfMaxX - lfMinX;
    lSize.y = lfMaxY - lfMinY;
    lSize.z = 0.0f;
    lSize.w = 0.0f;
    return lSize;
}

// ----------------------------------------------------------------------------
// GetFOVDegsToFitObjectToScreenSize @0x8220C258   (80 asm lines)
//
// The FOV that would make an object currently occupying `lSizeOnScreen` occupy
// `lTargetSize` instead: scale the current zoom by whichever axis needs the tighter fit.
//   0x8220C290..0x8220C2D0  vrefp128 + Newton reciprocals of lSizeOnScreen.x / .y
//   0x8220C2D8              GetZoomFromFOVDegs(the first argument)
//   0x8220C2F0..0x8220C31C  a second Newton pass on each reciprocal, then * lTargetSize.{x,y}
//   0x8220C31C              vminfp -- the SMALLER of the two ratios wins (fit, not fill)
//   0x8220C320..0x8220C32C  zoom * that ratio -> GetFOVDegsFromZoom
//   0x8220C348..0x8220C378  assert IsValid(lRequiredFOV)   CameraUtils.cpp:191
//
// ⚠️ THE FIRST PARAMETER IS NAMED `lvDistance` IN THE DWARF AND IS NOT A DISTANCE -- it goes
//   straight into GetZoomFromFOVDegs, so it is an FOV in degrees, and both console callers
//   pass the camera's current FOV. The name is the console's own and is kept.
// ⚠️ NO GUARD ON A ZERO lSizeOnScreen. A subject that projects to nothing gives an infinite
//   ratio, an infinite zoom and an FOV of 0 -- which is exactly what the IsValid assert here
//   is watching for. Callers are expected to have a visible subject.
// ----------------------------------------------------------------------------
VecFloat GetFOVDegsToFitObjectToScreenSize(VecFloat lvDistance,
                                           Vector2 lSizeOnScreen,
                                           Vector2 lTargetSize)
{
    const f32 lfZoom = GetZoomFromFOVDegs(lvDistance);

    const f32 lfRatioX = lTargetSize.x / lSizeOnScreen.x;
    const f32 lfRatioY = lTargetSize.y / lSizeOnScreen.y;
    const f32 lfRatio  = (lfRatioX < lfRatioY) ? lfRatioX : lfRatioY;   // vminfp

    const f32 lfRequiredFOV = GetFOVDegsFromZoom(lfZoom * lfRatio);

    CGS_ASSERT(rw::math::fpu::IsValid(lfRequiredFOV), "IsValid(lRequiredFOV)");   // .cpp:191

    return lfRequiredFOV;
}

// ----------------------------------------------------------------------------
// CreateAdjustedLookAt @0x82221EB8   (106 asm lines)
//
// Re-aim a look-at frame so the subject sits at an authored SCREEN-SPACE offset instead of
// dead centre: build the small rotation that points at that offset on the unit-depth plane,
// and pre-multiply it onto the frame.
//   0x82221F00..0x82221F30  lvFOV * (pi/180) * 0.5 -> XMVectorTan
//   0x82221F44              * 2 -- the FULL width of the unit-depth plane, so a screen offset
//                             expressed as a 0..1 fraction maps onto it directly
//   0x82221F3C..0x82221F80  the aspect reciprocal (vrefp + 2x Newton) scales the Y offset
//   0x82221F84/F88          vperm (mask @0x82CDA350) + vrlimi128 lane 2 == Vector3(x, y, 1)
//   0x82221F8C              CreateLookAt(origin, that) -- the offset aim
//   0x82221F94..0x8222203C  the four-row vmaddfp cascade == Mult(offsetAim, lLookAt)
// ----------------------------------------------------------------------------
Matrix44Affine CreateAdjustedLookAt(Matrix44Affine lLookAt,
                                    VecFloat lvFOV,
                                    VecFloat lvAspect,
                                    Vector2 lScreenOffset)
{
    const f32 lfPlaneWidthAtUnitDepth = 2.0f * rw::math::fpu::Tan(lvFOV * KF_HALF_DEGS_TO_RADS);

    const Vector3 lOffsetTarget = { lScreenOffset.x * lfPlaneWidthAtUnitDepth,
                                    lScreenOffset.y * (lfPlaneWidthAtUnitDepth / lvAspect),
                                    1.0f,
                                    0.0f };

    const Vector3 lOrigin = { 0.0f, 0.0f, 0.0f, 0.0f };

    return rw::math::vpu::Mult(CreateLookAt(lOrigin, lOffsetTarget), lLookAt);
}

}
}
}
