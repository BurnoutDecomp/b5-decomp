// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoringLinkStubs.cpp
// ============================================================================
// FLAG -- LINK STUBS. Not a reconstruction. Delete this whole file when the real TU
// lands.
//
// DELETE-WHEN
//   Delete this file the moment
//   GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.cpp exists (the .cpp
//   that BrnRoadRageModeScoring.h has always pointed at: "the full member layout + the
//   method BODIES land with this type's own TU later"). The two must NEVER be in one build
//   -- every symbol below is a duplicate of one the real TU will define.
//
// WHY IT EXISTS
//   BrnGameState::RoadRageModeScoring is embedded BY VALUE in the ScoringSystem keystone and
//   is driven from six committed call sites, but the type has a header and NO .cpp anywhere
//   in the tree, so every one of those calls is an unresolved external the moment the scoring
//   set is mounted. All fifteen declared methods are defined here so the link closes.
//
//   Offline Stunt Run -- the mode this wave is standing up -- never scores road rage: the mode
//   is selected before any scorer runs, and the road-rage scorer's own mbGameModeActive gate
//   stays false for it. That is what makes inert bodies correct here rather than merely
//   convenient: the console would take the same branches.
//
// WHAT THE REAL TU WILL NEED (recorded so the seed is not lost)
//   Only ONE of the fifteen has an out-of-line X360 symbol:
//     BrnGameState::RoadRageModeScoring::IncrementPlayerNumTakedowns  @ 0x823445D0
//   Its real body is substantial and is the reason the whole type is parked -- it bumps the
//   takedown counters, posts a game action through CgsModule::VariableEventQueue<13312,16>
//   when the next-extension threshold is crossed, and on the medal path calls back into
//   ScoringSystem::CheckRoadRageMedalAwarded / GetModeTimeRemaining / IncreaseTimeLimit
//   before re-arming the extension counter. Three of those ScoringSystem entry points and the
//   GameActionQueue type are still uncommitted, which is exactly why this file is stubs.
//   The others are console-inlined; ClearData/Construct are recovered from their inline
//   sites (see RECOVERY STATUS below), the rest still await recovery from theirs.
//
// SAFETY RULE APPLIED HERE
//   NOT ONE of these bodies is CGS_ASSERT(false). Six of them are on the live ScoringSystem
//   lifecycle path and a trap would fire on a normal offline run:
//     ClearData                  <- ScoringSystem::ClearData          (BrnScoringSystem_Lookup.cpp)
//     IsActive                   <- ScoringSystem::WriteDataToOutput  (BrnScoringSystem_Lifecycle.cpp)
//     GetNumTakedownsAchieved    <- ScoringSystem::WriteDataToOutput  (same)
//     GetTargetNumTakedowns      <- ScoringSystem::WriteDataToOutput  (same)
//     SetTakeDownTarget          <- ScoringSystem::UpdateB medal pass (BrnScoringSystem_UpdateB.cpp)
//     IncrementPlayerNumTakedowns<- ScoringSystem takedown finish     (BrnScoringSystem_Finish.cpp)
//   The remaining nine have no committed caller at all; they are defined only so the type's
//   declaration set is fully satisfied. They are inert on the same terms.
//
//   "Inert" here means NEUTRAL, not empty-where-empty-is-wrong: the clears really do zero the
//   ten members the home header documents, and the two counter setters/getters really do
//   store and return, so that WriteDataToOutput publishes a coherent all-zero road-rage block
//   (which is what the console publishes for a non-road-rage mode: its own else-arm writes 0
//   to both output fields). What is missing is the scoring BEHAVIOUR -- takedown counting,
//   the time-extension ladder and the damage-critical message flow -- none of which any
//   offline stunt run reaches.
//
//   RECOVERY STATUS, per method: ClearData and Construct ARE RECOVERED -- their bodies are
//   fully inlined at ScoringSystem::ClearData 0x8232A508..0x8232A534 and ScoringSystem::
//   Construct 0x8233809C..0x823380C8 (identical store blocks; miTargetNumTakedowns gets -1,
//   everything else 0/false), and the three WriteDataToOutput getters' member mapping is
//   asm-proven at 0x8232AE98 (+0x4B40 achieved / +0x4B4C target / +0x4B57 active; the type is
//   embedded at ScoringSystem+0x4B40). The REMAINING methods are stub-neutral, not recovered
//   (no symbol and no readable inline site found) -- do not cite those as evidence of what the
//   console stores; the real TU decides that.
// ============================================================================

