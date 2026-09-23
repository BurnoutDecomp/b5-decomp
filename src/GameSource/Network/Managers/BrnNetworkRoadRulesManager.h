// ===================================================================================
// BrnNetwork::NetworkRoadRulesManager  -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkRoadRulesManager.h
//
// The online road-rules (challenge high-score) sync manager embedded in BrnNetworkManager.
// It keeps a fixed table of up-to-7 per-player road-rules data slots (keyed by NetworkPlayerID),
// exchanges lobby scores and personal bests with the other players over two reliable message
// types, pulls the road-rules client-config (the server score key + the periodic reset date)
// down at auto-login, and drives the download / upload of road-rules high scores through the
// server's custom-commands component.
//
// LAYOUT (console byte offsets; derived from the constructor, Construct, Destruct, Prepare and
// every body that touches a member):
//   +0x0000  maRoadRulesData[7]             RoadRulesData, stride 0x2B8:
//              +0x000 mRoadRulesMessageSend  (RoadRulesMessage, 0x120)
//              +0x120 mRoadRulesMessageRecv  (RoadRulesMessage, 0x120)
//              +0x240 mRoadRulesPBMessageSend (RoadRulesPersonalBestMessage, 0x38)
//              +0x278 mRoadRulesPBMessageRecv (RoadRulesPersonalBestMessage, 0x38)
//              +0x2B0 mPlayerID, +0x2B4 mIndexOfNextChallengeToSend
//   +0x1308  maLocalRoadScoresToUpload[64]  ChallengePlayerScoreEntry, stride 40
//   +0x1D08  maLocalLobbyScores[64]         ChallengeData, stride 24
//   +0x2308  mRoadRulesPersonalBestBuffer   FifoQueue<NetworkOutRecvRoadRulesPBEvent,14>
//                                            (cursors +0x26F8/+0x26FC/+0x2700)
//   +0x2708  mBufferedRoadRulesRecvQueue    EventQueue<RoadRulesRecvData,14> (length +0x2710)
//   +0x3588  macRoadRulesServerKey[16]
//   +0x3598  mPersonalBestToSendBuffer      FifoQueue<NetworkInRoadRulesPBEvent,2>
//                                            (cursors +0x35F8/+0x35FC/+0x3600)
//   +0x3608  mu64RoadRulesID
//   +0x3610  mTimeUntilNextResultUpload, +0x3618 mTimeUntilNextResultDownload
//   +0x3620  mIndexOfNextChallengeToUpload / +0x3624 ToDownload / +0x3628 LocalToDownload
//   +0x362C  muTimeStampOfLastDownload, +0x3630 miNumRoadsConsideredForUpload
//   +0x3634  meState, +0x3638 mbBufferRoadRulesReceived
//   +0x363C  mpTimeManager, +0x3640 mpPlayerManager, +0x3644 mpServerInterface,
//   +0x3648  mpNetworkModule, +0x364C mbDownloadedLocalScores, +0x364D mbForceOverwriteServerRecords
//   +0x3650  mRoadRulesDebugComponent (0x10)
//   sizeof 0x3660
// The reference outline also lists a third Time (mTimeBetweenRoadRulesDownloadBatches) after
// mTimeUntilNextResultDownload; this build has none: the cursor triplet sits directly at +0x3620
// (Construct/Destruct/OnGameLaunching store it there and nothing touches +0x3620..+0x3627 as a
// Time).
//
// The absolute offsets hold only in a 32-bit build (the embedded messages carry a vptr and the
// event queue a pointer), so they are pinned under `sizeof(void*) != 4 ||`; members are reached
// by name.
#pragma once

#include <cstddef>                                        // offsetof (uncalled _AssertLayout)

#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"                // BrnUpdateSet (ProcessBeforeSimulation)
#include "SharedClasses/StreetData/BrnChallengeData.h"   // BrnStreetData::ChallengePlayerScoreEntry / ChallengeData
#include "GameShared/GameClasses/Containers/CgsFifoQueue.h"  // FifoQueue<T,N>
#include "GameShared/GameClasses/System/Timer/CgsTime.h"     // CgsSystem::Time (mTimeUntilNextResult*)
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // GameStateModuleIO::GameActionQueue (ProcessGameActions)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // NetworkPlayerID, RoadRulesMessageData, Road::ChallengeIndex
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // NetworkToGameStateInterface::RoadRulesReceivedQueue
#include "GameSource/Network/BrnNetworkModuleIO.h"           // NetworkOutRecvRoadRulesPBEvent, NetworkEventQueue
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"    // NetworkInRoadRulesPBEvent
#include "GameSource/Network/Messages/BrnRoadRulesMessage.h"             // RoadRulesMessage
#include "GameSource/Network/Messages/BrnRoadRulesPersonalBestMessage.h" // RoadRulesPersonalBestMessage
#include "GameSource/Network/Debug Components/BrnNetworkRoadRulesManagerDebugComponent.h"  // mRoadRulesDebugComponent

