// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wR_10.cpp
//   (road-rules wave partfile -- the friend high-score table)
//
//   StreetManager::CheckForNewHighScore
//   StreetManager::ClearAllChallengeDataForBuddy
//   StreetManager::SetChallengeFriendHighScore
//
// All three work on maNetworkChallengeData, the per-road friend high-score
// table (64 ChallengeHighScoreEntry records indexed by save-game challenge slot).
//
// Container asserts: the console inlines FastBitArray<2>::SetBit / IsBitSet,
// BitArray<64u>::SetBit and the Array<BufferedNewHighScore,5> count check at
// each call site, together with their streamed "Index N is out of range" /
// "Index: N, Number of bits: 64" / "Array used before Construct/Clear was
// called" assert blocks. Those belong to the container methods, which are
// called here by name instead of being re-expanded (the ChallengeManager
// convention). The per-pass "leEnumIndex <= E_SCORE_TYPE_COUNT" assert is the
// BrnStreetData::operator++ guard, reached through the committed operator.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"   // ChallengeHighScoreEntry GetScore/UpdateEntry/ClearScore/Copy/IsWholeChallengeOwnedBySamePlayer
#include "SharedClasses/StreetData/BrnChallengeData.h"                     // ScoreType, ContainsData, operator++
#include "SharedClasses/StreetData/BrnStreetData.h"                        // StreetData::GetRoad, Road::GetId
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"            // RoadRulesManager::GetCurrentRoadID
#include "GameSource/GameState/BrnCgsPlayerName.h"                         // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"             // CgsContainers::FastBitArray<2>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                 // CgsContainers::BitArray<64u>::SetBit
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"                // CgsNetwork::UsernameCompare
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT

#include <string.h>   // _strnicmp (the console's strnicmp)

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// Merge a friend's downloaded score record into the stored friend high score
// for one save-game challenge slot, and queue every score type it beats.
//
// First records which score types of the current high score belong to the
// local player (owner name == KAC_LOCAL_PLAYER_NAME_TEXT, case-insensitive,
// 16 chars). Then merges lpNewScore into a local copy of the current entry;
// for every score type the merge took, a BufferedNewHighScore is built (owner
// name, the slot's road id, score type, whole-road ownership, and whether the
// local player held that score before), any stale buffered entry for the same
// road + score type is dropped, and the new one is appended -- or, when the
// buffer is full, the overflow flag is raised and a lost local-player score is
// counted (only once the first download is over). The slot's bit in that score
// type's road-rules-changed array is set either way.
//
// Returns true when at least one score type was beaten. The merged entry is a
// local: the callers write the friend table themselves.
// ----------------------------------------------------------------------------
bool StreetManager::CheckForNewHighScore( s32 liSaveGameChallengeIndex,
                                          BrnStreetData::ChallengeHighScoreEntry* lpNewScore )
{
    BrnStreetData::ChallengeHighScoreEntry lCurrentHighScoreEntry;
    CgsNetwork::PlayerName                 lPlayerName;
    int32_t                                liScore;
    BrnStreetData::ScoreType               leScoreType;
    CgsContainers::FastBitArray<2>         lNewHighScoreTypes;
    CgsContainers::FastBitArray<2>         lScoresRuledByLocalPlayer;
    bool                                   lbNewHighScore;

    GetHighScoreEntry( liSaveGameChallengeIndex, &lCurrentHighScoreEntry, false );

    lScoresRuledByLocalPlayer.UnSetAll();

    for ( leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        if ( lCurrentHighScoreEntry.ContainsData( leScoreType ) )
        {
            lCurrentHighScoreEntry.GetScore( leScoreType, &liScore, &lPlayerName );

            if ( _strnicmp( lPlayerName.macName, KAC_LOCAL_PLAYER_NAME_TEXT, CgsNetwork::KI_USERNAME_LENGTH ) == 0 )
            {
                lScoresRuledByLocalPlayer.SetBit( static_cast<u32>( leScoreType ) );
            }
        }
    }

    lNewHighScoreTypes.UnSetAll();
    lbNewHighScore = false;

    if ( lCurrentHighScoreEntry.UpdateEntry( lpNewScore, &lNewHighScoreTypes ) )
    {
        for ( leScoreType = BrnStreetData::E_SCORE_TYPE_START;
              leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
              leScoreType++ )
        {
            if ( lNewHighScoreTypes.IsBitSet( static_cast<u32>( leScoreType ) ) )
            {
                BufferedNewHighScore lNewHighScoreToBuffer;

                lCurrentHighScoreEntry.GetScore( leScoreType, &liScore, &lPlayerName );
                lNewHighScoreToBuffer.mPlayerName.Construct( lPlayerName.macName );
                lNewHighScoreToBuffer.meScoreType = leScoreType;
                lNewHighScoreToBuffer.mRoadID     = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liSaveGameChallengeIndex];
                lNewHighScoreToBuffer.mbIsRoadWhollyOwnedByOnePlayer =
                    lCurrentHighScoreEntry.IsWholeChallengeOwnedBySamePlayer();
                lNewHighScoreToBuffer.mbWasRoadRuledByPlayerBefore =
                    lScoresRuledByLocalPlayer.IsBitSet( static_cast<u32>( leScoreType ) );

                mNewHighScoreBuffer.EraseMatchingEntries( lNewHighScoreToBuffer.mRoadID, leScoreType );

                if ( mNewHighScoreBuffer.IsFull() )
                {
                    mbTooManyHighScoresToBuffer = true;

                    if ( lNewHighScoreToBuffer.mbWasRoadRuledByPlayerBefore && !mbFirstDownload )
                    {
                        ++miNumScoresLost;
                    }
                }
                else
                {
                    mNewHighScoreBuffer.Append( lNewHighScoreToBuffer );
                }

                maRoadRulesChangedBitArrays[leScoreType].SetBit( static_cast<u32>( liSaveGameChallengeIndex ) );

                lbNewHighScore = true;
            }
        }
    }

    return lbNewHighScore;
}

