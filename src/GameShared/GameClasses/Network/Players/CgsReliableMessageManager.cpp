#include "GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"              // GetFrameDiffWrapped16 / UInt16IsLargerWrapped / IsReliable / OldMessagesAreValid
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h" // MessageWithPlayerIDs::GetSendingPlayerID
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"                    // CgsMemory::HeapMalloc::Free
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // CgsDev::Log::gpDebugPrint

// CgsNetwork::ReliableMessageManager -- reconstructed method bodies (BURNOUT_X360_ARTIST.XEX).
// The queue's full layout lives in the owning header; four of its methods are homed here
// (GetNextReliableMessageToResend / Release / Update / MessageIsDuplicate). MessageIsDuplicate
// dispatches two Message vtable slots -- slot 0 IsReliable() and slot 1 OldMessagesAreValid()
// (DWARF CgsMessage.cpp:376/:393) -- both now declared on the committed Message base and called
// through the family's plain-method idiom.

namespace CgsNetwork
{

// ---- GetNextReliableMessageToResend @ 0x82896328 ------------------------------------
// Walk the valid outgoing reliable-message slots (mabValidSendData) in ascending index
// order, considering only slots strictly after liPrevIndex, and return the first one bound
// for liPlayerID that is due to (re)send on lu16CurrentFrame: a slot never sent
// (mu16FrameLastSent == KU16_INVALID_FRAME/0xFFFF) fires immediately, otherwise one whose
// wrapped frame gap since its last send exceeds KI_FRAMES_TO_RESEND_RELIABLE_MESSAGE.
// Returns -1 when no further slot qualifies. (The X360 build inlines the FastBitArray
// iterator and its out-of-range/mask asserts here; reproduced through the committed
// FastBitArray helpers.)
s32 ReliableMessageManager::GetNextReliableMessageToResend(NetworkPlayerID liPlayerID,
                                                           u16 lu16CurrentFrame,
                                                           s32 liPrevIndex)
{
    for (s32 liIndex = mabValidSendData.GetFirstBitSet();
         liIndex != CgsContainers::FastBitArray<KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER>::KI_INVALID_BIT_INDEX;
         liIndex = mabValidSendData.GetNextBitSet(liIndex))
    {
        if (liIndex <= liPrevIndex)
            continue;

        const BufferedSendMessageData& lReliableMessageSendData = maReliableMessageSendData[liIndex];
        if (lReliableMessageSendData.mPlayerID != liPlayerID)
            continue;

        // Never sent yet -> send it now.
        if (lReliableMessageSendData.mu16FrameLastSent == 0xFFFF)
            return liIndex;

        // Otherwise resend once enough frames have elapsed since the last send.
        if (GetFrameDiffWrapped16(lu16CurrentFrame, lReliableMessageSendData.mu16FrameLastSent)
                > KI_FRAMES_TO_RESEND_RELIABLE_MESSAGE)
        {
            return liIndex;
        }
    }

    return -1;
}

// ---- Release @ 0x82890A10 ------------------------------------------------------------
// Tear the manager down: drop the player-manager back-pointer, free the reliable-message
// buffer through the heap allocator we were prepared with, forget the allocator, and clear
// the valid-send bit array. Always reports success.
bool ReliableMessageManager::Release()
{
    mpPlayerManager = nullptr;

    if (mpReliableMessageBuffer)
    {
        CGS_ASSERT(mpHeapAllocator, "mpHeapAllocator");
        mpHeapAllocator->Free(mpReliableMessageBuffer);
        mpReliableMessageBuffer = nullptr;
    }
    mpHeapAllocator = nullptr;

    // Clear the three 64-bit fields of the valid-send bit array (+0x8C8/+0x8D0/+0x8D8).
    mabValidSendData.UnSetAll();
    return true;
}

// ---- Update @ 0x8289AA50 -------------------------------------------------------------
// Per-frame pump: age out timed-out outgoing reliable messages, expire the rcvd-dup
// window, then recount the live buffered reliable messages by tallying the set bits of
// mabValidSendData. (The X360 build inlines the FastBitArray iteration and its container
// asserts here; reproduced through the committed FastBitArray helpers.)
void ReliableMessageManager::Update()
{
    CheckForReliableMessageTimeout();
    UpdateReliableMessagesReceived();

    miNumBufferedReliableMessages = 0;
    for (s32 liIndex = mabValidSendData.GetFirstBitSet();
         liIndex != CgsContainers::FastBitArray<KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER>::KI_INVALID_BIT_INDEX;
         liIndex = mabValidSendData.GetNextBitSet(liIndex))
    {
        ++miNumBufferedReliableMessages;
    }
}

// ---- MessageIsDuplicate @ 0x82874E50 -------------------------------------------------
// Receive-side duplicate/out-of-order filter for reliable messages. A non-reliable
// message can never be a duplicate (early out). Otherwise the fixed rcvd-dup window
// (maReliableMessagesRcvdData[140]) is scanned for an entry from the same sending player:
//   * an exact (player, frame, type) match => this message already arrived -> DUPLICATE;
//   * for message types that do NOT tolerate out-of-order arrivals (OldMessagesAreValid()
//     is false), a same-(player,type) entry whose remembered frame is newer than this
//     one's (wrapped 16-bit compare) means this arrival is stale -> DUPLICATE.
// Either way a hit logs through the debug stream and returns true. Otherwise the message
// is remembered: if an entry for this (player, type) already exists it is overwritten in
// place (asserting exactly one such slot was found), else it is written at the ring cursor
// (miReliableMessagesRecvdBufferIndex, wrapping at capacity) with a fresh valid countdown.
// Returns false (not a duplicate). (The X360 build dispatches Message vtable slots 0/1 --
// IsReliable / OldMessagesAreValid -- reproduced through the committed plain-method idiom.)
bool ReliableMessageManager::MessageIsDuplicate(Message* lpMessage)
{
    s32 liSameTypeAndPlayerIndex = -1;

    // vtable slot 0: only reliable messages participate in dup rejection.
    if (!lpMessage->IsReliable())
        return false;

    const NetworkPlayerID liSendingPlayerID =
        static_cast<MessageWithPlayerIDs*>(lpMessage)->GetSendingPlayerID();
    const s32 liType  = lpMessage->GetType();      // (s8)mi8Type sign-extended
    const s32 liFrame = lpMessage->mu16Frame;      // u16 widened (>= 0)

    for (s32 liIndex = 0; liIndex < KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER; ++liIndex)
    {
        const StoredRcvdMessageData& lReliableMessageRcvdData = maReliableMessagesRcvdData[liIndex];

        // Exact duplicate: same player, same frame, same type.
        if (lReliableMessageRcvdData.mPlayerID == liSendingPlayerID
            && lReliableMessageRcvdData.mu16FrameSent == lpMessage->mu16Frame
            && lReliableMessageRcvdData.miType == liType)
        {
            *CgsDev::Log::gpDebugPrint << "Dup reliable message frame " << liFrame
                                       << " type " << liType << "\n";
            return true;
        }

        // vtable slot 1: for ordered message types, an entry of the same (player, type)
        // holding a newer frame means this arrival is out-of-date.
        if (!lpMessage->OldMessagesAreValid())
        {
            if (lReliableMessageRcvdData.mPlayerID == liSendingPlayerID
                && lReliableMessageRcvdData.miType == liType
                && UInt16IsLargerWrapped(lReliableMessageRcvdData.mu16FrameSent, lpMessage->mu16Frame))
            {
                *CgsDev::Log::gpDebugPrint << "Old reliable message frame " << liFrame
                                           << " type " << liType << "\n";
                return true;
            }
        }

        // Remember the (single) existing slot holding this (player, type).
        if (lReliableMessageRcvdData.mPlayerID == liSendingPlayerID
            && lReliableMessageRcvdData.miType == liType)
        {
            CGS_ASSERT(liSameTypeAndPlayerIndex == -1, "liSameTypeAndPlayerIndex == -1");
            liSameTypeAndPlayerIndex = liIndex;
        }
    }

    if (liSameTypeAndPlayerIndex == -1)
    {
        // First message of this (player, type): append at the ring cursor.
        CGS_ASSERT(miReliableMessagesRecvdBufferIndex < KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER,
                   "miReliableMessagesRecvdBufferIndex < KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER");
        CGS_ASSERT(miReliableMessagesRecvdBufferIndex >= 0,
                   "miReliableMessagesRecvdBufferIndex >= 0");

        StoredRcvdMessageData& lReliableMessageRcvdData =
            maReliableMessagesRcvdData[miReliableMessagesRecvdBufferIndex];
        lReliableMessageRcvdData.mPlayerID       = liSendingPlayerID;
        lReliableMessageRcvdData.mu16FrameSent   = lpMessage->mu16Frame;
        lReliableMessageRcvdData.miType          = liType;
        lReliableMessageRcvdData.miValidCountdown = KI_FRAMES_TO_DISCARD_RELIABLE_MESSAGE_RECEIVED_DATA;

        if (++miReliableMessagesRecvdBufferIndex >= KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER)
            miReliableMessagesRecvdBufferIndex = 0;
        return false;
    }

    // A slot for this (player, type) already exists: refresh it in place.
    CGS_ASSERT(liSameTypeAndPlayerIndex < KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER,
               "liSameTypeAndPlayerIndex < KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER");
    CGS_ASSERT(liSameTypeAndPlayerIndex >= 0, "liSameTypeAndPlayerIndex >= 0");

    StoredRcvdMessageData& lReliableMessageRcvdData =
        maReliableMessagesRcvdData[liSameTypeAndPlayerIndex];
    lReliableMessageRcvdData.mPlayerID       = liSendingPlayerID;
    lReliableMessageRcvdData.mu16FrameSent   = lpMessage->mu16Frame;
    lReliableMessageRcvdData.miType          = liType;
    lReliableMessageRcvdData.miValidCountdown = KI_FRAMES_TO_DISCARD_RELIABLE_MESSAGE_RECEIVED_DATA;
    return false;
}

}
