// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_07.cpp
//   (wave B partfile -- group 7 "server-sync")
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::UpdateFriendHighScores           @ 0x82348C80
//
// Store-for-store against the frozen StreetManager layout header; every store /
// branch / early-out has an asm counterpart, and every offset the X360 folds
// (GetStreetData() == mpStreetData.operator->, the maNetworkChallengeData row
// stride, the save-game challenge-slot scan, the +0x1CC5 mbFirstDownload store)
// is mapped to its committed named member -- no raw `*(p+off)` casts.
//
// The other two group-7 ledger methods are BLOCKED on missing public accessors
// (see funcs_blocked in the structured result):
//   * FillInRoadRulesQuery @ 0x823365A8 -- its final store reads
//     BrnProgression::ProgressionManager::miNumberOfNumberOfCompleteRoadRulesRuledByPlayer
//     (the `*(pm + 133464) >= 64` owns-all-roads flag). That member is PRIVATE in
//     BrnProgressionManager.h with no public getter, and StreetManager is not a
//     friend of ProgressionManager -- unreachable without editing the frozen header.
//   * UpdateUserScoresFromServerRecords @ 0x82348FC0 -- assembles the profile's
//     road-rules id from BrnProgression::Profile::muRoadRulesIDHighBits /
//     muRoadRulesIDLowBits (both the per-record `mu64RoadRulesID` filter and the
//     type-232 score-summary payload). Both members are PRIVATE in BrnProfile.h
//     with no public getter, and StreetManager is not a friend of Profile
//     (identical to the ProcessConnectedOnlineEvent blocker reported in wB_05).
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

#include "GameSource/GameState/BrnGameStateModuleIO.h"                    // OutputBuffer::GetGameActionQueue / GetGuiOutputQueue
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"           // RoadRulesManager::GetCurrentRoadID
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"   // ChallengeHighScoreEntry Get/Set/ClearScore
#include "SharedClasses/StreetData/BrnChallengeData.h"                    // ChallengeData::ContainsData, ScoreType, operator++
#include "SharedClasses/StreetData/BrnStreetData.h"                       // StreetData::GetRoad / Road::GetId
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::PlayerName
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"               // RoadRulesDownloadEvent / RoadRulesMessageData
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                  // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"             // BaseEventQueue<T>::GetLength/GetEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

namespace BrnGameState
{
    namespace
    {
        // The road-score record game action (type 280, 104 bytes): the X360 posts
        // three contiguous stack locals -- the friend high-score entry, the local
        // player's score entry, and the road id -- as one 104-byte payload. Modelled
        // as the struct that layout represents (56 + 40 + 8 == 104).
        struct RoadRulesScoreRecordAction
        {
            BrnStreetData::ChallengeHighScoreEntry   mFriendHighScore;   // +0  (56B)
            BrnStreetData::ChallengePlayerScoreEntry mUserScore;         // +56 (40B)
            ::CgsID                                  mRoadID;            // +96 (8B)
        };
    }

