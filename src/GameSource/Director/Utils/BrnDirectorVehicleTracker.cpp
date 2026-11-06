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
// The race-end ramp tunables come from ARTIST's writable data at 0x82CDA550..0x82CDA558.
namespace
{
    f32 KF_DISTANCE_TO_FINISH_BEFORE_FULL_EFFECTS = 500.0f;
    f32 KF_DISTANCE_TO_FINISH_BEFORE_EFFECTS = 1000.0f;
    f32 KF_RACE_END_EFFECT_BLEND_FACTOR = 0.05f;
}
// ----------------------------------------------------------------------------
// DWARF BrnDirectorVehicleTracker.h:65 / :68 -- the crash-energy bands, read out of the image with
// tools/re/x360rd.py (the "writable .data has nothing to read" note that gated the classifier was
// wrong: the bytes are in the image, only Hex-Rays declines to print them):
//     0x82CDA548 = 0x42480000 = 50.0   (the `fsubs f11, maxMPH, [548]` operand @0x8223B3E8)
//     0x82CDA54C = 0x41A00000 = 20.0   (the `fsubs f13, maxMPH, [54C]` operand @0x8223B3F0)
// A crash under (top speed - 50 mph) is LOW energy, under (top speed - 20 mph) NORMAL, else HIGH.
// ----------------------------------------------------------------------------
f32 VehicleTracker::sfMinSpeedBelowMaxMPHForNormalCrash    = 50.0f;
f32 VehicleTracker::sfMinSpeedBelowMaxMPHForHighSpeedCrash = 20.0f;

// ----------------------------------------------------------------------------
// BrnDirector::VehicleTracker::Construct @0x821F77A8.
// Preserve the history journals and crash-class word; this reset only writes
// the five native fields below and calls the real score-data clear.
// ----------------------------------------------------------------------------
void VehicleTracker::Construct()
{
    mbFirstFrame = true;
    miVehicleIndex = -1;
    mScoreData.ClearData();
    mfRaceEndEffectAmount = 0.0f;
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

            // ⭐ THE CRASH-ENERGY BAND (2026-09-24, FX-DIRECTOR2; was GATED). @0x8223B3B8..0x8223B414:
            //     if (lbForce && crash info +0x24 mbHardstopVsWall)           -> HIGH   (0x8223B3C8 -> 0x8223B410)
            //     else if (+0x20 mfSpeedMPH <  maxMPH - sfMin..NormalCrash)   -> LOW    (fcmpu/bge @0x8223B3F4)
            //     else if (mfSpeedMPH       <  maxMPH - sfMin..HighSpeedCrash) -> NORMAL (fcmpu/blt @0x8223B404)
            //     else                                                         -> HIGH
            // The console reads the crash record straight out of the input buffer (+0x7900 / +0x7904 ==
            // the PlayerCrashInfo at +0x78E0, +0x20 / +0x24) and the car's top speed out of this frame's
            // VehicleInfo copy (+0x3D0, RaceCarState::mfMaxSpeedMPH). Every reason the gate gave is gone:
            // the two bands are read out of the image (above), and GetPlayerCrashInfo() is typed (FX-BRIDGES
            // CC-6). A NaN speed fails both `<` tests and lands HIGH, as the console's bge / blt do.
            // Consumers: ArbStateCrashing::ApplySlomoAndShake (LOW suppresses the crash slow motion) and
            // MomentHardStop's shot-group pick (it asserts "Unhandled crash type" on NOT_CRASHING).
            const Camera::PlayerCrashInfo* lpPlayerCrashInfo = lpInput->GetPlayerCrashInfo();
            if (lbForceNextWorldCrashToBeFastTopDown && lpPlayerCrashInfo->mbHardstopVsWall)
            {
                meCrashType = E_CRASH_HIGH_ENERGY;
            }
            else
            {
                const f32 lfSpeedMPH        = lpPlayerCrashInfo->mfSpeedMPH;                                           // cpp:90
                const f32 lfNormalThreshold = lVehicle.mRaceCarState.mfMaxSpeedMPH - sfMinSpeedBelowMaxMPHForNormalCrash;    // cpp:91
                const f32 lfHighThreshold   = lVehicle.mRaceCarState.mfMaxSpeedMPH - sfMinSpeedBelowMaxMPHForHighSpeedCrash; // cpp:92
                if (lfSpeedMPH < lfNormalThreshold)
                    meCrashType = E_CRASH_LOW_ENERGY;
                else if (lfSpeedMPH < lfHighThreshold)
                    meCrashType = E_CRASH_NORMAL;
                else
                    meCrashType = E_CRASH_HIGH_ENERGY;
            }
        }

        mScoreData = *lpInput->GetPlayerScoreData();
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
        const f32 lfDistanceToFinish = mScoreData.GetDistanceToFinishLive(); // ARTIST 8223B6A4: tracker+0x188

        f32 lfDesiredRaceEndAmount = 0.0f;
        if (lfDistanceToFinish < KF_DISTANCE_TO_FINISH_BEFORE_EFFECTS)
        {
            if (lfDistanceToFinish < KF_DISTANCE_TO_FINISH_BEFORE_FULL_EFFECTS)
            {
                lfDesiredRaceEndAmount = 1.0f;
            }
            else
            {
                const f32 lfRange = KF_DISTANCE_TO_FINISH_BEFORE_EFFECTS - KF_DISTANCE_TO_FINISH_BEFORE_FULL_EFFECTS;
                f32 lfNormalised = (lfDistanceToFinish - KF_DISTANCE_TO_FINISH_BEFORE_FULL_EFFECTS) / lfRange;
                if (lfNormalised < 0.0f) lfNormalised = 0.0f;
                if (lfNormalised > 1.0f) lfNormalised = 1.0f;
                lfDesiredRaceEndAmount = 1.0f - lfNormalised;
            }
        }

        mbFirstFrame = false;
        mfRaceEndEffectAmount =
            (lfDesiredRaceEndAmount - mfRaceEndEffectAmount) * KF_RACE_END_EFFECT_BLEND_FACTOR
            + mfRaceEndEffectAmount;
    }

    (void)lfSimTimeStep;
}

} // namespace BrnDirector
