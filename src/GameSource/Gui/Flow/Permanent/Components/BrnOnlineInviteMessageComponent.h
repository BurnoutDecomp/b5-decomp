#ifndef BRN_ONLINE_INVITE_MESSAGE_COMPONENT_H
#define BRN_ONLINE_INVITE_MESSAGE_COMPONENT_H

#include "types.hpp"

// BrnGui::OnlineInviteMessageComponent - the permanent GUI component that shows the
// "you've been invited" online message and runs its show / transition-out animation.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::OnlineInviteMessageComponent::DoTransitionComplete @ 0x824162E0
//
// MINIMAL-SLICE class: only DoTransitionComplete is in scope, so this models ONLY the
// three members it touches (the showing-state at +0x30, a secondary state at +0x34, and
// a bool flag at +0x68). The full component (its GuiComponent base, the message text /
// timers, the rest of the state machine) is uncommitted and OMITTED. FLAG: minimal-slice
// class. Offsets are not load-bearing on the 64-bit host; members are declared by name.

namespace BrnGui
{
    class OnlineInviteMessageComponent
    {
    public:
        // The component's showing-state machine. The transition-complete callback asserts
        // the state is TRANSITIONING_OUT and then resets it to HIDDEN. Only the two values
        // the asm names (0 reset target, 2 the asserted value) are attested; the
        // intermediate values are the conventional show/transition-in states.
        enum EShowingState
        {
            E_SHOWINGSTATE_HIDDEN           = 0, // value the callback writes back
            E_SHOWINGSTATE_TRANSITIONING_IN = 1,
            E_SHOWINGSTATE_TRANSITIONING_OUT = 2, // value the assert requires
            E_SHOWINGSTATE_SHOWING          = 3,
        };

        // 0x824162E0 -- called when the transition-out animation finishes: assert we were
        // transitioning out, drop to HIDDEN, and (if the secondary state indicates a
        // pending dismiss, value 4) latch the dismissed flag. Returns the X360 r3 result
        // (`this` on the non-assert path); kept as void* to mirror the guest _DWORD* return.
        void* DoTransitionComplete();

    private:
        // Guest +0x34: a secondary state the callback tests against 4. The match latches
        // the dismissed flag below; modelled as an opaque s32 state code.
        s32 miSecondaryState; // guest +0x34

        // Guest +0x30: the showing-state machine value (read/asserted/reset by the callback).
        EShowingState meShowingState; // guest +0x30

        // Guest +0x68: a bool flag set true when the secondary state == 4 at transition
        // complete (the asm stores a byte, so this is a byte-wide field, not a dword).
        bool mbDismissed; // guest +0x68
    };
}

#endif
