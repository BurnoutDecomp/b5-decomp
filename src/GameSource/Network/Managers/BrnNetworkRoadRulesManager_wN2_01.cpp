// ===================================================================================
// BrnNetwork::NetworkRoadRulesManager  -- recovered function bodies (part 2)
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkRoadRulesManager_wN2_01.cpp
//
// Construct / Release / Destruct, the per-frame ProcessBeforeSimulation, per-player
// registration (AddPlayer / RemovePlayer / Disconnected), the personal-best and lobby-score send
// paths, the upload step, and the reliable-message / upload completion callbacks.
// Member layout and console offsets: see the class banner in the header.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkRoadRulesManager.h"

#include <cstring>                                                                  // memset / memcpy

#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                          // CgsDev::Log::gpDebugPrint (fake-nack warning)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                    // VariableEventQueue<14000,16>::AddEvent
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                // PlayerManager::GetPlayerByID / GetMenuDataByID / IsPlayerTurnToSendRoundRobinMessage
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                // NetworkPlayer::RegisterMessageType, PlayerMenuData, KI_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"                     // TimeManager::GetFrameCount
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // ServerInterfaceConnection::IsLoggedIn
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"      // ServerInterfaceGames::IsLocalPlayerInGame / IsGameStarted
#include "GameSource/Network/BrnNetworkModule.h"                                    // BrnNetworkModule accessors
#include "GameSource/Network/BrnNetworkManager.h"                                   // BrnNetworkManager::OnAutoLoginProcessComplete / GetBuddyManager
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                          // NetworkOutRecvRoadRulesUploadedEvent
#include "GameSource/Network/BrnServerInterface.h"                                  // BrnServerInterface component accessors
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"         // ServerInterfaceCustomCommands::SetRoadRulesForLocalPlayer
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"     // BrnServerInterfaceDownloadableConfig::TimeBetweenRoadRulesUploads
#include "GameSource/Network/Parameters/BrnNetworkRoadRulesData.h"                  // RoadRulesUploadData
#include "GameSource/GameState/BrnCgsPlayerName.h"                                  // CgsNetwork::PlayerName::Construct
#include "GameSource/Network/Managers/X360/BrnNetworkBuddyManagerX360.h"            // BuddyManagerX360::IsFullBuddy (download filter)
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"             // StreetManager::CopyUserChallengeData

namespace BrnNetwork
{
    namespace
    {
        // Reliable message type ids registered per player.
        const s32 KI_ROAD_RULES_MESSAGE_TYPE               = 26;
        const s32 KI_ROAD_RULES_PERSONAL_BEST_MESSAGE_TYPE = 27;

        // Capacity of one road-rules message (its score-record array).
        const s32 KI_MAX_ROAD_RULES_MESSAGE_ENTRIES = 10;

        // The 16-bit wire frame window the messages are stamped with.
        const u32 KU_FRAME_WRAP = 0xFFFF;

        // Challenges per download batch, and the pause before the next batch.
        const s32 KI_NUM_CHALLENGES_PER_DOWNLOAD_BATCH = 10;
        const f32 KF_TIME_BETWEEN_DOWNLOAD_BATCHES     = 1.0f;
    }

