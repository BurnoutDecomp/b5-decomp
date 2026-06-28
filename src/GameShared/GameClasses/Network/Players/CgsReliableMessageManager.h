#pragma once

// ===================================================================================
// CgsNetwork::ReliableMessageManager -- minimal owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h
//
// The per-PlayerManager reliable-message queue: it buffers our outgoing reliable messages
// per remote player, hands the player pump the next one to (re)send, and drops them on ack.
//
// This header is INTENTIONALLY MINIMAL: it declares only the three X360-attested methods
// CgsNetworkPlayer.cpp drives (the player pump's view of the queue), with signatures from
// the DecFIGS DWARF (references/DecFIGS/dwarfdump/.../CgsReliableMessageManager.h:110/116/127),
// gated against the ARTIST binary. The full queue layout (the BufferedSendMessageData ring,
// the rcvd-dup window, etc.) is reconstructed in CgsReliableMessageManager.cpp's own TU; the
// body of each method below lives there. The send buffer holds
// KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER == 140 entries (the player pump asserts the index
// stays in [0, 140) -- see CgsNetworkPlayer::SendMessages).
// ===================================================================================

#include "types.hpp"

namespace CgsNetwork
{
    struct Message;

    typedef s32 NetworkPlayerID;   // mirrors MessageWithPlayerIDs::NetworkPlayerID

    struct ReliableMessageManager
    {
        // Capacity of the per-player outgoing reliable-message ring (the player pump
        // bounds-asserts its index against this -- KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER).
        static const s32 KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER = 140;

        // One buffered outgoing reliable message (DWARF CgsReliableMessageManager.h:62-70).
        // 16 bytes; mu16FrameFirstSent == KU16_INVALID_FRAME (0xFFFF) means "not sent yet".
        struct BufferedSendMessageData
        {
            NetworkPlayerID mPlayerID;          // +0x00
            s32             miLength;           // +0x04
            u16             mu16FrameFirstSent; // +0x08
            u16             mu16FrameLastSent;  // +0x0A
            Message*        mpMsg;              // +0x0C
        };

        // Return the buffered entry at liIndex (DWARF :104). The player pump reads its
        // mpMsg / frame fields and updates the resend frame stamps.
        BufferedSendMessageData* GetBufferedReliableMessage(s32 liIndex);

        // Return the index of the next reliable message that should be (re)sent to liPlayerID
        // on frame lu16CurrentFrame, scanning after liPrevIndex (-1 to start). Returns -1 when
        // there are none left. DWARF :110.
        s32  GetNextReliableMessageToResend(NetworkPlayerID liPlayerID, u16 lu16CurrentFrame,
                                            s32 liPrevIndex);

        // Buffer a freshly-sent reliable message so it can be resent until acked. DWARF :116.
        void AddBufferedReliableMessage(NetworkPlayerID liPlayerID, Message* lpMessage,
                                        s32 liFrame);

        // Drop every buffered reliable message we were holding for liPlayerID (called when the
        // player is released / disconnected). DWARF :127.
        void ClearPlayersSendReliableMessages(NetworkPlayerID liPlayerID);
    };
}
