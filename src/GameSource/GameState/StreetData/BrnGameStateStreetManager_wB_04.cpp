#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/Progression/BrnProgressionManager.h"        // ProgressionManager::OnTrophyUnlock / CheckForSpecialCarUnlocks / GetAchievementManager
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase::OnSetAllRoadRules
#include "SharedClasses/StreetData/BrnStreetData.h"                        // StreetData::GetRoadCount
#include "SharedClasses/StreetData/BrnChallengeData.h"                     // BrnStreetData::ScoreType
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT

// ---------------------------------------------------------------------------
// BrnGameStateStreetManager_wB_04.cpp  --  wave-B group 5 ("tallies+trophies")
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX against the frozen
// StreetManager layout:
//   StreetManager::UpdateTrophyUnlockOnRoadRuleWin  @ 0x82341780
//
// After the local player wins a road rule of the given ScoreType, this re-tallies
// the ruled-road counts (order: show-time/crash, time-trial/time, complete), asserts
// each against the total road count (StreetData::GetRoadCount, X360 read of
// StreetData+0x20 through mpStreetData.operator->), then fans out the road-rule
// trophies via the ProgressionManager (X360 mpProgressionManager @ +0x1D10):
//   CRASH: if all show-time roads ruled -> trophy 7 + OnSetAllRoadRules(CRASH),
//          then trophy 25;
//   TIME:  if all time-trial roads ruled -> trophy 6 + OnSetAllRoadRules(TIME),
//          then trophy 24;
//   any:   if all roads complete -> trophy 8; always trophy 23 +
//          ProgressionManager::CheckForSpecialCarUnlocks().
// OnSetAllRoadRules targets mpProgressionManager->GetAchievementManager() (X360
// *(pm+133432)).
//
// The three ruled-road tallies this function calls
// (GetNumberOfParShowTimeRoadsRuledByLocalPlayer @ 0x8233F230 /
//  GetNumberOfParTimeTrialRoadsRuledByLocalPlayer @ 0x8233F2C0 /
//  GetNumberOfCompleteRoadsRuledByLocalPlayer @ 0x8233F350) are declared on the
// frozen StreetManager header; their bodies are deferred (see funcs_blocked) -- they
// store into ProgressionManager counters that are private with no accessor -- so
// this function only calls them by name.
// ---------------------------------------------------------------------------

namespace BrnGameState
{

// @ 0x82341780.
void StreetManager::UpdateTrophyUnlockOnRoadRuleWin( BrnStreetData::ScoreType leScoreType )
{
    s32 liNumberOfShowTimeRoadsRuled  = GetNumberOfParShowTimeRoadsRuledByLocalPlayer();
    s32 liNumberOfTimeTrailRoadsRuled = GetNumberOfParTimeTrialRoadsRuledByLocalPlayer();
    s32 liNumberOfCompleteRoadsRuled  = GetNumberOfCompleteRoadsRuledByLocalPlayer();

    s32 liTotalNumberOfRoads = GetStreetData()->GetRoadCount();

    CGS_ASSERT( liNumberOfShowTimeRoadsRuled <= liTotalNumberOfRoads,  "liNumberOfShowTimeRoadsRuled <= liTotalNumberOfRoads" );
    CGS_ASSERT( liNumberOfTimeTrailRoadsRuled <= liTotalNumberOfRoads, "liNumberOfTimeTrailRoadsRuled <= liTotalNumberOfRoads" );

    switch ( leScoreType )
    {
    case BrnStreetData::E_SCORE_TYPE_CRASH:
        if ( liNumberOfShowTimeRoadsRuled == liTotalNumberOfRoads )
        {
            mpProgressionManager->OnTrophyUnlock( 7 );
            mpProgressionManager->GetAchievementManager()->OnSetAllRoadRules( BrnStreetData::E_SCORE_TYPE_CRASH );
        }
        mpProgressionManager->OnTrophyUnlock( 25 );
        break;

    case BrnStreetData::E_SCORE_TYPE_TIME:
        if ( liNumberOfTimeTrailRoadsRuled == liTotalNumberOfRoads )
        {
            mpProgressionManager->OnTrophyUnlock( 6 );
            mpProgressionManager->GetAchievementManager()->OnSetAllRoadRules( BrnStreetData::E_SCORE_TYPE_TIME );
        }
        mpProgressionManager->OnTrophyUnlock( 24 );
        break;

    default:
        break;
    }

    if ( liNumberOfCompleteRoadsRuled == liTotalNumberOfRoads )
        mpProgressionManager->OnTrophyUnlock( 8 );

    mpProgressionManager->OnTrophyUnlock( 23 );
    mpProgressionManager->CheckForSpecialCarUnlocks();
}

}   // namespace BrnGameState
