#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameEvents.h"                    // BuddyRemovedEvent / OnlineRoadRulesPersonalBestRecvEvent
#include "GameSource/GameState/BrnGameActions.h"                   // RoadRulesUpdateTargetScoreAction (action 280)
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // GameStateModuleIO::OutputBuffer (GetGameActionQueue / GetGuiOutputQueue)
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"    // RoadRulesManager::GetCurrentRoadID
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<13312,16>::AddEvent / CgsModule::Event
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT

// ---------------------------------------------------------------------------
// BrnGameStateStreetManager_wB_06.cpp  --  wave-B group 7 ("online-big")
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX against the frozen
// StreetManager layout:
//   StreetManager::ProcessNetworkHighScoreEvent @ 0x82349F10
//   StreetManager::ProcessBuddyRemoved          @ 0x8234A5A8
//
// Both handlers scan KAA_SAVE_GAME_CHALLENGE_ROAD_IDS for the road's persisted
// challenge slot (the inlined qword_82029FA0 table walk) and, when the current
// road's records changed, post the 104-byte road-score record game action
// (type 280) onto the OutputBuffer game-action queue. mpStreetData->... is the
// committed ResourcePtr operator-> (X360 StreetData_::oper on &mpStreetData).
// ---------------------------------------------------------------------------

namespace BrnGameState
{

// @ 0x82349F10. Applies a downloaded online personal-best record to the friend
// high-score table. Copies the 56-byte score off the event, resolves the road's
// save-game challenge slot, and -- when the record is a friend's best that becomes
// a new high score for the road the player is currently on -- posts the road-score
// game action (type 280) so the GUI refreshes.
void StreetManager::ProcessNetworkHighScoreEvent(
        GameStateModuleIO::OutputBuffer* lpOutput,
        const GameStateModuleIO::OnlineRoadRulesPersonalBestRecvEvent* lpPersonalBestEvent )
{
    CGS_ASSERT( lpPersonalBestEvent, "lpPersonalBestEvent" );

    BrnStreetData::ChallengeHighScoreEntry lPersonalBest = lpPersonalBestEvent->mPersonalBestScore;

    const BrnStreetData::ChallengeIndex liChallengeIndex = lpPersonalBestEvent->mPersonalBestChallengeIndex;
    CGS_ASSERT( liChallengeIndex < BrnGameState::KI_MAX_CHALLENGES && liChallengeIndex >= 0,
                "lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES && lChallengeIndex >= 0" );

    const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liChallengeIndex );

    BrnStreetData::ChallengeIndex liSlotIndex = 0;
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

    if ( liSlotIndex >= 0 )
    {
        if ( lpPersonalBestEvent->mbWasPBByFriend )
        {
            const bool lbIsNewHighScore = CheckForNewHighScore( liSlotIndex, &lPersonalBest );

            BrnStreetData::ChallengeHighScoreEntry lFriendEntry;
            lFriendEntry.Construct();
            GetChallengeFriendHighScore( liSlotIndex, &lFriendEntry, false );
            lFriendEntry.UpdateEntry( &lPersonalBest, nullptr );
            SetChallengeFriendHighScore( liSlotIndex, &lFriendEntry );

            if ( lbIsNewHighScore )
            {
                CGS_ASSERT( mpRoadRulesManager, "mpRoadRulesManager" );

                if ( mpRoadRulesManager->GetCurrentRoadID() == KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liSlotIndex] )
                {
                    GameStateModuleIO::RoadRulesUpdateTargetScoreAction lRecord;
                    GetChallengeUserScore( liSlotIndex, &lRecord.mUserScores, false );
                    GetChallengeFriendHighScore( liSlotIndex, &lRecord.mFriendScores, false );
                    lRecord.mRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liSlotIndex];

                    CGS_ASSERT( lpOutput, "lpOutput" );
                    CGS_ASSERT( lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()" );

                    CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
                    lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lRecord ),
                       GameStateModuleIO::E_ACTION_ROAD_RULES_UPDATE_TARGET_ROAD_SCORE, sizeof( lRecord ) );
                }
            }
        }
    }
}

// @ 0x8234A5A8. A buddy was removed from the friends list. Scrub their recorded
// scores; if that changed the current road's records, re-post the road-score game
// action (type 280) so the GUI drops the buddy's entry. Always post the
// buddy-scrubbed flag action (type 55) afterwards.
void StreetManager::ProcessBuddyRemoved(
        GameStateModuleIO::OutputBuffer* lpOutput,
        const GameStateModuleIO::BuddyRemovedEvent* lpBuddyRemovedEvent )
{
    CGS_ASSERT( lpOutput, "lpOutput" );
    CGS_ASSERT( lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()" );

    const bool lbScoresCleared = ClearAllChallengeDataForBuddy( &lpBuddyRemovedEvent->mRemovedBuddyName );

    CGS_ASSERT( mpRoadRulesManager, "mpRoadRulesManager" );

    if ( lbScoresCleared )
    {
        const ::CgsID lCurrentRoadID = mpRoadRulesManager->GetCurrentRoadID();

        BrnStreetData::ChallengeIndex liRoadIndex = 0;
        const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
        while ( *lpSlotRoadId != lCurrentRoadID )
        {
            ++lpSlotRoadId;
            ++liRoadIndex;
            if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
            {
                liRoadIndex = -1;
                break;
            }
        }

        GameStateModuleIO::RoadRulesUpdateTargetScoreAction lRecord;
        GetChallengeUserScore( liRoadIndex, &lRecord.mUserScores, false );
        GetChallengeFriendHighScore( liRoadIndex, &lRecord.mFriendScores, false );
        lRecord.mRoadId = mpRoadRulesManager->GetCurrentRoadID();

        CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
        lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lRecord ),
                       GameStateModuleIO::E_ACTION_ROAD_RULES_UPDATE_TARGET_ROAD_SCORE, sizeof( lRecord ) );
    }

    const u8 luBuddyScrubbedFlag = 0;
    CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
    lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &luBuddyScrubbedFlag ), 55, 1 );
}

} // namespace BrnGameState
