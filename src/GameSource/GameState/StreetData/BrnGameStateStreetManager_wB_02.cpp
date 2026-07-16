#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"  // ChallengeHighScoreEntry::Construct/Copy
#include "SharedClasses/StreetData/BrnChallengeData.h"                    // ChallengePlayerScoreEntry::Construct/Copy
#include "SharedClasses/StreetData/BrnStreetData.h"                       // StreetData::GetRoad / GetChallengeParScore, Road::mId, ChallengeParScoresEntry
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

// ---------------------------------------------------------------------------
// BrnGameStateStreetManager_wB_02.cpp  --  wave-B group 3 ("get-scores")
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX against the frozen
// StreetManager layout:
//   StreetManager::GetChallengeUserScore       @ 0x82335E08
//   StreetManager::GetChallengeFriendHighScore @ 0x82335F30
//
// (GetChallengeParScore @ 0x82336168 is deferred: the frozen BrnStreetData.h models
// BrnStreetData::ChallengeParScoresEntry as a plain POD with no Copy() accessor, so
// its faithful `ChallengeParScoresEntry::Copy(lpData, ...)` store cannot be spelled
// without editing a frozen header -- see funcs_blocked.)
//
// Both copy a challenge record out of a live table into the caller's entry,
// optionally remapping a road index through KAA_SAVE_GAME_CHALLENGE_ROAD_IDS
// (the inlined qword_82029FA0 scan). mpStreetData->... is the committed
// ResourcePtr operator-> (X360 StreetData_::oper on &mpStreetData, guest +0x1CC8).
// ---------------------------------------------------------------------------

namespace BrnGameState
{

// @ 0x82335E08. Copy the local player's ChallengePlayerScoreEntry for a slot into
// lpData (constructed empty first). When lbByRoadIndex is set, liIndex is first the
// road index and is remapped to its save-game challenge slot via the road id scan;
// a road with no save-game slot leaves lpData at its Construct'd default.
void StreetManager::GetChallengeUserScore( BrnStreetData::ChallengeIndex liIndex,
                                           BrnStreetData::ChallengePlayerScoreEntry* lpData,
                                           bool lbByRoadIndex ) const
{
    CGS_ASSERT( lpData, "lpData" );

    lpData->Construct();

    if ( lbByRoadIndex )
    {
        const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liIndex );

        liIndex = 0;
        const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
        while ( *lpSlotRoadId != lpRoad->GetId() )
        {
            ++lpSlotRoadId;
            ++liIndex;
            if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
            {
                return;
            }
        }
    }

    if ( liIndex >= 0 )
    {
        CGS_ASSERT( liIndex < BrnGameState::KI_MAX_CHALLENGES,
                    "liChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );

        const BrnStreetData::ChallengePlayerScoreEntry* lpChallenge = &maChallengeData[liIndex];
        CGS_ASSERT( lpChallenge, "lpChallenge" );
        lpData->Copy( lpChallenge );
    }
}

// @ 0x82335F30. Friend-table twin of GetChallengeUserScore over
// maNetworkChallengeData. NOTE the index bounds asserts fire up front, before the
// optional by-road-index remap.
void StreetManager::GetChallengeFriendHighScore( BrnStreetData::ChallengeIndex liIndex,
                                                 BrnStreetData::ChallengeHighScoreEntry* lpData,
                                                 bool lbByRoadIndex ) const
{
    CGS_ASSERT( liIndex < BrnGameState::KI_MAX_CHALLENGES,
                "liIndex < BrnGameState::KI_MAX_CHALLENGES" );
    CGS_ASSERT( liIndex >= 0, "liIndex >= 0" );
    CGS_ASSERT( lpData, "lpData" );

    lpData->Construct();

    BrnStreetData::ChallengeIndex liSlotIndex = liIndex;
    if ( lbByRoadIndex )
    {
        const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liIndex );

        liSlotIndex = 0;
        const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
        while ( *lpSlotRoadId != lpRoad->GetId() )
        {
            ++lpSlotRoadId;
            ++liSlotIndex;
            if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
            {
                return;
            }
        }
    }

    if ( liSlotIndex >= 0 )
    {
        const BrnStreetData::ChallengeHighScoreEntry* lpChallenge = &maNetworkChallengeData[liSlotIndex];
        CGS_ASSERT( lpChallenge, "lpChallenge" );
        lpData->Copy( lpChallenge );
    }
}

} // namespace BrnGameState
