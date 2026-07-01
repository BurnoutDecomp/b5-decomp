// ===================================================================================
// BrnNetwork::NetworkRoadRulesManager  -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkRoadRulesManager.h
//
// The online road-rules (challenge high-score) sync manager embedded in BrnNetworkManager.
// It keeps a fixed table of up-to-7 per-player road-rules data slots (keyed by NetworkPlayerID),
// pulls the road-rules client-config (the score-key string + the periodic reset date) down at
// auto-login, and drives the download / upload of road-rules high scores through the server.
//
// SHAPE: DecFIGS publishes a full DWARF outline for this class (BrnNetworkRoadRulesManager.h,
// gated on the X360 ledger below) which names every top-level member; every one of those names
// used here is independently re-derived and byte-confirmed against the X360 ARTIST pseudocode
// (constructor @ 0x827E2BE8, Construct/Destruct/Prepare/Process*/On*/Start*/Handle* bodies) --
// see the per-field X360 byte-offset provenance comments below. Sub-object layout INSIDE
// maRoadRulesData[]'s RoadRulesMessage/RoadRulesPersonalBestMessage members and inside the
// mRoadRulesPersonalBestBuffer / mBufferedRoadRulesRecvQueue FIFO containers is NOT re-derived
// (no function homed in this TU needs to read inside them by name yet) and is left as opaque,
// correctly-sized/-offset storage rather than fabricated.
//
// LAYOUT (absolute X360 byte offsets, the spine of the recovered bodies):
//   +0x0000  maRoadRulesData[7]                    -- 7 RoadRulesData slots, stride 0x2B8 (696 B)
//                                                      (each slot's key NetworkPlayerID sits at
//                                                       slot-relative +0x2B0, mIndexOfNextChallengeToSend
//                                                       at +0x2B4; DWARF BrnNetworkRoadRulesManager.h:212)
//   +0x1308  maLocalRoadScoresToUpload[64]          -- BrnStreetData::ChallengePlayerScoreEntry, stride 40 B
//   +0x1D08  maLocalLobbyScores[64]                 -- BrnStreetData::ChallengeData, stride 24 B
//   +0x2308  mRoadRulesPersonalBestBuffer           -- FifoQueue<NetworkOutRecvRoadRulesPBEvent,14> (opaque)
//   +0x2708  mBufferedRoadRulesRecvQueue             -- RoadRulesReceivedQueue (opaque; ctor inits @+0x2708/+0x270C/+0x2710)
//   +0x3588  maRoadRulesScoreKey[16]                -- "ROAD_RULES_SKEY" client-config string buffer
//   +0x3598  (opaque)                                -- mPersonalBestToSendBuffer: FifoQueue<NetworkInRoadRulesPBEvent,2> (opaque)
//   +0x3608  mu64RoadRulesID                        -- uint64_t (low word touched by ProcessNetworkEvents)
//   +0x3610  mTimeUntilNextResultUpload             -- CgsSystem::Time (8 B)
//   +0x3618  mTimeUntilNextResultDownload           -- CgsSystem::Time (8 B)
//   +0x3620  mIndexOfNextChallengeToUpload          -- s32 (sentinel 64 == none)
//   +0x3624  mIndexOfNextChallengeToDownload        -- s32 (sentinel 64 == none)
//   +0x3628  mIndexOfNextLocalChallengeToDownload   -- s32 (sentinel 64 == none)
//   +0x362C  muTimeStampOfLastDownload              -- u32
//   +0x3630  miNumRoadsConsideredForUpload          -- s32
//   +0x3634  meState                                -- state word; asm-confirmed literals: 0 == booting,
//                                                      1/2/3 == ProcessAfterSimulation's Uploading/
//                                                      Downloading/DownloadingLocal dispatch, 5 ==
//                                                      auto-login primed. These do NOT line up with the
//                                                      DWARF ERoadRulesState enum (UPLOADING=4,
//                                                      DOWNLOADING=5, DOWNLOADING_LOCAL=6) -- a version/
//                                                      merge-window drift between DecFIGS (PS3 Dec-2007)
//                                                      and this ARTIST (X360 Jan-2008) build; the type is
//                                                      kept as plain s32 rather than the DWARF enum so no
//                                                      wrong name is attached to a value.
//   +0x3638  mbBufferRoadRulesReceived              -- bool (word-store idiom; ProcessGameActions PrepareForMode/StopMode)
//   +0x363C  mpTimeManager                          -- CgsNetwork::TimeManager*
//   +0x3640  mpPlayerManager                        -- CgsNetwork::PlayerManager*
//   +0x3644  mpServerInterface                      -- BrnNetwork::BrnServerInterface*
//   +0x3648  mpNetworkModule                        -- BrnNetwork::BrnNetworkModule*
//   +0x364C  mbDownloadedLocalScores                -- bool
//   +0x364D  mbForceOverwriteServerRecords          -- bool
//   +0x3650  mRoadRulesDebugComponent               -- RoadRulesManagerDebugComponent (opaque; ctor installs its vtable)
//
// No fixed-byte sizeof/offset static_assert is emitted for the class as a whole: the PC gate
// targets x64, where the embedded pointers and per-slot sub-objects widen and shift every
// absolute byte offset relative to the X360 32-bit layout the bodies were read from. The
// by-name member walk the compiler emits is identical. The X360 absolute offsets are quoted
// in the .cpp only as provenance. RoadRulesData's OWN stride (0x2B8, pointer-free) and its key
// sub-offset (+0x2B0) ARE pointer-invariant and are pinned by a static_assert below.
#pragma once

