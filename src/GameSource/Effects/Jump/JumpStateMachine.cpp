// =============================================================================
// GameSource/Effects/Jump/JumpStateMachine.cpp  (X360 ARTIST)
//
// BrnEffects::JumpStateMachine -- the per-race-car effects state machine that drives
// the jump vapour trail and the landing dust / spark / debris effects. Reconstructed
// for SEMANTIC PARITY from the ARTIST X360 pseudocode + asm:
//
//   OnDetermineNextState @ 0x8229B9F8   OnChangeState  @ 0x82299510
//   SetVapourBlend       @ 0x82288A58   OnTick         -- no export (empty base slot)
//   FireWheelSparks      @ 0x82299670   FireWheelDebris
//
// The machine is ticked once per active car per frame: EffectsModule.cpp:1835 ->
// ActiveRaceCarData::Tick -> EffectsStateMachine::Tick (EffectsStateMachine.cpp:49),
// which calls OnDetermineNextState, then OnChangeState on any transition.
//
// The states this machine uses (EffectsStateMachine.h):
//   AllOff(0) -> Jumping(7) -> JumpingMovingDown(9) -> Landed(10) ->
//   FiringSparks(11) -> LandedWaiting(12) -> JumpingFinishing(13) -> AllOff(0)
// (LandedWaiting re-arms straight back to Jumping while CarState::mbJumping is set.)
//
// SetVapourBlend @ 0x82288A58
//   Compute and apply the jump-vapour LION effect's state blend. The car's linear
//   velocity direction and world Z axis, its upward speed and the debug "Jumping"
//   menu's vapour delay/ramp times drive two smoothstep ramps that are multiplied
//   together; the product is written to the vapour effect handle-run held in the
//   ActiveRaceCarData (mJumpEffectHandle). Reconstructed store-for-store from the
//   ARTIST X360 asm; SmoothStep call convention + Vector3/Vector2 brace-init mirror
//   the committed BoostStateMachine sibling.
//
//   Gates (all must hold or the blend stays 0):
//     speed = |mLinearVelocity| > 20.0                       (fast enough)
//     mLinearVelocity.y >= 0.0                           (horizontal or rising)
//     dot(normalize(mLinearVelocity), mTransform.zAxis) > 0  (facing into the jump)
//   Then blend = SmoothStep(dir) * SmoothStep(time); the debug "Force State Blend"
//   override replaces the computed blend when enabled.
// =============================================================================

#include "GameSource/Effects/Jump/JumpStateMachine.h"
#include "GameSource/Effects/EffectsModule.h"                 // BrnEffects::CarState, EffectsModule::ParticleModule()
#include "GameSource/Effects/ActiveRaceCarData.h"             // mJumpEffectHandle / maJumpLandingWheelEffectHandles
#include "GameSource/Effects/ParticleEffectHelper.h"          // RaceCarParticleEffectHelper
#include "GameSource/Effects/Particles/ParticleModule.h"      // BrnParticle::ParticleModule, LionEffect
#include "GameSource/Effects/Particles/BrnParticleDescription.h"  // ParticleDescription::HashString
#include "GameSource/Effects/BrnEffectsDebugComponent.h"      // EffectsDebugComponent / EffectsDebugJumping
#include "GameSource/Effects/Curves.h"                        // BrnEffects::Curves::SmoothStep
#include "GameSource/Effects/BrnEffectsUtils.h"               // Utils::Vector3Randomiser (FireWheelDebris)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // BrnPhysics::Vehicle::RaceCarState / WheelLite
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"    // rw::math::vpu::VecFloat (RandomVecFloat's splat)
#include <cmath>

namespace BrnEffects
{

// =============================================================================
// File-scope rodata recovered from the X360 build.
// =============================================================================

namespace
{
    // ---- SetVapourBlend literals (inlined at the call site; no named globals) ----
    const f32 KF_VAPOUR_MIN_SPEED = 20.0f;          // flt_820054CC (speed gate)
    const f32 KF_VAPOUR_DIR_LOW   = 0.70700002f;    // flt_82011C14 (dir smoothstep lower threshold)
    const f32 KF_VAPOUR_DIR_MID   = 0.85350001f;    // flt_82011C10 (dir smoothstep mid threshold)

    // ---- OnDetermineNextState / OnChangeState literals ----
    // The squared landing speed above which the per-wheel dust effects are started
    // (flt_82013AF8 == 196.0, i.e. |mLinearVelocity| > 14 m/s). The X360 compares the
    // vmsum3fp128 self-dot against it directly, so the compare stays in the squared domain.
    const f32 KF_LANDING_MIN_SPEED_SQ = 196.0f;

