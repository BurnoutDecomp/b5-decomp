#ifndef BRN_SHOWTIME_MESSAGE_COMPONENT_H
#define BRN_SHOWTIME_MESSAGE_COMPONENT_H

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"      // BrnFlaptComponent (transitive base)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"  // FlaptIconComponent (base)

// ============================================================================
// GameSource/Gui/Flow/Permanent/Components/BrnShowtimeMessageComponent.h
//
// BrnGui::BrnShowtimeMessageComponent -- the apt-driven "Showtime!" banner the
// AlwaysAvailableComponentsManager shows when a Showtime crash run starts. It is
// a thin animation-state-machine wrapper over a FlaptIconComponent: Show / Hide
// drive the icon clip between the "transIn" / "transOut" / "invisible" timeline
// labels via the inherited FlaptIconComponent::SetState, and a per-frame trigger
// callback (TransitionCompleteCallback -> HandleTransitionComplete) advances the
// state when a transition clip finishes.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Member name / logical type / enum
// values come from the DecFIGS DWARF (BrnShowtimeMessageComponent.h), gated on the
// X360 ledger; the single own field's offset is proven by the ARTIST asm (every
// state read/write is `lwz/stw 0x14(this)` in Construct @ 0x82424F28,
// Prepare @ 0x82424F88, Show @ 0x82416458, Hide @ 0x824164A0,
// HandleTransitionComplete @ 0x82416538).
//
// INHERITANCE (DWARF: ... : BrnShowtimeMessageComponentBase : FlaptIconComponent).
// BrnShowtimeMessageComponentBase is an EMPTY pass-through layer in the DWARF (it
// adds no members and the X360 ledger attests no method of its own), so it is
// modelled directly as `: public FlaptIconComponent` -- introducing a named empty
// base class would only add an ODR/maintenance surface for nothing the X360 build
// distinguishes. (If a later TU attests a BrnShowtimeMessageComponentBase method,
// split it out then.)
//
// LAYOUT (X360-attested; sizeof == 0x18, matching the owner
// AlwaysAvailableComponentsManager which embeds it BY VALUE at +0x1013C, with the
// next field macComposerTextId at +0x10154 -> a 0x18 footprint):
//   +0x00  vptr                              (FlaptIconComponent is polymorphic)
//   +0x04  BrnFlaptComponent base            (mpStateInterface @ +0x04,
//                                             mAptRef @ +0x08, sizeof 0x0C)
//   +0x10  muCurrentStateHash  (u32)         (FlaptIconComponent own field)
//   +0x14  meComponentState    (ComponentState)
// All member access is BY NAME; there are no raw-offset hacks. Byte size is not
// load-bearing on the 64-bit host (the inherited pointers widen).
// ============================================================================

namespace CgsGui { class StateInterface; }
namespace BrnFlapt { struct FileRef; }

namespace BrnGui
{
    class BrnShowtimeMessageComponent : public FlaptIconComponent
    {
    public:
        // The banner's transition state machine. Values are the raw ints the asm
        // compares/stores at +0x14 (DWARF enum BrnShowtimeMessageComponent::
        // ComponentState). Construct leaves +0x14 as 0 (E_CS_INVALID); Prepare sets
        // it to E_CS_INVISIBLE.
        enum ComponentState
        {
            E_CS_INVALID   = 0,
            E_CS_INVISIBLE = 1,
            E_CS_ENTERING  = 2,
            E_CS_VISIBLE   = 3,
            E_CS_HIDING    = 4,
            E_CS_COUNT     = 5,
        };

        // Construct @ 0x82424F28 -- bind the state interface (asserts non-null),
        // invalidate the inherited apt clip handle + current-state hash, and zero
        // the state field. VIRTUAL slot 0: this OVERRIDES FlaptIconComponent::Construct
        // (the manager dispatches through the vtable -- AlwaysAvailableComponentsManager::
        // Construct @0x824F3628 calls (**vptr)). Signature matches the base exactly
        // (const void*, StateInterface*, const void*); only the state interface is used.
        virtual void Construct(const void* lpcDEBUGName,
                               CgsGui::StateInterface* lpStateInterface,
                               const void* lpcParentName) override;

        // Prepare @ 0x82424F88 -- resolve+bind this banner's apt movie clip out of
        // lFile, reset its timeline, install the per-frame transition-complete
        // callback, and enter the INVISIBLE state. Non-virtual (direct call site).
        void Prepare(const char* lpcMovieClipName, const BrnFlapt::FileRef& lFile);

        // Show @ 0x82416458 -- play the "transIn" label and enter ENTERING.
        void Show();

        // Hide @ 0x824164A0 -- if lbImmediate, jump straight to "invisible"
        // (INVISIBLE); otherwise, when currently VISIBLE or ENTERING, play
        // "transOut" and enter HIDING.
        void Hide(bool lbImmediate);

    private:
        // HandleTransitionComplete @ 0x82416538 -- advance the state machine when a
        // transition clip reaches its frame trigger (ENTERING -> VISIBLE,
        // VISIBLE -> play "transOut" + HIDING, HIDING -> play "invisible" + INVISIBLE;
        // any other state asserts).
        void HandleTransitionComplete();

        // TransitionCompleteCallback @ 0x82416638 -- the MovieClipInstance frame-
        // trigger callback installed by Prepare; asserts the user-data and forwards
        // to HandleTransitionComplete. Signature matches MovieClipInstance::
        // FrameTriggerCallback (void(*)(void* lpUserData, u16)).
        static void TransitionCompleteCallback(void* lpUserData, u16 luFrame);

        // +0x14 : the current transition state.
        ComponentState meComponentState;
    };
}

#endif // BRN_SHOWTIME_MESSAGE_COMPONENT_H
