// ===================================================================================
// BrnNetwork::NetworkPlayerStatsManager  -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h
//
// The online player-stats manager embedded in BrnNetworkManager. It queues GUI stats
// requests, drives the per-player StatsUpdateMessage send/receive exchange, maintains a
// NetworkPlayerStatsResults cache of downloaded records, and uploads free-burn lobby stats
// and offline-progression records through the server-interface custom-commands component.
//
// SHAPE (member names/types/order, the EStatsStatus enum + StatsUpdateEntry nested struct,
// and the full method set) is from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h),
// gated against the X360 ARTIST binary.
//
// MEMBER-TYPE NOTE: the DWARF spells every NetworkPlayerID-keyed parameter / the
// StatsUpdateEntry::mPlayerID member with a drifting nested scope
// (RoadRulesRecvData / GuiEventNetworkLaunching / AggressiveMoveData :: NetworkPlayerID)
// across DIE copies. On X360 this is the single committed top-level typedef
// BrnNetwork::NetworkPlayerID (== s32) from BrnNetworkSharedIO.h; we use that throughout.
//
// No fixed-byte sizeof/offset static_assert is emitted: the PC gate targets x64, where the
// embedded pointers, sub-objects and NetworkPlayerStats records widen and shift every
// absolute byte offset relative to the X360 32-bit layout the bodies were read from. The
// by-name member walk the compiler emits is identical. The X360 absolute offsets are quoted
// in the .cpp only as provenance for the recovered bodies.
#pragma once

#include "types.hpp"
#include "GameSource/Network/Managers/BrnNetworkStatsRequestEventQueue.h"   // StatsRequestEvent / EventQueue<,32>
#include "GameSource/Network/Managers/BrnNetworkPlayerStatsResults.h"       // NetworkPlayerStatsResults / NetworkPlayerStats
#include "GameSource/Network/Messages/BrnStatsUpdateMessage.h"              // StatsUpdateMessage
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                 // NetworkPlayerID
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"                   // BrnNetworkModuleIO::NetworkInOfflineProgression
#include "GameSource/GameState/BrnGameActions.h"                            // BrnGameState::GameStateModuleIO::OnlineGameResults
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                    // CgsSystem::Time

namespace CgsNetwork
{
    class ServerInterfacePlayerInfo;   // pointer member only
    enum  EComponents;                 // ServerBusy / CheckForAndHandleServerErrors param
}

namespace BrnNetwork
{
    class BrnNetworkModule;            // pointer member only (mpNetworkModule)
    class BrnNetworkManager;           // pointer member only (mpNetworkManager)
    class BrnServerInterface;          // pointer member only (mpServerInterface)

    namespace BrnNetworkModuleIO
    {
        struct OutputBuffer;                // ProcessBeforeSimulation / queue-validate param
        struct PostSimulationInputBuffer;   // ProcessAfterSimulation param
    }

    struct NetworkInFreeburnChallengeEvent; // HandleFreeburnChallengeEvent param (other TU)

    namespace StatsInputInterface
    {
        struct StatsInputQueue;             // CopyEvents param (other TU)
    }

    class NetworkPlayerStatsManager
    {
    public:
        typedef CgsSystem::Time Time;       // DWARF/source spell CgsSystem::Time unqualified as `Time`

        // DWARF BrnNetworkPlayerStatsManager.h:69 -- the stats-processing state machine.
        enum EStatsStatus
        {
            E_STATS_STATUS_UNPREPARED                = 0,
            E_STATS_STATUS_IDLE                      = 1,
            E_STATS_STATUS_PROCESSING_EVENT          = 2,
            E_STATS_STATUS_DOWNLOADING_VIEWS         = 3,
            E_STATS_STATUS_SELECTING_VIEW            = 4,
            E_STATS_STATUS_DOWNLOADING_STATS         = 5,
            E_STATS_STATUS_UPLOADING_FREE_BURN_STATS = 6,
            E_STATS_STATUS_OFFLINE_PROGRESSION       = 7,
            E_STATS_STATUS_COUNT                     = 8,
        };

        // ---- public API (DWARF :91..189) ---------------------------------------
        void Construct(BrnNetworkModule* lpNetworkModule);
        bool Prepare();
        void ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer);
        bool Release();
        void Destruct();

