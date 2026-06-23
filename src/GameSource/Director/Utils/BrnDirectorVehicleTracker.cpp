// ============================================================================
// GameSource/Director/Utils/BrnDirectorVehicleTracker.cpp
//
// Bodies for BrnDirector::VehicleTracker: Construct (reset) and Update @0x8223B1A8
// (the per-frame sample/classify/ramp). GetImplicitVelocity @0x82205F70 is defined
// inline in the header. Reconstructed against the X360 binary's member layout (see
// the header's LAYOUT block).
// ============================================================================

#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"

#include "rw/math/vpu/vector3_operation.h"   // Vector3 lane ops

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// The crash-band thresholds and the race-end ramp constants are file-scope statics in the
// console image (rodata at 0x82CDA5xx). Their VALUES are not recoverable from the rodata
// dump here, so they are declared as the named tunables the body reads and given neutral
// placeholders; the body's STRUCTURE (which threshold gates which band, the ramp blend) is
// faithful to the asm. FLAG: placeholder values -- the rodata floats are not reproduced.
// ----------------------------------------------------------------------------
namespace
{
    // Speed (MPH) the car must be BELOW (max MPH minus this) for a crash to count as merely
    // "normal" rather than low-energy. flt_82CDA548.
    f32 sfMinSpeedBelowMaxMPHForNormalCrash = 0.0f;     // FLAG: rodata value not recovered
    // Speed (MPH) below the max for a crash to be classified high-energy. flt_82CDA54C.
    f32 sfMinSpeedBelowMaxMPHForHighSpeedCrash = 0.0f;  // FLAG: rodata value not recovered

    // Distance to the finish line at which the race-end cinematic effect reaches FULL
    // strength (amount 1.0). flt_82CDA550.
    f32 KF_DISTANCE_TO_FINISH_BEFORE_FULL_EFFECTS = 0.0f;  // FLAG: rodata value not recovered
    // Distance to the finish at which the effect STARTS ramping in (amount 0.0). flt_82CDA554.
    f32 KF_DISTANCE_TO_FINISH_BEFORE_EFFECTS = 0.0f;       // FLAG: rodata value not recovered
    // Per-frame blend factor the effect amount lerps toward its target by. flt_82CDA558.
    f32 KF_RACE_END_EFFECT_BLEND_FACTOR = 0.0f;            // FLAG: rodata value not recovered
}

// ----------------------------------------------------------------------------
// BrnDirector::VehicleTracker::Construct
//   Clear the score block and reset all four history journals to empty. (The console body
//   clears CarScoreData then resets the four journals' cursor/size; the journals' Construct
//   does the cursor/size reset.)
// ----------------------------------------------------------------------------
void VehicleTracker::Construct()
{
    mScoreData = CarScoreData();

    mPositionJournal.Construct();
    mLinearVelocityJournal.Construct();
    mMphJournal.Construct();
    mTimestepJournal.Construct();

    meCrashType           = E_CRASH_NOT_CRASHING;
    mfRaceEndEffectAmount = 0.0f;
    mbFirstFrame          = true;
    mbIsFirstFrameOfCrash = false;
}

