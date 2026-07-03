#pragma once

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                          // BrnFlapt::MovieClipRef
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                          // BrnFlapt::TextFieldRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"    // BrnFlaptComponent (base)

// BrnGui::BoostMessageItem - one boost-message HUD line: a Flapt component
// wrapping the "BoostMessage_text" text field and the "backing_mc" child clip,
// stepped through its TransitionIn/TransitionOut/Reset frames with the backing
// stopped on the boost-type frame (danger/aggression/stunt). Shape asm-derived
// from the five exported bodies (the asserts name the original source
// GameSource/Gui/Flow/HUD/Components/BrnBoostMessageItem.cpp). X360 layout:
// the BrnFlaptComponent base (iface +0x00, mAptRef +0x04), mMessageText
// +0x0C, mBackingRef +0x18, mbTransitionInProgress +0x20, meBoostType +0x24.
namespace BrnGui
{
    class BoostMessageItem : public BrnFlaptComponent
    {
    public:
        // The transition selector SetTransition maps through the frame table
        // (XEX .data @0x82F24988).
        // DWARF enum spelling (values pin the frame table order).
        enum ETransition
        {
            E_TRANSITION_ON    = 0,   // "TransitionIn"
            E_TRANSITION_OFF   = 1,   // "TransitionOut"
            E_TRANSITION_RESET = 2,   // "Reset"
            E_TRANSITION_MAX   = 3,
        };

        // @0x82411A18 (this TU, cpp:69) -- the base Construct (the streamed
        // "Invalid state interface" tripwire ahead of the base's h:113 one), then
        // invalidate both child refs and reset the latches (boost type to -1).
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);

        // @0x82420050 (this TU) -- the base Prepare (bind + timeline reset), then
        // attach the "BoostMessage_text" field (under "MessageTextField" within
        // this component's name) and resolve the "backing_mc" child clip.
        void Prepare(const char* lpacName, const BrnFlapt::FileRef& lrFile);

        // @0x82411B00 (this TU, cpp:179) -- push localised text into the message
        // field: with a non-negative boost amount it formats as the single integer
        // parameter (E_FORMAT_INTEGER), else the plain string-id lookup.
        void SetText(const char* lpcText, s32 liBoostAmount);

        // @0x82420158 (this TU, cpp:211/:213) -- play the transition frame
        // (asserting the type and that no transition is already running -- both
        // streamed, folded static, non-gating), raise the in-progress latch, and
        // refresh the backing's boost-type frame.
        void SetTransition(u32 luTransition, s32 leBoostType);

        // @0x82411BD0 (this TU, cpp:268/:269) -- stop the backing clip on the
        // boost-type frame when it changed (-1 == leave it alone). The bounds
        // asserts carry the original BrnWorld::E_BOOST_TYPE_* spelling.
        void UpdateBoostType(s32 leBoostType);

    private:
        // The transition frames (@0x82F24988) and the backing's boost-type frames
        // (@0x82F24994); definitions in the .cpp.
        static const char* const KAPC_TRANSITION_FRAMES[3];
        static const char* const KAPC_BOOST_TYPE_FRAMES[3];

        BrnFlapt::TextFieldRef mMessageText;   // X360 +0x0C ("BoostMessage_text")
        BrnFlapt::MovieClipRef mBackingRef;         // X360 +0x18 ("backing_mc")
        bool mbInTransition;                        // X360 +0x20 (DWARF name; the asm is byte-width)
        u8   maPad21[3];                            // X360 +0x21 .. +0x23
        s32  meCurrentBoostType;                    // X360 +0x24 (DWARF name; -1 == none, BrnWorld boost type)
    };
}
