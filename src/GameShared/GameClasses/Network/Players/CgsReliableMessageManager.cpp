#include "GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"   // GetFrameDiffWrapped16 (namespace CgsNetwork)
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"                    // CgsMemory::HeapMalloc::Free
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT

// CgsNetwork::ReliableMessageManager -- reconstructed method bodies (BURNOUT_X360_ARTIST.XEX).
// The queue's full layout lives in the owning header; three of its methods are homed here
// (GetNextReliableMessageToResend / Release / Update). MessageIsDuplicate is DWARF-attested and
// declared in the header but its body is NOT homed here: the X360 body dispatches a bool through
// the Message vtable's slot 1 (a predicate the DWARF does not name and the committed Message base
// does not declare), so homing it would require fabricating an un-attested virtual on the shared
// Message base. Left for a future TU that grounds that slot.

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

}
