#pragma once

// ===================================================================================
// CgsNetwork::SyncTimeMessageManager -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsSyncTimeBase.h
//
// The host/client clock-synchronisation message pool. It owns a fixed set of seven
// per-player SyncTimeMessage send/receive slots (one logical slot per connected peer),
// keyed by the peer's NetworkPlayerID, and drives the round-trip exchange that lets a
// client estimate the host clock.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsSyncTimeBase.h:38), gated against the X360
// ARTIST binary:
//
//   mNetworkPlayerID[7]       -- the peer id occupying each logical slot (-1 == free)
//   mSyncMessageToSend[7]     -- the outgoing SyncTimeMessage for each slot
//   mSyncMessageToReceive[7]  -- the incoming SyncTimeMessage for each slot
//   meMessageMode             -- host / client / none
//
// Construct (@0x8288A6A0) walks all seven slots: it sets every slot's player id to the
// -1 sentinel, initialises both the send and receive SyncTimeMessage (sentinel header
// fields, zeroed timestamps, -1 player ids) and finally puts the manager in
// E_MESSAGE_MODE_NONE. The other methods register/unregister a player's slot with its
// NetworkPlayer, prepare the outgoing message, and flip the mode.
//
// WIDTH MODELLING: every member is accessed BY NAME; the C++ struct is laid out natively
// (semantic parity, not byte-exact X360 layout -- the same rule the rest of the project
// follows, cf. CgsNetworkPlayer.h / CgsMessage.h). The X360 build models the message
// vtable as Message::mpVTable (no C++ `virtual`), so these are plain methods.
//
// NOTE: this is the canonical owning header for CgsNetwork::SyncTimeMessageManager. The
// older placeholder CgsSyncTimeMessageManager.h is now a thin shim that includes this.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Time/CgsSyncTimeMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

namespace CgsNetwork
{
    // Forward-declared: NetworkPlayer is a pointer-only parameter on the register/
    // unregister API, so an incomplete type suffices and avoids pulling the whole
    // player hierarchy into every includer of this header.
    struct NetworkPlayer;

    struct SyncTimeMessageManager
    {
        // The fixed number of per-player clock-sync slots this manager owns.
        static const s32 KI_MAX_SYNC_PLAYERS = 7;

        // The shared signed player-index typedef (mirrors MessageWithPlayerIDs).
        typedef MessageWithPlayerIDs::NetworkPlayerID NetworkPlayerID;

        // The "no player" sentinel both the slot ids and the message player ids carry
        // when free.
        static const NetworkPlayerID KI_INVALID_PLAYER_ID = -1;

        // The protocol message-type id this manager registers with each NetworkPlayer
        // (E_MESSAGE_TYPE_SYNC_TIME == 2 in the X360 EMessageType enum).
        static const s32 KI_E_MESSAGE_TYPE_SYNC_TIME = 2;

        // Whether this manager is acting as the session host or a client. The host
        // indexes its slots by the client's id; a client indexes by the host's id.
        enum EMessageMode
        {
            E_MESSAGE_MODE_HOST   = 0,
            E_MESSAGE_MODE_CLIENT = 1,
            E_MESSAGE_MODE_NONE   = 2,
            E_MESSAGE_MODE_COUNT  = 2,
        };

        void Construct();
        void Destruct();

        bool RegisterMessages(NetworkPlayer* lpPlayer);
        bool UnregisterMessages(NetworkPlayer* lpPlayer);

        void SwitchMode(EMessageMode leMessageMode);

        void PrepareSyncTimeMessageToSend(u16 lu16FrameNum,
                                          CgsSystem::Time& lClientSendTime,
                                          CgsSystem::Time& lHostTime,
                                          NetworkPlayerID  lHostPlayerID,
                                          NetworkPlayerID  lClientPlayerID);

        // Bodied in another TU (did not run before the milestone); declared here so the
        // rest of the hierarchy can call it by name.
        bool GetNextRecievedMessage(SyncTimeMessage** lppMessage);

    private:
        s32 GetPlayerMessageIndex(NetworkPlayerID lPlayerID);
        s32 AddNewPlayer(NetworkPlayerID lPlayerID);

        // --- layout (shape from the DWARF member list) -------------------------------
        NetworkPlayerID mNetworkPlayerID[KI_MAX_SYNC_PLAYERS];
        SyncTimeMessage mSyncMessageToSend[KI_MAX_SYNC_PLAYERS];
        SyncTimeMessage mSyncMessageToReceive[KI_MAX_SYNC_PLAYERS];
        EMessageMode    meMessageMode;
    };
} // namespace CgsNetwork
