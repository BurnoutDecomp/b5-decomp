// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_Finish.cpp
// ============================================================================
// Out-of-line BODIES for the ScoringSystem "Finish" group declared in the keystone
// header BrnScoringSystem.h lines 337-365: checkpoint-distance setup/query, the
// finish-distance cumulative pass, and the gameplay event hooks.
//
// SHAPE is DWARF-authoritative (signatures preserved verbatim from the keystone);
// BODY is reconstructed from the X360 pseudocode + assembly (the asm is authority on
// the optimizer artefacts, e.g. the unrolled cumulative-sum loop in
// ProcessFinishDistances). Every member is accessed BY NAME -- the X360 byte offsets
// were used only to identify which named member each access targets (proven against
// an offsetof probe of this build's ScoringSystem / CarData layout).
//
// METHODS LANDED HERE (compile clean):
//   SetCheckpointDistances                 (0x82310C30)
//   GetCheckpointDistanceToFinish          (0x82310CC8)
//   ProcessFinishDistances                 (0x823124F0)
//   OnPlayerDoesATakedown                  (0x8234CE08)
//   RegisterFinishForCar                   (0x8231F198) -- now unblocked: the CarScoreData
//       per-lap-time array (maaLapTimes, +0x24), completed-laps (+0x10), finish-position
//       (+0x14), has-finished (+0x45) and total-finish-time (+0x00, new SetFinishTime grow)
//       all have NAMED accessors after the CarScoreData grow. The lbFinished branch snaps the
//       finish time to the network grid via BrnNetwork::NetworkRounder::RoundTimeToNetworkAccuracy
//       (DWARF home BrnNetworkRounder.h; forward-declared above in its namespace model).
//   RaceCarHasReachedCheckPointWithinEvent (0x82326E50) -- now unblocked: bumps the CarData
//       checkpoint counter (+0x154, IncrementCheckpointCount) and, in Eliminator mode, reads the
//       car's live race position (CarScoreData +0x08, GetRacePosition) and flags
//       mbEliminationRequired when the car is last (position == actual-cars - 1).

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
// KI_MAX_LANDMARKS_IN_MODE (== 16) lives in GameStateModuleIO, already pulled in by the
// keystone via BrnGameStateSharedIO.h; no extra include needed beyond the assert macro.