#include <cstddef>                                        // offsetof (uncalled _AssertLayout)

#include "types.hpp"
#include "SharedClasses/StreetData/BrnChallengeData.h"   // BrnStreetData::ChallengePlayerScoreEntry (HandleNewPersonalBest param)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/System/Timer/CgsTime.h"     // CgsSystem::Time (mTimeUntilNextResult*)

namespace BrnNetwork
{
    class BrnNetworkModule;    // pointer-only forward (mpNetworkModule)
    class BrnServerInterface;  // pointer-only forward (mpServerInterface)

    namespace BrnNetworkModuleIO
    {
        struct PostSimulationInputBuffer;  // pointer-only forward (ProcessAfterSimulation param)
        class  NetworkEventQueue;          // pointer-only forward (ProcessNetworkEvents param)
    }
}

namespace CgsNetwork
{
    struct TimeManager;    // pointer-only forward (mpTimeManager)
    struct PlayerManager;  // pointer-only forward (mpPlayerManager)
}

namespace BrnNetwork
{
    class NetworkRoadRulesManager
    {
    public:
        // The C++ constructor (@ 0x827E2BE8) -- installs the embedded per-slot sub-object vtables
        // and clears the manager's own score/time scalar pair. Distinct from any Brn lifecycle
        // Construct().
        NetworkRoadRulesManager();

        // Debug "Trigger Personal Best": feed a fabricated personal-best record through the normal
        // new-PB pipeline (the debug component builds the record on the stack and hands it over).
        void HandleNewPersonalBest( BrnStreetData::ChallengePlayerScoreEntry* lpScoreEntry );

        // Debug "Get Road Rules High Scores": kick off the server-side road-rules score download.
        void StartDownloadingRoadRulesScoresFromServer();

        // @ 0x82564338 -- auto-login hook. Once the auto-login process has been primed (miState != 0)
        // it just signals the network manager that the process completed; before then, when the
        // current game mode warrants it, it pulls the road-rules client-config (score-key + reset
        // date), posts the reset-date network event, and kicks off the high-score download/upload.
        void OnAutoLogin();

        // @ 0x8255C808 -- BrnNetworkManager lifecycle: latch the four collaborator pointers and
        // reset the per-cycle scalars (state -> booting, local-scores flag cleared).
        bool Prepare( BrnNetworkModule* lpNetworkModule, BrnServerInterface* lpServerInterface,
                      CgsNetwork::PlayerManager* lpPlayerManager, CgsNetwork::TimeManager* lpTimeManager );

        // @ 0x8254A4B0 -- BrnNetworkManager::OnEnterGame forward: re-construct every local-lobby
        // challenge-data slot (in place, matching the binary's per-slot Construct() re-run).
        void OnEnterGame();

        // @ 0x8254A500 -- BrnNetworkManager::OnGameLaunching forward: clear the outstanding
        // download indices and, if an auto-login pull was pending/primed, signal it complete.
        void OnGameLaunching();

        // @ 0x82554D30 -- BrnNetworkManager::OnGameFinish forward: once the local player has left
        // the game component's active game, clear the download indices; always clear meState.
        void OnGameFinish();

        // @ 0x82554DE8 -- BrnNetworkManager::OnRoundFinish forward: same local-player-left check as
        // OnGameFinish, but only clears state+indices together (no unconditional meState clear).
        void OnRoundFinish();

        // @ 0x82554EA0 -- BrnNetworkManager::OnLeaveGame forward: asserts the local player already
        // left the game component's active game; drops back to the booting state unless a
        // download/upload cycle is in flight, and always clears the download indices.
        void OnLeaveGame();

