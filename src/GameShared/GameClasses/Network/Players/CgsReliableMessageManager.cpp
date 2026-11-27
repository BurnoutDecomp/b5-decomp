#include "GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // mNetStreamLogChannelOutput (0x82F335C8)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"              // GetFrameDiffWrapped16 / UInt16IsLargerWrapped / IsReliable / OldMessagesAreValid
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h" // MessageWithPlayerIDs::GetSendingPlayerID
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"                    // CgsMemory::HeapMalloc::Free
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // CgsDev::Log::gpDebugPrint
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Players\\CgsReliableMessageManager.cpp"
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
    mpPlayerManager = 0;

    if (mpReliableMessageBuffer)
    {
        CGS_ASSERT_AT(mpHeapAllocator,
            "mpHeapAllocator",
                "..\\..\\..\\GameShared\\GameClasses\\Network\\Players\\CgsReliableMessageManager.cpp", 160 /* ARTIST 0x82890A50 */);

        mpHeapAllocator->Free(mpReliableMessageBuffer);
        mpReliableMessageBuffer = 0;
    }
    mpHeapAllocator = 0;

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
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Dup reliable message frame " << liFrame
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
                CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Old reliable message frame " << liFrame
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

// FOLDED FROM CgsReliableMessageManager_wN2_01.cpp
#include "GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // GetPlayerByID / GetNextLocalPlayerID
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // NetworkPlayer::ReceiveAckOrNack
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"            // GetFrameDiffWrapped16
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h"      // SignalMessage::PrepareNack
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"                              // CgsMemory::HeapMalloc::Malloc
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                            // CgsDev::Log::gpDebugPrint

#include <cstring>   // memcpy

// CgsNetwork::ReliableMessageManager -- the send-pool lifecycle, the send-side timeouts and
// the received-message window expiry.
//
// The console inlines the FastBitArray iterator into every walk of mabValidSendData,
// out-of-range StrStream asserts included; the walks here go through the container's
// GetFirstBitSet / GetNextBitSet (the same convention as GetNextReliableMessageToResend).

