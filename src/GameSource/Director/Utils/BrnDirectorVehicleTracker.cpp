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
// ⭐ MOVED HERE FROM THE HEADER (2026-08-29, crash-camera wave), byte for byte. See the banner
// at BrnDirectorVehicleTracker.h for why: these three are declaration-only SLICES of types whose
// real homes are DirectorModule/BrnDirectorModuleIO.h, and leaving them in a header made that
// header impossible to include from any TU that also reaches the real ones -- which broke six
// TUs the first time an arbitrator state embedded VehicleTracker::ECrashType by value.
// Nothing is emitted for any of these accessors (all declaration-only), so confining the fork
// to the single TU that uses it costs nothing and leaks nothing.
// DELETE-WHEN: Update is re-fitted to the real InputBuffer API.
// ----------------------------------------------------------------------------
namespace DirectorIO
{
   // The motion data for one tracked vehicle the input buffer hands out. Update reads the
    // MPH, the linear velocity, and the world transform's position out of it. The vehicle
    // array is strided (0x4F0 per entry); the tracker indexes it by miVehicleIndex.
    struct VehicleInfo
    {
        f32                       GetMphLastFrame() const;        // +0x3CC
        rw::math::vpu::Vector3    GetLinearVelocity() const;      // +0x330
        rw::math::vpu::Vector3    GetWorldPosition() const;       // transform Pos (+0x220)
        // True while this vehicle is mid-crash (drives the crash-energy classification).
        bool                      IsCrashing() const;
        // The vehicle's top speed in MPH (the crash thresholds are measured below it).
        f32                       GetMaxMph() const;
    };

    // The per-frame timer the world timestep is sampled from.
    struct TimerStatusInterface
    {
        // The world sim timestep this frame (the body multiplies two timer fields: the raw
        // step at +0x1C by the slow-mo scale at +0x20).
        f32 GetWorldSimTimeStep() const;
    };

    class InputBuffer
    {
    public:
        // The bitset of race-car slots currently in use (Update asserts the tracked slot is set).
        const void*                 GetUsedRaceCars() const;
        bool                        IsRaceCarUsed(s32 liIndex) const;
        // The strided vehicle-info array (indexed by the tracked slot).
        const VehicleInfo&          GetVehicleInfo(s32 liIndex) const;
        // The frame timer interface.
        const TimerStatusInterface& GetTimerStatusInterface() const;
        // The score block for the tracked player car (copied into mScoreData).
        const CarScoreData&         GetPlayerScoreData() const;
        // The player car's current speed in MPH (the crash classifier compares it to the
        // vehicle's max MPH minus the band thresholds). InputBuffer +0x7900.
        f32                         GetPlayerSpeedMph() const;
        // The "force the next world crash to be a high-energy top-down" gate the arbitrator
        // raises on the input buffer. InputBuffer +0x7904.
        bool                        IsForcedFastTopDownCrashArmed() const;
    };
}

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
