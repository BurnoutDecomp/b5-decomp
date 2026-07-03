#pragma once

#include "types.hpp"
#include "GameSource/Gui/Flow/HUD/Components/BrnBoostMessageItem.h"   // BoostMessageItem (embedded)

// BrnGui::BoostMessageSlot - one slot of the boost-message HUD stack: the slot
// clip (whose "Position0".."%d-%d" frames animate the stack position) wrapping
// an embedded BoostMessageItem ("<name>_MessageItem"), with the in-use /
// in-transition latches, the message id, the stack position and the message
// time-to-live. The BoostMessageManager drives up to four of these. Shape
// asm-derived from the eight exported bodies (the asserts name the original
// source GameSource/Gui/Flow/HUD/Components/BrnBoostMessageSlot.cpp and the
// members mbInTransition / mMovieClipRef). X360 layout: the BrnFlaptComponent
// base (iface +0x00, mMovieClipRef +0x04), mMessageItem +0x0C (0x28 bytes),
// mbInUse +0x34, mbInTransition +0x35, miMessageId +0x38 (-1 == none),
// miSlotPosition +0x3C (0..3), mfTimeToLive +0x40.
namespace BrnGui
{
    class BoostMessageSlot : public BrnFlaptComponent
    {
    public:
        // @0x82411D30 (this TU, cpp:52) -- the streamed interface tripwire ahead
        // of the base's h:113 one, the embedded item's Construct (NULL name), and
        // the latch seeds (message id to -1).
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);

        // @0x82420C88 (this TU) -- bind the slot clip (the base Prepare content,
        // no parent prefix), then prepare the item under "<name>_MessageItem".
        void Prepare(const char* lpacName, const BrnFlapt::FileRef& lrFile);

        // @0x82420D60 (this TU, cpp:152) -- stage a message into a free slot:
        // latch the ttl/id/in-use, reset the stack position, push the text (with
        // the boost amount) and play the item's TransitionIn with the boost type.
        void SetMessage(const char* lpcText, s32 liMessageId, f32 lfTimeToLive,
                        s32 leBoostType, s32 liBoostAmount);

        // @0x82411E30 (this TU, cpp:177) -- update the live message's text.
        void SetMessageText(const char* lpcText, s32 liBoostAmount);

        // @0x82411EE8 (this TU) -- extend the ttl (max) and, when asked and no
        // transition is running on either clip, play the item's "MessageRefresh".
        void Refresh(f32 lfTimeToLive, bool lbPlayRefreshAnim);

        // @0x82411F40 (this TU, cpp:283/:285) / @0x824120E8 (cpp:313/:315) -- step
        // the slot one stack position up/down (the "%d-%d" transition frame on the
        // slot clip; asserts folded static, non-gating).
        void ShuffleUp();
        void ShuffleDown();

        // @0x82412290 (this TU, cpp:343) -- snap the slot clip back to "Position0".
        void ResetSlotPosition();

        // @0x82420E60 (this TU, cpp:266) -- the per-frame tick: run the ttl down
        // (transitioning the item out at zero, expiring the slot once both clips
        // settle), and consume the two clips' transition latches on their settle
        // frames (slot 9/18/26/33 with the <=34 tripwire; item 12/44/65).
        void Update(f32 lfTimeStep);

    private:
        BoostMessageItem mMessageItem;   // X360 +0x0C ("<name>_MessageItem")
        bool mbInUse;                    // X360 +0x34
        bool mbInTransition;             // X360 +0x35 (the slot clip's shuffle latch; assert-named)
        u8   maPad36[2];                 // X360 +0x36 .. +0x37
        s32  miMessageId;                // X360 +0x38 (-1 == none)
        s32  miSlotPosition;             // X360 +0x3C (0..3)
        f32  mfTimeToLive;               // X360 +0x40
    };
}
