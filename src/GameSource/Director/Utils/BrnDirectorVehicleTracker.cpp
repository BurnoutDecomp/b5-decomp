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
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"      // the REAL DirectorIO::InputBuffer
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"           // Camera::VehicleInfo (the race-car array)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatus::GetCurrentTimeStep

namespace BrnDirector
{

// THE PRIVATE `namespace DirectorIO { VehicleInfo; TimerStatusInterface; InputBuffer; }` SLICE
// THAT LIVED HERE IS GONE (2026-08-29, crash-camera wave). It was an ODR fork of types whose
// real home is DirectorModule/BrnDirectorModuleIO.h, and every accessor on it was declared and
// never defined -- which is precisely why VehicleTracker::Update could not link and why its
// caller in MainDirector::PreSceneQueryUpdate stayed gated for so long. Update now reads the
// real InputBuffer; see the notes at each re-fitted read.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// The race-end ramp constants are file-scope statics in the console image (.data at
// 0x82CDA55x). Their VALUES are not recoverable: Hex-Rays prints them as bare flt_82CDA55x
// symbols in every export because the region is writable, so there is nothing to read. They are
// declared here as the named tunables the body reads, with neutral placeholders; the body's
// STRUCTURE (which threshold gates which band, the ramp blend) is faithful to the asm.
// FLAG: placeholder values -- the .data floats are not reproduced.
// (The two CRASH-BAND thresholds that used to sit alongside them are gone with the classifier
//  they fed -- see the GATE inside Update for why running it on 0.0f would have switched the
//  crash slow motion OFF while looking like the console.)
// ----------------------------------------------------------------------------
namespace
{
    // Distance to the finish line at which the race-end cinematic effect reaches FULL
    // strength (amount 1.0). flt_82CDA550.
    f32 KF_DISTANCE_TO_FINISH_BEFORE_FULL_EFFECTS = 0.0f;  // FLAG: .data value not recovered
    // Distance to the finish at which the effect STARTS ramping in (amount 0.0). flt_82CDA554.
    f32 KF_DISTANCE_TO_FINISH_BEFORE_EFFECTS = 0.0f;       // FLAG: .data value not recovered
    // Per-frame blend factor the effect amount lerps toward its target by. flt_82CDA558.
    f32 KF_RACE_END_EFFECT_BLEND_FACTOR = 0.0f;            // FLAG: .data value not recovered
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
    CGS_ASSERT(lpInput->GetUsedRaceCars()->IsBitSet(static_cast<u32>(miVehicleIndex)),
               "lpInput->GetUsedRaceCars()->IsBitSet(miVehicleIndex)");

    // -- RE-FITTED TO THE REAL InputBuffer (2026-08-29, crash-camera wave). These three reads
    // used to go through the declaration-only slice this file used to carry in its header
    // (IsRaceCarUsed / GetVehicleInfo / GetTimerStatusInterface().GetWorldSimTimeStep()), none of
    // which was ever defined -- so this function could never link and its caller stayed gated.
    // The real accessors reach the same fields: the console's strided vehicle array IS
    // GetRaceCarInfo() (0x4F0 per entry, the exact stride the slice recorded), and its elements
    // are Camera::VehicleInfo, whose members carry the offsets the slice named by hand
    // (+0x330 mLinearVelocity, +0x3CC mfSpeedMPH, +0x220 mTransform.Pos()).
    const Camera::VehicleInfo& lVehicle = lpInput->GetRaceCarInfo()[miVehicleIndex];

    // World sim timestep this frame: the SIM TimerStatus base step times its multiplier --
    // exactly what the slice described as "the raw step at +0x1C by the slow-mo scale at +0x20",
    // and exactly what GetCurrentTimeStep() is.
    const f32 lfSimTimeStep =
        lpInput->GetTimerStatusInterface()->GetSimTimerStatus()->GetCurrentTimeStep();

    mbIsFirstFrameOfCrash = false;