// ----------------------------------------------------------------------------
// BrnDirector::VehicleTracker::Update @0x8223B1A8
// ----------------------------------------------------------------------------
void VehicleTracker::Update(
    const GameState* lpGameState,
    const DirectorIO::InputBuffer* lpInput,
    EActiveRaceCarIndex lePlayerCarIndex,
    bool lbForceNextWorldCrashToBeFastTopDown)
{
    CGS_ASSERT(lpGameState != NULL, "lpGameState != NULL");
    CGS_ASSERT(lpInput != NULL, "lpInput != NULL");
    CGS_ASSERT(miVehicleIndex != -1, "miVehicleIndex != -1");
    CGS_ASSERT(lpInput->IsRaceCarUsed(miVehicleIndex),
               "lpInput->GetUsedRaceCars()->IsBitSet(miVehicleIndex)");

    const DirectorIO::VehicleInfo& lVehicle = lpInput->GetVehicleInfo(miVehicleIndex);

    // World sim timestep this frame: the raw step scaled by the slow-mo factor.
    const f32 lfSimTimeStep = lpInput->GetTimerStatusInterface().GetWorldSimTimeStep();

    mbIsFirstFrameOfCrash = false;

    // Crash-energy classification (only for the player car).
    if (miVehicleIndex == lePlayerCarIndex)
    {
        if (!lVehicle.IsCrashing())
        {
            meCrashType = E_CRASH_NOT_CRASHING;
        }
        else if (meCrashType == E_CRASH_NOT_CRASHING)
        {
            // The crash just started this frame.
            mbIsFirstFrameOfCrash = true;

            if (lbForceNextWorldCrashToBeFastTopDown && lpInput->IsForcedFastTopDownCrashArmed())
            {
                meCrashType = E_CRASH_HIGH_ENERGY;
            }
            else
            {
                const f32 lfSpeedMPH       = lpInput->GetPlayerSpeedMph();
                const f32 lfNormalThreshold = lVehicle.GetMaxMph() - sfMinSpeedBelowMaxMPHForNormalCrash;
                const f32 lfHighThreshold   = lVehicle.GetMaxMph() - sfMinSpeedBelowMaxMPHForHighSpeedCrash;

                if (lfSpeedMPH < lfNormalThreshold)
                {
                    meCrashType = E_CRASH_LOW_ENERGY;
                }
                else if (lfSpeedMPH >= lfHighThreshold)
                {
                    meCrashType = E_CRASH_HIGH_ENERGY;
                }
                else
                {
                    meCrashType = E_CRASH_NORMAL;
                }
            }
        }

        // Copy the player's live score block.
        mScoreData = lpInput->GetPlayerScoreData();
    }

    // Push this frame's samples into the history journals: seed every slot on the first
    // frame (so the window reads as steady state), then push one newest sample thereafter.
    if (mbFirstFrame)
    {
        mMphJournal.SetAll(lVehicle.GetMphLastFrame());
        mTimestepJournal.SetAll(0.0f);
        mPositionJournal.SetAll(lVehicle.GetWorldPosition());
        mLinearVelocityJournal.SetAll(lVehicle.GetLinearVelocity());
    }
    else
    {
        mMphJournal.SetCurrent(lVehicle.GetMphLastFrame());
        mTimestepJournal.SetCurrent(lfSimTimeStep);
        mPositionJournal.SetCurrent(lVehicle.GetWorldPosition());
        mLinearVelocityJournal.SetCurrent(lVehicle.GetLinearVelocity());
    }

    // Ramp the race-end cinematic effect amount. It only ramps while racing (event state ==
    // RACING) and out of the countdown; otherwise the effect is held off and the amount
    // snaps to zero. (The console reads the GameState event-state journal head and the
    // countdown flag here.)
    if (lpGameState->GetCurrentEventState() != GameState::E_EVENT_STATE_RACING
        || lpGameState->IsInCountdown())
    {
        mbFirstFrame          = false;
        mfRaceEndEffectAmount = 0.0f;
    }
    else
    {
        const f32 lfDistanceToFinish = mScoreData.GetDistanceToFinish();

        f32 lfDesiredRaceEndAmount;
        if (lfDistanceToFinish >= KF_DISTANCE_TO_FINISH_BEFORE_EFFECTS)
        {
            lfDesiredRaceEndAmount = 0.0f;
        }
        else if (lfDistanceToFinish >= KF_DISTANCE_TO_FINISH_BEFORE_FULL_EFFECTS)
        {
            const f32 lfRange = KF_DISTANCE_TO_FINISH_BEFORE_EFFECTS - KF_DISTANCE_TO_FINISH_BEFORE_FULL_EFFECTS;
            const f32 lfNormalised = (lfDistanceToFinish - KF_DISTANCE_TO_FINISH_BEFORE_FULL_EFFECTS) / lfRange;
            // The body clamps the 1 - normalised result into [0, 1] (the fsel pair).
            f32 lfClamped = 1.0f - lfNormalised;
            if (lfClamped < 0.0f) lfClamped = 0.0f;
            if (lfClamped > 1.0f) lfClamped = 1.0f;
            lfDesiredRaceEndAmount = lfClamped;
        }
        else
        {
            lfDesiredRaceEndAmount = 1.0f;
        }

        mbFirstFrame = false;
        mfRaceEndEffectAmount =
            (lfDesiredRaceEndAmount - mfRaceEndEffectAmount) * KF_RACE_END_EFFECT_BLEND_FACTOR
            + mfRaceEndEffectAmount;
    }

    (void)lfSimTimeStep;
}

} // namespace BrnDirector