namespace CgsNetwork
{

typedef CgsContainers::FastBitArray<ReliableMessageManager::KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER>
    ValidSendBits;

// ---- Prepare -------------------------------------------------------------------------
// Bind the registry and the heap, carve the send pool into KI_MAX_RELIABLE_MESSAGE_SIZE
// slots (one per send entry, each slot's message field-reset), clear the received window
// and the valid-send bits.
bool ReliableMessageManager::Prepare(PlayerManager* lpPlayerManager,
                                     CgsMemory::HeapMalloc* lpHeapAllocator)
{
    mpPlayerManager = lpPlayerManager;
    mpHeapAllocator = lpHeapAllocator;
    CGS_ASSERT_AT(mpHeapAllocator, "mpHeapAllocator", CGS_ARTIST_FILE, 95);

    mpReliableMessageBuffer = static_cast<u8*>(mpHeapAllocator->Malloc(
        KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER * KI_MAX_RELIABLE_MESSAGE_SIZE, 4));
    CGS_ASSERT_AT(mpReliableMessageBuffer, "mpReliableMessageBuffer", CGS_ARTIST_FILE, 98);

    for (s32 liIndex = 0; liIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER; ++liIndex)
    {
        BufferedSendMessageData& lData = maReliableMessageSendData[liIndex];
        lData.mPlayerID          = -1;
        lData.miLength           = -1;
        lData.mu16FrameFirstSent = 0xFFFF;
        lData.mu16FrameLastSent  = 0xFFFF;
        lData.mpMsg = reinterpret_cast<Message*>(mpReliableMessageBuffer
                                                 + liIndex * KI_MAX_RELIABLE_MESSAGE_SIZE);
        lData.mpMsg->Construct();
    }

    // The received window is cleared without touching its ring cursor here.
    for (s32 liIndex = 0; liIndex < KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER; ++liIndex)
    {
        StoredRcvdMessageData& lData = maReliableMessagesRcvdData[liIndex];
        lData.mPlayerID        = -1;
        lData.mu16FrameSent    = 0xFFFF;
        lData.miType           = -1;
        lData.miValidCountdown = -1;
    }

    mabValidSendData.UnSetAll();
    return true;
}

// ---- AddBufferedReliableMessage ------------------------------------------------------
// Copy a valid reliable message bound for lPlayerID into the next free send slot, walking
// the ring from miReliableMessageSendIndex; asserts when the whole pool is in use.
void ReliableMessageManager::AddBufferedReliableMessage(NetworkPlayerID lPlayerID,
                                                        Message* lpMsg, s32 liLength)
{
    CGS_ASSERT_AT(lpMsg, "lpMsg", CGS_ARTIST_FILE, 271);
    CGS_ASSERT_AT(lpMsg->IsMessageValid(), "lpMsg->IsMessageValid()", CGS_ARTIST_FILE, 272);
    CGS_ASSERT_AT(liLength <= KI_MAX_RELIABLE_MESSAGE_SIZE, "liLength <= KI_MAX_RELIABLE_MESSAGE_SIZE", CGS_ARTIST_FILE, 275);

    BufferedSendMessageData* lpFreeSlot = 0;
    for (s32 liAttempt = 0; liAttempt < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER; ++liAttempt)
    {
        CGS_ASSERT_AT(miReliableMessageSendIndex >= 0, "miReliableMessageSendIndex >= 0", CGS_ARTIST_FILE, 280);
        CGS_ASSERT_AT(miReliableMessageSendIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER,
                   "miReliableMessageSendIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER", CGS_ARTIST_FILE, 281);
        CGS_ASSERT_AT(maReliableMessageSendData[miReliableMessageSendIndex].mpMsg,
                   "maReliableMessageSendData[miReliableMessageSendIndex].mpMsg", CGS_ARTIST_FILE, 282);

        if (!mabValidSendData.IsBitSet(static_cast<u32>(miReliableMessageSendIndex)))
        {
            CGS_ASSERT_AT(!maReliableMessageSendData[miReliableMessageSendIndex].mpMsg->IsMessageValid(),
                       "!maReliableMessageSendData[miReliableMessageSendIndex].mpMsg->IsMessageValid()", CGS_ARTIST_FILE, 288);
            lpFreeSlot = &maReliableMessageSendData[miReliableMessageSendIndex];
            break;
        }

        if (++miReliableMessageSendIndex >= KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER)
        {
            miReliableMessageSendIndex = 0;
        }
    }

    if (lpFreeSlot == 0)
    {
        CGS_FAIL_STREAM_AT("Failed to find a reliable message buffer to use", CGS_ARTIST_FILE, 306);
        return;
    }

    mabValidSendData.SetBit(static_cast<u32>(miReliableMessageSendIndex));
    memcpy(lpFreeSlot->mpMsg, lpMsg, liLength);
    lpFreeSlot->miLength           = liLength;
    lpFreeSlot->mPlayerID          = lPlayerID;
    lpFreeSlot->mu16FrameFirstSent = 0xFFFF;
    lpFreeSlot->mu16FrameLastSent  = 0xFFFF;
}

// ---- RemoveBufferedReliableMessage ---------------------------------------------------
// Free one send slot (clear its valid bit).
void ReliableMessageManager::RemoveBufferedReliableMessage(s32 liIndex)
{
    CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
    CGS_ASSERT(liIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER,
               "liIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER");
    mabValidSendData.UnSetBit(static_cast<u32>(liIndex));
}

// Free the first buffered message the ack / nack answers: same frame, same type and bound
// for the player the ack / nack came back to us from.
void ReliableMessageManager::RemoveBufferedReliableMessage(SignalMessage* lpAckOrNackMsg)
{
    CGS_ASSERT(lpAckOrNackMsg, "lpAckOrNackMsg");

    for (s32 liIndex = mabValidSendData.GetFirstBitSet();
         liIndex != ValidSendBits::KI_INVALID_BIT_INDEX;
         liIndex = mabValidSendData.GetNextBitSet(liIndex))
    {
        BufferedSendMessageData& lData = maReliableMessageSendData[liIndex];
        CGS_ASSERT(lData.mpMsg, "maReliableMessageSendData[lIt.GetIndex()].mpMsg");

        if (lData.mpMsg->mu16Frame == lpAckOrNackMsg->mu16Frame
            && lData.mpMsg->mi8Type == lpAckOrNackMsg->mi8Type
            && lData.mPlayerID == lpAckOrNackMsg->GetRecvingPlayerID())
        {
            RemoveBufferedReliableMessage(liIndex);
            return;
        }
    }
}

// ---- ClearPlayersSendReliableMessages ------------------------------------------------
// Drop every buffered reliable message bound for lPlayerID, telling that player's delivery
// callback it was not delivered (while we still have a local player), and rewind the send
// cursor.
void ReliableMessageManager::ClearPlayersSendReliableMessages(NetworkPlayerID lPlayerID)
{
    CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Clear buffered reliable messages to " << lPlayerID << "\n";

    miReliableMessageSendIndex = 0;

    NetworkPlayerID lLocalPlayerID = K_INVALID_PLAYER_ID;
    mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);

    for (s32 liIndex = mabValidSendData.GetFirstBitSet();
         liIndex != ValidSendBits::KI_INVALID_BIT_INDEX;
         liIndex = mabValidSendData.GetNextBitSet(liIndex))
    {
        BufferedSendMessageData& lData = maReliableMessageSendData[liIndex];
        if (lData.mPlayerID != lPlayerID)
        {
            continue;
        }

        if (lLocalPlayerID != K_INVALID_PLAYER_ID)
        {
            FakeNackMessage(lData.mpMsg, lLocalPlayerID, lData.mPlayerID, true);
        }
        RemoveBufferedReliableMessage(liIndex);
    }
}

