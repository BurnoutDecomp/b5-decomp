// ===================================================================================
// BrnNetwork::StandingsManager -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkStandingsManager.h
//
// The online elimination-round standings relay. It keeps a fixed table of per-player slots
// (KI_MAX_PLAYERS == 8), each holding a send/receive PlayerFinishedRoundMessage plus the
// player's recorded finish result (finish time, distance-from-finish, eliminator, eliminations,
// timed-out / won-round flags). When the local player finishes a mode the manager stamps the
// local slot and broadcasts a PlayerFinishedRoundMessage to every peer; inbound messages are
// turned back into the matching slot's result. The countdown timer / state machine drives the
// "waiting for everyone to finish" -> "timing out" -> "showing summary" round-end flow.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkStandingsManager.h),
// gated against the X360 ARTIST binary. The X360 manager-`this` member offsets pin the layout:
//   maStandingsData[8]  @ +0      (8 * 160-byte StandingsData stride == 1280)
//   mpNetworkModule     @ +1280   (a1[320]; HandlePlayerFinishedMode reads a1+1280)
//   mpNetworkManager    @ +1284   (a1[321]; AddPlayer/RemovePlayer test *(a1+1284) == -120,
//                                   i.e. mpNetworkManager->GetPlayerManager() != null at +120)
//   mpTimeManager       @ +1288   (a1[322]; SendPlayerFinishedRoundMessage reads frame/start-frame)
//
// X360-vs-DWARF ORDER DIVERGENCE (rung 1 arbitrates): the PS3 DWARF lists mfCountDownTimer
// BEFORE the three manager pointers, but the X360 asm places the pointers immediately after
// maStandingsData (module @ +1280, manager @ +1284, timeManager @ +1288). The remaining scalar
// state (mfCountDownTimer / meCountdownState / mbLocalPlayerFinished) therefore follows the
// pointers in the X360 build; they are ordered that way below. This project targets SEMANTIC
// parity on the host (x64, 8-byte pointers), so the host sizeof of StandingsData and the manager
// differ from the X360 layout; every member is accessed strictly BY NAME (stride-correct on
// either target). The 160-byte StandingsData stride is documented from the X360 asm only and is
// intentionally NOT static_asserted.
//
// StandingsData layout (X360, from ClearStandingsData @ 0x82545278 / AddPlayer @ 0x82550980 /
// _RoundFinishedMessageArrivedCallback @ 0x82545530 / HandlePlayerFinishedMode @ 0x82550BB8):
//   +0x00 (0)   mPlayerFinishedRoundMessageSend  (PlayerFinishedRoundMessage, 0x40)
//   +0x40 (64)  mPlayerFinishedRoundMessageRecv  (PlayerFinishedRoundMessage, 0x40)
//   +0x80 (128) mFinishTime                      (Time: seconds@+128, fraction@+132)
//   +0x88 (136) mPlayerID                        (NetworkPlayerID; clear == -1)
//   +0x8C (140) mEliminatorNetworkPlayerID       (NetworkPlayerID; clear == -1)
//   +0x90 (144) mfDistanceFromFinish             (f32; clear == -1.0)
//   +0x94 (148) miEliminations                   (s32; clear == 0)
//   +0x98 (152) miReserved0x98                   (s32; only ever cleared to 0 -- DWARF-silent; FLAGGED)
//   +0x9C (156) mbTimedOut                       (bool; clear == 0)
//   +0x9D (157) mbWonRound                       (bool; clear == 0)
//   +0x9E (158) mbResultsReceived                (bool; set to 1 when a result is recorded; clear == 0)
// The DWARF StandingsData lists only {2 messages, mFinishTime, mPlayerID,
// mEliminatorNetworkPlayerID, mfDistanceFromFinish, miEliminations, mbTimedOut}; the X360 build
// additionally stores at +152/+157/+158. miReserved0x98 (+152) is only ever zeroed (no other
// access in this TU) -- DWARF-silent, name unrecovered, FLAGGED. mbWonRound (+157) is the
// round-outcome companion forwarded to PlayerFinishedRoundMessage (FLAGGED name, matching that
// message's own X360-attested mbWonRound). mbResultsReceived (+158) is the "this slot has a
// recorded result" flag the callback/HandlePlayerFinishedMode set to 1 (provisional name).
//
// FUNCTION OWNERSHIP: this TU's ledger covers 10 functions (AddPlayer, ClearStandingsData,
// Construct, Destruct, Disconnected, GetStandingsDataEntry, HandlePlayerFinishedMode,
// RemovePlayer, SendPlayerFinishedRoundMessage, _RoundFinishedMessageArrivedCallback), bodied in
// the sibling .cpp. The remaining declared methods (Prepare/Release/Update/On*/SetState/
// HandleStandingsEvent/CountRecievedFinishTimes/HasSomeoneTimedOut/HasCountdownFinished/
// GetFinishTime/GetReceivedPlayerFinishedRoundMessages/EnsureCountdownTimerStartedOnLocalPlayer-
// Disconnect/_RoundFinishedMessageDeliveredCallback) are declared here for class shape; their
// bodies are NOT in this TU's slice (they land with their own ledger entries).
// ===================================================================================
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"            // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/System/Timer/CgsTime.h"              // CgsSystem::Time
#include "GameSource/Network/Messages/BrnPlayerFinishedRoundMessage.h" // PlayerFinishedRoundMessage, Time alias

