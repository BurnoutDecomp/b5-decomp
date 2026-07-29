#include "GameSource/Director/Camera/Utils/CameraUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/fpu/scalar_operation.h"            // Tan / IsZero
#include "rw/math/vpu/vector3_operation.h"           // Subtract / Normalize / IsZero

#include <cmath>   // std::atan / std::acos / std::asin / std::fabs / std::copysign

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
// Bodied here (1 ledger function, class:BrnDirector::Camera::Utils -- the Euler decomposition
// that feeds the fly-by camera's BANK; see the block comment at its definition for the
// branch-by-branch attestation):
//   EulerAnglesZXYFromMatrix44Affine        @0x82222180   (CameraUtils.cpp:453)
//
// DECLARATION-ONLY + FLAGGED (declared in CameraUtils.h, bodies not reconstructed --
// each is an inline VMX minimax / corner lattice / permute over UNATTESTED raw rodata
// coefficient constants; bodying them store-for-store would fabricate those
// tables, so they are left unbodied per the no-fabrication rule):
//   ApplyPitchAboutPointRads          @0x822183E0  (Sin/Cos minimax, rodata 82000BD0..82000C60)
//   CalcNearClipCorners               @0x821F25B8  (XMVectorTan + VMX corner lattice)
//   CreateAdjustedLookAt              @0x82221EB8  (vrefp128 + vperm128 mask unk_82CDA350)
//   FindNonParallelNormalisedVectorTo @0x822171B0  (the SECOND of its two returned constants
//                                                   is still unattested; unk_82181510 is now
//                                                   pinned -- see KV_AXIS_Y below)
//   GetFOVDegsToFitObjectToScreenArea @0x8220C398  (VMX rsqrt/reciprocal Newton-refine + vsel)
//   GetFOVDegsToFitObjectToScreenSize @0x8220C258  (two vrefp128 Newton-refine reciprocals)

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

}
}
}
