// ===================================================================================
// BrnGui::BrnShowtimeMessageComponent -- the apt-driven "Showtime!" banner
//   GameSource/Gui/Flow/Permanent/Components/BrnShowtimeMessageComponent.cpp
//
//   Construct                   @ 0x82424F28   (virtual slot 0)
//   Prepare                     @ 0x82424F88
//   Show                        @ 0x82416458
//   Hide                        @ 0x824164A0
//   HandleTransitionComplete    @ 0x82416538
//   TransitionCompleteCallback  @ 0x82416638
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. All but the callback are non-static
// members (asm: r3 = this); TransitionCompleteCallback is the static MovieClipInstance
// frame-trigger callback (it receives the component as lpUserData). Member access is
// BY NAME throughout; the X360 Begin/Fire/End dev-assert triplets fold into
// CGS_ASSERT(cond,"msg") per the module house style (see the BrnGuiFlaptComponent /
// BrnAchievementPopupComponent exemplars).
//
// The banner is a thin animation-state-machine wrapper over its FlaptIconComponent
// base. Show/Hide and the transition-complete handler drive the icon clip between
// the "transIn" / "transOut" / "invisible" timeline labels via the inherited
// FlaptIconComponent::SetState (the X360 calls it through the vtable: lwz r10,0(r3);
// lwz r11,0xC(r10); bctrl -- slot 3, FlaptIconComponent::SetState). A per-frame trigger
// callback installed in Prepare advances the state once a transition clip finishes.
//
// Construct/Prepare inline the base init exactly as the sibling components do:
// Construct = FlaptIconComponent::Construct's body (bind state interface, clear the apt
// handle + state hash) plus zeroing the state field; Prepare = FlaptIconComponent::
// Prepare (which forwards to BrnFlaptComponent::Prepare to resolve+bind+reset the clip,
// then clears the state hash) plus installing the trigger callback and entering INVISIBLE.
// ===================================================================================
#include "GameSource/Gui/Flow/Permanent/Components/BrnShowtimeMessageComponent.h"

#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"            // BrnFlapt::FileRef (Prepare param)
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

namespace BrnGui
{
    // The banner's transition timeline labels (X360 .rdata). Switching state plays
    // the matching label on the icon clip via the inherited SetState.
    static const char* const KAC_TRANS_IN_LABEL   = "transIn";
    static const char* const KAC_TRANS_OUT_LABEL  = "transOut";
    static const char* const KAC_INVISIBLE_LABEL  = "invisible";

    // @ 0x82424F28 -- bind the state interface, invalidate the inherited apt clip
    // handle + state hash, and zero the transition state. Dispatched through the
    // vtable by the manager (this is the class's first virtual).
    void BrnShowtimeMessageComponent::Construct(const void* /*lpcDEBUGName*/,
                                                CgsGui::StateInterface* lpStateInterface,
                                                const void* /*lpcParentName*/)
    {
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");

        // Inlined FlaptIconComponent::Construct: bind the state channel, invalidate the
        // apt clip handle, and clear the current-state hash.
        mpStateInterface        = lpStateInterface;
        mAptRef.mpMovieClipInst = 0;
        mAptRef.mpTransform     = 0;
        muCurrentStateHash      = 0;

        // The X360 leaves +0x14 as 0 here (E_CS_INVALID); Prepare sets it to INVISIBLE.
        meComponentState = E_CS_INVALID;
    }

    // @ 0x82424F88 -- resolve+bind this banner's apt movie clip out of lFile, reset its
    // timeline + state hash, install the per-frame transition-complete callback, and
    // enter the INVISIBLE state.
    void BrnShowtimeMessageComponent::Prepare(const char* lpcMovieClipName,
                                              const BrnFlapt::FileRef& lFile)
    {
        // Inlined FlaptIconComponent::Prepare (bare-name lookup, no parent prefix):
        // bind the clip into mAptRef, assert it, reset its timeline, clear the hash.
        FlaptIconComponent::Prepare(lpcMovieClipName, lFile, 0);

        // Install the frame-trigger callback on the bound clip so a finished
        // transition clip calls back into HandleTransitionComplete (the component is
        // passed as the callback's user data).
        mAptRef.SetFrameTriggerCallback(
            reinterpret_cast<void*>(&BrnShowtimeMessageComponent::TransitionCompleteCallback),
            this);

        meComponentState = E_CS_INVISIBLE;
    }

    // @ 0x82416458 -- start the show transition.
    void BrnShowtimeMessageComponent::Show()
    {
        SetState(KAC_TRANS_IN_LABEL);
        meComponentState = E_CS_ENTERING;
    }

    // @ 0x824164A0 -- start the hide transition (or jump straight to invisible).
    void BrnShowtimeMessageComponent::Hide(bool lbImmediate)
    {
        if (lbImmediate)
        {
            // Snap to invisible unless already there.
            if (meComponentState == E_CS_INVISIBLE)
            {
                return;
            }
            SetState(KAC_INVISIBLE_LABEL);
            meComponentState = E_CS_INVISIBLE;
        }
        else if (meComponentState == E_CS_VISIBLE || meComponentState == E_CS_ENTERING)
        {
            // Play the hide transition only while shown / showing.
            SetState(KAC_TRANS_OUT_LABEL);
            meComponentState = E_CS_HIDING;
        }
    }

    // @ 0x82416538 -- advance the state machine when a transition clip hits its frame
    // trigger.
    void BrnShowtimeMessageComponent::HandleTransitionComplete()
    {
        switch (meComponentState)
        {
            case E_CS_ENTERING:
                // The "transIn" clip finished: the banner is now fully shown.
                meComponentState = E_CS_VISIBLE;
                break;

            case E_CS_VISIBLE:
                // While visible the next trigger starts the hide transition.
                SetState(KAC_TRANS_OUT_LABEL);
                meComponentState = E_CS_HIDING;
                break;

            case E_CS_HIDING:
                // The "transOut" clip finished: park on the invisible label.
                SetState(KAC_INVISIBLE_LABEL);
                meComponentState = E_CS_INVISIBLE;
                break;

            default:
                // INVALID / INVISIBLE: a transition-complete here is a logic error.
                // The X360 streamed "...in state <meComponentState>\n" into the assert
                // buffer via StrStream; per the house style it folds into a plain
                // CGS_ASSERT failure message.
                CGS_ASSERT(false,
                           "Should not be received a trans complete in state ");
                break;
        }
    }

    // @ 0x82416638 -- the bound clip's frame-trigger callback: validate the user data
    // and forward to the component's transition-complete handler.
    void BrnShowtimeMessageComponent::TransitionCompleteCallback(void* lpUserData, u16 /*luFrame*/)
    {
        CGS_ASSERT(lpUserData != 0, "lpUserData");

        static_cast<BrnShowtimeMessageComponent*>(lpUserData)->HandleTransitionComplete();
    }
}
