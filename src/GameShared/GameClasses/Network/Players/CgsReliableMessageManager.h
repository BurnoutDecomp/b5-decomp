#pragma once

// ===================================================================================
// CgsNetwork::ReliableMessageManager -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h
//
// The per-PlayerManager reliable-message queue: it buffers our outgoing reliable messages
// per remote player, hands the player pump the next one to (re)send, drops them on ack,
// and (on the receive side) filters duplicate / out-of-order reliable messages through a
// fixed rcvd-dup window.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsReliableMessageManager.h), gated on the X360
// binary. Byte offsets read directly off the ARTIST asm dereferences:
//
//   +0x0000  mpPlayerManager                              (Release stores 0)
//   +0x0004  mpReliableMessageBuffer                      (heap buffer freed in Release)
//   +0x0008  maReliableMessageSendData[140]  (16B each -> ends +0x8C8)
//   +0x08C8  mabValidSendData  FastBitArray<140>  (3 x u64 -> 24B, cleared in Release)
//   +0x08E0  miReliableMessageSendIndex
//   +0x08E4  miMaxReliableMessagesSendToBuffer
//   +0x08E8  miNumBufferedReliableMessages                (Update recount target)
//   +0x08EC  maReliableMessagesRcvdData[140] (16B each -> ends +0x11AC)
//   +0x11AC  miReliableMessagesRecvdBufferIndex           (rcvd ring cursor)
//   +0x11B0  mpHeapAllocator
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"

namespace CgsMemory { class HeapMalloc; }

namespace CgsNetwork
{
    struct Message;
    struct PlayerManager;
    struct SignalMessage;

    typedef s32 NetworkPlayerID;   // mirrors MessageWithPlayerIDs::NetworkPlayerID

    // Frame-window tunables (DWARF CgsReliableMessageManager.cpp:34-38).
    const s32 KI_FRAMES_TO_DISCARD_RELIABLE_MESSAGE               = 3600;
    const s32 KI_FRAMES_TO_RESEND_RELIABLE_MESSAGE               = 7;
    const s32 KI_FRAMES_TO_DISCARD_RELIABLE_MESSAGE_RECEIVED_DATA = 10800;   // 0x2A30

    struct ReliableMessageManager
    {
        // Ring capacities (DWARF CgsReliableMessageManager.h:42-43). The player pump
        // bounds-asserts its send index against KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER.
        static const s32 KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER = 140;
        static const s32 KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER = 140;

        // One buffered outgoing reliable message (DWARF CgsReliableMessageManager.h:62-70).
        // 16 bytes; mu16FrameLastSent == KU16_INVALID_FRAME (0xFFFF) means "not sent yet".
        struct BufferedSendMessageData
        {
            NetworkPlayerID mPlayerID;          // +0x00
            s32             miLength;           // +0x04
            u16             mu16FrameFirstSent; // +0x08
            u16             mu16FrameLastSent;  // +0x0A  (GetNext reads elem+0x0A == asm 0x12)
            Message*        mpMsg;              // +0x0C
        };

        // One remembered received reliable message, for duplicate rejection
        // (DWARF CgsReliableMessageManager.h:157-166). 16 bytes.
        struct StoredRcvdMessageData
        {
            NetworkPlayerID mPlayerID;          // +0x00
            u16             mu16FrameSent;       // +0x04
            s32             miType;              // +0x08
            s32             miValidCountdown;    // +0x0C
        };

        // ---- layout (frozen; byte offsets above) ----
        PlayerManager*          mpPlayerManager;                                          // +0x0000
        u8*                     mpReliableMessageBuffer;                                  // +0x0004
        BufferedSendMessageData maReliableMessageSendData[KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER]; // +0x0008
        CgsContainers::FastBitArray<KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER> mabValidSendData;      // +0x08C8
        s32                     miReliableMessageSendIndex;                               // +0x08E0
        s32                     miMaxReliableMessagesSendToBuffer;                        // +0x08E4
        s32                     miNumBufferedReliableMessages;                            // +0x08E8
        StoredRcvdMessageData   maReliableMessagesRcvdData[KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER]; // +0x08EC
        s32                     miReliableMessagesRecvdBufferIndex;                       // +0x11AC
        CgsMemory::HeapMalloc*  mpHeapAllocator;                                          // +0x11B0

