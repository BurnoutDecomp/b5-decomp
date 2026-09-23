#include "GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h"
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
    CGS_ASSERT(mpHeapAllocator, "mpHeapAllocator");

    mpReliableMessageBuffer = static_cast<u8*>(mpHeapAllocator->Malloc(
        KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER * KI_MAX_RELIABLE_MESSAGE_SIZE, 4));
    CGS_ASSERT(mpReliableMessageBuffer, "mpReliableMessageBuffer");

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
    CGS_ASSERT(lpMsg, "lpMsg");
    CGS_ASSERT(lpMsg->IsMessageValid(), "lpMsg->IsMessageValid()");
    CGS_ASSERT(liLength <= KI_MAX_RELIABLE_MESSAGE_SIZE, "liLength <= KI_MAX_RELIABLE_MESSAGE_SIZE");

    BufferedSendMessageData* lpFreeSlot = nullptr;
    for (s32 liAttempt = 0; liAttempt < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER; ++liAttempt)
    {
        CGS_ASSERT(miReliableMessageSendIndex >= 0, "miReliableMessageSendIndex >= 0");
        CGS_ASSERT(miReliableMessageSendIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER,
                   "miReliableMessageSendIndex < KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER");
        CGS_ASSERT(maReliableMessageSendData[miReliableMessageSendIndex].mpMsg,
                   "maReliableMessageSendData[miReliableMessageSendIndex].mpMsg");

        if (!mabValidSendData.IsBitSet(static_cast<u32>(miReliableMessageSendIndex)))
        {
            CGS_ASSERT(!maReliableMessageSendData[miReliableMessageSendIndex].mpMsg->IsMessageValid(),
                       "!maReliableMessageSendData[miReliableMessageSendIndex].mpMsg->IsMessageValid()");
            lpFreeSlot = &maReliableMessageSendData[miReliableMessageSendIndex];
            break;
        }

        if (++miReliableMessageSendIndex >= KI_MAX_RELIABLE_MESSAGES_SEND_TO_BUFFER)
        {
            miReliableMessageSendIndex = 0;
        }
    }

    if (lpFreeSlot == nullptr)
    {
        CGS_ASSERT(false, "Failed to find a reliable message buffer to use");
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
    *CgsDev::Log::gpDebugPrint << "Clear buffered reliable messages to " << lPlayerID << "\n";

    miReliableMessageSendIndex = 0;

    NetworkPlayerID lLocalPlayerID;
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
            *CgsDev::Log::gpDebugPrint
                << "Tried to discard buffered reliable message, but it is marked as valid ("
                << static_cast<s32>(lpSendMessageData->mu16FrameLastSent) << " - "
                << static_cast<s32>(lpSendMessageData->mu16FrameFirstSent) << "> "
                << KI_FRAMES_TO_DISCARD_RELIABLE_MESSAGE << ")\n";
        }
        else
        {
            *CgsDev::Log::gpDebugPrint
                << "Triggered discard of buffered reliable message ("
                << static_cast<s32>(lpSendMessageData->mu16FrameLastSent) << " - "
                << static_cast<s32>(lpSendMessageData->mu16FrameFirstSent) << "> "
                << KI_FRAMES_TO_DISCARD_RELIABLE_MESSAGE << ")\n";

            NetworkPlayerID lLocalPlayerID;
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

        CGS_ASSERT(lData.miValidCountdown == 0, "maReliableMessagesRcvdData[i].miValidCountdown == 0");
        *CgsDev::Log::gpDebugPrint << "Removing reliable message received data for message type "
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
    CGS_ASSERT(lSendingPlayerID != K_INVALID_PLAYER_ID, "lSendingPlayerID != K_INVALID_PLAYER_ID");
    CGS_ASSERT(lRecvingPlayerID != K_INVALID_PLAYER_ID, "lRecvingPlayerID != K_INVALID_PLAYER_ID");

    lNackMessage.Construct();
    lNackMessage.PrepareNack(lpMessage, lSendingPlayerID, lRecvingPlayerID);

    *CgsDev::Log::gpDebugPrint << "Pretending a nack for msg type "
                               << static_cast<s32>(lpMessage->mi8Type) << " frame "
                               << static_cast<s32>(lpMessage->mu16Frame)
                               << " to player calling themselves index " << lSendingPlayerID
                               << "\n";
    *CgsDev::Log::gpDebugPrint << "We are calling ourselves " << lRecvingPlayerID << "\n";

    NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lRecvingPlayerID);
    if (lpNetworkPlayer != nullptr)
    {
        *CgsDev::Log::gpDebugPrint << "Telling player " << lRecvingPlayerID
                                   << " about non-delivered message\n";
        lpNetworkPlayer->ReceiveAckOrNack(&lNackMessage, lbAck);
    }
}

}