        // @ 0x8255CA48 -- ProcessAfterSimulation callee: walk the game-action queue, flip
        // mbBufferRoadRulesReceived for the PrepareForMode(23)/StopMode(39) action ids.
        void ProcessGameActions( const void* lpGameActionQueue );

        // @ 0x8256F648 -- BrnNetworkManager::ProcessAfterSimulation forward: the manager's per-frame
        // tick -- process buffered network events + game actions, try to send this frame's personal
        // best / high-score messages, then step whichever upload/download state is active.
        void ProcessAfterSimulation( const BrnNetworkModuleIO::PostSimulationInputBuffer* lpPostSimInput, bool lbHandleSendFlag );

        // @ 0x8254A0B8 -- kick off a road-rules score upload cycle: reset the upload cursor/counter
        // and the inter-upload timer. Reached from AttemptToUploadNewRoadRulesScores.
        void StartUploadingRoadRulesScoresToServer();

        // @ 0x8254A360 -- kick off a LOCAL road-rules score download cycle: reset the local download
        // cursor and the inter-download timer. Reached from AttemptToDownloadLocalRoadRulesHighScores.
        void StartDownloadingLocalRoadRulesScoresFromServer();

        // @ 0x825611D0 -- ProcessAfterSimulation callee: while a download cycle is active and the
        // inter-download timer has elapsed, request the next batch (<=10) of global road-rules high
        // scores from the server's custom-commands component.
        void HandleDownloadingRoadRulesScores();

        // @ 0x82561370 -- ProcessAfterSimulation callee: while a local-download cycle is active and
        // the inter-download timer has elapsed, request the next batch (<=10) of LOCAL road-rules
        // high scores from the server's custom-commands component.
        void HandleDownloadingLocalRoadRulesScores();

    private:
        // Number of per-player road-rules data slots (the maRoadRulesData array length).
        static const s32 KI_NUMBER_OF_PLAYER_DATA_SLOTS = 7;

        // Number of local-lobby / local-upload challenge-score slots (BrnGameState::KI_MAX_CHALLENGES).
        static const s32 KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS = 64;

        // One per-player road-rules data slot (X360 stride 0x2B8 == 696 B; DWARF
        // BrnNetworkRoadRulesManager.h:212 RoadRulesData). Its message sub-objects
        // (mRoadRulesMessageSend/Recv, mRoadRulesPBMessageSend/Recv) are not named here -- no
        // function homed in this TU reads inside them by name yet -- so the slot keeps them as
        // opaque storage; the two grounded scalar members the recovered bodies DO touch by name
        // (mPlayerID @ slot-relative +0x2B0, mIndexOfNextChallengeToSend @ +0x2B4) are named.
        struct RoadRulesData
        {
            u8              maOpaqueMessages[0x2B0];        // mRoadRulesMessage{Send,Recv}, mRoadRulesPBMessage{Send,Recv}
            NetworkPlayerID mPlayerID;                       // slot-relative +0x2B0 (key; -1 == free slot)
            s32             mIndexOfNextChallengeToSend;      // slot-relative +0x2B4 (StartSendingRoadRulesScoresToPlayer)
            // (mIndexOfNextChallengeToSend ends exactly at the 0x2B8 stride -- no tail pad needed)
        };

        // X360 0x82549F60 -- return the first free (mPlayerID == -1) slot, or nullptr after asserting
        // when the table is full. Reached by AddPlayer.
        RoadRulesData* GetNextFreeRoadRulesDataEntry();

        // X360 0x82549ED0 -- return the slot keyed by lPlayerID (asserts lPlayerID is valid first),
        // or nullptr when the player has no slot. Reached by RemovePlayer.
        RoadRulesData* GetRoadRulesDataForPlayer( NetworkPlayerID lPlayerID );

        // ---- helpers homed in THIS TU's .cpp; declared (callees of OnAutoLogin) ---------------
        // Bodies belong to this manager's own behavioural TUs (no ground-truth asm in this TU),
        // so they are declared-only here and as sibling forwards in the .cpp.
        u32  AttemptToDownloadLocalRoadRulesHighScores();
        u32  AttemptToDownloadRoadRulesHighScores();
        s32  AttemptToUploadNewRoadRulesScores();