namespace BrnGameState
{
    namespace GameStateModuleIO
    {
        struct FinishedModeAction;   // const& param of HandlePlayerFinishedMode (BrnGameActions.h)
    }
}

namespace CgsNetwork
{
    class  TimeManager;        // pointer-only (DWARF h:206)
    struct ReliableMessage;    // _RoundFinishedMessageArrivedCallback param (committed in CgsReliableMessage.h)
    struct SignalMessage;      // _RoundFinishedMessageDeliveredCallback param
}

namespace BrnNetwork
{
    class BrnNetworkModule;   // pointer-only (DWARF h:204)
    class BrnNetworkManager;  // pointer-only (DWARF h:205)

    namespace BrnNetworkModuleIO
    {
        struct OutputBuffer;   // GetReceivedPlayerFinishedRoundMessages param
    }

    // The DWARF/source spell CgsSystem::Time unqualified as `Time` (the established BrnNetwork
    // convention; same alias as BrnPlayerFinishedRoundMessage.h). The message header already
    // typedefs it in this namespace; re-stating it here would be a benign duplicate, so it is
    // reused via that include.

    // The player-table width. The X360 AddPlayer asserts "liIndex < KI_MAX_PLAYERS" and the
    // standings loops all run 8 times (the manager carries 8 StandingsData slots).
    const s32 KI_MAX_PLAYERS = 8;

    // DWARF BrnNetworkStandingsManager.h:61 -- one player's send/receive message pair plus the
    // recorded finish result. See the StandingsData layout note above for the X360 offsets.
    struct StandingsData
    {
        PlayerFinishedRoundMessage mPlayerFinishedRoundMessageSend; // +0x00 (DWARF :64)
        PlayerFinishedRoundMessage mPlayerFinishedRoundMessageRecv; // +0x40 (DWARF :65)
        Time            mFinishTime;                 // +0x80 (DWARF :67)
        NetworkPlayerID mPlayerID;                   // +0x88 (DWARF :69)
        NetworkPlayerID mEliminatorNetworkPlayerID;  // +0x8C (DWARF :70)
        f32             mfDistanceFromFinish;        // +0x90 (DWARF :72)
        s32             miEliminations;              // +0x94 (DWARF :73)
        s32             miReserved0x98;              // +0x98 (X360-only, DWARF-silent; only zeroed -- FLAGGED)
        bool            mbTimedOut;                  // +0x9C (DWARF :75)
        bool            mbWonRound;                  // +0x9D (X360-only, DWARF-omitted -- FLAGGED name)
        bool            mbResultsReceived;           // +0x9E (X360-only; "slot has a result" flag, provisional name)
    };