namespace BrnNetwork
{
    class BrnNetworkModule;    // pointer-only forward (mpNetworkModule)
    class BrnServerInterface;  // pointer-only forward (mpServerInterface)
    class RoadRulesUploadData; // GetRoadRulesDataToUpload param (Parameters/BrnNetworkRoadRulesData.h)
}

namespace CgsNetwork
{
    struct ReliableMessage;    // message-arrived callback param
    struct SignalMessage;      // message-delivered callback param
    struct TimeManager;        // pointer-only forward (mpTimeManager)
    struct PlayerManager;      // pointer-only forward (mpPlayerManager)
}

namespace BrnNetwork
{
    class NetworkRoadRulesManager
    {
    public:
        // The C++ constructor: the embedded messages and the debug component install their
        // vtables; the two result timers are zeroed.
        NetworkRoadRulesManager();

        // ---- lifecycle / per-frame / per-player (called by BrnNetworkManager) ----------------
        void Construct();
        bool Release();
        void Destruct();
        void ProcessBeforeSimulation( BrnNetworkModuleIO::OutputBuffer* lpOutput, f32 lfGameTimeStep,
                                      BrnUpdateSet lUpdateSet );
        void AddPlayer( NetworkPlayerID lPlayerID );
        void RemovePlayer( NetworkPlayerID lPlayerID );
        void Disconnected();

        // Route one new personal best into the local upload table or the lobby table (and
        // queue it for sending). Public: the debug component's "Trigger Personal Best" calls it.
        void HandleNewPersonalBest( const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent );

        // Debug "Get Road Rules High Scores": kick off the server-side road-rules score download.
        void StartDownloadingRoadRulesScoresFromServer();

        // Auto-login hook: pull the road-rules client-config and kick off the download/upload
        // cycle, or report the auto-login step complete once it has already run.
        void OnAutoLogin();

        // Latch the four collaborator pointers and reset the per-cycle scalars.
        bool Prepare( BrnNetworkModule* lpNetworkModule, BrnServerInterface* lpServerInterface,
                      CgsNetwork::PlayerManager* lpPlayerManager, CgsNetwork::TimeManager* lpTimeManager );

        void OnEnterGame();
        void OnGameLaunching();
        void OnGameFinish();
        void OnRoundFinish();
        void OnLeaveGame();

        // Per-frame tick: network events, game actions, personal-best / lobby-score sends, then
        // the active upload/download step.
        void ProcessAfterSimulation( const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput,
                                     bool lbAreWePlaying );

        void StartUploadingRoadRulesScoresToServer();
        void StartDownloadingLocalRoadRulesScoresFromServer();
        void HandleDownloadingRoadRulesScores();
        void HandleDownloadingLocalRoadRulesScores();

    private:
        // Number of per-player road-rules data slots (the maRoadRulesData array length).
        static const s32 KI_NUMBER_OF_PLAYER_DATA_SLOTS = 7;

        // Number of local challenge-score slots (BrnGameState::KI_MAX_CHALLENGES).
        static const s32 KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS = 64;

        // Length of the server score-key string buffer.
        static const s32 KI_SERVER_KEY_LENGTH = 16;

        // meState values. 1/2/3 are grounded by the per-frame dispatch (upload / download /
        // local download handlers) and the matching callback state checks; 4 by the
        // "meState != E_ROAD_RULES_STATE_IN_GAME" asserts. The reference enum spells these
        // with different numbers; this build's values are the ones below.
        enum ERoadRulesState
        {
            E_ROAD_RULES_STATE_BOOTING           = 0,
            E_ROAD_RULES_STATE_UPLOADING         = 1,
            E_ROAD_RULES_STATE_DOWNLOADING       = 2,
            E_ROAD_RULES_STATE_DOWNLOADING_LOCAL = 3,
            E_ROAD_RULES_STATE_IN_GAME           = 4,
        };

        // The idle-online state entered once auto-login has run and after every finished
        // upload/download batch. Its enumerator name is not recovered (the reference's remaining
        // names are CONNECTING and WAIT_TO_DOWNLOAD), so it stays a named literal.
        static const s32 KI_STATE_AUTO_LOGIN_PRIMED = 5;