        // ---- ProcessAfterSimulation siblings not bodied in this pass (thread RoadRulesData /
        //      FIFO-buffer sub-object offsets, or an unrecovered GetRoadRulesDataToUpload output
        //      struct, this TU does not yet name -- see class banner) --------------------------
        void ProcessNetworkEvents( const BrnNetworkModuleIO::NetworkEventQueue* lpNetworkEventQueue );
        void SendPersonalBestScore();
        // X360 a3 (HandleSendingRoadRulesScores(a1, a3)) is ProcessAfterSimulation's own bool
        // second parameter passed straight through -- DWARF doesn't otherwise name this param.
        void HandleSendingRoadRulesScores( bool lbHandleSendFlag );

        // @ 0x8256B7F8 -- its finish-vs-continue branch depends on GetRoadRulesDataToUpload's
        // (undeclared, X360-only) output record/count, so faithfully bodying it would require
        // fabricating that struct's shape; left declared-only rather than guessed.
        void HandleUploadingRoadRulesScores();

        // ---- ServerInterfaceCustomCommands::CustomCommandCallback targets (declared-only: their
        //      bodies thread maLocalRoadScoresToUpload/RoadRulesData sub-object offsets not named
        //      in this pass -- @ 0x8255D0C0 / 0x8255D4F8 / 0x82564730) ------------------------------
        static void _DownloadRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess );
        static void _DownloadLocalRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess );
        static void _UploadRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess );

        // ---- data layout (X360 absolute offsets in the class banner above) --------------------
        RoadRulesData maRoadRulesData[KI_NUMBER_OF_PLAYER_DATA_SLOTS];                         // +0x0000
        BrnStreetData::ChallengePlayerScoreEntry maLocalRoadScoresToUpload[KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS]; // +0x1308
        BrnStreetData::ChallengeData             maLocalLobbyScores[KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS];       // +0x1D08
        u8            maOpaquePersonalBestBuffer[0x2708 - 0x2308];      // +0x2308 mRoadRulesPersonalBestBuffer
        u8            maOpaqueRecvQueue[0x3588 - 0x2708];               // +0x2708 mBufferedRoadRulesRecvQueue
        char          maRoadRulesScoreKey[16];                          // +0x3588 ("ROAD_RULES_SKEY")
        u8            maOpaquePersonalBestToSendBuffer[0x3608 - 0x3598]; // +0x3598 mPersonalBestToSendBuffer
        u64           mu64RoadRulesID;                                  // +0x3608
        CgsSystem::Time mTimeUntilNextResultUpload;                     // +0x3610
        CgsSystem::Time mTimeUntilNextResultDownload;                   // +0x3618
        s32           mIndexOfNextChallengeToUpload;                    // +0x3620 (sentinel 64)
        s32           mIndexOfNextChallengeToDownload;                  // +0x3624 (sentinel 64)
        s32           mIndexOfNextLocalChallengeToDownload;             // +0x3628 (sentinel 64)
        u32           muTimeStampOfLastDownload;                        // +0x362C
        s32           miNumRoadsConsideredForUpload;                    // +0x3630
        s32           meState;                                          // +0x3634 (ERoadRulesState)
        bool          mbBufferRoadRulesReceived;                       // +0x3638
        u8            maPadToTimeManager[0x363C - 0x3639];              // -> +0x363C
        CgsNetwork::TimeManager*   mpTimeManager;                      // +0x363C
        CgsNetwork::PlayerManager* mpPlayerManager;                    // +0x3640
        BrnServerInterface* mpServerInterface;                         // +0x3644
        BrnNetworkModule*   mpNetworkModule;                           // +0x3648
        bool          mbDownloadedLocalScores;                        // +0x364C
        bool          mbForceOverwriteServerRecords;                  // +0x364D
        u8            maPadToDebugComponent[0x3650 - 0x364E];          // -> +0x3650
        void*         mRoadRulesDebugComponentVtable;                  // +0x3650 (opaque RoadRulesManagerDebugComponent)

        // Uncalled layout pin. RoadRulesData holds no pointers, so its stride (X360 slot stride
        // 0x2B8) and the key offset (slot-relative +0x2B0) are pointer-INVARIANT and hold on the
        // LLP64 gate host -- they are the spine of the GetRoadRules* slot scans.
        static void _AssertLayout()
        {
            static_assert( sizeof(RoadRulesData) == 0x2B8,
                           "RoadRulesData stride must be 696 (X360 0x2B8)" );
            static_assert( offsetof(RoadRulesData, mPlayerID) == 0x2B0,
                           "RoadRulesData key must sit at slot-relative +0x2B0" );
            static_assert( offsetof(RoadRulesData, mIndexOfNextChallengeToSend) == 0x2B4,
                           "RoadRulesData next-challenge-to-send must sit at slot-relative +0x2B4" );
        }
    };
}
