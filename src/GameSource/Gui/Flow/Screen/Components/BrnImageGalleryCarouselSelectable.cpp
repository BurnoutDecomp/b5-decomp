#include "GameSource/Gui/Flow/Screen/Components/BrnImageGalleryCarouselSelectable.h"

// BrnGui::ImageGalleryCarouselSelectable -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/Components/BrnImageGalleryCarouselSelectable.cpp):
//   ImageGalleryCarouselSelectable::Construct @0x82419A40
//   ImageGalleryCarouselSelectable::Update    @0x82419A90

namespace BrnGui
{

// @ 0x82419A40 -- the component subobject first (this+0x18), then the selectable.
void ImageGalleryCarouselSelectable::Construct(const char* lpacName,
                                               CgsGui::StateInterface* lpStateInterface,
                                               const char* lpacParentName, u64 luId)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    Selectable::Construct(lpacName, lpStateInterface, lpacParentName, luId);
    mbUsed = false;
}

// @ 0x82419A90 -- nothing to do until the selectable is dirtied; then the apt
// state mirrors highlight x used (or hides when not even highlightable).
void ImageGalleryCarouselSelectable::Update()
{
    if (!IsDirty())
        return;
    ClearFlag(E_FLAG_DIRTY);

    if (IsHighlighted())
    {
        AddOutputAptViewState("apt_state", mbUsed ? "selectUsed" : "selectUnused", false);
    }
    else if (IsHighlightable())
    {
        AddOutputAptViewState("apt_state", mbUsed ? "unselectedUsed" : "unselectedUnused", false);
    }
    else
    {
        AddOutputAptViewState("apt_state", "invisible", false);
    }
}

}
