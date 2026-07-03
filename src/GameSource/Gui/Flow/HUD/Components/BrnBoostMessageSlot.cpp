#include "GameSource/Gui/Flow/HUD/Components/BrnBoostMessageSlot.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::SPrintf / SnPrintf
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"           // BrnFlapt::FileRef
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h" // MovieClipInstance (the frame poll)

// BrnGui::BoostMessageSlot -- reconstructed from BURNOUT_X360_ARTIST.XEX (DWARF
// primary file GameSource/Gui/Flow/HUD/Components/BrnBoostMessageSlot.cpp).
//
// Bodied here (8 ledger functions):
//   Construct @0x82411D30   Prepare @0x82420C88   SetMessage @0x82420D60
//   SetMessageText @0x82411E30   Refresh @0x82411EE8   ShuffleUp @0x82411F40
//   ShuffleDown @0x824120E8   ResetSlotPosition @0x82412290   Update @0x82420E60
//   (Update + the seven above -- the ledger counts Construct/Prepare as one pair
//   with the rest.)

namespace BrnGui
{

namespace
{
    // The slot clip's settle frames (Update consumes the shuffle latch on them)
    // and the item clip's settle frames -- ONE-BASED, the accessor's contract
    // (the X360 inlines GetCurrentFrameOneBased as the zero-based +0x28 field
    // plus one and compares 10/19/27/34 and 13/45/66).
    const s32 KAI_SLOT_SETTLE_FRAMES[4] = { 10, 19, 27, 34 };
    const s32 KAI_ITEM_SETTLE_FRAMES[3] = { 13, 45, 66 };

