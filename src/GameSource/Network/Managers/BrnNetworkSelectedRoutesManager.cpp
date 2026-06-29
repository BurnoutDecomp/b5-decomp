// ===================================================================================
// BrnNetwork::SelectedRoutesManager -- implementation TU
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the per-function X360 addresses are noted on
// each body). The owning header (BrnNetworkSelectedRoutesManager.h) was grown by this TU to
// materialise the real typed members (it previously homed only the trivial Release leaf as a
// 0-size placeholder); Release / HaveRoutesChanged / SetRoutesChanged stay inline there.
//
// WHAT THIS MANAGER DOES
//   Synchronises the per-round "selected routes" (one SpecificGameModeEventInterface::Event
//   per round, up to KI_MAX_ROUNDS == 10) across the up-to-KI_MAX_PLAYERS == 7 remote peers
//   of a network session. Each tracked player owns a SelectedRoutesData slot with a reliable
//   send/recv SelectedRoutesMessage pair and two bit sets (rounds sent / rounds received).
//   SendRouteData pumps one outstanding round per turn to each player whose turn it is;
//   the arrived callback drops the received Event into maEvents and flags the routes changed.
//
// INLINED-ONLY HELPERS
//   GetSelectedRoutesDataEntry / ClearReceivedRounds / _SelectedRoutesMessageDeliveredCallback
//   have no standalone X360 symbol (folded into their callers). Their bodies are reconstructed
//   from the DWARF declaration intent + the inlined logic visible in the callers that DO have
//   pseudocode (AddPlayer/RemovePlayer/OnLeaveGame). The delivered-ack callback has no
//   recovered body and is a no-op (FLAGGED below).
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.h"
#include "GameSource/Network/BrnNetworkModule.h"                            // BrnNetworkModule / BrnNetworkManager
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"        // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"        // CgsNetwork::NetworkPlayer
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT

namespace BrnNetwork
{
    // ---------------------------------------------------------------------------------
    // The X360 build masks a SpecificGameModeEventInterface::Event's traffic-light trigger
    // id against 0xFFFF00 / 0xFF to test BrnTraffic::LightTriggerId::IsValid without pulling
    // in the BrnTrafficLightTrigger header (the same inlined predicate BrnSelectedRoutesMessage
    // reproduces). A handle is INVALID when its hull field is all-ones (0xFFFF00) or it equals
    // the bare 0xFF index sentinel.
    // ---------------------------------------------------------------------------------
    static bool IsLightTriggerIdValid(u32 luTriggerID)
    {
        if ((luTriggerID & 0x00FFFF00u) == 0x00FFFF00u)
            return false;
        if (luTriggerID == 0xFFu)
            return false;
        return true;
    }