// BrnNetwork::NetworkRounder::RoundTimeToNetworkAccuracy snaps a CgsSystem::Time to the
// network time grid in place. DWARF home is GameSource/Network/Utilities/BrnNetworkRounder.h
// (the .cpp peer is committed; this function's body is reconstructed/reviewed in that home but
// the project models the rounder as a namespace of free functions -- see RoundFloatToAccuracy
// in BrnNetworkRounder.cpp). There is no committed header for it yet, so it is forward-declared
// here in that namespace, matching the committed namespace-based model. RegisterFinishForCar's
// finish branch calls it (X360 0x8231F2DC) only when the lbFinished flag is set.
namespace BrnNetwork
{
    namespace NetworkRounder
    {
        void RoundTimeToNetworkAccuracy(CgsSystem::Time* lpTime);
    }
}

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // X360 0x82310C30. Store the per-checkpoint *separation* distance for the
    // checkpoint counted back from the finish line. The X360 fires two asserts: a
    // value-validity check (fcmpu f31,f31 -- a NaN self-compare) and the index bound
    // (luCheckpoint < KI_MAX_LANDMARKS_IN_MODE == 16).
    // ------------------------------------------------------------------------
    void ScoringSystem::SetCheckpointDistances(u32 luCheckpoint, f32 lfDistance)
    {
        CGS_ASSERT(lfDistance == lfDistance,
                   "RwMathFPU::IsValid( lfDistanceToFinish )");
        CGS_ASSERT(luCheckpoint < static_cast<u32>(GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE),
                   "( luCheckpointIndexFromFinish >= 0 ) && ( luCheckpointIndexFromFinish < (uint32_t)KI_MAX_LANDMARKS_IN_MODE )");

        mafCheckpointSeparations[luCheckpoint] = lfDistance;
    }

    // ------------------------------------------------------------------------
    // X360 0x82310CC8. Read back the precomputed distance-to-finish for a checkpoint.
    // Asserts the index bound and that the distances have been processed
    // (mbCheckPointDistancesToFinishReady) before the table is read.
    // ------------------------------------------------------------------------
    f32 ScoringSystem::GetCheckpointDistanceToFinish(u32 luCheckpoint) const
    {
        CGS_ASSERT(luCheckpoint < static_cast<u32>(GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE),
                   "luCheckpointIndex < (uint32_t)KI_MAX_LANDMARKS_IN_MODE");
        CGS_ASSERT(mbCheckPointDistancesToFinishReady,
                   "Distance to finish not ready");

        return mafCheckpointDistancesToFinish[luCheckpoint];
    }

    // ------------------------------------------------------------------------
    // X360 0x823124F0. Turn the per-checkpoint *separations* into cumulative
    // *distances-to-finish*, walking backwards from the finish line:
    //   dist[last]  = 0
    //   dist[i]     = dist[i+1] + separation[i]      (i = liNumCheckpoints-2 .. 0)
    // then flag the table ready and reset the running total race distance. The X360
    // emits a 4x-unrolled descending loop; reconstructed here as the plain backward
    // loop it represents.
    // ------------------------------------------------------------------------
    void ScoringSystem::ProcessFinishDistances(s32 liNumCheckpoints)
    {
        mafCheckpointDistancesToFinish[liNumCheckpoints - 1] = 0.0f;

        for (s32 liCheckpoint = liNumCheckpoints - 2; liCheckpoint >= 0; --liCheckpoint)
        {
            mafCheckpointDistancesToFinish[liCheckpoint] =
                mafCheckpointDistancesToFinish[liCheckpoint + 1] +
                mafCheckpointSeparations[liCheckpoint];
        }

        mfTotalRaceDistance = 0.0f;
        mbCheckPointDistancesToFinishReady = true;
    }

    // ------------------------------------------------------------------------
    // X360 0x8234CE08. Forward a player takedown to the Road-Rage sub-scorer, which
    // bumps the running takedown count and (on threshold crossings) queues the
    // appropriate game action. lTime is forwarded by value; the action queue is passed
    // straight through (Road-Rage models the not-yet-committed GameActionQueue as void*).
    // ------------------------------------------------------------------------
    void ScoringSystem::OnPlayerDoesATakedown(CgsSystem::Time lTime,
                                              InputBuffer::GameActionQueue* lpQueue)
    {
        mRoadRageModeScoring.IncrementPlayerNumTakedowns(this, lTime, lpQueue);
    }

    // ------------------------------------------------------------------------
    // X360 0x8231F198. Record a car crossing the finish line for one lap. Reads the car's
    // current completed-lap count, turns the elapsed-since-start time into THIS lap's split
    // (by subtracting every previously banked lap split), banks the split into maaLapTimes,
    // and bumps the completed-lap count. When the car has now completed every lap of the
    // event it also commits the cumulative finish time (sum of all lap splits, optionally
    // snapped to the network time grid), assigns the next finish position, marks the car
    // finished, and raises the system-wide "a car has finished the race" flag.
    //
    // lbFinished selects the network-accuracy time rounding (the X360 `if ( a2 )` gate).
    // ------------------------------------------------------------------------
    void ScoringSystem::RegisterFinishForCar(bool lbFinished, EActiveRaceCarIndex leRaceCarIndex,
                                             const CgsSystem::Time& lTime)
    {
        CarData* lpCarData = GetCarData(leRaceCarIndex);
        if (lpCarData == nullptr)
        {
            return;
        }

        GameStateModuleIO::CarScoreData* lpScoreData = lpCarData->GetScoreData();

        u32 luCompletedLaps = static_cast<u32>(lpScoreData->GetCompletedLaps());
        if (luCompletedLaps >= muTotalLaps)
        {
            return;
        }

        CGS_ASSERT(luCompletedLaps < 4u, "luCompletedLaps < BrnWorld::KU_MAX_LAPS");

        // This lap's split = (now - mode start) minus every lap already banked for this car.
        CgsSystem::Time lLapTime = lTime - mStartTime;
        for (u32 luLap = 0; luLap < luCompletedLaps; ++luLap)
        {
            lLapTime -= lpScoreData->GetLapTime(luLap);
        }

        lpScoreData->SetLapTime(luCompletedLaps, lLapTime);

        u32 luNewCompletedLaps = luCompletedLaps + 1u;
        lpScoreData->SetCompletedLaps(static_cast<s32>(luNewCompletedLaps));

        if (luNewCompletedLaps >= muTotalLaps)
        {
            // Cumulative finish time = sum of every banked lap split.
            CgsSystem::Time lTotalTime(0.0f);
            for (u32 luLap = 0; luLap < muTotalLaps; ++luLap)
            {
                lTotalTime += lpScoreData->GetLapTime(luLap);
            }
            lpScoreData->SetFinishTime(lTotalTime);

            if (lbFinished)
            {
                CgsSystem::Time lRoundedTime = lpScoreData->GetFinishTime();
                BrnNetwork::NetworkRounder::RoundTimeToNetworkAccuracy(&lRoundedTime);
                lpScoreData->SetFinishTime(lRoundedTime);
            }

            ++muNumCarsFinishedRace;
            lpScoreData->SetFinishPosition(static_cast<s32>(muNumCarsFinishedRace));
            lpScoreData->SetHasFinished(true);
            mbACarHasFinishedTheRace = true;
        }
    }

    // ------------------------------------------------------------------------
    // X360 0x82326E50. Hook fired when a race car triggers a checkpoint landmark inside an
    // event. Bumps the car's per-event checkpoint counter, then -- only in Eliminator mode --
    // flags that an elimination is now required if the car that reached this checkpoint is the
    // last-placed car (its live race position is the back of the field, count - 1).
    // ------------------------------------------------------------------------
    void ScoringSystem::RaceCarHasReachedCheckPointWithinEvent(EActiveRaceCarIndex leRaceCarIndex,
                                                               GameStateModuleIO::EGameModeType leGameMode)
    {
        CarData* lpCarData = GetCarData(leRaceCarIndex);
        CGS_ASSERT(lpCarData != nullptr, "No car data");
        if (lpCarData == nullptr)
        {
            return;
        }

        lpCarData->IncrementCheckpointCount();

        if (leGameMode == GameStateModuleIO::E_MODE_ELIMINATOR)
        {
            const s32 liRacePosition = lpCarData->GetScoreData()->GetRacePosition();
            if (liRacePosition == static_cast<s32>(muActualNumberOfCarsInCurrentMode) - 1)
            {
                mbEliminationRequired = true;
            }
        }
    }
}