    struct StandingsManager
    {
        // DWARF BrnNetworkStandingsManager.h:183 -- the round-end countdown state machine.
        enum ECountDownState
        {
            E_COUNTDOWN_STATE_WAITING_FOR_A_PLAYER_TO_FINISH = 0,
            E_COUNTDOWN_STATE_TIMING_OUT                     = 1,
            E_COUNTDOWN_STATE_SHOWING_SUMMARY                = 2,
            E_COUNTDOWN_WAITING_FOR_ROUND_START              = 3,
            E_COUNTDOWN_STATE_COUNT                          = 4,
        };

        // DWARF BrnNetworkStandingsManager.h:193 -- standings-state-machine input events.
        enum EStandingsEvent
        {
            E_STANDINGS_EVENT_REMOTE_RESULTS_RECIEVED          = 0,
            E_STANDINGS_EVENT_LOCAL_PLAYER_CROSSES_FINISH_LINE = 1,
            E_STANDINGS_EVENT_TIMEOUT                          = 2,
            E_STANDINGS_EVENT_COUNT                            = 3,
        };

        // ---- lifecycle / API (DWARF h:94-179) --------------------------------------
        void          Construct(BrnNetworkModule* lpNetworkModule, CgsNetwork::TimeManager* lpTimeManager);
        bool          Prepare();
        bool          Release();
        void          Destruct();
        void          Update(bool lbDiskEjected);
        void          AddPlayer(NetworkPlayerID lPlayerID);
        void          RemovePlayer(NetworkPlayerID lPlayerID);
        void          Disconnected();
        void          OnGameStart();
        void          OnRoundStart();
        void          OnRoundFinish();
        void          OnGameFinish();
        void          OnLeaveGame();
        void          SendPlayerFinishedRoundMessage();
        void          GetReceivedPlayerFinishedRoundMessages(BrnNetworkModuleIO::OutputBuffer* lpOutput);
        bool          HasCountdownFinished() const;
        Time          GetFinishTime(NetworkPlayerID lPlayerID);
        void          HandlePlayerFinishedMode(NetworkPlayerID lPlayerID,
                                               const BrnGameState::GameStateModuleIO::FinishedModeAction* lpFinishedModeAction);
        StandingsData* GetStandingsDataEntry(NetworkPlayerID lPlayerID);
        void          ClearStandingsData();
        void          EnsureCountdownTimerStartedOnLocalPlayerDisconnect();

    private:
        // ---- internals (DWARF h:211-240) -------------------------------------------
        s32  CountRecievedFinishTimes();
        bool HasSomeoneTimedOut();
        void SetState(ECountDownState leNewState);
        void HandleStandingsEvent(EStandingsEvent leEvent);

        // Reliable-message arrival / delivery callbacks (static; registered per peer in AddPlayer).
        static void _RoundFinishedMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                         NetworkPlayerID lSendingPlayerID,
                                                         void* lpUserData);
        static void _RoundFinishedMessageDeliveredCallback(bool lbSuccess, bool lbFakeNack,
                                                           CgsNetwork::SignalMessage* lpAck,
                                                           NetworkPlayerID lRecvingPlayerID,
                                                           void* lpUserData);

        // ---- data layout (X360 order: pointers immediately after the slot table) ---
        StandingsData              maStandingsData[KI_MAX_PLAYERS]; // +0x000 (8 x 160B == 1280)
        BrnNetworkModule*          mpNetworkModule;                 // X360 @ +1280 (a1[320])
        BrnNetworkManager*         mpNetworkManager;                // X360 @ +1284 (a1[321])
        CgsNetwork::TimeManager*   mpTimeManager;                   // X360 @ +1288 (a1[322])
        f32                        mfCountDownTimer;                // DWARF :203 (after the pointers on X360)
        ECountDownState            meCountdownState;                // DWARF :207
        bool                       mbLocalPlayerFinished;           // DWARF :208
    };
} // namespace BrnNetwork
