// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_03.cpp
//   (wave C partfile -- group 3 "profile / server score sync")
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::OnProfileLoaded                  @ 0x82349E20
//   BrnGameState::StreetManager::ProcessConnectedOnlineEvent      @ 0x8234A148
//   BrnGameState::StreetManager::UpdateUserScoresFromServerRecords@ 0x82348FC0
//
// Store-for-store against the frozen StreetManager layout header: every store,
// branch, early-out and assert has an asm counterpart and every offset the X360
// folds is spelled through a committed named accessor -- the profile road-rules
// accessors (Profile::GetNetworkChallengeData / GetUserChallengeData /
// GetNetworkChallengeDownloadTimestamp / Get/SetLastRoadRulesResetTime /
// GetRoadRulesID), StreetData::GetRoad (which owns its own bounds assert) and
// the OutputBuffer queue accessors. No raw `*(p + off)` casts.
//
// All three handlers publish the same 16-byte road-rules score-summary game
// action (type 232) onto the GUI output queue; OnProfileLoaded and
// ProcessConnectedOnlineEvent stamp the profile's download timestamp into it,
// UpdateUserScoresFromServerRecords posts a literal 0 there.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameEvents.h"                    // OnlineRoadRulesConnectInfoEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // OutputBuffer::GetGameActionQueue / GetGuiOutputQueue
#include "GameSource/GameState/Progression/BrnProgressionManager.h"// ProgressionManager::GetProfile / CheckForSpecialCarUnlocks / SendGameCompletionResults
#include "GameSource/GameState/Progression/BrnProfile.h"           // BrnProgression::Profile road-rules accessors
#include "SharedClasses/StreetData/BrnChallengeData.h"             // ChallengeData::ContainsData / GetScore / SetScore / SetClean / CompareScores, ScoreType, operator++
#include "SharedClasses/StreetData/BrnStreetData.h"                // StreetData::GetRoad / Road::GetId
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"        // BrnNetwork::RoadRulesMessageData
#include "GameShared/GameClasses/Module/CgsEventQueue.h"           // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"       // BaseEventQueue<T>::GetLength / GetEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // VariableEventQueue<13312,16>::AddEvent / CgsModule::Event
#include "GameShared/GameClasses/Core/CgsID.h"                     // CgsIDCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT

#include <cstring>   // memcpy

namespace BrnGameState
{
    namespace
    {
        // The 16-byte road-rules score-summary game action (type 232) all three
        // handlers post. The X360 builds it as three contiguous stack slots --
        // the profile's road-rules session id (8B), the download timestamp (4B)
        // and the posting StreetManager (4B) -- and hands the leading address to
        // VariableEventQueue<13312,16>::AddEvent(&payload, 232, 16).
        //
        // FLAG: the attested size is the X360's (32-bit pointers pack the manager
        // at +12, total 16). On the x64 gate the pointer aligns to +16 and the
        // struct grows to 24, so no static_assert is possible here -- the (232,16)
        // ints stay verbatim from the asm (the committed
        // CgsResource::Events::AcquireResourceRequest post is the same precedent).
        struct RoadRulesScoreSummaryPayload
        {
            u64            mu64RoadRulesID;       // +0
            u32            muDownloadTimestamp;   // +8
            StreetManager* mpStreetManager;       // +12 (X360)
        };
    }