        void AddEvent(const StatsRequestEvent* lpEvent);
        const NetworkPlayerStats* GetPlayerStatsByName(const char* lpcName);            // @ 0x825526C8
        const NetworkPlayerStats* GetPlayerStatsByID(NetworkPlayerID lPlayerID);
        const NetworkPlayerStats* GetLocalPlayerStats();
        void AddPlayer(NetworkPlayerID lPlayerID);
        void RemovePlayer(NetworkPlayerID lPlayerID);
        void RequestPlayerStats(const char* lpcName);
        void OnGameStart();
        void HandleGameResults(const BrnGameState::GameStateModuleIO::OnlineGameResults* lpResults);
        void HandleFreeburnChallengeEvent(const NetworkInFreeburnChallengeEvent* lpEvent);
        void ValidateQueueAndPostGettingStatsEvents(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void UploadFreeBurnLobbyStats(const BrnGameState::GameStateModuleIO::OnlineGameResults* lpResults);
        void UploadOfflineProgression(const BrnNetworkModuleIO::NetworkInOfflineProgression* lpOfflineProgression);
        void Disconnected();
        void HandleNewNumberOfRivals();
        void HandleNewNumberOfAchievements(s32 liNumberOfAchievements);

    private:
        // DWARF BrnNetworkPlayerStatsManager.h:194 -- one player's running send/receive update
        // message pair + which player it tracks + a dirty flag.
        struct StatsUpdateEntry
        {
            StatsUpdateMessage mSendMessage;     // +0x00
            StatsUpdateMessage mRecieveMessage;  // +0x30 (sic: spelling matches DWARF)
            NetworkPlayerID    mPlayerID;        // +0x60
            bool               mbOutOfDate;      // +0x64
        };

        // ---- private helpers (DWARF :231..309) ---------------------------------
        void CopyEvents(const StatsInputInterface::StatsInputQueue* lpQueue);
        void AddEventFromNetworkPlayerID(NetworkPlayerID lPlayerID);
        void CopyResultsToOutputBuffer(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void ProcessResults();
        bool CheckForAndHandleServerErrors(CgsNetwork::EComponents leComponent);
        bool CopyNextValidEventOutOfQueue();
        bool ServerBusy(CgsNetwork::EComponents leComponent);
        const char* GetPlayerNameFromID(NetworkPlayerID lPlayerID);
        bool IsThisLocalPlayer(const char* lpcName);
        NetworkPlayerStats* GetNonConstLocalPlayerStats();
        void ClearAllMessaage();                                          // (sic: spelling matches DWARF)
        StatsUpdateEntry* GetStatsUpdateEntry(NetworkPlayerID lPlayerID);
        void MarkNeedsUpdatingFlagInAllPlayers();
        void SendAndRecieveUpdateMessages();
        void UpdatePlayersStats(NetworkPlayerID lPlayerID, s32 liArg1, s32 liArg2, s32 liArg3, s32 liArg4);
        void UpdateLocalPlayersStat(NetworkPlayerStats::EStatsValue leValue, s32 liValue);
        void CheckForLocalPlayerTakedowns();

        // @ 0x82546BB0 -- consume one offline-progression IN-event: cache its freeburn-challenge
        // count, then either upload it now (when armed + idle + the custom-commands component is
        // idle) or buffer it for a later upload. Private helper reached by Process*Simulation;
        // not in the DWARF method list, recovered from the X360 binary.
        void HandleOfflineProgressionEvent(const BrnNetworkModuleIO::NetworkInOfflineProgression* lpEvent);

        // DWARF BrnNetworkPlayerStatsManager.h:202 -- the request-event FIFO capacity.
        static const s32 KI_STATS_EVENT_QUEUE_BUFFER_SIZE = 32;
        // Number of per-player stat-update entries (the maStatsUpdateEntry array length).
        static const s32 KI_NUMBER_OF_STATS_UPDATE_ENTRIES = 7;

        // ---- data layout (DWARF :205..226) -------------------------------------
        EventQueue<StatsRequestEvent, KI_STATS_EVENT_QUEUE_BUFFER_SIZE> mEventQueue; // +0x000
        StatsUpdateEntry          maStatsUpdateEntry[KI_NUMBER_OF_STATS_UPDATE_ENTRIES]; // +0x28C
        StatsRequestEvent         mCurrentEventBeingProcessed;
        NetworkPlayerStatsResults mPlayerStatsCache;                                 // +0x578 (this+1400)
        BrnGameState::GameStateModuleIO::OnlineGameResults mBufferedOnlineStats;
        BrnNetworkModuleIO::NetworkInOfflineProgression mBufferedOfflineProgression; // memcpy target (68 B)
        Time                      mTimeSinceUploadedOfflineProgression;

        BrnNetworkModule*         mpNetworkModule;
        BrnNetworkManager*        mpNetworkManager;
        BrnServerInterface*       mpServerInterface;
        CgsNetwork::ServerInterfacePlayerInfo* mpServerInterfacePlayerInfo;

        EStatsStatus              meCurrentStatus;
        s32                       miCachedNumberOfChallegesCompleted;   // (sic: spelling matches DWARF)
        bool                      mbWaitingToUploadStats;
        bool                      mbWaitingToUploadOfflineProgress;
        bool                      mbWaitingForOfflineProgressFromGamestate;

    public:
        // The C++ constructor (@ 0x827E1110) -- installs the embedded sub-objects and clears the
        // manager's own scalar state. Distinct from Construct() (the Brn lifecycle method).
        NetworkPlayerStatsManager();
    };
}