#include "GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.h"

namespace BrnGameState
{
    // ---- lifecycle -----------------------------------------------------------------

    // Stub-neutral: leave the record in the same all-zero shape ClearData below produces, so
    // a freshly constructed by-value embed never reads back uninitialised counters.
    void RoadRageModeScoring::Construct()
    {
        ClearData();
    }

    bool RoadRageModeScoring::Prepare(s32 liTargetNumTakedowns, u16 luRoadRageExtensionTime)
    {
        // Stub: record the two seeds so a caller that Prepares and then reads the target back
        // sees what it passed in. No mode is armed (mbGameModeActive stays false).
        miTargetNumTakedowns   = liTargetNumTakedowns;
        muRoadRageExtensionTime = luRoadRageExtensionTime;
        return true;
    }

    // Per-frame road-rage scoring. Inert: offline Stunt Run never drives it.
    void RoadRageModeScoring::Update(const void* /* lpRaceCarOutput */, f32 /* lfDeltaTime */)
    {
    }

    bool RoadRageModeScoring::Release()
    {
        return true;
    }

    void RoadRageModeScoring::Destruct()
    {
    }

    // LIVE PATH (ScoringSystem::ClearData). RECOVERED from the inlined block at
    // ScoringSystem::ClearData 0x8232A508..0x8232A534 (Construct 0x8233809C emits the
    // identical block): every member zeroed EXCEPT miTargetNumTakedowns, which the console
    // stores as -1 (`li r27,-1; stw r27,0x4B4C`).
    void RoadRageModeScoring::ClearData()
    {
        miNumTakedownsAchieved                 = 0;
        miNumTakedownsAchievedForNextExtention = 0;
        muRoadRageTriggerExtension             = 0;
        muRoadRageExtensionTime                = 0;
        miTargetNumTakedowns                   = -1;
        miNextTimeIncreaseIndex                = 0;
        mbDamageCriticalMessageNeedToBeSent    = false;
        mbPlayerDamageCritical                 = false;
        mbPlayerCarDestroyed                   = false;
        mbGameModeActive                       = false;
    }

    // LIVE PATH (ScoringSystem takedown finish). Inert: the real body (X360 0x823445D0) counts
    // the takedown, posts the threshold game action and runs the medal / time-extension ladder,
    // all of which is road-rage-only. Doing nothing leaves the counters at zero, which is what
    // an offline stunt run should report.
    void RoadRageModeScoring::IncrementPlayerNumTakedowns(ScoringSystem* /* lpScoringSystem */,
                                                          CgsSystem::Time /* lTime */,
                                                          void* /* lpGameActionQueue */)
    {
    }

    // ---- queries -------------------------------------------------------------------

    // LIVE PATH (ScoringSystem::WriteDataToOutput, inside the IsActive() gate).
    s32 RoadRageModeScoring::GetNumTakedownsAchieved() const
    {
        return miNumTakedownsAchieved;
    }

    // LIVE PATH (ScoringSystem::WriteDataToOutput, inside the IsActive() gate).
    s32 RoadRageModeScoring::GetTargetNumTakedowns() const
    {
        return miTargetNumTakedowns;
    }

    bool RoadRageModeScoring::PlayerCarWasDestroyed() const
    {
        return mbPlayerCarDestroyed;
    }

    bool RoadRageModeScoring::DoesDamageCriticalMessageNeedToBeSent() const
    {
        return mbDamageCriticalMessageNeedToBeSent;
    }

    void RoadRageModeScoring::ResetDamageCriticalMessageFlag()
    {
        mbDamageCriticalMessageNeedToBeSent = false;
    }

    // LIVE PATH (ScoringSystem::WriteDataToOutput). Reporting false is what routes that body
    // into its else-arm, which publishes 0 for both road-rage output scalars -- the correct
    // result for every non-road-rage mode, offline Stunt Run included.
    bool RoadRageModeScoring::IsActive()
    {
        return mbGameModeActive;
    }

    // LIVE PATH (ScoringSystem's medal pass). Stores so a later GetTargetNumTakedowns is
    // self-consistent; nothing else observes it while the mode is inactive.
    void RoadRageModeScoring::SetTakeDownTarget(s32 liTargetNumTakedowns)
    {
        miTargetNumTakedowns = liTargetNumTakedowns;
    }

    bool RoadRageModeScoring::HasBeatenRoadRageTarget() const
    {
        return miNumTakedownsAchieved >= miTargetNumTakedowns;
    }
}