    // The accumulated fade time (seconds spent falling) above which the landing debris
    // burst fires (flt_82001C98 == 1.0).
    const f32 KF_DEBRIS_FADE_TIME = 1.0f;

    // The LandedWaiting dwell before the machine finishes the jump (flt_8200DD24 == 3.0).
    const f32 KF_LANDED_WAITING_TIME = 3.0f;

    // ---- FireWheelDebris literals ------------------------------------------------
    // The speed ramp the burst count rides, and the count it maps onto. Below the low
    // speed the ramp is negative and the whole burst is skipped.
    const f32 KF_DEBRIS_SPEED_LOW  = 17.877777099609375f;
    const f32 KF_DEBRIS_SPEED_HIGH = 53.63333511352539f;
    const f32 KF_DEBRIS_MIN_BURST  = 20.0f;
    const f32 KF_DEBRIS_MAX_BURST  = 40.0f;

    // The debris size: draw^2 scaled onto [0.04, 0.10].
    const f32 KF_DEBRIS_SIZE_MIN   = 0.03999999910593033f;
    const f32 KF_DEBRIS_SIZE_RANGE = 0.06000000238418579f;

    // How much of the car's surface-tangential velocity a piece inherits: a draw in
    // [0.8, 1.1] (both dynamically-initialised splats).
    const f32 KF_DEBRIS_INHERIT_MIN = 0.800000011920929f;
    const f32 KF_DEBRIS_INHERIT_MAX = 1.100000023841858f;

    // The two ejection-cone bound PAIRS, lerped between by one shared draw: NARROW is the
    // pair used at interpolant 0, WIDE at 1. Lateral spread widens from +-3 to +-7 and the
    // upward component from [1, 4] to [1, 6].
    const Vector3 KV_DEBRIS_CONE_MIN_NARROW(-3.0f, 1.0f, -3.0f);
    const Vector3 KV_DEBRIS_CONE_MAX_NARROW(3.0f, 4.0f, 3.0f);
    const Vector3 KV_DEBRIS_CONE_MIN_WIDE(-7.0f, 1.0f, -7.0f);
    const Vector3 KV_DEBRIS_CONE_MAX_WIDE(7.0f, 6.0f, 7.0f);

    // The colour argument (a 1.0 splat). Ignored by SpawnDebris for every type but
    // eDebrisArray_Coloured, and this burst is Dark.
    const Vector4 KV_DEBRIS_WHITE(1.0f, 1.0f, 1.0f, 1.0f);

    // The three LION effect paths, read out of the ARTIST image at the pointer table
    // off_82CDAE58 / off_82CDAE5C / off_82CDAE60.
    //
    //   VAPOUR         -- off_82CDAE58, started on entry to Jumping.
    //   LANDING_DUST   -- off_82CDAE5C, the junkyard-VFX landing burst.
    //   LANDING_DUST_L -- off_82CDAE60, the ordinary in-world landing burst.
    // The selector between the last two is ParticleModule::mbIsInJunkyard.
    const char* const KPC_JUMP_VAPOUR_EFFECT =
        "gamedb://burnout5/Burnout/Effects/Cam_VaportrailF.lef.BurnoutFXLionEffectFile?ID=376725";
    const char* const KPC_LANDING_DUST_EFFECT =
        "gamedb://burnout5/Burnout/Effects/Cam_DustImpact.lef.BurnoutFXLionEffectFile?ID=382185";
    const char* const KPC_LANDING_DUST_LITE_EFFECT =
        "gamedb://burnout5/Burnout/Effects/Cam_DustImpactL.lef.BurnoutFXLionEffectFile?ID=614063";

    __vector4 ReciprocalSqrt2(__vector4 squared)
    {
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        __vector4 estimate = __vrsqrtefp(squared);
        __vector4 error = __vnmsubfp(squared, __vmulfp(estimate, estimate), one);
        estimate = __vmaddfp(__vmulfp(estimate, half), error, estimate);
        error = __vnmsubfp(squared, __vmulfp(estimate, estimate), one);
        return __vmaddfp(__vmulfp(estimate, half), error, estimate);
    }

    __vector4 Splat(f32 value)
    {
        return __vspltw(__lvlx(&value, 0), 0);
    }

