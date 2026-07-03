#include "GameSource/Director/Camera/Utils/CameraUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/fpu/scalar_operation.h"            // Tan / IsZero
#include "rw/math/vpu/vector3_operation.h"           // Subtract / Normalize / IsZero

#include <cmath>   // std::atan / std::acos

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
// DECLARATION-ONLY + FLAGGED (declared in CameraUtils.h, bodies not reconstructed --
// each is an inline VMX minimax / corner lattice / permute over UNATTESTED raw rodata
// coefficient or basis constants; bodying them store-for-store would fabricate those
// tables, so they are left unbodied per the no-fabrication rule):
//   ApplyPitchAboutPointRads          @0x822183E0  (Sin/Cos minimax, rodata 82000BD0..82000C60)
//   CalcNearClipCorners               @0x821F25B8  (XMVectorTan + VMX corner lattice)
//   CreateAdjustedLookAt              @0x82221EB8  (vrefp128 + vperm128 mask unk_82CDA350)
//   FindNonParallelNormalisedVectorTo @0x822171B0  (basis constant unk_82181510 unattested)
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
    const f32 KF_HALF_PI = 1.5707964f;
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

}
}
}