// ---- CheckForReliableMessageTimeout --------------------------------------------------
// A buffered reliable message sent more than KI_FRAMES_TO_DISCARD_RELIABLE_MESSAGE frames
// after its first send is discarded and reported to its sender's delivery callback as not
// delivered -- unless it is still marked valid (queued for resend this frame), which only
// logs.
void ReliableMessageManager::CheckForReliableMessageTimeout()
{
    for (s32 liIndex = mabValidSendData.GetFirstBitSet();
         liIndex != ValidSendBits::KI_INVALID_BIT_INDEX;
         liIndex = mabValidSendData.GetNextBitSet(liIndex))
    {
        BufferedSendMessageData* lpSendMessageData = &maReliableMessageSendData[liIndex];
        if (lpSendMessageData->mu16FrameFirstSent == 0xFFFF)
        {
            continue;
        }

        CGS_ASSERT(lpSendMessageData->mu16FrameLastSent != 0xFFFF,
                   "lpSendMessageData->mu16FrameLastSent != KU16_INVALID_FRAME");
        if (!(GetFrameDiffWrapped16(lpSendMessageData->mu16FrameLastSent,
                                    lpSendMessageData->mu16FrameFirstSent)
              > KI_FRAMES_TO_DISCARD_RELIABLE_MESSAGE))
        {
            continue;
        }

        if (lpSendMessageData->mpMsg->IsMessageValid())
        {
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput
                << "Tried to discard buffered reliable message, but it is marked as valid ("
                << static_cast<s32>(lpSendMessageData->mu16FrameLastSent) << " - "
                << static_cast<s32>(lpSendMessageData->mu16FrameFirstSent) << "> "
                << KI_FRAMES_TO_DISCARD_RELIABLE_MESSAGE << ")\n";
        }
        else
        {
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput
                << "Triggered discard of buffered reliable message ("
                << static_cast<s32>(lpSendMessageData->mu16FrameLastSent) << " - "
                << static_cast<s32>(lpSendMessageData->mu16FrameFirstSent) << "> "
                << KI_FRAMES_TO_DISCARD_RELIABLE_MESSAGE << ")\n";

            NetworkPlayerID lLocalPlayerID = K_INVALID_PLAYER_ID;
            mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
            FakeNackMessage(lpSendMessageData->mpMsg, lLocalPlayerID,
                            lpSendMessageData->mPlayerID, false);
            RemoveBufferedReliableMessage(liIndex);
        }
    }
}

// ---- UpdateReliableMessagesReceived --------------------------------------------------
// Count down every live entry of the received-duplicate window; an entry reaching zero is
// logged and forgotten.
void ReliableMessageManager::UpdateReliableMessagesReceived()
{
    for (s32 liIndex = 0; liIndex < KI_MAX_RELIABLE_MESSAGES_RECV_TO_BUFFER; ++liIndex)
    {
        StoredRcvdMessageData& lData = maReliableMessagesRcvdData[liIndex];
        if (lData.miValidCountdown <= 0)
        {
            continue;
        }
        if (--lData.miValidCountdown > 0)
        {
            continue;
        }

        CGS_ASSERT_AT(lData.miValidCountdown == 0,
            "maReliableMessagesRcvdData[i].miValidCountdown == 0",
            CGS_ARTIST_FILE, 721 /* ARTIST 0x82875250 */);
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Removing reliable message received data for message type "
                                   << lData.miType << " from player " << lData.mPlayerID
                                   << " with frame " << static_cast<s32>(lData.mu16FrameSent)
                                   << "\n";
        lData.mPlayerID        = -1;
        lData.mu16FrameSent    = 0xFFFF;
        lData.miType           = -1;
        lData.miValidCountdown = -1;
    }
}

// ---- FakeNackMessage -----------------------------------------------------------------
// Build a nack for lpMessage as if lRecvingPlayerID had sent it back to us
// (lSendingPlayerID), and hand it to that player's ack/nack handler.
void ReliableMessageManager::FakeNackMessage(Message* lpMessage, NetworkPlayerID lSendingPlayerID,
                                             NetworkPlayerID lRecvingPlayerID, bool lbAck)
{
    SignalMessage lNackMessage;
    CGS_ASSERT_AT(lSendingPlayerID != K_INVALID_PLAYER_ID,
        "lSendingPlayerID != K_INVALID_PLAYER_ID",
        CGS_ARTIST_FILE, 568 /* ARTIST 0x82890AF8 */);
    CGS_ASSERT_AT(lRecvingPlayerID != K_INVALID_PLAYER_ID,
        "lRecvingPlayerID != K_INVALID_PLAYER_ID",
        CGS_ARTIST_FILE, 569 /* ARTIST 0x82890B1C */);

    lNackMessage.Construct();
    lNackMessage.PrepareNack(lpMessage, lSendingPlayerID, lRecvingPlayerID);

    CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Pretending a nack for msg type "
                               << static_cast<s32>(lpMessage->mi8Type) << " frame "
                               << static_cast<s32>(lpMessage->mu16Frame)
                               << " to player calling themselves index " << lSendingPlayerID
                               << "\n";
    CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "We are calling ourselves " << lRecvingPlayerID << "\n";

    NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lRecvingPlayerID);
    if (lpNetworkPlayer != 0)
    {
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Telling player " << lRecvingPlayerID
                                   << " about non-delivered message\n";
        lpNetworkPlayer->ReceiveAckOrNack(&lNackMessage, lbAck);
    }
}

}
