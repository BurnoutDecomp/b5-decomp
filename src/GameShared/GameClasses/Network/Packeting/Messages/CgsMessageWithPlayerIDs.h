#pragma once

// ===================================================================================
// CgsNetwork::MessageWithPlayerIDs -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsMessageWithPlayerIDs.h:47), gated against the
// X360 binary: every leaf message constructor inlines this base's two-field init
// (e.g. BrnNetwork::CameraRequestMessage::Construct @ 0x8257ADC0 stores -1 to +0x20
// and +0x24 before chaining to Message::Construct), which fixes the layout:
//
//   +0x00  (CgsNetwork::Message base, size 0x20 -- vptr + bitstream cursors + scalars)
//   +0x20  NetworkPlayerID mSendingPlayerID   (stw -1 in every subclass ctor)
//   +0x24  NetworkPlayerID mRecvingPlayerID   (stw -1 in every subclass ctor)
//
// This subclass adds the two player-id words and a GetPackedMessageSize override --
// sizeof == 0x28.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"

namespace CgsNetwork
{
    // A Message that records which player sent it and which player is meant to
    // receive it. -1 is the "no player" sentinel both fields are constructed with.
    struct MessageWithPlayerIDs : Message
    {
        // DWARF CgsMessageWithPlayerIDs.h:82-83 spells these as a nested typedef; the
        // store width is a full 32-bit word (stw) and the sentinel is -1, so the
        // logical type is a signed 32-bit player index.
        typedef s32 NetworkPlayerID;

        static const NetworkPlayerID KI_INVALID_PLAYER_ID = -1;

        NetworkPlayerID mSendingPlayerID;   // +0x20
        NetworkPlayerID mRecvingPlayerID;   // +0x24

        // Inlined at every call site on the console: both ids to the sentinel, then the base's
        // field reset.
        void            Construct()
        {
            mSendingPlayerID = KI_INVALID_PLAYER_ID;
            mRecvingPlayerID = KI_INVALID_PLAYER_ID;
            Message::Construct();
        }
        // Inlined at every call site on the console: range assert, then the word store.
        void            SetSendingPlayerID(NetworkPlayerID lSendingPlayerID)
        {
            CGS_ASSERT(lSendingPlayerID >= 0, "lSendingPlayerID>=0");
            mSendingPlayerID = lSendingPlayerID;
        }
        NetworkPlayerID GetSendingPlayerID() const;
        NetworkPlayerID GetSendingPlayerIDForNack() const { return mSendingPlayerID; }
        void            SetRecvingPlayerID(NetworkPlayerID lRecvingPlayerID)
        {
            CGS_ASSERT(lRecvingPlayerID >= 0, "lRecvingPlayerID>=0");
            mRecvingPlayerID = lRecvingPlayerID;
        }
        NetworkPlayerID GetRecvingPlayerID() const;
        NetworkPlayerID GetRecvingPlayerIDForNack() const { return mRecvingPlayerID; }
        s32             GetPackedMessageSize() override;
    };

    static_assert(sizeof(void*) != 4 || offsetof(MessageWithPlayerIDs, mSendingPlayerID) == 0x20,
                  "MessageWithPlayerIDs::mSendingPlayerID @ +0x20");
    static_assert(sizeof(void*) != 4 || sizeof(MessageWithPlayerIDs) == 0x28, "sizeof(MessageWithPlayerIDs) == 0x28");
} // namespace CgsNetwork
