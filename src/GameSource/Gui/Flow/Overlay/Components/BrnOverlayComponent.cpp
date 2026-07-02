#include "GameSource/Gui/Flow/Overlay/Components/BrnOverlayComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
//   TransitionCompleteCallback @0x8241C198 -- the apt "transition complete" timeline
//   callback. The apt system hands the OverlayComponent back as the user-data pointer;
//   the callback asserts it is non-null, then raises the component's transition-complete
//   flag. The X360 inlines SetTransitionComplete to a direct `stb 1, 0x14`; we restore the
//   named setter call. The second callback argument (a u16 timeline/frame tag) is unused
//   by this handler (the X360 never touches r4).
//
//   RunOverlay(flash id, frame label) @0x824B1498 (ledger TU class:BrnGui::OverlayComponent)
//   -- assert the flash id (h:115), play the overlay movie on it, rebind
//   mTransitionMovieClipRef to the "singleOverlay_mc" child on the resulting frame
//   (FindChildMovieClipOnFrame @0x8246C8B0 -- the on-frame variant, NOT the recursive
//   search), then play the phase label on that child. Called by
//   BrnGui::BaseOverlayState::Update with "waiting"/"transin"/"transout".

namespace BrnGui
{

// @ 0x8241C198
void OverlayComponent::TransitionCompleteCallback(void* lpUserData, u16 /*luArg*/)
{
    CGS_ASSERT(NULL != lpUserData, "NULL != lpUserData");

    OverlayComponent* lpThis = static_cast<OverlayComponent*>(lpUserData);
    lpThis->SetTransitionComplete(true);
}

// @ 0x824B1498
void OverlayComponent::RunOverlay(const char* lpcOverlayFlashId, const char* lpcFrameLabel)
{
    // Non-gating tripwire (h:115): the popup family must have filled in its flash id.
    CGS_ASSERT(NULL != lpcOverlayFlashId, "NULL != lpcOverlayFlashId");

    mAptRef.GotoAndPlayLabel(lpcOverlayFlashId);

    // The frame the label landed on carries the popup's single-overlay child; adopt it
    // as the transition clip and run the phase label on it.
    BrnFlapt::MovieClipRef lSingleOverlayClip;
    mTransitionMovieClipRef =
        *mAptRef.FindChildMovieClipOnFrame(&lSingleOverlayClip, "singleOverlay_mc");
    mTransitionMovieClipRef.GotoAndPlayLabel(lpcFrameLabel);
}

} // namespace BrnGui
