// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_04.cpp
//   (wave C partfile -- group 4 "query + rivals + complete tally")
//
// Faithful de-optimisation of BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::FillInRoadRulesQuery                    @ 0x823365A8
//   BrnGameState::StreetManager::FindRivalsByDistrict                    @ 0x82336360
//   BrnGameState::StreetManager::GetNumberOfCompleteRoadsRuledByLocalPlayer
//                                                                        @ 0x8233F350
//
// Every asm store / branch / call has a counterpart here; all state is reached
// through the frozen-header named members (BrnGameStateStreetManager.h), never a
// raw-offset cast:
//   * `GetStreetData()` == the asm's `addi rX, this, 0x1CC8` +
//     BrnStreetData::StreetData_::oper/operator-_ (ResourcePtr<StreetData>::operator->);
//     the `lwz r11, 0x20(r3)` that follows it is the inlined StreetData::GetRoadCount
//     (miRoadCount @ StreetData+0x20), and the
//     `lwz r11,0x10(rX) / add / ld 0x10(r11)` triple is the inlined
//     StreetData::GetRoad(i)->GetId() (road table @ +0x10, 64-byte stride, mId @ +0x10)
//     -- the bounds assert baked at BrnStreetData.h:621 belongs to GetRoad, so it is
//     not duplicated here.
//   * the ProgressionManager (this+0x1D10) counter at +133464 (0x20958) is reached
//     through the committed Get/SetNumberOfCompleteRoadRulesRuledByPlayer accessors.
//   * `mpProgressionManager->GetProgressionData()` == the asm's null-checked
//     ResourcePtr<ProgressionData> read at pm+133348 (0x208E4); the rival table/count
//     (ProgressionData +0x28/+0x2C) is reached through GetRival/GetRivalCount, and the
//     baked "liIndex < miRivalCount" assert (BrnProgressionData.h:460) belongs to
//     GetRival, so it is not duplicated here either.
//
// TRAPS kept faithful:
//   * every count the X360 re-materialises on each loop-condition evaluation
//     (RoadRulesBatchQueryAction::miNumRoads, StreetData::GetRoadCount(),
//     ProgressionData::GetRivalCount()) stays IN the loop condition -- not hoisted.
//   * FillInRoadRulesQuery stores miNumRoads BEFORE the <= KI_MAX_CHALLENGES assert
//     (asm: `stw r11, 0x200(r26)` at 0x823365D0 precedes the `ble`).
//   * FindRivalsByDistrict dereferences the progression data without a null guard --
//     the X360 loads GetRivalCount off a possibly-NULL pointer (0x823363AC reads
//     0x2C(r28) with r28 == 0 on the null path); reproduced as-is, no added check.
//   * `(_cntlzw(HasPlayerBeaten...Score(...) - 1) & 0x20) != 0` is the compiler's
//     "== 1" test, i.e. == E_ROAD_RULE_COMPLETION_STATUS_BEATEN.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameActions.h"                     // GameStateModuleIO::RoadRulesBatchQueryAction
#include "GameSource/GameState/Progression/BrnProgressionManager.h"  // ProgressionManager::GetProgressionData / complete-rules counter
#include "SharedClasses/Progression/BrnProgressionData.h"            // ProgressionData::GetRival / GetRivalCount
#include "SharedClasses/Progression/BrnRival.h"                      // Rival::GetId / GetDistrict
#include "SharedClasses/StreetData/BrnStreetData.h"                  // StreetData::GetRoadCount / GetRoad / Road::GetId
#include "SharedClasses/StreetData/BrnChallengeData.h"               // BrnStreetData::ScoreType
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT

namespace BrnGameState
{

// @ 0x823365A8. Fill the road-rules batch query action the GUI issues: the road count,
// one road id per road, the four per-road "beaten" booleans (par TIME/CRASH then friend
// TIME/CRASH -- the X360 issues the four calls in exactly that order), and finally the
// offline owns-all-roads flag taken from the ProgressionManager's complete-rules tally.
void StreetManager::FillInRoadRulesQuery( GameStateModuleIO::RoadRulesBatchQueryAction* lpRulesQueryAction )
{
    lpRulesQueryAction->miNumRoads = GetStreetData()->GetRoadCount();

    CGS_ASSERT( lpRulesQueryAction->miNumRoads <= KI_MAX_CHALLENGES,
                "lpRulesQueryAction->miNumRoads <= KI_MAX_CHALLENGES" );

    for ( s32 liRoadIndex = 0; liRoadIndex < lpRulesQueryAction->miNumRoads; ++liRoadIndex )
    {
        lpRulesQueryAction->maRoadIds[liRoadIndex] = GetStreetData()->GetRoad( liRoadIndex )->GetId();

        lpRulesQueryAction->mabPlayerBeatenParTime[liRoadIndex] =
            HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_TIME )
            == E_ROAD_RULE_COMPLETION_STATUS_BEATEN;

        lpRulesQueryAction->mabPlayerBeatenParCrash[liRoadIndex] =
            HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_CRASH )
            == E_ROAD_RULE_COMPLETION_STATUS_BEATEN;

        lpRulesQueryAction->mabPlayerBestOnlineTime[liRoadIndex] =
            HasPlayerBeatenFriendScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_TIME )
            == E_ROAD_RULE_COMPLETION_STATUS_BEATEN;

        lpRulesQueryAction->mabPlayerBestOnlineCrash[liRoadIndex] =
            HasPlayerBeatenFriendScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_CRASH )
            == E_ROAD_RULE_COMPLETION_STATUS_BEATEN;
    }

    // Signed compare (cmpwi cr6, r11, 0x40 / bge) of the ProgressionManager's
    // complete-road-rules tally against KI_MAX_CHALLENGES.
    lpRulesQueryAction->mbPlayerOwnsAllRoadsOffline =
        static_cast<s32>( mpProgressionManager->GetNumberOfCompleteRoadRulesRuledByPlayer() ) >= KI_MAX_CHALLENGES;
}

// @ 0x82336360. Collect up to liMaxRivals rival ids whose district matches leDistrict
// from the progression data's rival table, returning how many were written.
// The found-count cut-out is tested at the TOP of each iteration, before the rival is
// fetched, so the last iteration never reaches GetRival's bounds assert.
s32 StreetManager::FindRivalsByDistrict( s32 leDistrict, ::CgsID* lpaRivalIds, s32 liMaxRivals )
{
    const BrnProgression::ProgressionData* lpProgressionData = mpProgressionManager->GetProgressionData();

    s32 liNumberOfRivalsFound = 0;

    for ( s32 liRivalIndex = 0; liRivalIndex < lpProgressionData->GetRivalCount(); ++liRivalIndex )
    {
        if ( liNumberOfRivalsFound >= liMaxRivals )
        {
            break;
        }

        const BrnProgression::Rival* lpRival = lpProgressionData->GetRival( liRivalIndex );

        if ( static_cast<s32>( lpRival->GetDistrict() ) == leDistrict )
        {
            lpaRivalIds[liNumberOfRivalsFound] = lpRival->GetId();
            ++liNumberOfRivalsFound;
        }
    }

    return liNumberOfRivalsFound;
}

// @ 0x8233F350. Tally the roads whose par score the local player has beaten for BOTH
// score types (a "complete" road rule), then mirror the tally into the ProgressionManager
// counter (+133464). Unlike the two single-type tallies this store is UNCONDITIONAL --
// the X360 has no max-guard here, so the counter can go down.
s32 StreetManager::GetNumberOfCompleteRoadsRuledByLocalPlayer()
{
    s32 liNumberOfRoadsRuled = 0;

    for ( s32 liRoadIndex = 0; liRoadIndex < GetStreetData()->GetRoadCount(); ++liRoadIndex )
    {
        if ( HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_TIME )
             == E_ROAD_RULE_COMPLETION_STATUS_BEATEN
             && HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_CRASH )
                == E_ROAD_RULE_COMPLETION_STATUS_BEATEN )
        {
            ++liNumberOfRoadsRuled;
        }
    }

    mpProgressionManager->SetNumberOfCompleteRoadRulesRuledByPlayer( liNumberOfRoadsRuled );

    return liNumberOfRoadsRuled;
}

}