    // -----------------------------------------------------------------------
    // @ 0x82348C80. Apply a batch of downloaded friend high-score records to the
    // network challenge table. For every record in the downloaded-events queue:
    // build a ChallengeHighScoreEntry from the record's names/scores, and -- for
    // roads that carry a save-game challenge slot -- merge it into the network
    // table row (per score type: adopt the downloaded score, else clear it). When
    // the merge produced a new high score AND the record is for the road the player
    // is currently on, re-post the road-score game action (type 280).
    // -----------------------------------------------------------------------
    void StreetManager::UpdateFriendHighScores( const void* lpDownloadedQueue,
                                                GameStateModuleIO::OutputBuffer* lpOutput )
    {
        CGS_ASSERT( lpOutput, "lpOutput" );

        // X360 builds an empty scratch player name up front (Construct("") then a
        // redundant macName[0] = 0). It is unreferenced past construction -- kept for
        // fidelity with the retail body.
        CgsNetwork::PlayerName lEmptyName;
        lEmptyName.Construct( "" );
        lEmptyName.macName[0] = '\0';

        const CgsModule::EventQueue<BrnNetwork::RoadRulesDownloadEvent, 40>* lpQueue =
            static_cast<const CgsModule::EventQueue<BrnNetwork::RoadRulesDownloadEvent, 40>*>( lpDownloadedQueue );

        for ( s32 liRecord = 0; liRecord < lpQueue->GetLength(); ++liRecord )
        {
            // 56-byte record copy (X360 copies the queue element to the stack before use).
            BrnNetwork::RoadRulesDownloadEvent lDownloadRecord = lpQueue->GetEvent( liRecord );

            const BrnStreetData::RoadIndex liRoadIndex = lDownloadRecord.mRoadRulesData.mChallengeIndex;
            const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liRoadIndex );   // carries its own bounds assert

            // Inlined save-game challenge-slot scan (the qword_82029FA0 walk): a road
            // that has no persisted challenge slot skips the merge but still runs the
            // last-slot bookkeeping below.
            bool lbRoadHasSaveSlot = false;
            const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
            while ( true )
            {
                if ( *lpSlotRoadId == lpRoad->GetId() )
                {
                    lbRoadHasSaveSlot = true;
                    break;
                }
                ++lpSlotRoadId;
                if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
                {
                    break;
                }
            }

            if ( lbRoadHasSaveSlot )
            {
                BrnStreetData::ChallengeHighScoreEntry lDownloadedEntry;
                CreateHighScoreEntryFromDown(
                    &lDownloadedEntry, 0,
                    reinterpret_cast<const CgsNetwork::PlayerName*>( lDownloadRecord.maPlayerNames ),
                    lDownloadRecord.mRoadRulesData.maScores );

                // X360 re-materializes the road count AFTER building the downloaded entry
                // (0x82348E18-0x82348E28) and early-outs the whole merge, the (280,104)
                // post AND the last-slot bookkeeping below for an out-of-range index
                // (reachable via the continue-on-assert path of the GetRoad bounds check).
                if ( liRoadIndex >= mpStreetData->GetRoadCount() )
                {
                    continue;
                }

                const bool lbIsNewHighScore = CheckForNewHighScore( liRoadIndex, &lDownloadedEntry );

                for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                      leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
                      leScoreType++ )
                {
                    if ( lDownloadedEntry.ContainsData( leScoreType ) )
                    {
                        int32_t                liScore;
                        CgsNetwork::PlayerName lScoreName;
                        lDownloadedEntry.GetScore( leScoreType, &liScore, &lScoreName );
                        maNetworkChallengeData[liRoadIndex].SetScore( leScoreType, liScore, &lScoreName );
                    }
                    else
                    {
                        maNetworkChallengeData[liRoadIndex].ClearScore( leScoreType );
                    }
                }

                if ( lbIsNewHighScore )
                {
                    CGS_ASSERT( mpRoadRulesManager, "mpRoadRulesManager" );

                    if ( mpRoadRulesManager->GetCurrentRoadID() == KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liRoadIndex] )
                    {
                        RoadRulesScoreRecordAction lAction;
                        GetChallengeUserScore( liRoadIndex, &lAction.mUserScore, false );
                        GetChallengeFriendHighScore( liRoadIndex, &lAction.mFriendHighScore, false );
                        lAction.mRoadID = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liRoadIndex];

                        CGS_ASSERT( lpOutput, "lpOutput" );
                        CGS_ASSERT( lpOutput->GetGameActionQueue() != NULL, "lpOutput->GetGameActionQueue()" );

                        lpOutput->GetGuiOutputQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>( &lAction ), 280, 104 );
                    }
                }
            }

            // The last challenge slot having been reached clears the first-download flag.
            if ( liRoadIndex == 63 )
            {
                mbFirstDownload = false;
            }
        }
    }
}
