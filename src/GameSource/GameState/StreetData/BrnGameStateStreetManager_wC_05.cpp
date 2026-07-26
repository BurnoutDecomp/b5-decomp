// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_05.cpp
//   (wave C partfile -- group 5 "tiny trio")
//
// Faithful de-optimisation of BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::GetChallengeParScore                    @ 0x82336168
//   BrnGameState::StreetManager::GetNumberOfParShowTimeRoadsRuledByLocalPlayer
//                                                                        @ 0x8233F230
//   BrnGameState::StreetManager::GetNumberOfParTimeTrialRoadsRuledByLocalPlayer
//                                                                        @ 0x8233F2C0
//
// Every asm store / branch / call has a counterpart here; all state is reached
// through the frozen-header named members (BrnGameStateStreetManager.h), never a
// raw-offset cast:
//   * `mpStreetData->` == the asm's `addi rX, this, 0x1CC8` +
//     BrnStreetData::StreetData_::oper (ResourcePtr<StreetData>::operator->);
//     the `lwz r11, 0x20(r3)` that follows it is the inlined
//     StreetData::GetRoadCount (miRoadCount @ StreetData+0x20).
//   * the tally writebacks target the ProgressionManager (this+0x1D10) counters
//     +133456 (0x20950, par-crash) and +133460 (0x20954, par-time) through the
//     committed Get/SetNumberOfPar{Crash,Time}RoadRulesRuledByPlayer accessors.
//
// TRAP kept faithful: the road count is re-materialised (operator-> + load) on
// EVERY loop-condition evaluation in the X360 code, so GetRoadCount() stays in
// the loop condition and is not hoisted into a local.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/Progression/BrnProgressionManager.h"  // ProgressionManager par-rules counters
#include "SharedClasses/StreetData/BrnStreetData.h"                  // StreetData::GetRoadCount / GetChallengeParScore
#include "SharedClasses/StreetData/BrnChallengeData.h"               // ChallengeParScoresEntry::Copy / ScoreType
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT

namespace BrnGameState
{

// @ 0x82336168. Copy the compiled par-score record for a challenge road out of the
// street data. The bounds check belongs to StreetData::GetChallengeParScore (its own
// baked assert); this function only owns the null-destination assert (baked line 3750).
void StreetManager::GetChallengeParScore( BrnStreetData::ChallengeIndex liIndex,
                                          BrnStreetData::ChallengeParScoresEntry* lpData ) const
{
    CGS_ASSERT( lpData, "lpData" );

    lpData->Copy( mpStreetData->GetChallengeParScore( liIndex ) );
}

// @ 0x8233F230. Tally the roads whose CRASH ("show time") par score the local player
// has beaten, then raise the ProgressionManager's par-crash counter to that tally --
// the X360 store is guarded by a signed greater-than compare, so the counter only ever
// grows here.
s32 StreetManager::GetNumberOfParShowTimeRoadsRuledByLocalPlayer()
{
    s32 liNumberOfRoadsRuled = 0;

    for ( s32 liRoadIndex = 0; liRoadIndex < GetStreetData()->GetRoadCount(); ++liRoadIndex )
    {
        if ( HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_CRASH )
             == E_ROAD_RULE_COMPLETION_STATUS_BEATEN )
        {
            ++liNumberOfRoadsRuled;
        }
    }

    if ( liNumberOfRoadsRuled >
         static_cast<s32>( mpProgressionManager->GetNumberOfParCrashRoadRulesRuledByPlayer() ) )
    {
        mpProgressionManager->SetNumberOfParCrashRoadRulesRuledByPlayer( liNumberOfRoadsRuled );
    }

    return liNumberOfRoadsRuled;
}

// @ 0x8233F2C0. TIME twin of the above, maxing the ProgressionManager's par-time
// counter (+133460).
s32 StreetManager::GetNumberOfParTimeTrialRoadsRuledByLocalPlayer()
{
    s32 liNumberOfRoadsRuled = 0;

    for ( s32 liRoadIndex = 0; liRoadIndex < GetStreetData()->GetRoadCount(); ++liRoadIndex )
    {
        if ( HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_TIME )
             == E_ROAD_RULE_COMPLETION_STATUS_BEATEN )
        {
            ++liNumberOfRoadsRuled;
        }
    }

    if ( liNumberOfRoadsRuled >
         static_cast<s32>( mpProgressionManager->GetNumberOfParTimeRoadRulesRuledByPlayer() ) )
    {
        mpProgressionManager->SetNumberOfParTimeRoadRulesRuledByPlayer( liNumberOfRoadsRuled );
    }

    return liNumberOfRoadsRuled;
}

}