        typedef FifoQueue<BrnNetworkModuleIO::NetworkOutRecvRoadRulesPBEvent, 14> RoadRulesPersonalBestBuffer;
        typedef FifoQueue<BrnNetworkModuleIO::NetworkInRoadRulesPBEvent, 2>       RoadRulesPBBuffer;

        // One per-player road-rules data slot (console stride 0x2B8).
        struct RoadRulesData
        {
            RoadRulesMessage             mRoadRulesMessageSend;        // +0x000
            RoadRulesMessage             mRoadRulesMessageRecv;        // +0x120
            RoadRulesPersonalBestMessage mRoadRulesPBMessageSend;      // +0x240
            RoadRulesPersonalBestMessage mRoadRulesPBMessageRecv;      // +0x278
            NetworkPlayerID              mPlayerID;                    // +0x2B0 (-1 == free slot)
            Road::ChallengeIndex         mIndexOfNextChallengeToSend;  // +0x2B4
        };

        // The slot keyed by lPlayerID (asserts the id is valid), or nullptr.
        RoadRulesData* GetRoadRulesDataEntry( NetworkPlayerID lPlayerID );
        // The first free slot, or nullptr after asserting when the table is full.
        RoadRulesData* GetNextFreeRoadRulesDataEntry();

        void StartSendingRoadRulesScoresToPlayer( RoadRulesData* lpPlayerDataEntry );
        void ProcessNetworkEvents( const BrnNetworkModuleIO::NetworkEventQueue* lpNetworkEventQueue );
        void ProcessGameActions( const BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue );
        void UpdateLocalRoadRulesScoresWithNewPB( const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent );
        void UpdateLocalLobbyScoresWithNewPB( const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent );
        void SendPersonalBestScore();
        s32  AttemptToUploadNewRoadRulesScores();
        u32  AttemptToDownloadRoadRulesHighScores();
        u32  AttemptToDownloadLocalRoadRulesHighScores();
        void HandleSendingRoadRulesScores( bool lbAreWePlaying );
        // Fill lpRoadRulesMessageData with up to one message's worth of lobby scores from
        // lStartIndex on; returns how many challenge slots were walked.
        Road::ChallengeIndex GetRoadRulesDataToSend( RoadRulesMessageData* lpRoadRulesMessageData,
                                                     s32* lpiNumScores, Road::ChallengeIndex lStartIndex );
        void HandleUploadingRoadRulesScores();
        void GetRoadRulesDataToUpload( RoadRulesUploadData* lpUploadData );

        // Reliable-message callbacks registered per player by AddPlayer (user data == manager).
        static void _RoadRulesMessageArrivedCallback( CgsNetwork::ReliableMessage* lpMessage,
                                                      NetworkPlayerID lSendingPlayerID, void* lpUserData );
        static void _RoadRulesMessageDeliveredCallback( bool lbDelivered, bool lbFakeNack,
                                                        CgsNetwork::SignalMessage* lpAck,
                                                        NetworkPlayerID lRecvingPlayerID, void* lpUserData );
        static void _RoadRulesPersonalBestArrivedCallback( CgsNetwork::ReliableMessage* lpMessage,
                                                           NetworkPlayerID lSendingPlayerID, void* lpUserData );
        static void _RoadRulesPersonalBestDeliveredCallback( bool lbDelivered, bool lbFakeNack,
                                                             CgsNetwork::SignalMessage* lpAck,
                                                             NetworkPlayerID lRecvingPlayerID, void* lpUserData );

