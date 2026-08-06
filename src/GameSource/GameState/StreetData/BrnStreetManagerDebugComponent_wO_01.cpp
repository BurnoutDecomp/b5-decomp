// ===========================================================================
// GameSource/GameState/StreetData/BrnStreetManagerDebugComponent_wO_01.cpp
//   (wave O partfile -- the TU's last un-bodied function)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManagerDebugComponent::Update  @ 0x8234A7E0
//
// This is the per-frame pump StreetManager::Update calls on the embedded debug
// component. It is a pure no-op unless the "Send Scores to Network" debug action
// has armed mbOutputRoadRulesScoresToNetwork; when it has, it republishes the
// local road-rules score summary onto the output buffer's variable game-action
// queue (the same 16-byte type-232 action the three committed StreetManager
// producers post in BrnGameStateStreetManager_wC_03.cpp) and disarms the flag.
//
// The parked note that once blocked this function ("un-named ProgressionManager /
// Profile members Profile+0x1CD1C / Profile+0x1CD30 / ProgressionManager+0x1D0")
// is obsolete: every one of those offsets now has a committed named accessor, and
// the last of the three was MISATTRIBUTED -- 0x1D0 off the ProgressionManager is
// 0x170 (the embedded Profile) + 0x60, i.e. the profile's own
// muTimeStampOfLastRoadRulesDownload (BrnProfile.h:463/:491), which the X360
// folded into a single `lwz r10,0x1D0(r10)`.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"  // StreetManager::GetProgressionManager (:450)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer::GetGameActionQueue / GetGuiOutputQueue
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::GetProfile (:103)
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::GetRoadRulesID / GetNetworkChallengeDownloadTimestamp
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<13312,16>::AddEvent / CgsModule::Event
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

namespace BrnGameState
{
    namespace
    {
        // The 16-byte road-rules score-summary game action (type 232). Copied
        // verbatim from the committed precedent in
        // BrnGameStateStreetManager_wC_03.cpp (an anonymous-namespace type is
        // local to its TU, so this duplicate is a distinct type -- no ODR issue;
        // re-homing both onto the DWARF-named
        // GameStateModuleIO::RoadRulesChallengeScoresAction is a later,
        // both-sides-at-once job). The X360 builds it as three contiguous stack
        // slots -- the profile's road-rules session id (8B), the download
        // timestamp (4B) and the posting StreetManager (4B) -- and hands the
        // leading address to VariableEventQueue<13312,16>::AddEvent(&payload, 232, 16).
        //
        // HOST-vs-CONSOLE (gotcha 1). The X360 immediate is 16 because 32-bit
        // pointers pack mpStreetManager at +12. MEASURED on the x64 gate: the
        // pointer aligns to +16 and the struct is sizeof == 24. Posting the
        // console 16 here would copy the u64, the u32 and 4 bytes of padding and
        // DROP THE MANAGER POINTER ENTIRELY, so the byte count is sizeof(lSummary)
        // and the console value survives only in this comment. The payload is
        // zero-initialised so the alignment hole is never an indeterminate read.
        // The event id 232 IS verbatim from the asm.
        struct RoadRulesScoreSummaryPayload
        {
            u64            mu64RoadRulesID;       // +0
            u32            muDownloadTimestamp;   // +8
            StreetManager* mpStreetManager;       // +12 (X360)
        };
    }

    // -----------------------------------------------------------------------
    // @ 0x8234A7E0. Per-frame pump. The whole body sits under the armed flag
    // (`lbz r11,0x14(r30); beq -> exit`), and the flag is cleared INSIDE the
    // guard (`stb 0 -> 0x14(r30)` after the AddEvent), so the summary is posted
    // exactly once per press of the debug action.
    //
    // MEASURED: the two queue fetches are two separate `bl` to the SAME X360
    // symbol (0x8231D4B8 -- OutputBuffer::GetGameActionQueue, whose exported name
    // is clipped mid-`OutputBuffer` to `BrnGameState::GameStateModuleIO::Ou` (35
    // chars). Identified by its this+4 return and its "Not locked for writing"
    // assert at BrnGameStateModuleIO.h:266; already homed at
    // BrnGameStateModuleIO.h:468.) The first is the assert subject and is
    // spelled with the name its assert string carries; the second feeds AddEvent
    // and is spelled GetGuiOutputQueue(), the concrete-typed twin of that same
    // accessor (BrnGameStateModuleIO.h:492) -- the forward-declared incomplete
    // GameActionQueue cannot be called through. Same function, two spellings,
    // exactly as the wC_03 producers do it.
    // -----------------------------------------------------------------------
    void StreetManagerDebugComponent::Update( GameStateModuleIO::OutputBuffer* lpOutput )
    {
        if ( mbOutputRoadRulesScoresToNetwork )
        {
            CGS_ASSERT( lpOutput != NULL, "lpOutput" );
            CGS_ASSERT( lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()" );
            CGS_ASSERT( mpStreetManager->GetProgressionManager(),
                        "mpStreetManager->mpProgressionManager" );
            // The X360 spells this null-check `addic. r11,pm,0x170` -- the folded
            // `&pm->mProfile != NULL` of the embedded-by-value profile.
            CGS_ASSERT( mpStreetManager->GetProgressionManager()->GetProfile(),
                        "mpStreetManager->mpProgressionManager->GetProfile()" );

            BrnProgression::Profile* lpProfile = mpStreetManager->GetProgressionManager()->GetProfile();

            RoadRulesScoreSummaryPayload lSummary = {};
            // `lwzx` the high half (profile+0x1CD30) and the low half
            // (profile+0x1CD1C), then `extldi/add` -- literally GetRoadRulesID().
            lSummary.mu64RoadRulesID     = lpProfile->GetRoadRulesID();
            lSummary.muDownloadTimestamp = lpProfile->GetNetworkChallengeDownloadTimestamp();
            lSummary.mpStreetManager     = mpStreetManager;

            lpOutput->GetGuiOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lSummary ), 232, sizeof( lSummary ) );

            mbOutputRoadRulesScoresToNetwork = false;
        }
    }
}