    // -----------------------------------------------------------------------
    // @ 0x82349E20. The profile finished loading: adopt its persisted road-rules
    // tables wholesale (network high scores first, then the local player's
    // scores) and publish the score summary so the GUI can rebuild its lists.
    // -----------------------------------------------------------------------
    void StreetManager::OnProfileLoaded( GameStateModuleIO::OutputBuffer* lpOutput )
    {
        CGS_ASSERT( mpProgressionManager, "mpProgressionManager" );
        CGS_ASSERT( mpProgressionManager->GetProfile(), "mpProgressionManager->GetProfile()" );

        BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();

        memcpy( maNetworkChallengeData, lpProfile->GetNetworkChallengeData( 0 ), sizeof( maNetworkChallengeData ) );
        memcpy( maChallengeData,        lpProfile->GetUserChallengeData( 0 ),    sizeof( maChallengeData ) );

        RoadRulesScoreSummaryPayload lSummary;
        lSummary.mu64RoadRulesID     = lpProfile->GetRoadRulesID();
        lSummary.muDownloadTimestamp = lpProfile->GetNetworkChallengeDownloadTimestamp();
        lSummary.mpStreetManager     = this;

        lpOutput->GetGuiOutputQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>( &lSummary ), 232, 16 );
    }

    // -----------------------------------------------------------------------
    // @ 0x8234A148. Connected to the road-rules service: when the server reports
    // a different reset time than the profile last saw, every stored challenge
    // record is stale -- wipe them, publish the (now empty) score summary, and
    // only then stamp the new reset time into the profile.
    // -----------------------------------------------------------------------
    void StreetManager::ProcessConnectedOnlineEvent(
            GameStateModuleIO::OutputBuffer* lpOutput,
            const GameStateModuleIO::OnlineRoadRulesConnectInfoEvent* lpConnectInfoEvent )
    {
        CGS_ASSERT( mpProgressionManager, "mpProgressionManager" );
        CGS_ASSERT( mpProgressionManager->GetProfile(), "mpProgressionManager->GetProfile()" );

        if ( mpProgressionManager->GetProfile()->GetLastRoadRulesResetTime()
             != lpConnectInfoEvent->muLastRoadRulesResetTime )
        {
            ClearAllChallengeData();

            BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();

            RoadRulesScoreSummaryPayload lSummary;
            lSummary.mu64RoadRulesID     = lpProfile->GetRoadRulesID();
            lSummary.muDownloadTimestamp = lpProfile->GetNetworkChallengeDownloadTimestamp();
            lSummary.mpStreetManager     = this;

            lpOutput->GetGuiOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lSummary ), 232, 16 );

            mpProgressionManager->GetProfile()->SetLastRoadRulesResetTime(
                lpConnectInfoEvent->muLastRoadRulesResetTime );
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x82348FC0. Merge the road-rules scores the server sent back for THIS
    // profile into the local player's challenge table. Records stamped with a
    // different road-rules session id belong to another profile: they are
    // dropped and raise the "foreign records" notification at the end.
    //
    // Per record the server score wins when the local table has nothing for that
    // score type, or when the server score ranks better; the adopted score is
    // credited to the "PUSMC01" car and marked clean (it is already on the
    // server, so it must not be re-uploaded).
    // -----------------------------------------------------------------------
    void StreetManager::UpdateUserScoresFromServerRecords( const void* lpLocalDownloadedQueue,
                                                           GameStateModuleIO::OutputBuffer* lpOutput )
    {
        bool lbDroppedForeignRecords = false;

        const CgsModule::EventQueue<BrnNetwork::RoadRulesMessageData, 40>* lpQueue =
            static_cast<const CgsModule::EventQueue<BrnNetwork::RoadRulesMessageData, 40>*>( lpLocalDownloadedQueue );

        if ( lpQueue->GetLength() > 0 )
        {
            CGS_ASSERT( mpProgressionManager, "mpProgressionManager" );
            CGS_ASSERT( mpProgressionManager->GetProfile(), "mpProgressionManager->GetProfile()" );

            for ( s32 liRecord = 0; liRecord < lpQueue->GetLength(); ++liRecord )
            {
                // 24-byte record copy (the X360 copies the queue element to the
                // stack before touching it); GetEvent carries its own asserts.
                BrnNetwork::RoadRulesMessageData lRecord = lpQueue->GetEvent( liRecord );

                if ( lRecord.mu64RoadRulesID != mpProgressionManager->GetProfile()->GetRoadRulesID() )
                {
                    lbDroppedForeignRecords = true;
                    continue;
                }

                const ::CgsID lPusmc01Car = CgsIDCompress( "PUSMC01" );

                const BrnStreetData::ChallengeIndex liRoadIndex = lRecord.mChallengeIndex;
                const ::CgsID lRoadId = mpStreetData->GetRoad( liRoadIndex )->GetId();   // GetRoad owns its bounds assert

                // Inlined GetSaveGameChallengeIndexByRoadID: the save-game slot
                // scan is a pure gate here -- a road with no persisted challenge
                // slot is skipped, the slot value itself is never used.
                s32 liSaveGameSlotIndex = 0;
                const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
                while ( *lpSlotRoadId != lRoadId )
                {
                    ++lpSlotRoadId;
                    ++liSaveGameSlotIndex;
                    if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
                    {
                        liSaveGameSlotIndex = -1;
                        break;
                    }
                }
                if ( liSaveGameSlotIndex < 0 )
                {
                    continue;
                }

                // The factory builds the server-side entry from the record's raw
                // score pair. (The X360 passes `this` in the unused slot; the
                // committed factory ignores that parameter.)
                BrnStreetData::ChallengePlayerScoreEntry lFactoryOut;
                BrnStreetData::ChallengePlayerScoreEntry lServerEntry =
                    *CreateUserChallengeScoreFr( &lFactoryOut, 0, lRecord.maScores );

                BrnStreetData::ChallengePlayerScoreEntry lLocalEntry;
                lLocalEntry.Construct();
                GetChallengeUserScore( liRoadIndex, &lLocalEntry, false );

                for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                      leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
                      leScoreType++ )
                {
                    if ( lServerEntry.ContainsData( leScoreType ) )
                    {
                        const int32_t liServerScore = lServerEntry.GetScore( leScoreType );

                        if ( !lLocalEntry.ContainsData( leScoreType )
                             || lServerEntry.CompareScores( leScoreType,
                                                            liServerScore,
                                                            lLocalEntry.GetScore( leScoreType ) ) < 0 )
                        {
                            lLocalEntry.SetScore( leScoreType, liServerScore );
                            lLocalEntry.SetCarID( leScoreType, lPusmc01Car );
                            lLocalEntry.SetClean( leScoreType );
                        }
                    }
                    else if ( lLocalEntry.ContainsData( leScoreType ) )
                    {
                        // Server has nothing for this type: re-store the local score.
                        lLocalEntry.SetScore( leScoreType, lLocalEntry.GetScore( leScoreType ) );
                    }
                }

                SetChallengeUserScore( liRoadIndex, &lLocalEntry, false );
            }

            RoadRulesScoreSummaryPayload lSummary;
            lSummary.mu64RoadRulesID     = mpProgressionManager->GetProfile()->GetRoadRulesID();
            lSummary.muDownloadTimestamp = 0;
            lSummary.mpStreetManager     = this;

            lpOutput->GetGuiOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lSummary ), 232, 16 );

            if ( lbDroppedForeignRecords )
            {
                // FLAG: the X360 posts a stack byte it never writes (the type-234
                // notification carries no payload); zero-initialised here so the
                // reconstruction has no indeterminate read.
                u8 lu8ForeignRecordsNotification = 0;
                lpOutput->GetGuiOutputQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>( &lu8ForeignRecordsNotification ), 234, 1 );
            }

            UpdateTrophyUnlockOnRoadRuleWin( BrnStreetData::E_SCORE_TYPE_TIME );
            UpdateTrophyUnlockOnRoadRuleWin( BrnStreetData::E_SCORE_TYPE_CRASH );

            mpProgressionManager->CheckForSpecialCarUnlocks();
            mpProgressionManager->SendGameCompletionResults( lpOutput->GetGuiOutputQueue() );
        }
    }
}