    // ---------------------------------------------------------------------------------
    // Construct @ 0x8255BC38   [EXECUTED in goal trace]
    //   Asserts both injected pointers, clears the network-module pointer, stores the time
    //   manager, zeroes miNumRounds, then for every player slot: zeroes the round-send frame
    //   stamps, marks the slot free (mPlayerID = -1), Constructs both reliable messages, and
    //   clears both round bit sets.
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::Construct(BrnNetworkModule* lpNetworkModule,
                                          CgsNetwork::TimeManager* lpTimeManager)
    {
        CGS_ASSERT(lpNetworkModule != 0, "lpNetworkModule");
        CGS_ASSERT(lpTimeManager != 0, "lpTimeManager");

        // The X360 stores 0 into mpNetworkModule here (HIDWORD(v6)==a2 is a decompiler
        // register-pair artifact; the store is LODWORD(v6)==0). The full network-module wiring
        // happens elsewhere; Construct leaves it null.
        mpNetworkModule = 0;
        mpTimeManager   = lpTimeManager;
        miNumRounds     = 0;

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_PLAYERS; ++liPlayerIndex)
        {
            SelectedRoutesData& lData = maSelectedRoutesData[liPlayerIndex];

            for (s32 liRoundNum = 0; liRoundNum < KI_MAX_ROUNDS; ++liRoundNum)
                lData.mau16FrameNumRoundSent[liRoundNum] = 0;

            lData.mPlayerID = KI_INVALID_PLAYER_ID;
            lData.mMessageSend.Construct();
            lData.mMessageRecv.Construct();
            lData.mRoundsSent.UnSetAll();
            lData.mRoundsReceived.UnSetAll();
        }
    }

    // ---------------------------------------------------------------------------------
    // Prepare @ 0x82548B58
    //   Resets the per-round event table for a fresh game: clears mbRoutesChanged + miNumRounds,
    //   then Constructs every maEvents[] entry to the empty event (event id 0, default trigger,
    //   no landmarks). Returns true.
    // ---------------------------------------------------------------------------------
    bool SelectedRoutesManager::Prepare()
    {
        mbRoutesChanged = false;
        miNumRounds     = 0;

        for (u32 luRoundIndex = 0; luRoundIndex < KI_MAX_ROUNDS; ++luRoundIndex)
        {
            const u32 luDefaultTriggerID = 0xFFFFFFFFu; // X360 stores -1 (invalid trigger)
            maEvents[luRoundIndex].Construct(0, luDefaultTriggerID, 0, 0);
        }
        return true;
    }

    // ---------------------------------------------------------------------------------
    // Destruct @ 0x8255BD18
    //   Mirror of Construct's per-slot reset (zero frame stamps, free the slot, Construct both
    //   messages, clear both bit sets) and then clear the module/time-manager/round-count
    //   trailing scalars.
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::Destruct()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_PLAYERS; ++liPlayerIndex)
        {
            SelectedRoutesData& lData = maSelectedRoutesData[liPlayerIndex];

            for (s32 liRoundNum = 0; liRoundNum < KI_MAX_ROUNDS; ++liRoundNum)
                lData.mau16FrameNumRoundSent[liRoundNum] = 0;

            lData.mPlayerID = KI_INVALID_PLAYER_ID;
            lData.mMessageSend.Construct();
            lData.mMessageRecv.Construct();
            lData.mRoundsSent.UnSetAll();
            lData.mRoundsReceived.UnSetAll();
        }

        mpNetworkModule = 0;
        mpTimeManager   = 0;
        miNumRounds     = 0;
    }

    // ---------------------------------------------------------------------------------
    // Update @ 0x82560C40
    //   Per-frame entry point -- forwards straight to SendRouteData.
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::Update()
    {
        SendRouteData();
    }

    // ---------------------------------------------------------------------------------
    // GetSelectedRoutesDataEntry  (inlined-only; no standalone X360 symbol -- DWARF :148)
    //   Linear scan for the slot tracking lPlayerID; null if none. Reconstructed from the
    //   inlined scan AddPlayer/RemovePlayer emit (compare each slot's mPlayerID, advancing the
    //   216-byte stride). AddPlayer uses the "first free slot" variant; this id-match variant is
    //   the declared private helper RemovePlayer's scan corresponds to.
    // ---------------------------------------------------------------------------------
    SelectedRoutesManager::SelectedRoutesData*
    SelectedRoutesManager::GetSelectedRoutesDataEntry(NetworkPlayerID lPlayerID)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
        {
            if (maSelectedRoutesData[liIndex].mPlayerID == lPlayerID)
                return &maSelectedRoutesData[liIndex];
        }
        return 0;
    }

    // ---------------------------------------------------------------------------------
    // AddPlayer @ 0x8255BDA0
    //   Registers a newly-joined player: asserts the player manager + the looked-up
    //   NetworkPlayer, claims the first FREE slot (mPlayerID == -1) -- asserting one exists --
    //   registers the selected-routes reliable message type on that player (type 23, length 88,
    //   the slot's send/recv messages, both callbacks, manager `this` as user data), stamps the
    //   slot with the player id, clears its two round bit sets, and zeroes its frame stamps.
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::AddPlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetPlayerManager() != 0,
                   "mpNetworkModule->GetNetworkManager()->GetPlayerManager()");

        CgsNetwork::PlayerManager* lpPlayerManager =
            mpNetworkModule->GetNetworkManager()->GetPlayerManager();
        CgsNetwork::NetworkPlayer* lpNetworkPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);

        if (lpNetworkPlayer != 0)
        {
            // Claim the first free slot.
            SelectedRoutesData* lpDataEntry = 0;
            for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
            {
                if (maSelectedRoutesData[liIndex].mPlayerID == KI_INVALID_PLAYER_ID)
                {
                    lpDataEntry = &maSelectedRoutesData[liIndex];
                    break;
                }
            }
            CGS_ASSERT(lpDataEntry != 0, "lpDataEntry");

            lpNetworkPlayer->RegisterMessageType(
                KI_SELECTED_ROUTES_MESSAGE_TYPE,
                KI_SELECTED_ROUTES_MESSAGE_LENGTH,
                &lpDataEntry->mMessageSend,
                &lpDataEntry->mMessageRecv,
                _SelectedRoutesMessageArrivedCallback,
                _SelectedRoutesMessageDeliveredCallback,
                this);

            lpDataEntry->mPlayerID = lPlayerID;
            lpDataEntry->mRoundsSent.UnSetAll();
            lpDataEntry->mRoundsReceived.UnSetAll();
            for (s32 liRoundNum = 0; liRoundNum < KI_MAX_ROUNDS; ++liRoundNum)
                lpDataEntry->mau16FrameNumRoundSent[liRoundNum] = 0;
        }
    }

    // ---------------------------------------------------------------------------------
    // RemovePlayer @ 0x8255BEC8
    //   Frees the slot tracking lPlayerID (mark it free, clear both bit sets, zero the frame
    //   stamps) and, if the player is still in the registry, unregisters the reliable message
    //   type from it.
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetPlayerManager() != 0,
                   "mpNetworkModule->GetNetworkManager()->GetPlayerManager()");

        CgsNetwork::PlayerManager* lpPlayerManager =
            mpNetworkModule->GetNetworkManager()->GetPlayerManager();
        CgsNetwork::NetworkPlayer* lpNetworkPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);

        SelectedRoutesData* lpEntry = GetSelectedRoutesDataEntry(lPlayerID);
        if (lpEntry != 0)
        {
            lpEntry->mPlayerID = KI_INVALID_PLAYER_ID;
            lpEntry->mRoundsSent.UnSetAll();
            lpEntry->mRoundsReceived.UnSetAll();
            for (s32 liRoundNum = 0; liRoundNum < KI_MAX_ROUNDS; ++liRoundNum)
                lpEntry->mau16FrameNumRoundSent[liRoundNum] = 0;
        }

        if (lpNetworkPlayer != 0)
            lpNetworkPlayer->UnRegisterMessageType(KI_SELECTED_ROUTES_MESSAGE_TYPE);
    }

    // ---------------------------------------------------------------------------------
    // Disconnected @ 0x8255BFB8
    //   Session-wide teardown: RemovePlayer every still-tracked slot (asserting it was freed),
    //   then reset miNumRounds and re-Construct the per-round event table to empty.
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::Disconnected()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_PLAYERS; ++liPlayerIndex)
        {
            if (maSelectedRoutesData[liPlayerIndex].mPlayerID != KI_INVALID_PLAYER_ID)
            {
                RemovePlayer(maSelectedRoutesData[liPlayerIndex].mPlayerID);
                CGS_ASSERT(maSelectedRoutesData[liPlayerIndex].mPlayerID == KI_INVALID_PLAYER_ID,
                           "maSelectedRoutesData[liPlayerIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID");
            }
        }

        miNumRounds = 0;
        for (u32 luRoundNum = 0; luRoundNum < KI_MAX_ROUNDS; ++luRoundNum)
        {
            const u32 luDefaultTriggerID = 0xFFFFFFFFu; // X360 stores -1 (invalid trigger)
            maEvents[luRoundNum].Construct(0, luDefaultTriggerID, 0, 0);
        }
    }

    // ---------------------------------------------------------------------------------
    // OnLeaveGame @ 0x82548BB0
    //   Lighter reset than Disconnected (the players stay registered): clear miNumRounds +
    //   mbRoutesChanged, then re-Construct the per-round event table to empty.
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::OnLeaveGame()
    {
        miNumRounds     = 0;
        mbRoutesChanged = false;

        for (u32 luRoundNum = 0; luRoundNum < KI_MAX_ROUNDS; ++luRoundNum)
        {
            const u32 luDefaultTriggerID = 0xFFFFFFFFu; // X360 stores -1 (invalid trigger)
            maEvents[luRoundNum].Construct(0, luDefaultTriggerID, 0, 0);
        }
    }

    // ---------------------------------------------------------------------------------
    // HaveAllPlayersReceivedRoutes @ 0x8255C068
    //   True iff every tracked player has received every round in [0, miNumRounds). For each
    //   used slot the X360 finds the first NOT-yet-received round (the first clear bit of
    //   mRoundsReceived, via its complement-and-lowest-set-bit idiom); if that round is still
    //   inside the active range, that player is behind and the answer is false.
    // ---------------------------------------------------------------------------------
    bool SelectedRoutesManager::HaveAllPlayersReceivedRoutes() const
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
        {
            const SelectedRoutesData& lData = maSelectedRoutesData[liIndex];
            if (lData.mPlayerID == KI_INVALID_PLAYER_ID)
                continue;

            const s32 liFirstUnreceivedRound = lData.mRoundsReceived.GetFirstClearBit();
            if (liFirstUnreceivedRound != CgsContainers::BitArray<KI_MAX_ROUNDS>::KI_INVALID_BITINDEX
                && liFirstUnreceivedRound < miNumRounds)
            {
                return false;
            }
        }
        return true;
    }

    // ---------------------------------------------------------------------------------
    // SendRouteData @ 0x8255C168
    //   The per-frame send pump. Asserts the time manager, takes the current network frame
    //   number (wrapped to 16 bits). For each tracked player whose first NOT-yet-sent round
    //   (first clear bit of mRoundsSent) is a real round inside the active range, if it is that
    //   player's turn to send a round-robin message AND the round's event carries a valid route
    //   (>0 landmarks, or a valid light-trigger id), it stamps the send frame, marks the round
    //   sent, and prepares the reliable message for that round.
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::SendRouteData()
    {
        CGS_ASSERT(mpTimeManager != 0, "mpTimeManager");

        const u16 lu16CurrentFrame = mpTimeManager->GetCurrentFrameNumber();

        for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
        {
            SelectedRoutesData& lData = maSelectedRoutesData[liIndex];

            const s32 liRoundToSend = lData.mRoundsSent.GetFirstClearBit();
            if (lData.mPlayerID == KI_INVALID_PLAYER_ID)
                continue;
            if (liRoundToSend == CgsContainers::BitArray<KI_MAX_ROUNDS>::KI_INVALID_BITINDEX)
                continue;
            if (liRoundToSend >= miNumRounds)
                continue;

            CgsNetwork::PlayerManager* lpPlayerManager =
                mpNetworkModule->GetNetworkManager()->GetPlayerManager();
            if (!lpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(lData.mPlayerID, false, 0))
                continue;

            // Only send rounds whose event carries a real route. X360 gates SOLELY on the
            // light-trigger-id validity (no landmark-count term on this path).
            const Event& lEvent = maEvents[liRoundToSend];
            if (!IsLightTriggerIdValid(lEvent.GetTrafficLightTriggerId()))
                continue;

            lData.mau16FrameNumRoundSent[liRoundToSend] = lu16CurrentFrame;

            CGS_ASSERT(liRoundToSend < KI_MAX_ROUNDS, "liRoundToSend < BitArray capacity");
            lData.mRoundsSent.SetBit(static_cast<u32>(liRoundToSend));

            lData.mMessageSend.PrepareForSend(lu16CurrentFrame, liRoundToSend,
                                              &maEvents[liRoundToSend]);
        }
    }

    // ---------------------------------------------------------------------------------
    // SetRouteData @ 0x82560C48
    //   Installs liNumRounds rounds of route events: copies each event into maEvents[], with a
    //   per-mode debug validity assert (some modes must carry NO landmarks, one mode skips the
    //   check, the rest require a route -- landmarks or a valid light-trigger id). Then, for
    //   every tracked player, clears the received-round bit set + the frame stamps so the new
    //   route set re-syncs. Finally flags the routes changed.
    //
    //   The X360 third argument (the active game-mode type, dropped by the PS3 DWARF) selects
    //   the per-event assert lane; modelled as a switch on (mode - E_MODE_ONLINE_FUGITIVE),
    //   matching the X360 `switch(a4 - 12)` jump table (cases 0/2/5 -> no-landmarks, case 3 ->
    //   skip, default -> require-route).
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::SetRouteData(s32 liNumRounds, const Event* laEvents,
                                             BrnGameState::GameStateModuleIO::EGameModeType leGameModeType)
    {
        miNumRounds = liNumRounds;

        for (s32 liRoundIndex = 0; liRoundIndex < miNumRounds; ++liRoundIndex)
        {
            maEvents[liRoundIndex] = laEvents[liRoundIndex];

            const Event& lEvent = maEvents[liRoundIndex];
            switch (static_cast<s32>(leGameModeType) - 12)
            {
                case 0: // fall-through
                case 2:
                case 5:
                    CGS_ASSERT(lEvent.GetNumLandmarks() == 0,
                               "maEvents[ liRoundIndex ].GetNumLandmarks() == 0");
                    break;
                case 3:
                    break;
                default:
                    if (lEvent.GetNumLandmarks() <= 0)
                    {
                        CGS_ASSERT(lEvent.GetNumLandmarks() > 0
                                       || !IsLightTriggerIdValid(lEvent.GetTrafficLightTriggerId()),
                                   "( maEvents[ liRoundIndex ].GetNumLandmarks() > 0 ) || "
                                   "( !maEvents[ liRoundIndex ].GetLightTriggerID().IsValid() )");
                    }
                    break;
            }
        }

        for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
        {
            SelectedRoutesData& lData = maSelectedRoutesData[liIndex];
            if (lData.mPlayerID != KI_INVALID_PLAYER_ID)
            {
                lData.mRoundsSent.UnSetAll();
                lData.mRoundsReceived.UnSetAll();
                for (s32 liRoundNum = 0; liRoundNum < KI_MAX_ROUNDS; ++liRoundNum)
                    lData.mau16FrameNumRoundSent[liRoundNum] = 0;
            }
        }

        mbRoutesChanged = true;
    }

    // ---------------------------------------------------------------------------------
    // GetRouteData @ 0x82548BE8
    //   Copies the whole per-round event table out into the caller's array (10 contiguous
    //   44-byte Event copies).
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::GetRouteData(Event* laEvents) const
    {
        for (s32 liRoundNum = 0; liRoundNum < KI_MAX_ROUNDS; ++liRoundNum)
            laEvents[liRoundNum] = maEvents[liRoundNum];
    }

    // ---------------------------------------------------------------------------------
    // ClearReceivedRounds  (inlined-only; no standalone X360 symbol -- DWARF :151)
    //   Clears every tracked player's received-round bit set + send-frame stamps. Reconstructed
    //   from the per-slot reset the SetRouteData / Disconnected paths inline (the declared
    //   private helper the DWARF names; its locals are liIndex + luRoundNum).
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::ClearReceivedRounds()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
        {
            SelectedRoutesData& lData = maSelectedRoutesData[liIndex];
            if (lData.mPlayerID != KI_INVALID_PLAYER_ID)
            {
                lData.mRoundsReceived.UnSetAll();
                for (u32 luRoundNum = 0; luRoundNum < KI_MAX_ROUNDS; ++luRoundNum)
                    lData.mau16FrameNumRoundSent[luRoundNum] = 0;
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // _SelectedRoutesMessageArrivedCallback @ 0x82548CC0
    //   Reliable-message arrival handler. Retrieves the round number + Event payload out of the
    //   delivered message and drops the Event into maEvents[roundNum], then flags the routes
    //   changed. (lpUserData is the manager `this`; the sending player id is unused here.)
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::_SelectedRoutesMessageArrivedCallback(
        CgsNetwork::ReliableMessage* lpMessage, NetworkPlayerID /*lSendingPlayerID*/, void* lpUserData)
    {
        SelectedRoutesManager* lpManager = static_cast<SelectedRoutesManager*>(lpUserData);
        SelectedRoutesMessage* lpSelectedRoutesMessage =
            static_cast<SelectedRoutesMessage*>(lpMessage);

        s32   liRoundNum = 0;
        Event lEvent;
        lpSelectedRoutesMessage->Retrieve(&liRoundNum, &lEvent);

        lpManager->maEvents[liRoundNum] = lEvent;
        lpManager->mbRoutesChanged      = true;
    }

    // ---------------------------------------------------------------------------------
    // _SelectedRoutesMessageDeliveredCallback  (inlined-only; no recovered body -- DWARF :167)
    //   The reliable-send ack/nack handler. No standalone X360 symbol and no recovered body;
    //   reproduced as a no-op. FLAGGED: body not attested -- the empty handler is the
    //   conventional default for a delivered callback this manager registers but does not act
    //   on (the round-resend is driven by the round bit sets, not the ack).
    // ---------------------------------------------------------------------------------
    void SelectedRoutesManager::_SelectedRoutesMessageDeliveredCallback(
        bool /*lbDelivered*/, bool /*lbWasReliable*/, CgsNetwork::SignalMessage* /*lpMessage*/,
        NetworkPlayerID /*lToPlayerID*/, void* /*lpUserData*/)
    {
    }
} // namespace BrnNetwork
