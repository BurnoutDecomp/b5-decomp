// =============================================================================
// GameSource/Effects/Jump/JumpStateMachine.cpp  (X360 ARTIST)
//
// SetVapourBlend @ 0x82288A58
//   Compute and apply the jump-vapour LION effect's state blend. The car's linear
//   velocity direction and world Z axis, its upward speed and the debug "Jumping"
//   menu's vapour delay/ramp times drive two smoothstep ramps that are multiplied
//   together; the product is written to the vapour effect handle-run held in the
//   ActiveRaceCarData (mpActiveRaceCar + 0x114). Reconstructed store-for-store from
//   the ARTIST X360 asm; SmoothStep call convention + Vector3/Vector2 brace-init
//   mirror the committed BoostStateMachine sibling.
//
//   Gates (all must hold or the blend stays 0):
//     speed = |mLinearVelocity| > 20.0                       (fast enough)
//     mLinearVelocity.y >= speed                             (essentially moving up)
//     dot(normalize(mLinearVelocity), mTransform.zAxis) > 0  (facing into the jump)
//   Then blend = SmoothStep(dir) * SmoothStep(time); the debug "Force State Blend"
//   override replaces the computed blend when enabled.
// =============================================================================

#include "GameSource/Effects/Jump/JumpStateMachine.h"
#include "GameSource/Effects/ParticleEffectHelper.h"          // RaceCarParticleEffectHelper
#include "GameSource/Effects/BrnEffectsDebugComponent.h"      // EffectsDebugComponent / EffectsDebugJumping
#include "GameSource/Effects/Curves.h"                        // BrnEffects::Curves::SmoothStep
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // BrnPhysics::Vehicle::RaceCarState
#include <cmath>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the NOT-RECONSTRUCTED announcement
#include <cstdio>   // snprintf

namespace BrnEffects
{

namespace
{
    // Rodata literals inlined at the call site in the X360 build (no named globals).
    const f32 KF_VAPOUR_MIN_SPEED = 20.0f;          // flt_820054CC (speed gate)
    const f32 KF_VAPOUR_DIR_LOW   = 0.70700002f;    // flt_82011C14 (dir smoothstep lower threshold)
    const f32 KF_VAPOUR_DIR_MID   = 0.85350001f;    // flt_82011C10 (dir smoothstep mid threshold)
    const u32 KU_VAPOUR_HANDLE_OFFSET = 0x114;      // mpActiveRaceCar + 0x114 -> vapour handle-run
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
    const f32 lfSpeed = sqrtf(lvVel.x * lvVel.x + lvVel.y * lvVel.y + lvVel.z * lvVel.z);

