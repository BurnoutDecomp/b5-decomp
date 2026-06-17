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
//   SetCheckpointDistances           (0x82310C30)
//   GetCheckpointDistanceToFinish    (0x82310CC8)
//   ProcessFinishDistances           (0x823124F0)
//   OnPlayerDoesATakedown            (0x8234CE08)
//
// DEFERRED (declare-only, body NOT emitted here -- see methods_blocked in the report):
//   RegisterFinishForCar             (0x8231F198) -- needs CarScoreData per-lap-time
//       array + finish-position + per-car finished-flag NAMED members/accessors that
//       are currently opaque storage blobs (maStorage18 / miField10 / miField14) in
//       CarScoreData (DWARF home BrnGameStateSharedIO.h). Reconstructing the body would
//       require ad-hoc-slicing that shared record; deferred per the dependency rule.
//   RaceCarHasReachedCheckPointWithinEvent (0x82326E50) -- the finish-detection branch
//       compares a CarScoreData cumulative-checkpoint field (opaque miField08, no named
//       accessor) against miTotalCheckpoints and sets an as-yet-unidentified ScoringSystem
//       bool flag; same blocked-on-CarScoreData situation. Deferred.

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
// KI_MAX_LANDMARKS_IN_MODE (== 16) lives in GameStateModuleIO, already pulled in by the
// keystone via BrnGameStateSharedIO.h; no extra include needed beyond the assert macro.

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
}