    Vector3 CrossNative(const Vector3& a, const Vector3& b)
    {
        const __vector4 ayzx = __vpermwi(a.mV, 0x63);
        const __vector4 byzx = __vpermwi(b.mV, 0x63);
        return Vector3(__vpermwi(__vnmsubfp(ayzx, b.mV, __vmulfp(a.mV, byzx)), 0x63));
    }

}

// @ 0x82288A58
void JumpStateMachine::SetVapourBlend(f32 lfFadeTime,
                                      RaceCarParticleEffectHelper& lHelper) const
{
    const BrnPhysics::Vehicle::RaceCarState* lpCar   = lHelper.RaceCarState();
    const EffectsDebugComponent*             lpDebug = lHelper.DebugComponent();

    f32 lfBlend = 0.0f;

    // speed = |mLinearVelocity| (X360: vmsum3fp128 + rsqrt Newton refine -> magnitude,
    // vsel-guarded so |v|^2 == 0 yields 0).
    const Vector3& lvVel = lpCar->mLinearVelocity;
    const __vector4 squared = __vmsum3fp(lvVel.mV, lvVel.mV);
    const __vector4 inverseSpeed = ReciprocalSqrt2(squared);
    const __vector4 zero = __vspltisw(0);
    const __vector4 speed = __vsel(__vmulfp(squared, inverseSpeed), zero, __vcmpeqfp(squared, zero));
    const f32 lfSpeed = speed.x;

    if (!(lfSpeed <= KF_VAPOUR_MIN_SPEED))
    {
        unsigned int risingCR;
        __vcmpgefpR(__vspltw(lvVel.mV, 1), zero, &risingCR);
        if ((risingCR & 0x80u) != 0)
        {
            const Vector3& lvForward = lpCar->mTransform.zAxis;
            const f32 lfDot = __vmsum3fp(__vmulfp(lvVel.mV, inverseSpeed), lvForward.mV).x;

            if (!(lfDot <= 0.0f))
            {
                const f32 lfVapourDelay = lpDebug->JumpParams().VapourStartDelay();   // +0x70
                const f32 lfVapourEnd   = lpDebug->JumpParams().VapourRampEndTime()   // +0x74
                                        + lfVapourDelay;

                // Ramp 1: alignment dot -> [0,1] over [0.707, 1.0] (mid 0.8535).
                const rw::math::fpu::Vector3Template<f32> lvDirParams(KF_VAPOUR_DIR_LOW, KF_VAPOUR_DIR_MID, 1.0f);
                const rw::math::fpu::Vector2Template<f32> lvDirScale(0.0f, 1.0f);

                // Ramp 2: fade time -> [0,0.5] over [delay, delay+ramp] (mid midpoint).
                const rw::math::fpu::Vector3Template<f32> lvTimeParams(lfVapourDelay,
                    (lfVapourEnd - lfVapourDelay) * 0.5f + lfVapourDelay, lfVapourEnd);
                const rw::math::fpu::Vector2Template<f32> lvTimeScale(0.0f, 0.5f);

                const f32 lfDirBlend  = BrnEffects::Curves::SmoothStep::Evaluate(lvDirParams, lvDirScale, lfDot);
                const f32 lfTimeBlend = BrnEffects::Curves::SmoothStep::Evaluate(lvTimeParams, lvTimeScale, lfFadeTime);
                lfBlend = lfDirBlend * lfTimeBlend;
            }
        }
    }

    // Debug "Force State Blend" override.
    if (lpDebug->IsForceStateBlend())
    {
        lfBlend = lpDebug->ForceStateBlendValue();
    }

    // Apply to the single jump-vapour effect handle-run in the active-race-car data
    // (the console reaches it as mpActiveRaceCar + 0x114). One u32 handle, count == 1.
    const u32* lpuVapourHandle = &lHelper.ActiveRaceCar()->mJumpEffectHandle;
    lHelper.SetEffectStateBlend(lpuVapourHandle, 1, lfBlend);
}

// =============================================================================
// FireWheelSparks @ 0x82299670 (DWARF JumpStateMachine.cpp:308) -- THE LANDING SPARKS.
// FX-CRASHVFX 2026-09-25 (C2): parked until now on EffectsModule::FireJumpSparks, which had
// no body; both are bodied. In the asm's order:
//   gate     both REAR wheels on the ground -- CarState +0x40 -> maWheels[2] (+0xE0) and
//            maWheels[3] (+0x150), mRoadContact.mbIsOnGround (+0x28), `beq` out on either
//            (0x82299690..0x822996B0)
//   ground   the ActiveRaceCarData's ground height (helper +4, `lfs f31, 0x138(r10)`)
//   lerps    TWO RandomVecFloat() draws on the effects ring (helper +8 -> +0x2C3C0), each the
//            vector slot ((cursor + 3) & 4) and one LCG step: A = t1 * 0.5 (`vmulfp128`),
//            B = 0.5 * t2 + 0.5 (`vmaddcfp128 v125 = v0 * v125 + v0`) -- one point on each
//            half of the rear axle
//   points   posA = A * (p3 - p2) + p2 and normalA likewise (`vsubfp`, then ONE `vmaddcfp128`
//            per lane, 0x822997F4 / 0x822997F8); posB = (p3 - p2) * B + p2 and normalB
//            likewise (0x82299820 / 0x82299824) -- all four lanes
//   calls    EffectsModule::FireJumpSparks(dt, time, posA, normalA, the helper's race-car
//            state, ground, wheel 2's tag), then (..., posB, normalB, ..., wheel 3's tag)
// =============================================================================
void JumpStateMachine::FireWheelSparks(CarState& lCarState,
                                       RaceCarParticleEffectHelper& lHelper) const
{
    const BrnPhysics::Vehicle::RaceCarState* const lpCarState = lCarState.mpCarState;
    const BrnPhysics::Vehicle::WheelLite& lRearLeftWheel  = lpCarState->maWheels[2];
    const BrnPhysics::Vehicle::WheelLite& lRearRightWheel = lpCarState->maWheels[3];
    if (!lRearLeftWheel.mRoadContact.mbIsOnGround || !lRearRightWheel.mRoadContact.mbIsOnGround)
    {
        return;
    }

    const ActiveRaceCarData& lRaceCarData = *lHelper.ActiveRaceCar();
    const f32 lfGroundHeight = lRaceCarData.GetGroundPositionY();
    CgsNumeric::Random& lRandom = lHelper.GetEffectsModule()->RandomNumberGenerator();

    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    const __vector4 lerpA = __vmulfp(lRandom.RandomVecFloat().mV, half);
    const __vector4 lerpB = __vmaddfp(lRandom.RandomVecFloat().mV, half, half);
    const __vector4 posSpan = __vsubfp(lRearRightWheel.mRoadContact.mPosition.mV, lRearLeftWheel.mRoadContact.mPosition.mV);
    const __vector4 normalSpan = __vsubfp(lRearRightWheel.mRoadContact.mNormal.mV, lRearLeftWheel.mRoadContact.mNormal.mV);
    const Vector3 lSparksSpawnPosA(__vmaddfp(lerpA, posSpan, lRearLeftWheel.mRoadContact.mPosition.mV));
    const Vector3 lSparksSpawnPosB(__vmaddfp(posSpan, lerpB, lRearLeftWheel.mRoadContact.mPosition.mV));
    const Vector3 lSparksSpawnNormalA(__vmaddfp(lerpA, normalSpan, lRearLeftWheel.mRoadContact.mNormal.mV));
    const Vector3 lSparksSpawnNormalB(__vmaddfp(normalSpan, lerpB, lRearLeftWheel.mRoadContact.mNormal.mV));

    lHelper.GetEffectsModule()->FireJumpSparks(lCarState.GetDt(), lCarState.GetTime(), lSparksSpawnPosA,
                                               lSparksSpawnNormalA, lHelper.RaceCarState(), lfGroundHeight,
                                               lRearLeftWheel.mRoadContact.mCollisionTag);
    lHelper.GetEffectsModule()->FireJumpSparks(lCarState.GetDt(), lCarState.GetTime(), lSparksSpawnPosB,
                                               lSparksSpawnNormalB, lHelper.RaceCarState(), lfGroundHeight,
                                               lRearRightWheel.mRoadContact.mCollisionTag);
}

// =============================================================================
// FireWheelDebris
//
// The landing debris burst: a spray of dark debris thrown off the line between the two
// REAR wheel contact patches, sized and counted by how fast the car is travelling.
//
// SPEED -> BURST COUNT. |mLinearVelocity| is mapped onto [0, 1] across
// KF_DEBRIS_SPEED_LOW .. KF_DEBRIS_SPEED_HIGH; below the low end the ramp goes negative
// and the whole function returns. One scalar ring draw scales that ramp, the product is
// clamped at 1, and the count is the truncated KF_DEBRIS_MIN_BURST + t * (MAX - MIN).
//
// THE SPAWN SEGMENT is the two rear wheels' road-contact points, each lifted along the
// car's world up axis (mTransform.yAxis) by that wheel's radius; each burst picks a
// point on it with one ring draw. The mean of the two contact NORMALS is the surface the
// inherited velocity is flattened against: the car's velocity minus its component along
// that mean, scaled by a draw in [0.8, 1.1].
//
// THE EJECTION VELOCITY is Vector3Randomiser::RandomiseXYZ over a cone whose two bounds
// are themselves lerped (by ONE shared draw, hence DrawInterpolant) between a narrow and
// a wide pair -- so a single burst is either tight or broad, never mixed.
//
// THE ROTATION AXIS is a uniform point on the unit sphere by Marsaglia rejection: draw
// u, v in [-1, 1) until u*u + v*v < 1, then (2*u*s, 2*v*s, 2*(u*u+v*v) - 1) with
// s = sqrt(1 - u*u - v*v).
//
// The six burst-shape vectors are dynamically-initialised .bss splats (a literal read of
// their addresses returns zero by definition); each was recovered through its CRT init
// startup initialiser, which was located and disassembled to read the values off.
// =============================================================================
void JumpStateMachine::FireWheelDebris(CarState& lCarState,
                                       RaceCarParticleEffectHelper& lHelper) const
{
    const BrnPhysics::Vehicle::RaceCarState* lpCar = lHelper.RaceCarState();
    CgsNumeric::Random& lrRandom = lHelper.GetEffectsModule()->RandomNumberGenerator();

    // speed = |mLinearVelocity| (the original is a reciprocal-square-root approximation with
    // two Newton refinements, guarded so |v|^2 == 0 yields 0).
    const Vector3& lvVelocity = lpCar->mLinearVelocity;
    const __vector4 speedSq = __vmsum3fp(lvVelocity.mV, lvVelocity.mV);
    const __vector4 zero = __vspltisw(0);
    const f32 lfSpeed = __vsel(__vmulfp(speedSq, ReciprocalSqrt2(speedSq)), zero, __vcmpeqfp(speedSq, zero)).x;

    const f32 lfSpeedRamp = (1.0f / (KF_DEBRIS_SPEED_HIGH - KF_DEBRIS_SPEED_LOW))
                          * (lfSpeed - KF_DEBRIS_SPEED_LOW);
    if (lfSpeedRamp < 0.0f)
    {
        return;
    }

    f32 lfBurstRamp = lrRandom.RandomFloat() * lfSpeedRamp;
    if ((lfBurstRamp - 1.0f) >= 0.0f)
    {
        lfBurstRamp = 1.0f;
    }
    const s32 liBurstCount = static_cast<s32>(
        lfBurstRamp * (KF_DEBRIS_MAX_BURST - KF_DEBRIS_MIN_BURST) + KF_DEBRIS_MIN_BURST);

    // ---- the spawn segment and the surface the inherited velocity flattens against ----
    const BrnPhysics::Vehicle::WheelLite& lrRearLeft  = lpCar->maWheels[2];
    const BrnPhysics::Vehicle::WheelLite& lrRearRight = lpCar->maWheels[3];
    const Vector3& lvUp = lpCar->mTransform.yAxis;

    const Vector3 lvSegmentStart(__vmaddfp(lvUp.mV, Splat(lrRearLeft.mfRadius), lrRearLeft.mRoadContact.mPosition.mV));
    const Vector3 lvSegmentEnd(__vmaddfp(lvUp.mV, Splat(lrRearRight.mfRadius), lrRearRight.mRoadContact.mPosition.mV));
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    const Vector3 lvMeanNormal(__vmulfp(__vaddfp(lrRearLeft.mRoadContact.mNormal.mV, lrRearRight.mRoadContact.mNormal.mV), half));

    const f32 lfConeWidth = Utils::Vector3Randomiser::DrawInterpolant(lrRandom);
    const __vector4 coneWidth = Splat(lfConeWidth);
    const Vector3 lvConeMin(__vmaddfp(__vsubfp(KV_DEBRIS_CONE_MIN_WIDE.mV, KV_DEBRIS_CONE_MIN_NARROW.mV), coneWidth, KV_DEBRIS_CONE_MIN_NARROW.mV));
    const Vector3 lvConeMax(__vmaddfp(__vsubfp(KV_DEBRIS_CONE_MAX_WIDE.mV, KV_DEBRIS_CONE_MAX_NARROW.mV), coneWidth, KV_DEBRIS_CONE_MAX_NARROW.mV));
    const __vector4 alongNormal = __vmsum3fp(lvVelocity.mV, lvMeanNormal.mV);
    const Vector3 lvTangential(__vsubfp(lvVelocity.mV, __vmulfp(lvMeanNormal.mV, alongNormal)));

    Utils::Vector3Randomiser lPositionRandomiser;
    lPositionRandomiser.Prepare(lvSegmentStart, lvSegmentEnd);

    Utils::Vector3Randomiser lEjectionRandomiser;
    lEjectionRandomiser.Prepare(lvConeMin, lvConeMax);

    Utils::Vector3Randomiser lInheritRandomiser;
    lInheritRandomiser.Prepare(Vector3(__vmulfp(lvTangential.mV, Splat(KF_DEBRIS_INHERIT_MIN))),
                               Vector3(__vmulfp(lvTangential.mV, Splat(KF_DEBRIS_INHERIT_MAX))));

    for (s32 liBurst = 0; liBurst < liBurstCount; ++liBurst)
    {
        const Vector3 lvPosition  = lPositionRandomiser.RandomInterpolate(lrRandom);
        const Vector3 lvEjection  = lEjectionRandomiser.RandomiseXYZ(lrRandom);
        const Vector3 lvInherited = lInheritRandomiser.RandomInterpolate(lrRandom);
        const Vector3 lvDebrisVelocity(__vaddfp(lvEjection.mV, lvInherited.mV));

        // Marsaglia rejection: a uniform point on the unit sphere.
        f32 lfU;
        f32 lfV;
        f32 lfRadiusSq;
        do
        {
            lfU = lrRandom.RandomFloat(-1.0f, 1.0f);
            lfV = lrRandom.RandomFloat(-1.0f, 1.0f);
            lfRadiusSq = lfV * lfV + lfU * lfU;
        }
        while (lfRadiusSq >= 1.0f);

        const f32 lfSphereScale = sqrtf(1.0f - lfRadiusSq) * 2.0f;
        const Vector3 lvRotationAxis(lfSphereScale * lfU, lfSphereScale * lfV, lfRadiusSq * 2.0f - 1.0f);

        const f32 lfSizeDraw = lrRandom.RandomFloat();
        const f32 lfSize = (lfSizeDraw * lfSizeDraw) * KF_DEBRIS_SIZE_RANGE + KF_DEBRIS_SIZE_MIN;

        lHelper.ParticleModule().SpawnDebris(BrnParticle::Native::eDebrisArray_Dark,
                                             lvPosition,
                                             lvDebrisVelocity,
                                             lvRotationAxis,
                                             KV_DEBRIS_WHITE,
                                             lfSize,
                                             lCarState.GetTime());
    }
}

// =============================================================================
// OnDetermineNextState @ 0x8229B9F8
//   The jump-effects transition table. Returns the next EffectsState; returning the
//   current state is the console's own "no transition" value (the base Tick's
//   `leNextState != mState` test then does nothing).
// =============================================================================
EffectsState JumpStateMachine::OnDetermineNextState(CarState& lCarState,
                                                    bool lbStateTimerExpired,
                                                    EffectsState leCurrentState,
                                                    RaceCarParticleEffectHelper& lHelper)
{
    // 0x8229BA18 / 0x8229BA24 -- the two unconditional kills, before the switch.
    // A crashing car (CarState +0x4D) or a hidden car (RaceCarState +0x452) drops the
    // whole machine straight to AllOff, whose entry action stops every jump effect.
    if (lCarState.mbCrashing || lHelper.RaceCarState()->mbIsHidden)
    {
        return EffectsStateAllOff;
    }

    EffectsState leNextState = leCurrentState;

    switch (leCurrentState)
    {
        case EffectsStateAllOff:                 // 0
            // Arm on the physics jump flag; clear the per-wheel landing latches first.
            if (lCarState.mbJumping)             // CarState +0x4E
            {
                for (u32 luWheel = 0; luWheel < ActiveRaceCarData::KU_NUM_WHEELS; ++luWheel)
                {
                    mbWheelsOnGround[luWheel] = false;   // this +0x10..+0x13
                }
                leNextState = EffectsStateJumping;
            }
            break;

        case EffectsStateJumping:                // 7
            // A pure entry state: the vapour effect is started by OnChangeState, and the
            // machine falls through to the "moving down" state on the very next tick.
            leNextState = EffectsStateJumpingMovingDown;
            break;

        case EffectsStateJumpingMovingDown:      // 9
            // Accumulate airborne time and drive the vapour trail's state blend from it.
            mFadeTime += lCarState.GetDt();      // this +0x0C += CarState +0x10 (mDt)
            SetVapourBlend(mFadeTime, lHelper);
            if (!lCarState.mbJumping)
            {
                leNextState = EffectsStateLanded;
            }
            break;

        case EffectsStateLanded:                 // 10
        {
            ActiveRaceCarData* const lpActiveRaceCar = lHelper.ActiveRaceCar();

            // The vapour trail ends the moment the car is back on the deck.
            lHelper.StopEffect(lpActiveRaceCar->mJumpEffectHandle);

            const BrnPhysics::Vehicle::RaceCarState* const lpCar = lCarState.mpCarState;

            // Landing speed gate, kept in the squared domain exactly as the console
            // (vmsum3fp128 self-dot of mLinearVelocity vs 196.0).
            const Vector3& lvVel = lpCar->mLinearVelocity;
            const f32 lfSpeedSq = __vmsum3fp(lvVel.mV, lvVel.mV).x;
            const bool lbFastLanding = (lfSpeedSq > KF_LANDING_MIN_SPEED_SQ);

            // ParticleModule +0x23136. In the junkyard the landing burst uses the full
            // dust effect and the car's own orientation; in the world it uses the lite
            // effect and a basis built from the wheel's contact.
            const bool lbInJunkyard = lHelper.ParticleModule().mbIsInJunkyard;

            bool lbStartedAnyWheel = false;

            if (lbFastLanding || lbInJunkyard)
            {
                for (u32 luWheel = 0; luWheel < ActiveRaceCarData::KU_NUM_WHEELS; ++luWheel)
                {
                    const BrnPhysics::Vehicle::WheelLite& lWheel = lpCar->maWheels[luWheel];
                    if (!lWheel.mRoadContact.mbIsOnGround)   // wheel +0x28
                    {
                        continue;
                    }

                    const char* const lpcEffectName =
                        lbInJunkyard ? KPC_LANDING_DUST_EFFECT : KPC_LANDING_DUST_LITE_EFFECT;

                    u32& lruHandle = lpActiveRaceCar->maJumpLandingWheelEffectHandles[luWheel];
                    const u32 luWorldIndex = lHelper.WorldIndex();

                    lHelper.StopEffect(lruHandle);
                    lruHandle = lHelper.ParticleModule().StartLionEffect(
                                    BrnParticle::ParticleDescription::HashString(lpcEffectName),
                                    lpcEffectName,
                                    luWorldIndex);

                    // The console resolves the freshly-stored handle through the EFFECTS
                    // MODULE's own particle module (helper +0x08, member +0xA80), not
                    // through the helper's module reference -- transcribed as written.
                    BrnParticle::LionEffect* const lpEffect =
                        lHelper.GetEffectsModule()->ParticleModule().GetLionEffect(lruHandle);

                    if (lpEffect != 0)
                    {
                        if (lbInJunkyard)
                        {
                            // Junkyard arm: the effect wears the car's orientation, planted
                            // at the wheel's contact point.
                            const Matrix44Affine& lCarTransform = lpCar->mTransform;
                            lpEffect->mTransform.xAxis = lCarTransform.xAxis;
                            lpEffect->mTransform.yAxis = lCarTransform.yAxis;
                            lpEffect->mTransform.zAxis = lCarTransform.zAxis;
                            lpEffect->mTransform.wAxis = lWheel.mRoadContact.mPosition;
                        }
                        else
                        {
                            // World arm: build a contact basis out of the road normal and
                            // the wheel velocity projected onto the contact plane, and hand
                            // the effect that projected velocity as an override.
                            const Vector3& lvNormal = lWheel.mRoadContact.mNormal;   // wheel +0x10
                            const Vector3& lvWheelVel = lWheel.mVelocity;            // wheel +0x30

                            // dot3 broadcast: the X360 subtracts in all four lanes, so the
                            // unused w lane rides along exactly as it does on the console.
                            const __vector4 alongNormal = __vmsum3fp(lvNormal.mV, lvWheelVel.mV);
                            const Vector3 lvTangent(__vsubfp(lvWheelVel.mV, __vmulfp(lvNormal.mV, alongNormal)));
                            lpEffect->mfVelocityX = lvTangent.mV.x;
                            lpEffect->mfVelocityY = lvTangent.mV.y;
                            lpEffect->mfVelocityZ = lvTangent.mV.z;
                            lpEffect->muFlags |= (BrnParticle::LionEffect::EPPE_FLAG_OVERRIDE_VELOCITY
                                                | BrnParticle::LionEffect::EPPE_FLAG_CHANGED);
                            const Vector3 lvForward(__vmulfp(lvTangent.mV, ReciprocalSqrt2(__vmsum3fp(lvTangent.mV, lvTangent.mV))));
                            const Vector3 lvRight = CrossNative(lvNormal, lvForward);

                            lpEffect->mTransform.xAxis = lvRight;
                            lpEffect->mTransform.yAxis = lvNormal;
                            lpEffect->mTransform.zAxis = lvForward;
                            lpEffect->mTransform.wAxis = lWheel.mRoadContact.mPosition;
                        }

                        lpEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
                    }

                    mbWheelsOnGround[luWheel] = true;
                    lbStartedAnyWheel = true;
                }

                if (lbStartedAnyWheel)
                {
                    FireWheelSparks(lCarState, lHelper);
                }
            }

            // A long fall earns a debris burst, once, then the fall clock is reset.
            if (mFadeTime > KF_DEBRIS_FADE_TIME)
            {
                FireWheelDebris(lCarState, lHelper);
                mFadeTime = 0.0f;
            }

            // Move on once every wheel has touched down -- or immediately when the landing
            // was slow, because the speed gate above never armed and no wheel latch will
            // ever be set (this is the junkyard-only path out of Landed).
            if ((mbWheelsOnGround[0] && mbWheelsOnGround[1]
                 && mbWheelsOnGround[2] && mbWheelsOnGround[3])
                || !lbFastLanding)
            {
                leNextState = EffectsStateFiringSparks;
            }
            break;
        }

        case EffectsStateFiringSparks:           // 11
            // Keep spraying sparks for as long as the debug "Landing Sparks Time" timer
            // (seeded by OnChangeState) runs.
            FireWheelSparks(lCarState, lHelper);
            if (lbStateTimerExpired)
            {
                leNextState = EffectsStateLandedWaiting;
            }
            break;

        case EffectsStateLandedWaiting:          // 12
            // A new jump inside the dwell re-arms the machine immediately (the console
            // compares the CarState +0x4E byte against 1); otherwise the 3 s dwell ends
            // the jump.
            if (lCarState.mbJumping)
            {
                mTime = 0.0f;                    // base +0x08 (SetStateTimer, inlined)
                leNextState = EffectsStateJumping;
            }
            else if (lbStateTimerExpired)
            {
                leNextState = EffectsStateJumpingFinishing;
            }
            break;

        case EffectsStateJumpingFinishing:       // 13
            leNextState = EffectsStateAllOff;
            break;

        default:
            CGS_ASSERT(false, "Unknown state");   // .cpp:242
            break;
    }

    return leNextState;
}

// =============================================================================
// OnChangeState @ 0x82299510
//   Entry action for each jump state: start/stop the vapour and landing-dust LION
//   effects and seed the state timer.
// =============================================================================
void JumpStateMachine::OnChangeState(EffectsState leNewState,
                                     RaceCarParticleEffectHelper& lHelper,
                                     CarState& /*lCarState*/)
{

    ActiveRaceCarData* const lpActiveRaceCar = lHelper.ActiveRaceCar();

    switch (leNewState)
    {
        case EffectsStateAllOff:                 // 0
            // Everything off: the vapour trail and all four landing-dust runs.
            lHelper.StopEffect(lpActiveRaceCar->mJumpEffectHandle);
            for (u32 luWheel = 0; luWheel < ActiveRaceCarData::KU_NUM_WHEELS; ++luWheel)
            {
                lHelper.StopEffect(lpActiveRaceCar->maJumpLandingWheelEffectHandles[luWheel]);
            }
            break;

        case EffectsStateJumping:                // 7
        {
            // Restart the vapour trail from scratch and re-zero the fall clock, then push
            // the (still zero) blend straight into the fresh handle.
            const u32 luWorldIndex = lHelper.WorldIndex();

            lHelper.StopEffect(lpActiveRaceCar->mJumpEffectHandle);
            lpActiveRaceCar->mJumpEffectHandle = lHelper.ParticleModule().StartLionEffect(
                BrnParticle::ParticleDescription::HashString(KPC_JUMP_VAPOUR_EFFECT),
                KPC_JUMP_VAPOUR_EFFECT,
                luWorldIndex);

            mFadeTime = 0.0f;                    // this +0x0C
            SetVapourBlend(mFadeTime, lHelper);  // the console passes the same f1 it just stored
            break;
        }

        case EffectsStateJumpingMovingDown:      // 9
        case EffectsStateLanded:                 // 10
        case EffectsStateJumpingFinishing:       // 13
            // No entry action.
            break;

        case EffectsStateFiringSparks:           // 11
            // Spark dwell comes from the debug "Jumping" menu (EffectsDebugComponent +0x6C).
            mTime = lHelper.DebugComponent()->JumpParams().LandingSparksTime();
            break;

        case EffectsStateLandedWaiting:          // 12
            mTime = KF_LANDED_WAITING_TIME;      // base +0x08 (flt_8200DD24 == 3.0)
            break;

        default:
            CGS_ASSERT(false, "Unknown state");   // .cpp:302
            break;
    }
}

void JumpStateMachine::OnTick(CarState& /*lCarState*/,
                              RaceCarParticleEffectHelper& /*lHelper*/)
{
    // No X360 export: the base's empty slot, ICF-folded. Empty is faithful.
}

} // namespace BrnEffects
