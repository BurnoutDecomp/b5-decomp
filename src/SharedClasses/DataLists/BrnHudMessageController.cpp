#include "SharedClasses/DataLists/BrnHudMessageController.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnResource::HudMessageController -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file SharedClasses/DataLists/BrnHudMessageController.cpp).
//
// Bodied here:
//   HudMessageController::GetIndexFromMessageHash        @ 0x8267D4C8   (cpp:144)
//   HudMessageController::GetMessageTimeToWaitFromIndex  @ 0x824FC678
//   HudMessageController::GetMessageAvailabilityBitset   @ 0x824FC770
//   HudMessageController::GetMessageHashFromIndex        @ 0x824FC108
//   HudMessageController::GetMessageParamCount           @ 0x824FC200
//   HudMessageController::GetMessageParamType            @ 0x824FC328
//   HudMessageController::GetNextMessageIdInGroup        @ 0x824FC4C0
//   HudMessageController::GetPreviousMessageIdInGroup    @ 0x824FC5A0
//
// The loaded record fields are named per the full DWARF GuiHudMessageData/Resource layout
// (mppHudMessageData / miHudMessageCount / mMessageIdHash / muAvailabilityBitSet / mfTimeToWait
// / meMessageGroup / maiParamCount / maaeParams). The X360 re-derefs the resource pointer
// (ResourcePtr<GuiHudMessageResource>::operator->) on every count check + record fetch.

namespace BrnResource
{

// @ 0x8267D4C8 -- linear-scan the loaded message table for the record whose 64-bit
// message-id hash matches; -1 when the bundle is not loaded (non-gating assert,
// cpp:148) or the hash is absent.
s32 HudMessageController::GetIndexFromMessageHash(CgsID lId) const
{
    if (!mbMessagesUsed)
    {
        CGS_ASSERT(false, "Message bundle not loaded");   // cpp:148
        return -1;
    }

    s32 liIndex = 0;   // cpp:152
    if (mMessagesPtr->miHudMessageCount <= 0)
        return -1;

    while (mMessagesPtr->mppHudMessageData[liIndex]->mMessageIdHash != lId)
    {
        ++liIndex;
        if (liIndex >= mMessagesPtr->miHudMessageCount)
            return -1;
    }
    return liIndex;
}

// @ 0x824FC678 -- the inter-message wait time (seconds) for the record at liIndex, loaded with
// `lfs f1,0x120` (mfTimeToWait). Loaded-guard + index-range are non-gating asserts.
f32 HudMessageController::GetMessageTimeToWaitFromIndex(s32 liIndex) const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");   // h:324
    CGS_ASSERT(liIndex >= 0 && liIndex < mMessagesPtr->miHudMessageCount,
               "Invalid message index");           // h:325

    return mMessagesPtr->mppHudMessageData[liIndex]->mfTimeToWait;
}

// @ 0x824FC770 -- the per-message availability bitset (which game modes/contexts the message is
// allowed in) for the record at liIndex (mAvailabilityBitSet @+0x118). Non-gating guards.
u32 HudMessageController::GetMessageAvailabilityBitset(s32 liIndex) const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");   // h:341
    CGS_ASSERT(liIndex >= 0 && liIndex < mMessagesPtr->miHudMessageCount,
               "Invalid message index");           // h:342

    return mMessagesPtr->mppHudMessageData[liIndex]->muAvailabilityBitSet;
}

// @ 0x824FC108 -- the 64-bit message-id hash of the record at liIndex (ld r3,0x110 ==
// mMessageIdHash). Loaded-guard + index-range are non-gating.
CgsID HudMessageController::GetMessageHashFromIndex(s32 liIndex) const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");   // h:203
    CGS_ASSERT(liIndex >= 0 && liIndex < mMessagesPtr->miHudMessageCount,
               "Invalid message index");           // h:204

    return mMessagesPtr->mppHudMessageData[liIndex]->mMessageIdHash;
}

// @ 0x824FC200 -- how many parameters the liString'th string of the record at liMessageIndex takes
// (maiParamCount[liString] @+0x130). Every assert here is non-gating.
s32 HudMessageController::GetMessageParamCount(s32 liMessageIndex, s32 liString) const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");   // h:220
    CGS_ASSERT(liString < CgsGui::GuiHudMessageData::KI_MAX_NUM_STRINGS,
               "liString < CgsGui::GuiHudMessageData::KI_MAX_NUM_STRINGS");   // h:221
    CGS_ASSERT(liMessageIndex >= 0 && liMessageIndex < mMessagesPtr->miHudMessageCount,
               "Invalid message index");           // h:222

    return mMessagesPtr->mppHudMessageData[liMessageIndex]->maiParamCount[liString];
}

