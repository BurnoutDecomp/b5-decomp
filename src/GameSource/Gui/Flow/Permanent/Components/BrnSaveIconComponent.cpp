#include "GameSource/Gui/Flow/Permanent/Components/BrnSaveIconComponent.h"

// BrnGui::BrnSaveIconComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Permanent/Components/BrnSaveIconComponent.cpp):
//   BrnSaveIconComponent::Prepare      @0x82424E28  (called by
//       BrnGui::AlwaysAvailableComponentsManager::PrepareFlapt)
//   BrnSaveIconComponent::ShowSaveIcon @0x824163A8  (AlwaysAvailableComponentsManager::Update)
//   BrnSaveIconComponent::HideSaveIcon @0x82416400  (AlwaysAvailableComponentsManager::Update)
//
// Prepare (asm walk): the whole FlaptIconComponent::Prepare(name, file, NULL) chain rides
// inlined (its "lacName" assert @ BrnGuiFlaptIconComponent.cpp:65, the base
// BrnFlaptComponent::Prepare's "lacName != NULL" assert @ h:133 and clip bind --
// FileRef::FindComponent, the two-word MovieClipRef copy, the "mpMovieClipInst" assert @
// BrnFlaptMovieClipRef.h:272, ResetTimeline -- then the icon's muCurrentStateHash reset);
// expressed through the committed base call. Then blank the icon (the virtual SetState
// dispatch the asm makes through vtbl+0xC), mark it invisible, and hide the clip.
//
// Show/Hide (asm walk): make the clip visible, play the AnimIn / AnimOut label through
// the same virtual SetState, and latch meComponentState (this+0x14).

namespace BrnGui
{
    // @ 0x82424E28
    void BrnSaveIconComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
    {
        FlaptIconComponent::Prepare(lacName, lFile, NULL);

        SetState("Blank");
        meComponentState = E_CS_INVISIBLE;
        mAptRef.SetVisible(false);
    }

    // @ 0x824163A8
    void BrnSaveIconComponent::ShowSaveIcon()
    {
        mAptRef.SetVisible(true);
        SetState("AnimIn");
        meComponentState = E_CS_VISIBLE;
    }

    // @ 0x82416400
    void BrnSaveIconComponent::HideSaveIcon()
    {
        // The X360 makes the clip visible for the slide-out too -- AnimOut plays the exit
        // animation on a visible clip; the state latches invisible.
        mAptRef.SetVisible(true);
        SetState("AnimOut");
        meComponentState = E_CS_INVISIBLE;
    }
}