    if (lfSpeed > KF_VAPOUR_MIN_SPEED)
    {
        // Only fire while the velocity is (essentially) straight up: vel.y >= |vel|.
        if (lvVel.y >= lfSpeed)
        {
            // Direction alignment: dot(unit velocity, car world Z axis).
            const f32 lfInvSpeed = 1.0f / lfSpeed;
            const Vector3& lvForward = lpCar->mTransform.zAxis;
            const f32 lfDot = (lvVel.x * lfInvSpeed) * lvForward.x
                            + (lvVel.y * lfInvSpeed) * lvForward.y
                            + (lvVel.z * lfInvSpeed) * lvForward.z;

            if (lfDot > 0.0f)
            {
                const f32 lfVapourDelay = lpDebug->JumpParams().VapourStartDelay();   // +0x70
                const f32 lfVapourEnd   = lpDebug->JumpParams().VapourRampEndTime()   // +0x74
                                        + lfVapourDelay;

                // Ramp 1: alignment dot -> [0,1] over [0.707, 1.0] (mid 0.8535).
                const Vector3 lvDirParams = { KF_VAPOUR_DIR_LOW, KF_VAPOUR_DIR_MID, 1.0f, 0.0f };
                const Vector2 lvDirScale  = { 0.0f, 1.0f, 0.0f, 0.0f };

                // Ramp 2: fade time -> [0,0.5] over [delay, delay+ramp] (mid midpoint).
                const Vector3 lvTimeParams = { lfVapourDelay,
                                               ((lfVapourEnd - lfVapourDelay) * 0.5f) + lfVapourDelay,
                                               lfVapourEnd, 0.0f };
                const Vector2 lvTimeScale  = { 0.0f, 0.5f, 0.0f, 0.0f };

                BrnEffects::Curves::SmoothStep lCurve;
                const f32 lfDirBlend  = lCurve.Evaluate(lvDirParams, lvDirScale, lfDot);
                const f32 lfTimeBlend = lCurve.Evaluate(lvTimeParams, lvTimeScale, lfFadeTime);
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
    // (mpActiveRaceCar + 0x114). One u32 handle, count == 1.
    const u32* lpuVapourHandle =
        reinterpret_cast<const u32*>(
            reinterpret_cast<const u8*>(lHelper.ActiveRaceCar()) + KU_VAPOUR_HANDLE_OFFSET);
    lHelper.SetEffectStateBlend(lpuVapourHandle, 1, lfBlend);
}


    // =====================================================================================
    // The three EffectsStateMachine overrides. X360:
    //   OnDetermineNextState @0x8229B9F8 (211 pseudocode lines)
    //   OnChangeState        @0x82299510 (54)
    //   OnTick               -- no export; the base's empty slot, ICF-folded
    //
    // ⛔ NOT RECONSTRUCTED IN THIS WAVE, and announced rather than faked. Both bodied halves
    // exist only to start and follow LION effects: OnChangeState's entry actions are
    // StartLionEffect / StopLionEffect on the jump and landing effects, and
    // OnDetermineNextState's transitions are driven by those handles plus FireWheelSparks
    // @0x82299670 / FireWheelDebris @0x822939D8. On this build StartLionEffect always returns
    // KU_HANDLE_INVALID (BrnParticle::ParticleDescriptionCollection has no committed layout --
    // see ParticleModule_Lifecycle.cpp), so a faithful transcription of the transition ladder
    // would drive a machine whose every effect handle is invalid. The honest shape is to leave
    // the machine in its current state and say so once.
    //
    // This is REACHABLE code (ActiveRaceCarData::Tick ticks the jump machine every frame for
    // every active car), which is why it is a loud no-op and not a trap stub.
    // =====================================================================================
    namespace
    {
        void LogJumpNotReconstructed(bool& lrbLogged, const char* lpcWhat)
        {
            if (lrbLogged)
                return;
            lrbLogged = true;
            char lacMsg[224];
            std::snprintf(lacMsg, sizeof(lacMsg), "[effects] NOT RECONSTRUCTED: %s\n", lpcWhat);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    EffectsState JumpStateMachine::OnDetermineNextState(CarState& /*lCarState*/,
                                                        bool /*lbStateTimerExpired*/,
                                                        EffectsState leCurrentState,
                                                        RaceCarParticleEffectHelper& /*lHelper*/)
    {
        static bool sbLogged = false;
        LogJumpNotReconstructed(sbLogged,
            "JumpStateMachine::OnDetermineNextState @0x8229B9F8 (its transitions key on Lion "
            "effect handles, and StartLionEffect returns KU_HANDLE_INVALID on this build); the "
            "jump machine holds its current state");
        return leCurrentState;
    }

    void JumpStateMachine::OnChangeState(EffectsState /*leNewState*/,
                                         RaceCarParticleEffectHelper& /*lHelper*/,
                                         CarState& /*lCarState*/)
    {
        static bool sbLogged = false;
        LogJumpNotReconstructed(sbLogged,
            "JumpStateMachine::OnChangeState @0x82299510 (Start/StopLionEffect on the jump and "
            "landing effects)");
    }

    void JumpStateMachine::OnTick(CarState& /*lCarState*/,
                                  RaceCarParticleEffectHelper& /*lHelper*/)
    {
        // No X360 export: the base's empty slot, ICF-folded. Empty is faithful.
    }
} // namespace BrnEffects
