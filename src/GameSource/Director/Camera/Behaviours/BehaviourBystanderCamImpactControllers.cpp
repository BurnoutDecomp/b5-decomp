// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourBystanderCamImpactControllers.cpp
//
// ⭐ A PARTFILE OF BehaviourBystanderCam.cpp -- see the header's banner for why the split
// exists (two reconstructions of BehaviourBystanderCam.h coexist in this tree and would be a
// C2011 the moment the arbitrator state container reached one of them). Bodies, X360-pinned:
//   BrnDirector::Camera::ImpactSlomoController::Update   @0x82227230
//   BrnDirector::Camera::ImpactShakeController::Update   @0x82243720
//
// ⛔⛔ WHY THIS FILE EXISTS AT ALL, AND WHAT IT CORRECTS.
// Both bodies already existed in BehaviourBystanderCam.cpp -- and a previous wave's handoff
// recorded that "ApplySlomoAndShake is a MOUNT away, not a decompile away". THAT WAS NOT TRUE,
// and it is worth stating plainly because it was the load-bearing claim of the whole plan:
// that TU reaches every foreign object through a `namespace detail` layer of ~30 free-function
// shims (`Camera_SetTimeScale`, `AllVehicleData_GetPlayer`, `PlayerTracker_GetCurrentVelY`,
// `Vehicle_GetSpeed`, ...) that are DECLARED AND NEVER DEFINED, anywhere in the tree. Mounting
// it would have opened ~28 unresolved externals, and -- worse -- the one that carries the whole
// feature, `Camera_SetTimeScale`, is the single observable effect of the slow-motion
// controller. A quiet stub for it would have linked green and produced no slow motion.
// So the two bodies are re-expressed here AGAINST THE REAL TYPES: Camera, AllVehicleData,
// VehicleTracker, VehicleRef, CameraImpactEffect, CameraShake. No shims, no offset casts.
//
// ⭐ AND THE TIME-SCALE SLOT RESOLVES TO A CHANNEL THIS TREE ALREADY PUBLISHES.
// The asm writes `stfs f0, 0x104(camera)`. Camera::mEffects is at camera +0x68 and
// CameraEffects::mfSimTimeScale is at effects +0x9C, so 0x68 + 0x9C == 0x104 -- i.e. the slot
// IS Camera::mEffects.mfSimTimeScale, the exact float MainDirector's slow-motion gate reads
// (BrnMainDirector.cpp: `lCamera.GetEffects().mfSimTimeScale`, cross-checked there as
// stack 0xC0 + 0x104 == 0x1C4). One channel, both ends real.
//
// SOURCE-OF-TRUTH: the ARTIST X360 pseudocode+asm for behaviour and calling convention; the
// DecFIGS DWARF for declaration shape. Both call sites' argument lists were recovered FROM THE
// ASM, because Hex-Rays loses four of six pointer arguments at each of them (the f32 timestep
// takes f1 and leaves the r5 GPR slot unwritten, so the decompiler stops tracking the tail).
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCamImpactControllers.h"

#include "GameSource/Director/Camera/Camera.h"                        // Camera::Camera (mEffects / mTransform / mfFOV)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"             // Utils::GetZoomFromFOVDegs
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"        // Camera::VehicleInfo (the published car record)
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"      // AllVehicleData::GetPlayer
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"      // VehicleTracker (the velocity journal)
#include "GameSource/Director/Utils/BrnVehicleRef.h"                  // VehicleRef::Get
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h" // DebugPrinter (threaded only)

