#include "SharedClasses/DataLists/BrnHudMessageController.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnResource::HudMessageController -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file SharedClasses/DataLists/BrnHudMessageController.cpp).
//
// Bodied here (1 ledger function):
//   HudMessageController::GetIndexFromMessageHash @ 0x8267D4C8   (cpp:144)

namespace BrnResource
{

// @ 0x8267D4C8 -- linear-scan the loaded message table for the record whose 64-bit
// message-id hash matches; -1 when the bundle is not loaded (non-gating assert,
// cpp:148) or the hash is absent. The X360 re-derefs the resource pointer
// (ResourcePtr<GuiHudMessageResource>::operator->, a real out-of-line symbol there)
// on every count check and every record fetch -- reproduced call-for-call.
s32 HudMessageController::GetIndexFromMessageHash(CgsID lId) const
{
    if (!mbMessagesUsed)
    {
        CGS_ASSERT(false, "Message bundle not loaded");   // cpp:148
        return -1;
    }

    s32 liIndex = 0;   // cpp:152
    if (mMessagesPtr->miMessageCount <= 0)
        return -1;

    while (mMessagesPtr->mppMessages[liIndex]->mHudMessageId != lId)
    {
        ++liIndex;
        if (liIndex >= mMessagesPtr->miMessageCount)
            return -1;
    }
    return liIndex;
}

}
