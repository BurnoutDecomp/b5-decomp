// ===================================================================================
// BrnNetwork::StandingsManager -- implementation
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkStandingsManager.cpp
//
// Reconstructed from the X360 ARTIST pseudocode + ASM (spine), the DecFIGS DWARF (shape), and
// the project conventions. All member / interface access is BY NAME through the reconstructed
// headers; no raw offset pointer arithmetic. This TU's ledger covers 10 functions:
//   Construct, Destruct, ClearStandingsData, AddPlayer, RemovePlayer, Disconnected,
//   GetStandingsDataEntry, HandlePlayerFinishedMode, SendPlayerFinishedRoundMessage,
//   _RoundFinishedMessageArrivedCallback.
//
// The X360 build interleaves dev-only CgsDev::StrStream debug formatting into the assert paths
// (e.g. "Bad distance ..."); the project assert macro CGS_ASSERT carries the condition message
// and supplies file/line itself, so the streamed numeric formatting (sub_821F0F40 et al.) has no
// observable effect and is folded into the CGS_ASSERT at each site.
// ===================================================================================
#include "GameSource/Network/Managers/BrnNetworkStandingsManager.h"

#include "GameSource/Network/BrnNetworkModule.h"                                // BrnNetworkModule::GetNetworkManager / GetGameStateToNetworkInterface
#include "GameSource/Network/BrnNetworkManager.h"                               // BrnNetworkManager::GetPlayerManager / GetCurrentRoundNumber
#include "GameSource/Network/BrnNetworkPlayer.h"                                 // BrnNetworkPlayer::SetHasWonRound (the registry stores BrnNetworkPlayer-derived peers)
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"  // GameStateToNetworkInterface::GetNetworkPlayerID
#include "GameSource/GameState/BrnGameActions.h"                                // FinishedModeAction
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"            // PlayerManager::GetPlayerByID / GetNextPlayerID / GetNextLocalPlayerID
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"            // NetworkPlayer::RegisterMessageType / UnRegisterMessageType / GetRegisteredSendMessage
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"                 // CgsNetwork::TimeManager
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnNetwork
{
    // The reliable-message type id the per-player send/recv PlayerFinishedRoundMessages register
    // under (X360 RegisterMessageType(player, 12, ...) / UnRegisterMessageType(player, 12) /
    // GetRegisteredSendMessage(player, 12)). Named here for the (Un)Register / lookup calls.
    static const s32 KI_PLAYER_FINISHED_ROUND_MESSAGE_TYPE = 12;

    // The packed wire length RegisterMessageType reserves for the message (X360 passes 64 == 0x40).
    static const s32 KI_PLAYER_FINISHED_ROUND_MESSAGE_LENGTH = 64;

    // "No player" sentinel, mirroring the sibling Network TUs' file-local constant (the X360
    // asserts spell "CgsNetwork::K_INVALID_PLAYER_ID"; NetworkPlayerID == s32, sentinel == -1).
    static const NetworkPlayerID K_INVALID_PLAYER_ID = -1;

    // The "no distance recorded" distance sentinel. ClearStandingsData stores it into every slot's
    // mfDistanceFromFinish and SendPlayerFinishedRoundMessage asserts the value is >= it (X360
    // flt_820037C8, which the decompiler resolves to -1.0).
    static const f32 KF_INVALID_DISTANCE_FROM_FINISH = -1.0f;

    // FLAGGED / UNRESOLVED CONSTANT: HandlePlayerFinishedMode asserts the just-stored distance is
    // NOT equal to a distinct "bad distance" sentinel (X360 flt_82F29918, compared with `==`). That
    // float symbol's literal value is NOT recoverable from this TU's data (the decompiler left it
    // un-resolved, unlike flt_820037C8 == -1.0), so it is NOT fabricated here. The assert is a
    // dev-only guard with no gameplay side effect; the placeholder below is intentionally set equal
    // to the -1.0 cleared-distance sentinel so the guard NEVER fires on a freshly-cleared slot
    // (the only safe, non-fabricating choice). Re-ground KF_BAD_DISTANCE_SENTINEL against
    // flt_82F29918 when its rodata value is available.
    static const f32 KF_BAD_DISTANCE_SENTINEL = -1.0f;   // FLAGGED placeholder (flt_82F29918 unresolved)

    // ---------------------------------------------------------------------------------
    // ClearStandingsData  (X360 @ 0x82545278)
    //   Reset every slot's recorded result -- finish time 0, distance -1.0, eliminator -1, all
    //   counters/flags 0. Does NOT touch mPlayerID (that is owned by AddPlayer/RemovePlayer).
    // ---------------------------------------------------------------------------------
    void StandingsManager::ClearStandingsData()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_PLAYERS; ++liPlayerIndex)
        {
            StandingsData& lData = maStandingsData[liPlayerIndex];
            lData.mFinishTime.SetFloatVal(0.0f);
            lData.mfDistanceFromFinish       = -1.0f;
            lData.mEliminatorNetworkPlayerID = K_INVALID_PLAYER_ID;
            lData.miEliminations             = 0;
            lData.miReserved0x98             = 0;
            lData.mbTimedOut                 = false;
            lData.mbWonRound                 = false;
            lData.mbResultsReceived          = false;
        }
    }

    // ---------------------------------------------------------------------------------
    // Construct  (X360 @ 0x825508C8)
    //   Wire up the module / manager / time-manager, construct each slot's send+recv messages,
    //   mark every slot empty (mPlayerID == -1), then reset the recorded results.
    // ---------------------------------------------------------------------------------
    void StandingsManager::Construct(BrnNetworkModule* lpNetworkModule, CgsNetwork::TimeManager* lpTimeManager)
    {
        mpNetworkModule  = lpNetworkModule;
        mpNetworkManager = lpNetworkModule->GetNetworkManager();
        mpTimeManager    = lpTimeManager;

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_PLAYERS; ++liPlayerIndex)
        {
            StandingsData& lData = maStandingsData[liPlayerIndex];
            lData.mPlayerID = K_INVALID_PLAYER_ID;
            lData.mPlayerFinishedRoundMessageSend.Construct();
            lData.mPlayerFinishedRoundMessageRecv.Construct();
        }

        ClearStandingsData();
    }

    // ---------------------------------------------------------------------------------
    // Destruct  (X360 @ 0x82550930)
    //   ClearStandingsData, then re-mark every slot empty and re-construct its two messages.
    // ---------------------------------------------------------------------------------
    void StandingsManager::Destruct()
    {
        ClearStandingsData();

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_PLAYERS; ++liPlayerIndex)
        {
            StandingsData& lData = maStandingsData[liPlayerIndex];
            lData.mPlayerID = K_INVALID_PLAYER_ID;
            lData.mPlayerFinishedRoundMessageSend.Construct();
            lData.mPlayerFinishedRoundMessageRecv.Construct();
        }
    }

    // ---------------------------------------------------------------------------------
    // GetStandingsDataEntry  (X360 @ 0x825451E0)
    //   Linear search for the slot whose mPlayerID matches; null when not found.
    // ---------------------------------------------------------------------------------
    StandingsData* StandingsManager::GetStandingsDataEntry(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
        {
            if (maStandingsData[liIndex].mPlayerID == lPlayerID)
            {
                return &maStandingsData[liIndex];
            }
        }
        return nullptr;
    }

    // ---------------------------------------------------------------------------------
    // AddPlayer  (X360 @ 0x82550980)
    //   Register the per-peer reliable PlayerFinishedRoundMessage send/recv pair (type 12) on the
    //   first empty slot, claim that slot for lPlayerID and zero its finish time.
    // ---------------------------------------------------------------------------------
    void StandingsManager::AddPlayer(NetworkPlayerID lPlayerID)
    {
        CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();
        CGS_ASSERT(lpPlayerManager != nullptr, "mpNetworkManager->GetPlayerManager()");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);

        StandingsData* lpDataEntry = nullptr;
        s32 liIndex = 0;
        for (; liIndex < KI_MAX_PLAYERS; ++liIndex)
        {
            if (maStandingsData[liIndex].mPlayerID == K_INVALID_PLAYER_ID)
            {
                lpDataEntry = &maStandingsData[liIndex];
                break;
            }
        }
        CGS_ASSERT(liIndex < KI_MAX_PLAYERS, "liIndex < KI_MAX_PLAYERS");

        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->RegisterMessageType(KI_PLAYER_FINISHED_ROUND_MESSAGE_TYPE,
                                                 KI_PLAYER_FINISHED_ROUND_MESSAGE_LENGTH,
                                                 &lpDataEntry->mPlayerFinishedRoundMessageSend,
                                                 &lpDataEntry->mPlayerFinishedRoundMessageRecv,
                                                 _RoundFinishedMessageArrivedCallback,
                                                 _RoundFinishedMessageDeliveredCallback,
                                                 this);
        }

        lpDataEntry->mPlayerID = lPlayerID;
        lpDataEntry->mFinishTime.SetFloatVal(0.0f);
    }

    // ---------------------------------------------------------------------------------
    // RemovePlayer  (X360 @ 0x82550A88)
    //   Unregister the peer's PlayerFinishedRoundMessage channel and release its slot.
    // ---------------------------------------------------------------------------------
    void StandingsManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();
        CGS_ASSERT(lpPlayerManager != nullptr, "mpNetworkManager->GetPlayerManager()");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->UnRegisterMessageType(KI_PLAYER_FINISHED_ROUND_MESSAGE_TYPE);
        }

        StandingsData* lpDataEntry = GetStandingsDataEntry(lPlayerID);
        CGS_ASSERT(lpDataEntry != nullptr, "lpDataEntry");

        lpDataEntry->mPlayerID = K_INVALID_PLAYER_ID;
        lpDataEntry->mFinishTime.SetFloatVal(0.0f);
    }

    // ---------------------------------------------------------------------------------
    // Disconnected  (X360 @ 0x82550B40)
    //   Tear down every still-occupied slot (RemovePlayer), asserting each is released.
    // ---------------------------------------------------------------------------------
    void StandingsManager::Disconnected()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_PLAYERS; ++liPlayerIndex)
        {
            if (maStandingsData[liPlayerIndex].mPlayerID != K_INVALID_PLAYER_ID)
            {
                RemovePlayer(maStandingsData[liPlayerIndex].mPlayerID);
                CGS_ASSERT(maStandingsData[liPlayerIndex].mPlayerID == K_INVALID_PLAYER_ID,
                           "maStandingsData[liPlayerIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID");
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // HandlePlayerFinishedMode  (X360 @ 0x82550BB8)
    //   Stamp lPlayerID's slot from the finished-mode action (finish time, distance, eliminator
    //   ARCI -> network id, eliminations, timed-out / won-round), then broadcast it.
    // ---------------------------------------------------------------------------------
    void StandingsManager::HandlePlayerFinishedMode(NetworkPlayerID lPlayerID,
                                                    const BrnGameState::GameStateModuleIO::FinishedModeAction* lpFinishedModeAction)
    {
        CGS_ASSERT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        CGS_ASSERT(lpFinishedModeAction != nullptr, "lpModeResultsAction");

        StandingsData* lpStandingsData = GetStandingsDataEntry(lPlayerID);
        CGS_ASSERT(lpStandingsData != nullptr, "lpStandingsData");
        if (lpStandingsData == nullptr)
        {
            return;
        }

        lpStandingsData->mFinishTime          = lpFinishedModeAction->mFinishTime;
        lpStandingsData->mfDistanceFromFinish = lpFinishedModeAction->mfDistanceFromFinish;
        CGS_ASSERT(lpStandingsData->mfDistanceFromFinish != KF_BAD_DISTANCE_SENTINEL,
                   "Bad distance in standings #1, dist from finish");

        // Map the eliminator's active-race-car index to a network player id (-1 stays -1). The
        // action carries the global ::EActiveRaceCarIndex; the IO mapping table is keyed on the
        // BrnNetwork-local EActiveRaceCarIndex (same underlying s32), so convert across.
        if (static_cast<s32>(lpFinishedModeAction->meEliminatorIndex) == -1)
        {
            lpStandingsData->mEliminatorNetworkPlayerID = K_INVALID_PLAYER_ID;
        }
        else
        {
            lpStandingsData->mEliminatorNetworkPlayerID =
                mpNetworkModule->GetGameStateToNetworkInterface()->GetNetworkPlayerID(
                    static_cast<BrnNetwork::EActiveRaceCarIndex>(lpFinishedModeAction->meEliminatorIndex));
        }

        lpStandingsData->miEliminations    = lpFinishedModeAction->miEliminations;
        lpStandingsData->mbTimedOut         = lpFinishedModeAction->mbTimedOut;
        lpStandingsData->mbWonRound         = lpFinishedModeAction->mbWonRound;
        lpStandingsData->mbResultsReceived  = true;

        SendPlayerFinishedRoundMessage();
    }

    // ---------------------------------------------------------------------------------
    // SendPlayerFinishedRoundMessage  (X360 @ 0x825452F8)
    //   Broadcast the local player's recorded finish result to every peer's registered
    //   PlayerFinishedRoundMessage send channel. Gated on the time manager having a valid start
    //   frame (the round is actually running).
    // ---------------------------------------------------------------------------------
    void StandingsManager::SendPlayerFinishedRoundMessage()
    {
        CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();
        CGS_ASSERT(lpPlayerManager != nullptr, "lpPlayerManager");

        // Only relay once the round's start frame has been set (mu16StartFrame != 0xFFFF).
        if (mpTimeManager == nullptr || !mpTimeManager->HasStartFrameBeenSetValid())
        {
            return;
        }

        NetworkPlayerID lLocalPlayerID = K_INVALID_PLAYER_ID;
        bool lbHaveLocalPlayer = lpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
        CGS_ASSERT(lbHaveLocalPlayer, "lpPlayerManager->GetNextLocalPlayerID( &lLocalPlayerID )");
        CGS_ASSERT(lLocalPlayerID != K_INVALID_PLAYER_ID, "lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        StandingsData* lpLocalPlayerStandingsData = GetStandingsDataEntry(lLocalPlayerID);
        CGS_ASSERT(lpLocalPlayerStandingsData != nullptr, "lpLocalPlayerStandingsData");

        CGS_ASSERT(lpLocalPlayerStandingsData->mfDistanceFromFinish >= KF_INVALID_DISTANCE_FROM_FINISH,
                   "Bad distance reported #1, dist from finish");

        const u16 lu16CurrentFrame   = mpTimeManager->GetU16FrameCount();
        const u8  lu8CurrentRound     = mpNetworkManager->GetCurrentRoundNumber();

        NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
        while (lpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            CgsNetwork::NetworkPlayer* lpNetPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);
            if (lpNetPlayer != nullptr)
            {
                PlayerFinishedRoundMessage* lpMessage = static_cast<PlayerFinishedRoundMessage*>(
                    lpNetPlayer->GetRegisteredSendMessage(KI_PLAYER_FINISHED_ROUND_MESSAGE_TYPE));

                lpMessage->PrepareForSend(lu16CurrentFrame,
                                          lu8CurrentRound,
                                          lpLocalPlayerStandingsData->mFinishTime,
                                          lpLocalPlayerStandingsData->mfDistanceFromFinish,
                                          lpLocalPlayerStandingsData->mEliminatorNetworkPlayerID,
                                          lpLocalPlayerStandingsData->miEliminations,
                                          lpLocalPlayerStandingsData->mbTimedOut,
                                          lpLocalPlayerStandingsData->mbWonRound);
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // _RoundFinishedMessageArrivedCallback  (X360 @ 0x82545530)
    //   A peer reported its finish result: retrieve the message and -- if it is for the current
    //   round -- record it into that player's slot. A won-round message also flags the sending
    //   network player on the registry.
    // ---------------------------------------------------------------------------------
    void StandingsManager::_RoundFinishedMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                NetworkPlayerID lSendingPlayerID,
                                                                void* lpUserData)
    {
        StandingsManager* lpStandingsManager = static_cast<StandingsManager*>(lpUserData);
        CGS_ASSERT(lpStandingsManager != nullptr, "lpStandingsManager");

        PlayerFinishedRoundMessage* lpPlayerFinishedRoundMessage =
            static_cast<PlayerFinishedRoundMessage*>(lpMessage);
        CGS_ASSERT(lpPlayerFinishedRoundMessage != nullptr, "lpPlayerFinishedRoundMessage");

        CgsNetwork::PlayerManager* lpPlayerManager = lpStandingsManager->mpNetworkManager->GetPlayerManager();
        CGS_ASSERT(lpPlayerManager != nullptr, "lpPlayerManager");

        StandingsData* lpStandings = lpStandingsManager->GetStandingsDataEntry(lSendingPlayerID);
        CGS_ASSERT(lpStandings != nullptr, "lpStandings");
        if (lpStandings == nullptr)
        {
            return;
        }

        u8              lu8RoundIndex            = 0;
        Time            lFinishTime;
        f32             lfDistanceFromFinish      = 0.0f;
        NetworkPlayerID lEliminatorNetworkPlayerID = K_INVALID_PLAYER_ID;
        s32             liEliminations            = 0;
        bool            lbTimedOut                = false;
        bool            lbWonRound                = false;

        lpPlayerFinishedRoundMessage->Retrieve(&lu8RoundIndex, &lFinishTime, &lfDistanceFromFinish,
                                               &lEliminatorNetworkPlayerID, &liEliminations,
                                               &lbTimedOut, &lbWonRound);

        // Only accept the result if it is for the round we are currently running.
        if (lu8RoundIndex == lpStandingsManager->mpNetworkManager->GetCurrentRoundNumber())
        {
            lpStandings->mFinishTime                = lFinishTime;
            lpStandings->mfDistanceFromFinish       = lfDistanceFromFinish;
            lpStandings->mEliminatorNetworkPlayerID = lEliminatorNetworkPlayerID;
            lpStandings->miEliminations             = liEliminations;
            lpStandings->mbTimedOut                 = lbTimedOut;
            lpStandings->mbWonRound                 = lbWonRound;
            lpStandings->mbResultsReceived          = true;

            if (lbWonRound)
            {
                // The session registry stores BrnNetwork::BrnNetworkPlayer-derived peers; the X360
                // sets the won-round flag at the derived +0x2232 (this+8754).
                BrnNetworkPlayer* lpNetPlayer =
                    static_cast<BrnNetworkPlayer*>(lpPlayerManager->GetPlayerByID(lSendingPlayerID));
                if (lpNetPlayer != nullptr)
                {
                    lpNetPlayer->SetHasWonRound(true);
                }
            }
        }
    }
} // namespace BrnNetwork