    // -----------------------------------------------------------------------------
    // Reset everything: BOOTING, every slot freed with its four messages re-constructed, the
    // local score tables re-constructed, the cursors stopped, the timers zeroed, the three
    // queues constructed and emptied, buffering of received scores on, the server key / id /
    // force-overwrite flag / local-scores flag cleared, and the debug component bound.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::Construct()
    {
        meState = E_ROAD_RULES_STATE_BOOTING;

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            RoadRulesData& lData = maRoadRulesData[liIndex];
            lData.mPlayerID                  = CgsNetwork::KI_INVALID_PLAYER_ID;
            lData.mIndexOfNextChallengeToSend = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            lData.mRoadRulesMessageSend.Construct();
            lData.mRoadRulesMessageRecv.Construct();
            lData.mRoadRulesPBMessageSend.Construct();
            lData.mRoadRulesPBMessageRecv.Construct();
        }

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS; ++liIndex )
        {
            maLocalRoadScoresToUpload[liIndex].Construct();
            maLocalLobbyScores[liIndex].Construct();
        }

        mIndexOfNextChallengeToUpload        = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextChallengeToDownload      = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        muTimeStampOfLastDownload            = 0;
        miNumRoadsConsideredForUpload        = 0;
        mTimeUntilNextResultUpload.SetFloatVal( 0.0f );
        mTimeUntilNextResultDownload.SetFloatVal( 0.0f );

        mBufferedRoadRulesRecvQueue.Construct();
        mBufferedRoadRulesRecvQueue.Clear();
        mRoadRulesPersonalBestBuffer.Construct();
        mRoadRulesPersonalBestBuffer.Clear();
        mbBufferRoadRulesReceived = true;
        memset( macRoadRulesServerKey, 0, sizeof( macRoadRulesServerKey ) );
        mu64RoadRulesID = 0;
        mbForceOverwriteServerRecords = false;
        mPersonalBestToSendBuffer.Construct();
        mPersonalBestToSendBuffer.Clear();
        mbDownloadedLocalScores = false;

        mRoadRulesDebugComponent.Construct( this );
    }

    // -----------------------------------------------------------------------------
    // Empty the buffered received-scores queue, drop the four collaborators and clear the
    // local-scores flag.
    // -----------------------------------------------------------------------------
    bool NetworkRoadRulesManager::Release()
    {
        mBufferedRoadRulesRecvQueue.Clear();
        mpTimeManager     = nullptr;
        mpPlayerManager   = nullptr;
        mpServerInterface = nullptr;
        mpNetworkModule   = nullptr;
        mbDownloadedLocalScores = false;
        return true;
    }

    // -----------------------------------------------------------------------------
    // The Construct reset in teardown order, then unbind the debug component.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::Destruct()
    {
        mbDownloadedLocalScores = false;
        mPersonalBestToSendBuffer.Clear();
        mu64RoadRulesID = 0;
        mbForceOverwriteServerRecords = false;
        memset( macRoadRulesServerKey, 0, sizeof( macRoadRulesServerKey ) );
        mbBufferRoadRulesReceived = true;
        mRoadRulesPersonalBestBuffer.Clear();
        mBufferedRoadRulesRecvQueue.Clear();
        mIndexOfNextChallengeToUpload        = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextChallengeToDownload      = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        muTimeStampOfLastDownload            = 0;
        miNumRoadsConsideredForUpload        = 0;
        mTimeUntilNextResultUpload.SetFloatVal( 0.0f );
        mTimeUntilNextResultDownload.SetFloatVal( 0.0f );

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS; ++liIndex )
        {
            maLocalRoadScoresToUpload[liIndex].Construct();
            maLocalLobbyScores[liIndex].Construct();
        }

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            RoadRulesData& lData = maRoadRulesData[liIndex];
            lData.mPlayerID                  = CgsNetwork::KI_INVALID_PLAYER_ID;
            lData.mIndexOfNextChallengeToSend = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            lData.mRoadRulesMessageSend.Construct();
            lData.mRoadRulesMessageRecv.Construct();
            lData.mRoadRulesPBMessageSend.Construct();
            lData.mRoadRulesPBMessageRecv.Construct();
        }

        meState = E_ROAD_RULES_STATE_BOOTING;
        mRoadRulesDebugComponent.Destruct();
    }

    // -----------------------------------------------------------------------------
    // While received scores are not being buffered, hand the buffered received-scores queue to
    // the game (only when the game's queue is empty) and forward one buffered personal best.
    // Then run the state machine: tick the upload / download timers in their states; in the
    // idle-online state move on to whichever server cursor still has work (local download,
    // then download, then upload), or finish the auto-login cycle and go back to BOOTING.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::ProcessBeforeSimulation( BrnNetworkModuleIO::OutputBuffer* lpOutput,
                                                           f32 lfGameTimeStep, BrnUpdateSet lUpdateSet )
    {
        (void)lUpdateSet;

        CGS_ASSERT( lpOutput != nullptr, "lpOutput" );

        if ( !mbBufferRoadRulesReceived )
        {
            CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
            CGS_ASSERT( mpNetworkModule->GetNetworkToGameStateInterface() != nullptr,
                        "mpNetworkModule->GetNetworkToGameStateInterface()" );

            BrnNetworkModuleIO::NetworkToGameStateInterface::RoadRulesReceivedQueue* lpOutputRoadRulesRecvQueue =
                mpNetworkModule->GetNetworkToGameStateInterface()->GetRoadRulesReceivedQueue();
            CGS_ASSERT( lpOutputRoadRulesRecvQueue != nullptr, "lpOutputRoadRulesRecvQueue" );

            if ( lpOutputRoadRulesRecvQueue->GetLength() == 0 )
            {
                lpOutputRoadRulesRecvQueue->Append( mBufferedRoadRulesRecvQueue );
                mBufferedRoadRulesRecvQueue.Clear();
            }

            CGS_ASSERT( mpNetworkModule->GetNetworkEventQueue() != nullptr,
                        "mpNetworkModule->GetNetworkEventQueue()" );

            if ( mRoadRulesPersonalBestBuffer.GetLength() > 0 )
            {
                BrnNetworkModuleIO::NetworkOutRecvRoadRulesPBEvent lRoadRulesPbEvent;
                mRoadRulesPersonalBestBuffer.Pop( &lRoadRulesPbEvent );
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>( &lRoadRulesPbEvent ),
                    lRoadRulesPbEvent.GetEventType(), sizeof( lRoadRulesPbEvent ) );
            }
        }

        switch ( meState )
        {
            case E_ROAD_RULES_STATE_UPLOADING:
                mTimeUntilNextResultUpload -= CgsSystem::Time( lfGameTimeStep );
                break;

            case E_ROAD_RULES_STATE_DOWNLOADING:
            case E_ROAD_RULES_STATE_DOWNLOADING_LOCAL:
                mTimeUntilNextResultDownload -= CgsSystem::Time( lfGameTimeStep );
                break;

            case KI_STATE_AUTO_LOGIN_PRIMED:
                if ( mIndexOfNextLocalChallengeToDownload < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
                {
                    CGS_ASSERT( meState != E_ROAD_RULES_STATE_IN_GAME, "meState != E_ROAD_RULES_STATE_IN_GAME" );
                    meState = E_ROAD_RULES_STATE_DOWNLOADING_LOCAL;
                }
                else if ( mIndexOfNextChallengeToDownload < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
                {
                    CGS_ASSERT( meState != E_ROAD_RULES_STATE_IN_GAME, "meState != E_ROAD_RULES_STATE_IN_GAME" );
                    meState = E_ROAD_RULES_STATE_DOWNLOADING;
                }
                else if ( mIndexOfNextChallengeToUpload < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
                {
                    CGS_ASSERT( meState != E_ROAD_RULES_STATE_IN_GAME, "meState != E_ROAD_RULES_STATE_IN_GAME" );
                    meState = E_ROAD_RULES_STATE_UPLOADING;
                }
                else
                {
                    CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
                    CGS_ASSERT( mpNetworkModule->GetNetworkManager() != nullptr,
                                "mpNetworkModule->GetNetworkManager()" );

                    mpNetworkModule->GetNetworkManager()->OnAutoLoginProcessComplete( 0 );
                    meState = E_ROAD_RULES_STATE_BOOTING;
                }
                break;

            default:
                break;
        }
    }

    // -----------------------------------------------------------------------------
    // For a player the player manager knows: claim a free slot, register the road-rules and
    // personal-best reliable message pairs on the player (both registered with the road-rules
    // message length), key the slot and start sending our lobby scores to them.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::AddPlayer( NetworkPlayerID lPlayerID )
    {
        CGS_ASSERT( mpPlayerManager != nullptr, "mpPlayerManager" );

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID( lPlayerID );
        if ( lpNetworkPlayer == nullptr )
        {
            return;
        }

        RoadRulesData* lpDataEntry = GetNextFreeRoadRulesDataEntry();
        CGS_ASSERT( lpDataEntry != nullptr, "lpDataEntry" );

        lpNetworkPlayer->RegisterMessageType( KI_ROAD_RULES_MESSAGE_TYPE, sizeof( RoadRulesMessage ),
                                              &lpDataEntry->mRoadRulesMessageSend,
                                              &lpDataEntry->mRoadRulesMessageRecv,
                                              &_RoadRulesMessageArrivedCallback,
                                              &_RoadRulesMessageDeliveredCallback,
                                              this );
        lpNetworkPlayer->RegisterMessageType( KI_ROAD_RULES_PERSONAL_BEST_MESSAGE_TYPE, sizeof( RoadRulesMessage ),
                                              &lpDataEntry->mRoadRulesPBMessageSend,
                                              &lpDataEntry->mRoadRulesPBMessageRecv,
                                              &_RoadRulesPersonalBestArrivedCallback,
                                              &_RoadRulesPersonalBestDeliveredCallback,
                                              this );

        lpDataEntry->mPlayerID = lPlayerID;
        StartSendingRoadRulesScoresToPlayer( lpDataEntry );
    }

    // -----------------------------------------------------------------------------
    // Unregister both message types on the player and free its slot.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::RemovePlayer( NetworkPlayerID lPlayerID )
    {
        CGS_ASSERT( mpPlayerManager != nullptr, "mpPlayerManager" );

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID( lPlayerID );
        if ( lpNetworkPlayer == nullptr )
        {
            return;
        }

        lpNetworkPlayer->UnRegisterMessageType( KI_ROAD_RULES_MESSAGE_TYPE );
        lpNetworkPlayer->UnRegisterMessageType( KI_ROAD_RULES_PERSONAL_BEST_MESSAGE_TYPE );

        RoadRulesData* lpDataEntry = GetRoadRulesDataEntry( lPlayerID );
        CGS_ASSERT( lpDataEntry != nullptr, "lpDataEntry" );

        lpDataEntry->mPlayerID                   = CgsNetwork::KI_INVALID_PLAYER_ID;
        lpDataEntry->mIndexOfNextChallengeToSend = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
    }

    // -----------------------------------------------------------------------------
    // Remove every keyed player (each slot must come back free), stop the three server cursors
    // and go back to BOOTING.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::Disconnected()
    {
        for ( s32 liPlayerIndex = 0; liPlayerIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liPlayerIndex )
        {
            if ( maRoadRulesData[liPlayerIndex].mPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID )
            {
                RemovePlayer( maRoadRulesData[liPlayerIndex].mPlayerID );
                CGS_ASSERT( maRoadRulesData[liPlayerIndex].mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID,
                            "maRoadRulesData[liPlayerIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID" );
            }
        }

        mIndexOfNextChallengeToUpload        = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextChallengeToDownload      = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        meState                              = E_ROAD_RULES_STATE_BOOTING;
    }

    // -----------------------------------------------------------------------------
    // A lobby personal best goes to the lobby table (and is queued for sending); any other goes
    // to the local upload table.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::HandleNewPersonalBest(
        const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent )
    {
        CGS_ASSERT( lpNetworkRoadRulesPBEvent != nullptr, "lpNetworkRoadRulesPBEvent" );

        if ( lpNetworkRoadRulesPBEvent->mbLobbyPersonalBest )
        {
            UpdateLocalLobbyScoresWithNewPB( lpNetworkRoadRulesPBEvent );
        }
        else
        {
            UpdateLocalRoadRulesScoresWithNewPB( lpNetworkRoadRulesPBEvent );
        }
    }

    // -----------------------------------------------------------------------------
    // Rewind a keyed player's lobby-score send cursor to the first challenge.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::StartSendingRoadRulesScoresToPlayer( RoadRulesData* lpPlayerDataEntry )
    {
        CGS_ASSERT( lpPlayerDataEntry != nullptr, "lpPlayerDataEntry" );
        CGS_ASSERT( lpPlayerDataEntry->mPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID,
                    "lpPlayerDataEntry->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID" );

        lpPlayerDataEntry->mIndexOfNextChallengeToSend = 0;
    }

    // -----------------------------------------------------------------------------
    // With a personal best queued: collect every keyed player's personal-best send message
    // (giving up for this frame if any of them is still in flight), pop the personal best and
    // send it to all of them.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::SendPersonalBestScore()
    {
        CGS_ASSERT( mpPlayerManager != nullptr, "mpPlayerManager" );
        CGS_ASSERT( mpTimeManager != nullptr, "mpTimeManager" );

        if ( mPersonalBestToSendBuffer.GetLength() <= 0 )
        {
            return;
        }

        RoadRulesPersonalBestMessage* lapRoadRulesMessages[KI_NUMBER_OF_PLAYER_DATA_SLOTS];
        s32 liNumMessages = 0;

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            if ( maRoadRulesData[liIndex].mPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID )
            {
                lapRoadRulesMessages[liNumMessages] = &maRoadRulesData[liIndex].mRoadRulesPBMessageSend;
                if ( lapRoadRulesMessages[liNumMessages]->IsMessageValid() )
                {
                    return;
                }
                ++liNumMessages;
            }
        }

        const u16 lu16Frame = static_cast<u16>( mpTimeManager->GetFrameCount() % KU_FRAME_WRAP );

        BrnNetworkModuleIO::NetworkInRoadRulesPBEvent lPersonalBest;
        mPersonalBestToSendBuffer.Pop( &lPersonalBest );

        for ( s32 liIndex = 0; liIndex < liNumMessages; ++liIndex )
        {
            RoadRulesPersonalBestMessage* lpRoadRulesMessage = lapRoadRulesMessages[liIndex];
            CGS_ASSERT( !lpRoadRulesMessage->IsMessageValid(), "!lpRoadRulesMessage->IsMessageValid()" );
            lpRoadRulesMessage->PrepareForSend( lu16Frame, lPersonalBest.mChallengeIndex,
                                                lPersonalBest.mPersonalBestScore );
        }
    }

    // -----------------------------------------------------------------------------
    // Round-robin the keyed players that still have lobby scores to receive: for the first one
    // whose turn it is, gather the next message's worth of scores; with none left, stop that
    // player's cursor; otherwise send them (unless its message is still in flight) and stop.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::HandleSendingRoadRulesScores( bool lbAreWePlaying )
    {
        CGS_ASSERT( mpPlayerManager != nullptr, "mpPlayerManager" );
        CGS_ASSERT( mpTimeManager != nullptr, "mpTimeManager" );

        const u16 lu16Frame = static_cast<u16>( mpTimeManager->GetFrameCount() % KU_FRAME_WRAP );

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            RoadRulesData* lpData = &maRoadRulesData[liIndex];

            if ( lpData->mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID
                 || lpData->mIndexOfNextChallengeToSend >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                continue;
            }

            if ( !mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage( lpData->mPlayerID, lbAreWePlaying, 0 ) )
            {
                continue;
            }

            RoadRulesMessageData laRoadRulesMessageData[KI_MAX_ROAD_RULES_MESSAGE_ENTRIES];
            s32 liNumScores;
            const Road::ChallengeIndex liNumChallengesWalked =
                GetRoadRulesDataToSend( laRoadRulesMessageData, &liNumScores, lpData->mIndexOfNextChallengeToSend );

            if ( liNumScores <= 0 )
            {
                lpData->mIndexOfNextChallengeToSend = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
                continue;
            }

            if ( lpData->mRoadRulesMessageSend.IsMessageValid() )
            {
                continue;
            }

            lpData->mRoadRulesMessageSend.PrepareForSend( lu16Frame, liNumScores, laRoadRulesMessageData );
            lpData->mIndexOfNextChallengeToSend += liNumChallengesWalked;
            return;
        }
    }

    // -----------------------------------------------------------------------------
    // From lStartIndex on, copy every lobby challenge that holds a score into the next record
    // (scores by type, challenge index) until the message is full or the table ends. Returns the
    // number of challenge slots walked.
    // -----------------------------------------------------------------------------
    Road::ChallengeIndex NetworkRoadRulesManager::GetRoadRulesDataToSend( RoadRulesMessageData* lpRoadRulesMessageData,
                                                                         s32* lpiNumScores,
                                                                         Road::ChallengeIndex lStartIndex )
    {
        s32 liNumScores = 0;
        Road::ChallengeIndex lChallengeIndex = lStartIndex;

        for ( ; lChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS; ++lChallengeIndex )
        {
            if ( liNumScores >= KI_MAX_ROAD_RULES_MESSAGE_ENTRIES )
            {
                break;
            }

            CGS_ASSERT( lChallengeIndex >= 0, "lChallengeIndex >= 0" );
            CGS_ASSERT( lChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS,
                        "lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );

            const BrnStreetData::ChallengeData& lLobbyScore = maLocalLobbyScores[lChallengeIndex];
            if ( !lLobbyScore.ContainsData( BrnStreetData::E_SCORE_TYPE_COUNT ) )
            {
                continue;
            }

            RoadRulesMessageData& lRecord = lpRoadRulesMessageData[liNumScores];
            lRecord.mChallengeIndex = 0;
            memset( lRecord.maScores, 0, sizeof( lRecord.maScores ) );

            for ( s32 liScoreType = BrnStreetData::E_SCORE_TYPE_START;
                  liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liScoreType )
            {
                const BrnStreetData::ScoreType leScoreType = static_cast<BrnStreetData::ScoreType>( liScoreType );
                if ( lLobbyScore.ContainsData( leScoreType ) )
                {
                    lRecord.maScores[liScoreType] = lLobbyScore.GetScore( leScoreType );
                }
            }

            lRecord.mChallengeIndex = lChallengeIndex;
            ++liNumScores;
        }

        *lpiNumScores = liNumScores;
        return lChallengeIndex - lStartIndex;
    }

    // -----------------------------------------------------------------------------
    // Copy the personal best's scores into the lobby table, then queue the event for sending to
    // the other players.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::UpdateLocalLobbyScoresWithNewPB(
        const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent )
    {
        CGS_ASSERT( lpNetworkRoadRulesPBEvent != nullptr, "lpNetworkRoadRulesPBEvent" );
        CGS_ASSERT( lpNetworkRoadRulesPBEvent->mChallengeIndex >= 0,
                    "lpNetworkRoadRulesPBEvent->mChallengeIndex >= 0" );
        CGS_ASSERT( lpNetworkRoadRulesPBEvent->mChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS,
                    "lpNetworkRoadRulesPBEvent->mChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );

        BrnStreetData::ChallengeData& lLobbyScore = maLocalLobbyScores[lpNetworkRoadRulesPBEvent->mChallengeIndex];
        const BrnStreetData::ChallengePlayerScoreEntry& lPersonalBest = lpNetworkRoadRulesPBEvent->mPersonalBestScore;

        for ( s32 liScoreType = BrnStreetData::E_SCORE_TYPE_START; liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; )
        {
            const BrnStreetData::ScoreType leScoreType = static_cast<BrnStreetData::ScoreType>( liScoreType );
            if ( lPersonalBest.ContainsData( leScoreType ) )
            {
                lLobbyScore.SetScore( leScoreType, lPersonalBest.GetScore( leScoreType ) );
            }

            ++liScoreType;
            CGS_ASSERT( liScoreType <= BrnStreetData::E_SCORE_TYPE_COUNT, "leEnumIndex <= E_SCORE_TYPE_COUNT" );
        }

        mPersonalBestToSendBuffer.Push( lpNetworkRoadRulesPBEvent );
    }

    // -----------------------------------------------------------------------------
    // UPLOADING step: when no batch is outstanding, the inter-upload timer has run out, there is
    // upload work left and the custom-commands component is idle, gather the dirty local scores
    // and send them; with nothing to send, finish the upload cycle (idle-online).
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::HandleUploadingRoadRulesScores()
    {
        const bool lbReadyToUpload =
            ( miNumRoadsConsideredForUpload == 0 ) && !( mTimeUntilNextResultUpload.GetFloatVal() > 0.0f );

        if ( mIndexOfNextChallengeToUpload >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS || !lbReadyToUpload )
        {
            return;
        }

        if ( mpServerInterface->GetStatus( CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS )
             != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE )
        {
            return;
        }

        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != nullptr,
                    "mpServerInterface->GetConnectionComponent()" );
        CGS_ASSERT( mpServerInterface->GetGameComponent() != nullptr,
                    "mpServerInterface->GetGameComponent()" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
                    "mpServerInterface->GetConnectionComponent()->IsLoggedIn()" );
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
                    "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || "
                    "( !mpServerInterface->GetGameComponent()->IsGameStarted() )" );

        RoadRulesUploadData lUploadData;
        GetRoadRulesDataToUpload( &lUploadData );

        if ( lUploadData.GetNumScores() > 0 )
        {
            CGS_ASSERT( mpServerInterface->GetCustomCommandsComponent() != nullptr,
                        "mpServerInterface->GetCustomCommandsComponent()" );
            CGS_ASSERT( NULL != macRoadRulesServerKey, "NULL != macRoadRulesServerKey" );

            mpServerInterface->GetCustomCommandsComponent()->SetRoadRulesForLocalPlayer(
                &lUploadData, macRoadRulesServerKey, &_UploadRoadRulesCallback, this );
        }
        else
        {
            miNumRoadsConsideredForUpload = 0;
            mbForceOverwriteServerRecords = false;
            meState                       = KI_STATE_AUTO_LOGIN_PRIMED;
            mIndexOfNextChallengeToUpload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        }
    }

    // The street-data snapshot event's copy of the local scores (inline; its only user is this
    // manager, and StreetManager is complete here).
    inline void BrnNetworkModuleIO::NetworkInRoadRulesDataEvent::GetRoadRulesScoreData(
        BrnStreetData::ChallengePlayerScoreEntry* lpDest, s32 liSizeInBytes ) const
    {
        mpStreetManager->CopyUserChallengeData( lpDest, liSizeInBytes );
    }

    // -----------------------------------------------------------------------------
    // The game's network events: a street-data snapshot refreshes the local upload table and the
    // download stamp / road-rules id, a personal best goes to the lobby or the upload table, and
    // the overwrite request forces the next upload to send every recorded score.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::ProcessNetworkEvents( const BrnNetworkModuleIO::NetworkEventQueue* lpNetworkEventQueue )
    {
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liEventType = lpNetworkEventQueue->GetFirstEvent( &lpEvent, &liSize );

        while ( lpEvent != nullptr )
        {
            switch ( liEventType )
            {
            case BrnNetworkModuleIO::NetworkInRoadRulesDataEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInRoadRulesDataEvent* lpRoadRulesScoreDataEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInRoadRulesDataEvent*>( lpEvent );
                CGS_ASSERT( lpRoadRulesScoreDataEvent != nullptr, "lpRoadRulesScoreDataEvent" );

                lpRoadRulesScoreDataEvent->GetRoadRulesScoreData( maLocalRoadScoresToUpload,
                                                                  sizeof( maLocalRoadScoresToUpload ) );
                muTimeStampOfLastDownload = lpRoadRulesScoreDataEvent->GetTimeStampOfLastRoadRulesDownload();
                mu64RoadRulesID           = lpRoadRulesScoreDataEvent->GetRoadRulesID();
                break;
            }

            case BrnNetworkModuleIO::NetworkInRoadRulesPBEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent*>( lpEvent );
                CGS_ASSERT( lpNetworkRoadRulesPBEvent != nullptr, "lpNetworkRoadRulesPBEvent" );

                if ( lpNetworkRoadRulesPBEvent->mbLobbyPersonalBest )
                {
                    UpdateLocalLobbyScoresWithNewPB( lpNetworkRoadRulesPBEvent );
                }
                else
                {
                    UpdateLocalRoadRulesScoresWithNewPB( lpNetworkRoadRulesPBEvent );
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInRoadRulesOverwriteServerRecord::KI_EVENT_TYPE:
                mbForceOverwriteServerRecords = true;
                break;

            default:
                break;
            }

            const CgsModule::Event* lpNextEvent = nullptr;
            liEventType = lpNetworkEventQueue->GetNextEvent( lpEvent, &lpNextEvent, &liSize );
            lpEvent = lpNextEvent;
        }
    }

    // -----------------------------------------------------------------------------
    // Copy a new road personal best into the local upload table: every recorded score type takes
    // the new score and the car that set it.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::UpdateLocalRoadRulesScoresWithNewPB(
        const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent )
    {
        CGS_ASSERT( lpNetworkRoadRulesPBEvent != nullptr, "lpNetworkRoadRulesPBEvent" );
        CGS_ASSERT( lpNetworkRoadRulesPBEvent->mChallengeIndex >= 0,
                    "lpNetworkRoadRulesPBEvent->mChallengeIndex >= 0" );
        CGS_ASSERT( lpNetworkRoadRulesPBEvent->mChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS,
                    "lpNetworkRoadRulesPBEvent->mChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );

        BrnStreetData::ChallengePlayerScoreEntry& lRoadScore =
            maLocalRoadScoresToUpload[lpNetworkRoadRulesPBEvent->mChallengeIndex];
        const BrnStreetData::ChallengePlayerScoreEntry& lPersonalBest = lpNetworkRoadRulesPBEvent->mPersonalBestScore;

        for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
              leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; leScoreType++ )
        {
            if ( lPersonalBest.ContainsData( leScoreType ) )
            {
                lRoadScore.SetScore( leScoreType, lPersonalBest.GetScore( leScoreType ) );
                lRoadScore.SetCarID( leScoreType, lPersonalBest.GetCarID( leScoreType ) );
            }
        }
    }

    // -----------------------------------------------------------------------------
    // Fill one upload batch from the local upload table, starting at the next challenge to
    // upload: the dirty scores, or every score (zero when unrecorded) while the server records are
    // being overwritten. A challenge whose scores do not all fit is left to start the next batch.
    // Records how many challenges the batch covers.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::GetRoadRulesDataToUpload( RoadRulesUploadData* lpUploadData )
    {
        bool lbSpaceLeft = true;

        lpUploadData->Construct( mu64RoadRulesID );

        Road::ChallengeIndex lChallengeIndex = mIndexOfNextChallengeToUpload;
        for ( ; lChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS && lbSpaceLeft; ++lChallengeIndex )
        {
            CGS_ASSERT( lChallengeIndex >= 0, "lChallengeIndex >= 0" );
            CGS_ASSERT( lChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS,
                        "lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );

            const BrnStreetData::ChallengePlayerScoreEntry* lpRoadScoreDataEntry =
                &maLocalRoadScoresToUpload[lChallengeIndex];
            CGS_ASSERT( lpRoadScoreDataEntry != nullptr, "lpRoadScoreDataEntry" );

            if ( !mbForceOverwriteServerRecords && !lpRoadScoreDataEntry->IsDirty( BrnStreetData::E_SCORE_TYPE_COUNT ) )
            {
                continue;
            }

            for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                  leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; leScoreType++ )
            {
                s32   liScore;
                CgsID lCarID;
                if ( lpRoadScoreDataEntry->IsDirty( leScoreType ) )
                {
                    lCarID  = lpRoadScoreDataEntry->GetCarID( leScoreType );
                    liScore = lpRoadScoreDataEntry->GetScore( leScoreType );
                }
                else if ( !mbForceOverwriteServerRecords )
                {
                    continue;
                }
                else
                {
                    liScore = 0;
                    lCarID  = 0;
                    if ( lpRoadScoreDataEntry->ContainsData( leScoreType ) )
                    {
                        liScore = lpRoadScoreDataEntry->GetScore( leScoreType );
                        lCarID  = lpRoadScoreDataEntry->GetCarID( leScoreType );
                    }
                }

                const s32 liRecordIndex = lChallengeIndex * BrnStreetData::E_SCORE_TYPE_COUNT + leScoreType;
                if ( lpUploadData->SetScoreData( liRecordIndex, liScore, lCarID ) )
                {
                    // The batch is full. A challenge cut short before its last score type is
                    // uploaded again, whole, by the next batch.
                    if ( leScoreType != BrnStreetData::E_SCORE_TYPE_COUNT - 1 )
                    {
                        --lChallengeIndex;
                    }
                    lbSpaceLeft = false;
                    break;
                }
            }
        }

        miNumRoadsConsideredForUpload = lChallengeIndex - mIndexOfNextChallengeToUpload;
    }

    // -----------------------------------------------------------------------------
    // A road-rules high-score download batch finished. On success, every downloaded score of the
    // batch's challenges (up to ten, two score types each) is handed to the game -- scores set by
    // players who are no longer friends are dropped -- and the next batch is scheduled; with no
    // data, or once the last challenge is covered, the download cycle ends (idle-online). On
    // failure the cycle ends and a custom-commands error is cleared.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_DownloadRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess )
    {
        CGS_ASSERT( lpData != nullptr, "lpData" );
        NetworkRoadRulesManager* lpRoadRulesManager = static_cast<NetworkRoadRulesManager*>( lpData );
        CGS_ASSERT( lpRoadRulesManager != nullptr, "lpRoadRulesManager" );

        if ( lbSuccess && lpRoadRulesManager->meState == E_ROAD_RULES_STATE_DOWNLOADING )
        {
            RoadRulesDownloadData* lpDownloadedData = static_cast<RoadRulesDownloadData*>( lpResult );
            CGS_ASSERT( lpDownloadedData != nullptr, "lpDownloadedData" );

            if ( lpDownloadedData->GetNumScores() == 0 )
            {
                lpRoadRulesManager->mIndexOfNextChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
                lpRoadRulesManager->meState                         = KI_STATE_AUTO_LOGIN_PRIMED;
                return;
            }

            Road::ChallengeIndex lEndDownloadIndex =
                lpRoadRulesManager->mIndexOfNextChallengeToDownload + KI_NUM_CHALLENGES_PER_DOWNLOAD_BATCH;
            if ( lEndDownloadIndex >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                lEndDownloadIndex = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            }

            s32 liDownloadIndex = 0;
            for ( Road::ChallengeIndex lChallengeIndex = lpRoadRulesManager->mIndexOfNextChallengeToDownload;
                  lChallengeIndex < lEndDownloadIndex; ++lChallengeIndex )
            {
                RoadRulesDownloadEvent lDownloadEvent;
                lDownloadEvent.Construct();

                CGS_ASSERT( lpRoadRulesManager->mpNetworkModule != nullptr, "lpRoadRulesManager->mpNetworkModule" );

                for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                      leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; leScoreType++ )
                {
                    char lacName[KI_ROAD_RULES_NAME_LENGTH];
                    s32  liScoreType;
                    s32  liQuantisedScore;
                    lpDownloadedData->GetRoadRulesDataForDownloadIndex( liDownloadIndex, lacName, &liScoreType,
                                                                        &liQuantisedScore );
                    CGS_ASSERT( leScoreType == static_cast<BrnStreetData::ScoreType>( liScoreType ),
                                "leScoreType == static_cast<BrnStreetData::ScoreType>(liScoreType)" );

                    BrnNetworkManager* lpNetworkManager = lpRoadRulesManager->mpNetworkModule->GetNetworkManager();
                    CgsNetwork::PlayerName lPlayerName;
                    lPlayerName.Construct( lacName );

                    if ( lacName[0] != 0 && !lpNetworkManager->GetBuddyManager()->IsFullBuddy( &lPlayerName ) )
                    {
                        *CgsDev::Log::gpDebugPrint << "Discarding downloaded road rule for " << lacName
                                                   << " because he is no longer our friend\n";
                    }
                    else
                    {
                        lDownloadEvent.mRoadRulesData.maScores[leScoreType] = liQuantisedScore;
                        lDownloadEvent.maPlayerNames[leScoreType].Construct( lacName );
                        if ( lacName[0] != 0 )
                        {
                            *CgsDev::Log::gpDebugPrint << "Downloaded road rule for " << lacName
                                                       << " because he is still our friend\n";
                        }
                    }

                    ++liDownloadIndex;
                }

                lDownloadEvent.mRoadRulesData.mChallengeIndex = lChallengeIndex;

                CGS_ASSERT( lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface() != nullptr,
                            "lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface()" );
                BrnNetworkModuleIO::NetworkToGameStateInterface* lpNetworkToGameStateInterface =
                    lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface();
                CGS_ASSERT( lpNetworkToGameStateInterface->GetRoadRulesDownloadedQueue() != nullptr,
                            "lpNetworkToGameStateInterface->GetRoadRulesDownloadedQueue()" );
                lpNetworkToGameStateInterface->GetRoadRulesDownloadedQueue()->AddEvent( lDownloadEvent );
            }

            lpRoadRulesManager->mIndexOfNextChallengeToDownload += KI_NUM_CHALLENGES_PER_DOWNLOAD_BATCH;
            lpRoadRulesManager->mTimeUntilNextResultDownload.SetFloatVal( KF_TIME_BETWEEN_DOWNLOAD_BATCHES );

            if ( lpRoadRulesManager->mIndexOfNextChallengeToDownload >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                lpRoadRulesManager->mIndexOfNextChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
                if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
                {
                    lpRoadRulesManager->meState = KI_STATE_AUTO_LOGIN_PRIMED;
                }
            }
        }
        else
        {
            BrnServerInterface* lpServerInterface = lpRoadRulesManager->mpServerInterface;
            CGS_ASSERT( lpServerInterface != nullptr, "lpServerInterface" );

            lpRoadRulesManager->mIndexOfNextChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
            {
                lpRoadRulesManager->meState = KI_STATE_AUTO_LOGIN_PRIMED;
            }

            CGS_ASSERT( lpServerInterface->GetCustomCommandsComponent() != nullptr,
                        "lpServerInterface->GetCustomCommandsComponent()" );
            if ( lpServerInterface->GetCustomCommandsComponent()->GetStatus()
                 == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR )
            {
                lpServerInterface->GetCustomCommandsComponent()->ClearLastError();
            }
        }
    }

    // -----------------------------------------------------------------------------
    // The local player's own road-rules download batch finished. On success, each challenge of
    // the batch gets its two downloaded scores (by score type) as a score record handed to the
    // game, and the next batch is scheduled; once the last challenge is covered the local scores
    // count as downloaded and the cycle ends (idle-online). On failure the cycle ends and a
    // custom-commands error is cleared.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_DownloadLocalRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess )
    {
        CGS_ASSERT( lpData != nullptr, "lpData" );
        NetworkRoadRulesManager* lpRoadRulesManager = static_cast<NetworkRoadRulesManager*>( lpData );
        CGS_ASSERT( lpRoadRulesManager != nullptr, "lpRoadRulesManager" );

        if ( lbSuccess && lpRoadRulesManager->meState == E_ROAD_RULES_STATE_DOWNLOADING_LOCAL )
        {
            RoadRulesLocalPlayerDownloadedScores* lpDownloadedData =
                static_cast<RoadRulesLocalPlayerDownloadedScores*>( lpResult );
            CGS_ASSERT( lpDownloadedData != nullptr, "lpDownloadedData" );

            Road::ChallengeIndex lEndDownloadIndex =
                lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload + KI_NUM_CHALLENGES_PER_DOWNLOAD_BATCH;
            if ( lEndDownloadIndex >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                lEndDownloadIndex = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            }

            s32 liDownloadIndex = 0;
            for ( Road::ChallengeIndex lChallengeIndex = lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload;
                  lChallengeIndex < lEndDownloadIndex; ++lChallengeIndex )
            {
                RoadRulesMessageData lScoreData;
                lScoreData.mChallengeIndex = 0;
                memset( lScoreData.maScores, 0, sizeof( lScoreData.maScores ) );

                for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                      leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; leScoreType++ )
                {
                    s32 liScoreType;
                    s32 liScore;
                    lpDownloadedData->GetRoadRulesDataForDownloadIndex( liDownloadIndex, &liScoreType, &liScore );
                    lScoreData.maScores[liScoreType] = liScore;
                    ++liDownloadIndex;
                }

                lScoreData.mu64RoadRulesID = lpDownloadedData->GetRoadRulesID();
                lScoreData.mChallengeIndex = lChallengeIndex;

                CGS_ASSERT( lpRoadRulesManager->mpNetworkModule != nullptr, "lpRoadRulesManager->mpNetworkModule" );
                CGS_ASSERT( lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface() != nullptr,
                            "lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface()" );
                BrnNetworkModuleIO::NetworkToGameStateInterface* lpNetworkToGameStateInterface =
                    lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface();
                CGS_ASSERT( lpNetworkToGameStateInterface->GetLocalRoadRulesDownloadedQueue() != nullptr,
                            "lpNetworkToGameStateInterface->GetLocalRoadRulesDownloadedQueue()" );
                lpNetworkToGameStateInterface->GetLocalRoadRulesDownloadedQueue()->AddEvent( lScoreData );
            }

            lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload += KI_NUM_CHALLENGES_PER_DOWNLOAD_BATCH;
            lpRoadRulesManager->mTimeUntilNextResultDownload.SetFloatVal( KF_TIME_BETWEEN_DOWNLOAD_BATCHES );

            if ( lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
                if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
                {
                    lpRoadRulesManager->meState                 = KI_STATE_AUTO_LOGIN_PRIMED;
                    lpRoadRulesManager->mbDownloadedLocalScores = true;
                }
            }
        }
        else
        {
            BrnServerInterface* lpServerInterface = lpRoadRulesManager->mpServerInterface;
            CGS_ASSERT( lpServerInterface != nullptr, "lpServerInterface" );

            lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
            {
                lpRoadRulesManager->meState = KI_STATE_AUTO_LOGIN_PRIMED;
            }

            CGS_ASSERT( lpServerInterface->GetCustomCommandsComponent() != nullptr,
                        "lpServerInterface->GetCustomCommandsComponent()" );
            if ( lpServerInterface->GetCustomCommandsComponent()->GetStatus()
                 == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR )
            {
                lpServerInterface->GetCustomCommandsComponent()->ClearLastError();
            }
        }
    }

    // -----------------------------------------------------------------------------
    // Reliable road-rules message from another player: unpack its lobby scores into a received
    // record stamped with the sender's name and id, and queue it for the game (into the manager's
    // own buffer while received scores are being buffered).
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_RoadRulesMessageArrivedCallback( CgsNetwork::ReliableMessage* lpMessage,
                                                                    NetworkPlayerID lSendingPlayerID, void* lpUserData )
    {
        NetworkRoadRulesManager* lpRoadRulesManager = static_cast<NetworkRoadRulesManager*>( lpUserData );
        CGS_ASSERT( lpRoadRulesManager != nullptr, "lpRoadRulesManager" );

        RoadRulesMessage* lpRoadRulesMessage = static_cast<RoadRulesMessage*>( lpMessage );
        CGS_ASSERT( lpRoadRulesMessage != nullptr, "lpRoadRulesMessage" );

        s32                  liNumRoadRulesScoresRecv;
        RoadRulesMessageData laRoadRulesMessageData[KI_MAX_ROAD_RULES_MESSAGE_ENTRIES];
        const bool lbRetrieved = lpRoadRulesMessage->Retrieve( &liNumRoadRulesScoresRecv, laRoadRulesMessageData );
        CGS_ASSERT( lbRetrieved,
                    "lpRoadRulesMessage->Retrieve(&liNumRoadRulesScoresRecv, laRoadRulesMessageData)" );

        CGS_ASSERT( lpRoadRulesManager->mpPlayerManager != nullptr, "lpRoadRulesManager->mpPlayerManager" );
        CgsNetwork::PlayerMenuData* lpPlayerMenuData =
            lpRoadRulesManager->mpPlayerManager->GetMenuDataByID( lSendingPlayerID );
        CGS_ASSERT( lpPlayerMenuData != nullptr, "lpPlayerMenuData" );

        CgsNetwork::PlayerName lPlayerName;
        lPlayerName.Construct( lpPlayerMenuData->macName );

        RoadRulesRecvData lRoadRulesRecvData;
        memcpy( lRoadRulesRecvData.maRoadRulesData, laRoadRulesMessageData,
                sizeof( RoadRulesMessageData ) * liNumRoadRulesScoresRecv );
        lRoadRulesRecvData.miNumRoadRulesScoresRecv = liNumRoadRulesScoresRecv;
        lRoadRulesRecvData.mPlayerName              = lPlayerName;
        lRoadRulesRecvData.mPlayerID                = lSendingPlayerID;

        CGS_ASSERT( lpRoadRulesManager->mpNetworkModule != nullptr, "lpRoadRulesManager->mpNetworkModule" );
        CGS_ASSERT( lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface() != nullptr,
                    "lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface()" );
        BrnNetworkModuleIO::NetworkToGameStateInterface* lpNetworkToGameStateInterface =
            lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface();
        CGS_ASSERT( lpNetworkToGameStateInterface->GetRoadRulesDownloadedQueue() != nullptr,
                    "lpNetworkToGameStateInterface->GetRoadRulesDownloadedQueue()" );

        if ( lpRoadRulesManager->mbBufferRoadRulesReceived )
        {
            CGS_ASSERT( lpRoadRulesManager->mBufferedRoadRulesRecvQueue.GetLength()
                            < lpRoadRulesManager->mBufferedRoadRulesRecvQueue.GetMaxLength(),
                        "lpRoadRulesManager->mBufferedRoadRulesRecvQueue.GetLength() < "
                        "lpRoadRulesManager->mBufferedRoadRulesRecvQueue.GetMaxLength()" );
            lpRoadRulesManager->mBufferedRoadRulesRecvQueue.AddEvent( lRoadRulesRecvData );
        }
        else
        {
            CGS_ASSERT( lpNetworkToGameStateInterface->GetRoadRulesReceivedQueue()->GetLength()
                            < lpNetworkToGameStateInterface->GetRoadRulesReceivedQueue()->GetMaxLength(),
                        "lpNetworkToGameStateInterface->GetRoadRulesReceivedQueue()->GetLength() < "
                        "lpNetworkToGameStateInterface->GetRoadRulesReceivedQueue()->GetMaxLength()" );
            lpNetworkToGameStateInterface->GetRoadRulesReceivedQueue()->AddEvent( lRoadRulesRecvData );
        }
    }

    // -----------------------------------------------------------------------------
    // Delivery report for a road-rules message: only a fake nack is logged.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_RoadRulesMessageDeliveredCallback( bool lbDelivered, bool lbFakeNack,
                                                                      CgsNetwork::SignalMessage* lpAck,
                                                                      NetworkPlayerID lRecvingPlayerID, void* lpUserData )
    {
        (void)lbDelivered;
        (void)lpAck;
        (void)lRecvingPlayerID;
        (void)lpUserData;

        if ( lbFakeNack )
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Fack Nack found in ";
            *CgsDev::Log::gpDebugPrint << "BrnNetwork::NetworkRoadRulesManager::_RoadRulesMessageDeliveredCallback";
            *CgsDev::Log::gpDebugPrint << "\n";
        }
    }

    // -----------------------------------------------------------------------------
    // Reliable personal-best message from another player: unpack it into a personal-best event
    // stamped with the sender, flag whether the sender is a full buddy, and either queue it
    // straight to the game (while received scores are being buffered) or hold it in the
    // personal-best buffer for ProcessBeforeSimulation.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_RoadRulesPersonalBestArrivedCallback( CgsNetwork::ReliableMessage* lpMessage,
                                                                         NetworkPlayerID lSendingPlayerID, void* lpUserData )
    {
        NetworkRoadRulesManager* lpRoadRulesManager = static_cast<NetworkRoadRulesManager*>( lpUserData );
        CGS_ASSERT( lpRoadRulesManager != nullptr, "lpRoadRulesManager" );

        CGS_ASSERT( lpRoadRulesManager->mpPlayerManager != nullptr, "lpRoadRulesManager->mpPlayerManager" );
        CgsNetwork::PlayerMenuData* lpPlayerMenuData =
            lpRoadRulesManager->mpPlayerManager->GetMenuDataByID( lSendingPlayerID );
        CGS_ASSERT( lpPlayerMenuData != nullptr, "lpPlayerMenuData" );

        CgsNetwork::PlayerName lPlayerName;
        lPlayerName.Construct( lpPlayerMenuData->macName );

        BrnNetworkModuleIO::NetworkOutRecvRoadRulesPBEvent lRoadRulesPbEvent;
        lRoadRulesPbEvent.mPersonalBestPlayerID = lSendingPlayerID;

        RoadRulesPersonalBestMessage* lpRoadRulesPBMessage = static_cast<RoadRulesPersonalBestMessage*>( lpMessage );
        CGS_ASSERT( lpRoadRulesPBMessage != nullptr, "lpRoadRulesPBMessage" );
        lpRoadRulesPBMessage->Retrieve( &lPlayerName, &lRoadRulesPbEvent.mPersonalBestChallengeIndex,
                                        &lRoadRulesPbEvent.mPersonalBestScore );

        CGS_ASSERT( lpRoadRulesManager->mpNetworkModule != nullptr, "lpRoadRulesManager->mpNetworkModule" );
        CGS_ASSERT( lpRoadRulesManager->mpNetworkModule->GetNetworkManager() != nullptr,
                    "lpRoadRulesManager->mpNetworkModule->GetNetworkManager()" );
        auto* lpBuddyManager = lpRoadRulesManager->mpNetworkModule->GetNetworkManager()->GetBuddyManager();
        CGS_ASSERT( lpBuddyManager != nullptr, "lpBuddyManager" );
        lRoadRulesPbEvent.mbWasPBByFriend = lpBuddyManager->IsFullBuddy( &lPlayerName );

        if ( !lpRoadRulesManager->mbBufferRoadRulesReceived )
        {
            lpRoadRulesManager->mRoadRulesPersonalBestBuffer.Push( &lRoadRulesPbEvent );
        }
        else
        {
            CGS_ASSERT( lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue() != nullptr,
                        "lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue()" );
            lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lRoadRulesPbEvent ),
                lRoadRulesPbEvent.GetEventType(), sizeof( lRoadRulesPbEvent ) );
        }
    }

    // -----------------------------------------------------------------------------
    // Delivery report for a personal-best message: only a fake nack is logged.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_RoadRulesPersonalBestDeliveredCallback( bool lbDelivered, bool lbFakeNack,
                                                                           CgsNetwork::SignalMessage* lpAck,
                                                                           NetworkPlayerID lRecvingPlayerID, void* lpUserData )
    {
        (void)lbDelivered;
        (void)lpAck;
        (void)lRecvingPlayerID;
        (void)lpUserData;

        if ( lbFakeNack )
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Fack Nack found in ";
            *CgsDev::Log::gpDebugPrint << "BrnNetwork::NetworkRoadRulesManager::_RoadRulesPersonalBestDeliveredCallback";
            *CgsDev::Log::gpDebugPrint << "\n";
        }
    }

    // -----------------------------------------------------------------------------
    // Upload completion. On success while UPLOADING: mark the batch's local scores clean, tell the
    // game which challenge range was uploaded, advance the upload cursor past the batch, restart
    // the inter-upload timer, and once the table is done leave the upload cycle (idle-online
    // unless IN_GAME) and clear the force-overwrite flag. Otherwise leave the upload cycle the
    // same way and clear a pending custom-commands error.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_UploadRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess )
    {
        (void)lpResult;

        NetworkRoadRulesManager* lpRoadRulesManager = static_cast<NetworkRoadRulesManager*>( lpData );
        CGS_ASSERT( lpRoadRulesManager != nullptr, "lpRoadRulesManager" );

        BrnServerInterface* lpServerInterface = lpRoadRulesManager->mpServerInterface;
        CGS_ASSERT( lpServerInterface != nullptr, "lpServerInterface" );

        if ( lbSuccess && lpRoadRulesManager->meState == E_ROAD_RULES_STATE_UPLOADING )
        {
            const Road::ChallengeIndex lEndUploadIndex =
                lpRoadRulesManager->mIndexOfNextChallengeToUpload + lpRoadRulesManager->miNumRoadsConsideredForUpload;

            for ( Road::ChallengeIndex lChallengeIndex = lpRoadRulesManager->mIndexOfNextChallengeToUpload;
                  lChallengeIndex < lEndUploadIndex; ++lChallengeIndex )
            {
                CGS_ASSERT( lChallengeIndex >= 0, "lChallengeIndex >= 0" );
                CGS_ASSERT( lChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS,
                            "lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );

                BrnStreetData::ChallengePlayerScoreEntry* lpChallengeData =
                    &lpRoadRulesManager->maLocalRoadScoresToUpload[lChallengeIndex];
                CGS_ASSERT( lpChallengeData != nullptr, "lpChallengeData" );
                lpChallengeData->mDirty.UnSetAll();
            }

            BrnNetworkModuleIO::NetworkOutRecvRoadRulesUploadedEvent lUploadedEvent;
            lUploadedEvent.mStartUploadIndex = lpRoadRulesManager->mIndexOfNextChallengeToUpload;
            lUploadedEvent.mEndUploadIndex   = lEndUploadIndex;

            CGS_ASSERT( lpRoadRulesManager->mpNetworkModule != nullptr, "lpRoadRulesManager->mpNetworkModule" );
            CGS_ASSERT( lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue() != nullptr,
                        "lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue()" );
            lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lUploadedEvent ),
                lUploadedEvent.GetEventType(), sizeof( lUploadedEvent ) );

            CGS_ASSERT( lpServerInterface->GetDownloadableConfigComponent() != nullptr,
                        "lpServerInterface->GetDownloadableConfigComponent()" );

            lpRoadRulesManager->mIndexOfNextChallengeToUpload = lEndUploadIndex;
            lpRoadRulesManager->miNumRoadsConsideredForUpload = 0;
            lpRoadRulesManager->mTimeUntilNextResultUpload.SetFloatVal(
                lpServerInterface->GetDownloadableConfigComponent()->TimeBetweenRoadRulesUploads() );

            if ( lpRoadRulesManager->mIndexOfNextChallengeToUpload == KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
                {
                    lpRoadRulesManager->meState = KI_STATE_AUTO_LOGIN_PRIMED;
                }
                lpRoadRulesManager->mbForceOverwriteServerRecords = false;
            }
        }
        else
        {
            if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
            {
                lpRoadRulesManager->meState = KI_STATE_AUTO_LOGIN_PRIMED;
            }

            CGS_ASSERT( lpServerInterface->GetCustomCommandsComponent() != nullptr,
                        "lpServerInterface->GetCustomCommandsComponent()" );
            if ( lpServerInterface->GetCustomCommandsComponent()->GetStatus()
                 == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR )
            {
                lpServerInterface->GetCustomCommandsComponent()->ClearLastError();
            }
        }
    }
}