// @ 0x824FC328 -- the type of parameter liParam of string liString for the record at
// liMessageIndex (maaeParams[liString][liParam] @+0x13C, flattened row-major). The liParam bound
// reuses the same "Invalid message index" rodata string (reproduced verbatim). Non-gating.
CgsGui::HudMessageParamTypes HudMessageController::GetMessageParamType(
    s32 liMessageIndex, s32 liString, s32 liParam) const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");   // h:241
    CGS_ASSERT(liString < CgsGui::GuiHudMessageData::KI_MAX_NUM_STRINGS,
               "liString < CgsGui::GuiHudMessageData::KI_MAX_NUM_STRINGS");   // h:242
    CGS_ASSERT(liMessageIndex >= 0 && liMessageIndex < mMessagesPtr->miHudMessageCount,
               "Invalid message index");           // h:243
    CGS_ASSERT(liParam >= 0 && liParam < CgsGui::GuiHudMessageData::KI_MAX_PARAMS_PER_STRING,
               "Invalid message index");           // h:244

    return mMessagesPtr->mppHudMessageData[liMessageIndex]->maaeParams[liString][liParam];
}

// @ 0x824FC4C0 -- find the next message id (wrapping) whose group matches leGroup, starting just
// past liCurrentId. leGroup == E_HUDMESSAGEGROUP_ALL (0) matches immediately (the `&& leGroup`
// short-circuit). If nothing in [liCurrentId+1, count) matches, rescan [0, liCurrentId); on total
// failure return liCurrentId. (record group @+0x12C = meMessageGroup, count @+8 = miHudMessageCount.)
s32 HudMessageController::GetNextMessageIdInGroup(s32 liCurrentId,
                                                 CgsGui::HudMessageGroups leGroup) const
{
    s32 liIndex = liCurrentId + 1;
    if (liIndex < mMessagesPtr->miHudMessageCount)
    {
        while (mMessagesPtr->mppHudMessageData[liIndex]->meMessageGroup != leGroup && leGroup != 0)
        {
            ++liIndex;
            if (liIndex >= mMessagesPtr->miHudMessageCount)
                goto searchFromStart;
        }
        return liIndex;
    }

searchFromStart:
    if (liCurrentId > 0)
    {
        s32 liCount = 0;
        s32 liScan = 0;
        while (mMessagesPtr->mppHudMessageData[liScan]->meMessageGroup != leGroup && leGroup != 0)
        {
            ++liCount;
            ++liScan;
            if (liCount >= liCurrentId)
                return liCurrentId;
        }
        return liCount;
    }
    return liCurrentId;
}

// @ 0x824FC5A0 -- from a starting message id, walk BACKWARD (descending index) to the previous
// loaded record whose group matches leGroup; when leGroup is E_HUDMESSAGEGROUP_ALL (0) any record
// matches. If the descending scan runs off the front it wraps to the end of the table and keeps
// descending toward liCurrentId; returns liCurrentId when no other member of the group exists.
s32 HudMessageController::GetPreviousMessageIdInGroup(s32 liCurrentId, CgsGui::HudMessageGroups leGroup) const
{
    s32 liIndex = liCurrentId - 1;
    if (liIndex >= 0)
    {
        while (mMessagesPtr->mppHudMessageData[liIndex]->meMessageGroup != leGroup
               && leGroup != CgsGui::E_HUDMESSAGEGROUP_ALL)
        {
            --liIndex;
            if (liIndex < 0)
                goto scanFromEnd;
        }
        return liIndex;
    }

scanFromEnd:
    {
        s32 liEndIndex = mMessagesPtr->miHudMessageCount - 1;
        if (liEndIndex <= liCurrentId)
            return liCurrentId;

        while (mMessagesPtr->mppHudMessageData[liEndIndex]->meMessageGroup != leGroup
               && leGroup != CgsGui::E_HUDMESSAGEGROUP_ALL)
        {
            --liEndIndex;
            if (liEndIndex <= liCurrentId)
                return liCurrentId;
        }
        return liEndIndex;
    }
}

}