    // Crash-energy classification (only for the player car).
    if (miVehicleIndex == lePlayerCarIndex)
    {
        if (!lVehicle.mRaceCarState.mbCrashing)
        {
            meCrashType = E_CRASH_NOT_CRASHING;
        }
        else if (meCrashType == E_CRASH_NOT_CRASHING)
        {
            // The crash just started this frame.
            mbIsFirstFrameOfCrash = true;

            // GATE: THE CRASH-ENERGY BAND. The console picks LOW / NORMAL / HIGH here; this build
            // leaves meCrashType at E_CRASH_NOT_CRASHING. Running the classifier anyway would be
            // WORSE THAN NOT RUNNING IT, for three stacked reasons:
            //
            //   (a) its two thresholds are .data floats (flt_82CDA548 / flt_82CDA54C) whose
            //       VALUES are in no export -- Hex-Rays leaves them as bare symbols because the
            //       region is writable, so there is nothing to read. The placeholders at the top
            //       of this file are 0.0f;
            //   (b) with 0.0f the first test degenerates to `speed < maxSpeed`, true for
            //       essentially every crash, so the answer would be E_CRASH_LOW_ENERGY every
            //       time -- and E_CRASH_LOW_ENERGY IS THE ONE VALUE THAT SUPPRESSES THE CRASH
            //       SLOW MOTION (ArbStateCrashing::ApplySlomoAndShake @0x8224F8D8 tests exactly
            //       that enumerator). Classifying on invented constants would switch the feature
            //       off while looking like the console;
            //   (c) the two inputs it needs -- InputBuffer +0x7900 (the player speed in MPH) and
            //       +0x7904 (the "force the next world crash to be a fast top-down" arm) -- have
            //       no accessor on the real InputBuffer. The slice this file used to carry named
            //       them; nothing ever defined them.
            //
            // THE DIVERGENCE, STATED PLAINLY: NOT_CRASHING is the console value only while the car
            // is NOT crashing, and it is the PERMISSIVE value downstream -- the crash camera will
            // slow down crashes the console might have classified LOW_ENERGY and left at real
            // time. That is a visible behavioural difference, not a no-op.
            // DELETE-WHEN: the two .data floats are read out of the image AND the two InputBuffer
            // accessors are homed. The block goes back verbatim then; its shape is in the asm at
            // 0x8223B1A8 and in this file's history.
            (void)lbForceNextWorldCrashToBeFastTopDown;
        }

        // GATE: `mScoreData = lpInput->GetPlayerScoreData();`. The real InputBuffer models the
        //   score region as one opaque span (mScoreAndBoostBlock, @0x3110..0x3238) with no
        //   CarScoreData accessor, so there is nothing to copy BY NAME, and reaching into the
        //   span by offset is the hack this wave has been retiring rather than adding.
        //   CONSEQUENCE: mScoreData stays at its Construct() value, so the race-end effect ramp
        //   below reads a zero distance-to-finish. That ramp runs only while the event state is
        //   RACING -- which this build does not reach -- and its own three constants are
        //   unrecovered placeholders too, so nothing observable changes today.
        //   DELETE-WHEN: InputBuffer grows a named GetPlayerScoreData().
    }

    // Push this frame's samples into the history journals: seed every slot on the first
    // frame (so the window reads as steady state), then push one newest sample thereafter.
    if (mbFirstFrame)
    {
        mMphJournal.SetAll(lVehicle.mRaceCarState.mfSpeedMPH);
        mTimestepJournal.SetAll(0.0f);
        mPositionJournal.SetAll(lVehicle.mRaceCarState.mTransform.Pos());
        mLinearVelocityJournal.SetAll(lVehicle.mRaceCarState.mLinearVelocity);
    }
    else
    {
        mMphJournal.SetCurrent(lVehicle.mRaceCarState.mfSpeedMPH);
        mTimestepJournal.SetCurrent(lfSimTimeStep);
        mPositionJournal.SetCurrent(lVehicle.mRaceCarState.mTransform.Pos());
        mLinearVelocityJournal.SetCurrent(lVehicle.mRaceCarState.mLinearVelocity);
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