#include <cmath>   // std::fabs (the low-speed dampening in the shake controller)

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // ---- ImpactSlomoController constants, read from the X360 .rdata ----------------------
    const f32 KF_SLOMO_DURATION          = 2.0f;        // flt_82CDAD34 -- burst length, seconds
    const f32 KF_MIN_TIME_BETWEEN_SLOMOS = 2.0f;        // flt_82CDAD30 -- cool-down, seconds
    const f32 KF_SLOMO_TIME_SCALE        = 0.2857143f;  // flt_8200177C -- the in-burst sim-time scale
    const f32 KF_REAL_TIME_SCALE         = 1.0f;        // flt_82001C98 -- normal real time
    const f32 KF_MIN_AIR_HEIGHT_M        = 1.0f;        // flt_82001C98 -- the vertical-distance gate

    // ⭐ THE NAME AND THE NUMBER, both recovered. The vcmpgtfp gate splats lane 0 of the
    // 16-byte literal unk_82FAA730, whose initialiser @0x82C49480 multiplies flt_8200D5F8
    // (30.0) by flt_82F31928 (0.447039992 == MPH -> m/s) and squares it:
    // (30 MPH == 13.4112 m/s)^2 == 179.860275 exactly. So it is a minimum-speed gate of 30 MPH
    // held in squared m/s -- unit and all.
    const f32 KF_MIN_VELOCITY_SQUARED_MPS = 179.860275f;

    // ---- ImpactShakeController constants ------------------------------------------------
    const f32 KF_IMPACT_FORCE_SCALE  = 8.0e-08f;  // flt_82CDAD7C (7.99999995e-08) impact -> strength
    const f32 KF_SPEED_RANGE_MPH     = 30.0f;     // flt_82CDAD78  low-speed dampening range
    const f32 KF_FADE_NEAR_SQ        = 25.0f;     // flt_82CDAD74  distance-fade near radius^2
    const f32 KF_FADE_FAR_SQ         = 6400.0f;   // flt_82CDAD70  distance-fade far radius^2
    const f32 KF_MAX_IMPACT_STRENGTH = 0.5f;      // flt_82001DA0  first clamp's upper bound
    const f32 KF_UNIT                = 1.0f;      // flt_82001C98
    const f32 KF_ZERO                = 0.0f;      // flt_82001CC0
}

// ============================================================================
// BrnDirector::Camera::ImpactSlomoController::Update @0x82227230
//
// A three-armed slow-motion timer, state held in two floats plus the first-frame latch:
//
//   mbFirstFrameOfSlomo = false
//   if (mfTimeInSlomo <= 0)                         // not currently in a burst
//       lbShouldDoSlomo = |linVel.current|^2 > 30MPH^2
//                      && linVel.previous.y > 0 && linVel.current.y < 0   // a vertical flip
//                      && (no ground under the car || more than 1 m above it)
//       if (mfTimeSinceLastSlomo >= 2.0 && lbShouldDoSlomo)
//            enter:   mfTimeInSlomo += dt; simTimeScale = 0.2857143; firstFrame = true
//       else if (!lbDontSetRealTime)
//            simTimeScale = 1.0
//       mfTimeSinceLastSlomo += dt
//   else if (mfTimeInSlomo < 2.0)                   // sustaining
//       mfTimeInSlomo += dt; simTimeScale = 0.2857143
//   else                                            // expired
//       mfTimeInSlomo = 0; mfTimeSinceLastSlomo = 0
//       if (!lbDontSetRealTime) simTimeScale = 1.0
//
// ⚠️ CORRECTED HERE (2026-08-29): the two player-record probes at +0x1E8 and +0x1E0 were
// previously read as "isOnGround" and "airTime". They are neither. RaceCarState::
// mAboveGroundTestResult sits at +0x1C0 (mIntersectionPosition +0x1C0, mIntersectionNormal
// +0x1D0), so +0x1E0 is mfVerticalDistance and +0x1E8 is mbValid -- the car's height above the
// ground and whether the ground test found anything at all. The gate is "airborne", expressed
// as "no ground beneath, or more than a metre above it", which is what a slow-motion trigger on
// a downward flip should read. The old names would have sent the next reader looking for an
// air-time field on the published record; there isn't one at that offset.
// ============================================================================
void ImpactSlomoController::Update(Camera& lrCamera, f32 lfTimestep,
                                   const AllVehicleData& lrVehicles,
                                   const VehicleTracker& lrPlayerTracker,
                                   DebugPrinter& /*lrDebugPrinter*/,
                                   bool lbDontSetRealTime)
{
    mbFirstFrameOfSlomo = false;                       // stb 0, 8(this)

    if (mfTimeInSlomo <= 0.0f)                         // not currently in a burst
    {
        bool lbShouldDoSlomo = false;

        // The velocity journal of the tracked car (asm: the callee is handed tracker + 0x90,
        // which is VehicleTracker::mLinearVelocityJournal).
        const VehicleTracker::Vector3Journal& lrLinearVelocity =
            lrPlayerTracker.GetLinearVelocityJournal();

        const rw::math::vpu::Vector3 lCurrentVelocity = lrLinearVelocity.GetCurrent();

        // vmsum3fp128 v0,v0,v0 then vcmpgtfp against the splatted 30 MPH^2 literal.
        if (rw::math::vpu::MagnitudeSquared(lCurrentVelocity) > KF_MIN_VELOCITY_SQUARED_MPS)
        {
            // vspltw lane 1 (.y) of each sample: rising last frame, falling this frame.
            if (lrLinearVelocity.GetPrevious(0).y > 0.0f && lCurrentVelocity.y < 0.0f)
            {
                const VehicleInfo& lrPlayer = lrVehicles.GetPlayer();
                const BrnPhysics::Vehicle::AboveGroundTestResult& lrGround =
                    lrPlayer.mRaceCarState.mAboveGroundTestResult;

                // lbz 0x1E8(player) -- no valid ground result at all => airborne.
                if (!lrGround.mbValid)
                {
                    lbShouldDoSlomo = true;
                }
                // lfs 0x1E0(player) -- otherwise, more than a metre clear of the ground.
                // (The console re-issues GetPlayer() for this second probe; one read here.)
                else if (lrVehicles.GetPlayer().mRaceCarState.mAboveGroundTestResult
                             .mfVerticalDistance > KF_MIN_AIR_HEIGHT_M)
                {
                    lbShouldDoSlomo = true;
                }
            }
        }

        if (mfTimeSinceLastSlomo >= KF_MIN_TIME_BETWEEN_SLOMOS && lbShouldDoSlomo)
        {
            mfTimeInSlomo += lfTimestep;                                  // stfs ..., 4(this)
            lrCamera.GetEffects().mfSimTimeScale = KF_SLOMO_TIME_SCALE;   // stfs, 0x104(camera)
            mbFirstFrameOfSlomo = true;                                   // stb 1, 8(this)
        }
        else if (!lbDontSetRealTime)
        {
            lrCamera.GetEffects().mfSimTimeScale = KF_REAL_TIME_SCALE;    // stfs 1.0, 0x104(camera)
        }

        mfTimeSinceLastSlomo += lfTimestep;                               // stfs ..., 0(this)
    }
    else if (mfTimeInSlomo < KF_SLOMO_DURATION)        // sustaining the burst
    {
        mfTimeInSlomo += lfTimestep;
        lrCamera.GetEffects().mfSimTimeScale = KF_SLOMO_TIME_SCALE;
    }
    else                                               // the burst has expired
    {
        mfTimeInSlomo        = 0.0f;                   // stfs 0.0, 4(this)
        mfTimeSinceLastSlomo = 0.0f;                   // stfs 0.0, 0(this)
        if (!lbDontSetRealTime)
        {
            lrCamera.GetEffects().mfSimTimeScale = KF_REAL_TIME_SCALE;
        }
    }
}