// ----------------------------------------------------------------------------
// A buddy left the friends list: blank every friend high score they own.
//
// Walks all 64 friend high-score slots that hold any score; for each score type
// owned by lpBuddyName (lobby-name compare) the score is cleared. Returns true
// when one of the cleared scores sat on the road the player is currently on
// (the slot index is used as the road index here, as the console does), so the
// caller can re-post that road's score record to the GUI.
// ----------------------------------------------------------------------------
bool StreetManager::ClearAllChallengeDataForBuddy( const CgsNetwork::PlayerName* lpBuddyName )
{
    bool lbClearedScoreForCurrentRoad = false;

    for ( BrnStreetData::ChallengeIndex lChallengeIndex = 0;
          lChallengeIndex < KI_MAX_CHALLENGES;
          ++lChallengeIndex )
    {
        BrnStreetData::ChallengeHighScoreEntry& lrEntry = maNetworkChallengeData[lChallengeIndex];

        if ( lrEntry.ContainsData( BrnStreetData::E_SCORE_TYPE_COUNT ) )
        {
            for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                  leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
                  leScoreType++ )
            {
                if ( lrEntry.ContainsData( leScoreType ) )
                {
                    CgsNetwork::PlayerName lPlayerName;
                    int32_t                liScore;

                    lrEntry.GetScore( leScoreType, &liScore, &lPlayerName );

                    if ( CgsNetwork::UsernameCompare( lPlayerName.macName, lpBuddyName->macName ) == 0 )
                    {
                        // The road id is read before the current road id is fetched.
                        const ::CgsID lRoadId = mpStreetData->GetRoad( lChallengeIndex )->GetId();

                        if ( mpRoadRulesManager->GetCurrentRoadID() == lRoadId )
                        {
                            lbClearedScoreForCurrentRoad = true;
                        }

                        lrEntry.ClearScore( leScoreType );
                    }
                }
            }
        }
    }

    return lbClearedScoreForCurrentRoad;
}

// ----------------------------------------------------------------------------
// Overwrite one friend high-score slot with lpData.
// ----------------------------------------------------------------------------
void StreetManager::SetChallengeFriendHighScore( BrnStreetData::ChallengeIndex liIndex,
                                                 const BrnStreetData::ChallengeHighScoreEntry* lpData )
{
    CGS_ASSERT( liIndex < BrnGameState::KI_MAX_CHALLENGES, "liIndex < BrnGameState::KI_MAX_CHALLENGES" );
    CGS_ASSERT( liIndex >= 0, "liIndex >= 0" );
    CGS_ASSERT( lpData, "lpData" );

    maNetworkChallengeData[liIndex].Copy( lpData );
}

} // namespace BrnGameState