        // ServerInterfaceCustomCommands completion callbacks (user data == manager).
        static void _UploadRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess );
        static void _DownloadRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess );
        static void _DownloadLocalRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess );

        // ---- data layout (console offsets in the banner above) --------------------------------
        RoadRulesData                                     maRoadRulesData[KI_NUMBER_OF_PLAYER_DATA_SLOTS];               // +0x0000
        BrnStreetData::ChallengePlayerScoreEntry          maLocalRoadScoresToUpload[KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS]; // +0x1308
        BrnStreetData::ChallengeData                      maLocalLobbyScores[KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS];        // +0x1D08
        RoadRulesPersonalBestBuffer                       mRoadRulesPersonalBestBuffer;                                  // +0x2308
        BrnNetworkModuleIO::NetworkToGameStateInterface::RoadRulesReceivedQueue mBufferedRoadRulesRecvQueue;           // +0x2708
        char                                              macRoadRulesServerKey[KI_SERVER_KEY_LENGTH];                   // +0x3588
        RoadRulesPBBuffer                                 mPersonalBestToSendBuffer;                                     // +0x3598
        u64                                               mu64RoadRulesID;                                               // +0x3608
        CgsSystem::Time                                   mTimeUntilNextResultUpload;                                    // +0x3610
        CgsSystem::Time                                   mTimeUntilNextResultDownload;                                  // +0x3618
        Road::ChallengeIndex                              mIndexOfNextChallengeToUpload;                                 // +0x3620 (64 == none)
        Road::ChallengeIndex                              mIndexOfNextChallengeToDownload;                               // +0x3624 (64 == none)
        Road::ChallengeIndex                              mIndexOfNextLocalChallengeToDownload;                          // +0x3628 (64 == none)
        u32                                               muTimeStampOfLastDownload;                                     // +0x362C
        s32                                               miNumRoadsConsideredForUpload;                                 // +0x3630
        s32                                               meState;                                                       // +0x3634
        bool                                              mbBufferRoadRulesReceived;                                     // +0x3638
        CgsNetwork::TimeManager*                          mpTimeManager;                                                 // +0x363C
        CgsNetwork::PlayerManager*                        mpPlayerManager;                                               // +0x3640
        BrnServerInterface*                               mpServerInterface;                                             // +0x3644
        BrnNetworkModule*                                 mpNetworkModule;                                               // +0x3648
        bool                                              mbDownloadedLocalScores;                                       // +0x364C
        bool                                              mbForceOverwriteServerRecords;                                 // +0x364D
        RoadRulesManagerDebugComponent                    mRoadRulesDebugComponent;                                      // +0x3650

        // Uncalled layout pin (console layout; inert on the 64-bit host).
        static void _AssertLayout()
        {
            static_assert( sizeof(void*) != 4 || sizeof(RoadRulesData) == 0x2B8, "RoadRulesData stride 0x2B8" );
            static_assert( sizeof(void*) != 4 || offsetof(RoadRulesData, mRoadRulesMessageRecv) == 0x120, "mRoadRulesMessageRecv @ +0x120" );
            static_assert( sizeof(void*) != 4 || offsetof(RoadRulesData, mRoadRulesPBMessageSend) == 0x240, "mRoadRulesPBMessageSend @ +0x240" );
            static_assert( sizeof(void*) != 4 || offsetof(RoadRulesData, mRoadRulesPBMessageRecv) == 0x278, "mRoadRulesPBMessageRecv @ +0x278" );
            static_assert( sizeof(void*) != 4 || offsetof(RoadRulesData, mPlayerID) == 0x2B0, "mPlayerID @ +0x2B0" );
            static_assert( sizeof(void*) != 4 || offsetof(RoadRulesData, mIndexOfNextChallengeToSend) == 0x2B4, "mIndexOfNextChallengeToSend @ +0x2B4" );

            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, maLocalRoadScoresToUpload) == 0x1308, "maLocalRoadScoresToUpload @ +0x1308" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, maLocalLobbyScores) == 0x1D08, "maLocalLobbyScores @ +0x1D08" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, mRoadRulesPersonalBestBuffer) == 0x2308, "mRoadRulesPersonalBestBuffer @ +0x2308" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, mBufferedRoadRulesRecvQueue) == 0x2708, "mBufferedRoadRulesRecvQueue @ +0x2708" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, macRoadRulesServerKey) == 0x3588, "macRoadRulesServerKey @ +0x3588" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, mPersonalBestToSendBuffer) == 0x3598, "mPersonalBestToSendBuffer @ +0x3598" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, mu64RoadRulesID) == 0x3608, "mu64RoadRulesID @ +0x3608" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, mTimeUntilNextResultUpload) == 0x3610, "mTimeUntilNextResultUpload @ +0x3610" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, mIndexOfNextChallengeToUpload) == 0x3620, "mIndexOfNextChallengeToUpload @ +0x3620" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, meState) == 0x3634, "meState @ +0x3634" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, mbBufferRoadRulesReceived) == 0x3638, "mbBufferRoadRulesReceived @ +0x3638" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, mpTimeManager) == 0x363C, "mpTimeManager @ +0x363C" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, mbDownloadedLocalScores) == 0x364C, "mbDownloadedLocalScores @ +0x364C" );
            static_assert( sizeof(void*) != 4 || offsetof(NetworkRoadRulesManager, mRoadRulesDebugComponent) == 0x3650, "mRoadRulesDebugComponent @ +0x3650" );
            static_assert( sizeof(void*) != 4 || sizeof(NetworkRoadRulesManager) == 0x3660, "NetworkRoadRulesManager is 0x3660 bytes" );
        }
    };
}