// ============================================================================
// BrnDirector::Camera::ImpactShakeController::Update @0x82243720
//
//   strength = 8e-08 * vehicle.mfHardestImpact                    // *(veh+0x4E0)
//   absSpeed = |vehicle.mRaceCarState.mfSpeedMPH|                 // |*(veh+0x3CC)|
//   if (absSpeed < 30) strength *= absSpeed / 30                  // low-speed dampening
//   strength = clamp(strength, 0, 0.5)                            // the fneg/fsel pair
//   distSq   = |camera.pos - vehicle.pos|^2                       // vsubfp + vmsum3fp128
//   if (distSq > 25) strength *= 1 - (clamp(distSq,25,6400) - 25) / (6400 - 25)
//   strength = clamp(strength, 0, 1)
//   mImpactEffect.RegisterImpact(strength / GetZoomFromFOVDegs(camera.mfFOV))
//   <build a default CameraImpactEffect::Parameters on the stack>
//   CameraShake::Update(mImpactEffect.mCameraShake, camera.mTransform, params.mShakeParams,
//                       random, dt * params.mfShakeFrequencyScale,
//                       impactFactor * params.mfShakeMagnitude)
//   impactFactor += -impactFactor * params.mfShakeDecayFactor
//
// ⭐ THE SEVEN-FLOAT STACK BLOCK IS A `CameraImpactEffect::Parameters`, NOT A LOOSE ARRAY.
// The previous reconstruction carried it as `float laShakeParams[7]` with 0.05 / 15.0 / 5.0
// spelled as bare literals at the two use sites. Read against the type, the block is exactly
// {CameraShake::Parameters mShakeParams (0.06, 0.0, 1.15, 0.11); mfShakeDecayFactor 0.05;
//  mfShakeMagnitude 15.0; mfShakeFrequencyScale 5.0} -- and each of the three trailing tunables
// is then used for precisely the thing its DWARF name says: the decay eases the factor toward
// zero, the magnitude scales the shake, the frequency scale multiplies the timestep. The stack
// store offsets (var_70 .. var_58, four bytes apart) pin each lane's source literal.
// ============================================================================
void ImpactShakeController::Update(Camera& lrCamera, f32 lfTimestep,
                                   const AllVehicleData& lrVehicles,
                                   const VehicleTracker& /*lrPlayerTracker*/,
                                   Utils::Random& lrRandom,
                                   DebugPrinter& /*lrDebugPrinter*/,
                                   const VehicleRef& lrVehicleRef)
{
    // The console re-resolves the reference for each probe (three VehicleRef::Get calls).
    f32 lfStrength =
        KF_IMPACT_FORCE_SCALE * lrVehicleRef.Get(&lrVehicles)->mfHardestImpact;   // veh +0x4E0

    const f32 lfAbsSpeed =
        std::fabs(lrVehicleRef.Get(&lrVehicles)->mRaceCarState.mfSpeedMPH);       // veh +0x3CC
    if (lfAbsSpeed < KF_SPEED_RANGE_MPH)
    {
        lfStrength = (lfAbsSpeed / KF_SPEED_RANGE_MPH) * lfStrength;
    }

    // clamp(strength, 0.0, 0.5) -- the console's fneg/fsel lower bound then fsub/fsel upper.
    if (lfStrength < KF_ZERO)                { lfStrength = KF_ZERO; }
    if (lfStrength > KF_MAX_IMPACT_STRENGTH) { lfStrength = KF_MAX_IMPACT_STRENGTH; }

    // Distance fade. asm: lvx camera+0x30 and (vehicle + 0x1F0) + 0x30 -- the position row of
    // the camera's transform and of the car's published RaceCarState::mTransform.
    const rw::math::vpu::Vector3 lCameraToCar =
        lrCamera.mTransform.Pos() - lrVehicleRef.Get(&lrVehicles)->mRaceCarState.mTransform.Pos();
    const f32 lfDistSq = rw::math::vpu::MagnitudeSquared(lCameraToCar);
    if (lfDistSq > KF_FADE_NEAR_SQ)
    {
        const f32 lfClamped = (lfDistSq < KF_FADE_FAR_SQ) ? lfDistSq : KF_FADE_FAR_SQ;
        const f32 lfFade    = KF_UNIT -
                              ((lfClamped - KF_FADE_NEAR_SQ) / (KF_FADE_FAR_SQ - KF_FADE_NEAR_SQ));
        lfStrength = lfFade * lfStrength;
    }

    // clamp(strength, 0.0, 1.0).
    if (lfStrength < KF_ZERO) { lfStrength = KF_ZERO; }
    if (lfStrength > KF_UNIT) { lfStrength = KF_UNIT; }

    mImpactEffect.RegisterImpact(lfStrength / Utils::GetZoomFromFOVDegs(lrCamera.mfFOV));

    // The impact-effect tunings the console builds on the stack for this one call.
    Utils::CameraImpactEffect::Parameters lParameters;
    lParameters.mShakeParams.Construct();          // 0.06 / 0.0 / 1.15 / 0.11 (the seed defaults)
    lParameters.mfShakeDecayFactor    = 0.05f;     // flt_820047C8
    lParameters.mfShakeMagnitude      = 15.0f;     // flt_820047C4
    lParameters.mfShakeFrequencyScale = 5.0f;      // flt_8200426C

    const f32 lfImpactFactor = mImpactEffect.GetImpactFactor();   // lfs 0(this), read BEFORE the shake

    mImpactEffect.GetCameraShake().Update(lrCamera.mTransform,
                                          lParameters.mShakeParams,
                                          lrRandom,
                                          lfTimestep * lParameters.mfShakeFrequencyScale,
                                          lfImpactFactor * lParameters.mfShakeMagnitude);

    // Ease the registered factor back toward zero (fmadds: f = -f * decay + f).
    mImpactEffect.SetImpactFactor((-mImpactEffect.GetImpactFactor() *
                                   lParameters.mfShakeDecayFactor) +
                                  mImpactEffect.GetImpactFactor());
}

} // namespace Camera
} // namespace BrnDirector