        // Largest message a send slot holds (the slot stride; the pool is 140 of them). A slot
        // holds a byte copy of the registered message object, so it is sized for the largest
        // reliable message on the host: the image message, 0x248 bytes here. The console slot is
        // 0x238, that message's console size. BrnImageMessage.cpp checks the bound.
        static const s32 KI_MAX_RELIABLE_MESSAGE_SIZE = 0x248;

        // ---- public interface (DWARF-attested, gated on X360 ledger) ----
        // Construct and Destruct are inlined into the player registry's own Construct /
        // Destruct on the console: every pointer, cursor and count to zero and the valid-send
        // bits cleared (Destruct keeps the registry back-pointer).
        void  Construct()
        {
            mpHeapAllocator                    = nullptr;
            mpReliableMessageBuffer            = nullptr;
            miReliableMessageSendIndex         = 0;
            miReliableMessagesRecvdBufferIndex = 0;
            mpPlayerManager                    = nullptr;
            miNumBufferedReliableMessages      = 0;
            mabValidSendData.UnSetAll();
        }
        bool  Prepare(PlayerManager* lpPlayerManager, CgsMemory::HeapMalloc* lpHeapAllocator);
        void  Update();
        bool  Release();
        void  Destruct()
        {
            miNumBufferedReliableMessages      = 0;
            mpReliableMessageBuffer            = nullptr;
            miReliableMessageSendIndex         = 0;
            miReliableMessagesRecvdBufferIndex = 0;
            mpHeapAllocator                    = nullptr;
            mabValidSendData.UnSetAll();
        }

        // Inlined into the player pump on the console.
        BufferedSendMessageData* GetBufferedReliableMessage(s32 liIndex)
        {
            CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
            CGS_ASSERT(liIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER,
                       "liIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER");
            return &maReliableMessageSendData[liIndex];
        }
        s32   GetNextReliableMessageToResend(NetworkPlayerID liPlayerID, u16 lu16CurrentFrame,
                                             s32 liPrevIndex);
        void  AddBufferedReliableMessage(NetworkPlayerID liPlayerID, Message* lpMessage, s32 liLength);
        void  RemoveBufferedReliableMessage(s32 liIndex);
        // Drop the buffered reliable message an ack or nack answers (same frame, type and
        // addressee).
        void  RemoveBufferedReliableMessage(SignalMessage* lpAckOrNackMsg);
        void  ClearSendReliableMessages();
        void  ClearPlayersSendReliableMessages(NetworkPlayerID liPlayerID);
        bool  MessageIsDuplicate(Message* lpMessage);
        // Forget every remembered received message and restart the ring cursor (inlined into
        // the registry's OnRoundStart on the console).
        void  ClearRcvdReliableMessages()
        {
            for (s32 liIndex = 0; liIndex < KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER; ++liIndex)
            {
                StoredRcvdMessageData& lData = maReliableMessagesRcvdData[liIndex];
                lData.mPlayerID        = -1;
                lData.mu16FrameSent    = 0xFFFF;
                lData.miType           = -1;
                lData.miValidCountdown = -1;
            }
            miReliableMessagesRecvdBufferIndex = 0;
        }

    private:
        // Age-out helpers driven every Update (bodies in their own TUs).
        void  CheckForReliableMessageTimeout();
        void  UpdateReliableMessagesReceived();

        // Report a buffered reliable message as nacked to its sender's delivery callback
        // (used when the message is dropped on timeout or when its player leaves).
        void  FakeNackMessage(Message* lpMessage, NetworkPlayerID lSendingPlayerID,
                              NetworkPlayerID lRecvingPlayerID, bool lbAck);
    };
}