    const s32 KI_MAX_SLOT_POSITION = 3;   // the shuffle bounds (positions 0..3)
}

// @ 0x82411D30 -- cpp:52. The streamed tripwire ahead of the base's h:113 one,
// the item's Construct (NULL name), the latch seeds.
void BoostMessageSlot::Construct(const char* /*lpacName*/, CgsGui::StateInterface* lpStateInterface,
                                 const char* /*lpacParentName*/)
{
    CGS_ASSERT(lpStateInterface != 0, "Invalid state interface");   // :52 (streamed; folded)
    BrnFlaptComponent::Construct(lpStateInterface);                 // the h:113 tripwire + iface store + clip clear
    mMessageItem.Construct(0, lpStateInterface, 0);
    mbInUse        = false;
    mbInTransition = false;
    miSlotPosition = 0;
    mfTimeToLive   = 0.0f;
    miMessageId    = -1;
}

// @ 0x82420C88 -- bind the slot clip (the base Prepare content, no parent
// prefix), then the item under the "<name>_MessageItem" composite.
void BoostMessageSlot::Prepare(const char* lpacName, const BrnFlapt::FileRef& lrFile)
{
    BrnFlaptComponent::Prepare(lpacName, lrFile, 0);   // inlined on the X360

    char lacComposite[160];   // X360 sp+0x60 (formatted through a 128 cap)
    CgsCore::SPrintf(lacComposite, 128, "%s_%s", lpacName, "MessageItem");
    lacComposite[127] = 0;
    mMessageItem.Prepare(lacComposite, lrFile);
}

// @ 0x82420D60 -- cpp:152. Stage a message into a free slot.
void BoostMessageSlot::SetMessage(const char* lpcText, s32 liMessageId, f32 lfTimeToLive,
                                  s32 leBoostType, s32 liBoostAmount)
{
    CGS_ASSERT(!mbInTransition && !mbInUse,
               "Slot must be available and not in transition");   // :152 (streamed; folded)

    mfTimeToLive = lfTimeToLive;
    miMessageId  = liMessageId;
    mbInUse      = true;
    ResetSlotPosition();
    mMessageItem.SetText(lpcText, liBoostAmount);
    mMessageItem.SetTransition(BoostMessageItem::E_TRANSITION_ON, leBoostType);
}

// @ 0x82411E30 -- cpp:177.
void BoostMessageSlot::SetMessageText(const char* lpcText, s32 liBoostAmount)
{
    CGS_ASSERT(mbInUse, "Setting text on an expired object is nonsensical");   // :177 (streamed; folded)
    mMessageItem.SetText(lpcText, liBoostAmount);
}

// @ 0x82411EE8 -- extend the ttl and (when asked, with both clips settled) play
// the item's refresh pulse.
void BoostMessageSlot::Refresh(f32 lfTimeToLive, bool lbPlayRefreshAnim)
{
    if (mfTimeToLive < lfTimeToLive)
        mfTimeToLive = lfTimeToLive;
    if (lbPlayRefreshAnim && !mbInTransition && !mMessageItem.IsInTransition())
        mMessageItem.PlayLabel("MessageRefresh");
}

// @ 0x82411F40 -- cpp:283/:285. Play the "<pos>-<pos+1>" shuffle frame and step
// the position (asserts folded static, non-gating).
void BoostMessageSlot::ShuffleUp()
{
    CGS_ASSERT(!mbInTransition, "Cannot shuffle when already shuffling");        // :283
    CGS_ASSERT(miSlotPosition < KI_MAX_SLOT_POSITION, "Shuffling beyond our bounds");
    CGS_ASSERT(mbInUse, "Cannot shuffle when not in use");                       // :285

    char lacLabel[32];
    CgsCore::SPrintf(lacLabel, 32, "%d-%d", miSlotPosition, miSlotPosition + 1);
    mAptRef.GotoAndPlayLabel(lacLabel);
    mbInTransition = true;
    ++miSlotPosition;
}

// @ 0x824120E8 -- cpp:313/:315. The downward counterpart ("<pos>-<pos-1>").
void BoostMessageSlot::ShuffleDown()
{
    CGS_ASSERT(!mbInTransition, "Cannot shuffle when already shuffling");        // :313
    CGS_ASSERT(miSlotPosition > 0, "Shuffling below the baseline");
    CGS_ASSERT(mbInUse, "Cannot shuffle when not in use");                       // :315

    char lacLabel[32];
    CgsCore::SPrintf(lacLabel, 32, "%d-%d", miSlotPosition, miSlotPosition - 1);
    mAptRef.GotoAndPlayLabel(lacLabel);
    mbInTransition = true;
    --miSlotPosition;
}

// @ 0x82412290 -- cpp:343. Snap back to the baseline frame.
void BoostMessageSlot::ResetSlotPosition()
{
    CGS_ASSERT(!mbInTransition,
               "Please wait for all transitions to complete before changing slot position");   // :343 (streamed; folded)
    mAptRef.GotoAndStopLabel("Position0");
    miSlotPosition = 0;
}

// @ 0x82420E60 -- cpp:266. The per-frame tick.
void BoostMessageSlot::Update(f32 lfTimeStep)
{
    if (mbInUse)
    {
        if (mfTimeToLive < 0.0f)
        {
            // Expired and both clips settled: free the slot.
            if (!mbInTransition && !mMessageItem.IsInTransition())
            {
                mbInUse        = false;
                miSlotPosition = 0;
                miMessageId    = -1;
            }
        }
        else
        {
            // The ttl only advances while positive, or once the out-transition
            // has been issued -- while the item is still transitioning (in) at
            // expiry, the ttl FREEZES (the X360 skips the store).
            f32 lfRemaining = mfTimeToLive - lfTimeStep;
            if (lfRemaining > 0.0f)
            {
                mfTimeToLive = lfRemaining;
            }
            else if (!mMessageItem.IsInTransition())
            {
                // Time up: transition the message out (no boost-type change).
                mMessageItem.SetTransition(BoostMessageItem::E_TRANSITION_OFF, -1);
                mfTimeToLive = mfTimeToLive - lfTimeStep;
            }
        }
    }

    // Consume the slot clip's shuffle latch on its settle frames (with the :266
    // tripwire while a shuffle overruns frame 34)...
    const s32 liSlotFrame = mAptRef.mpMovieClipInst->GetCurrentFrameOneBased();
    if (liSlotFrame == KAI_SLOT_SETTLE_FRAMES[0] || liSlotFrame == KAI_SLOT_SETTLE_FRAMES[1]
        || liSlotFrame == KAI_SLOT_SETTLE_FRAMES[2] || liSlotFrame == KAI_SLOT_SETTLE_FRAMES[3])
    {
        mbInTransition = false;
    }
    CGS_ASSERT(!mbInTransition || liSlotFrame <= 34,
               "!mbInTransition || mMovieClipRef.GetCurrentFrameOneBased()<=34");   // :266 (non-gating)

    // ...and the item clip's transition latch on its settle frames.
    const s32 liItemFrame = mMessageItem.GetAptRef().mpMovieClipInst->GetCurrentFrameOneBased();
    if (liItemFrame == KAI_ITEM_SETTLE_FRAMES[0] || liItemFrame == KAI_ITEM_SETTLE_FRAMES[1]
        || liItemFrame == KAI_ITEM_SETTLE_FRAMES[2])
    {
        mMessageItem.ClearInTransition();
    }
}

}
