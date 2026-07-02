#include "GameSource/Gui/Flow/Screen/Components/BrnImageGallerySelectable.h"

#include <cstring>   // std::strcmp (the X360 inlines the compares)

// BrnGui::ImageGallerySelectable -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/Components/BrnImageGallerySelectable.cpp):
//   ImageGallerySelectable::Construct               @0x82419878
//   ImageGallerySelectable::Update                  @0x82419910
//   ImageGallerySelectable::HandleLoadNotifications @0x82419998

namespace BrnGui
{

const char ImageGallerySelectable::KAC_CATEGORY_TEXT_NAME[12]  = "Category_mc";
const char ImageGallerySelectable::KAC_COLLECTED_TEXT_NAME[13] = "Collected_mc";

// @ 0x82419878 -- the component subobject first (this+0x18), then the selectable,
// then the two text-field children: each Construct dispatched through the field's
// vtable slot 0 (TextField overrides the component Construct), passing this
// component's state interface (+0xA0) and its own name buffer (+0x1C) as the parent.
void ImageGallerySelectable::Construct(const char* lpacName,
                                       CgsGui::StateInterface* lpStateInterface,
                                       const char* lpacParentName, u64 luId)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    Selectable::Construct(lpacName, lpStateInterface, lpacParentName, luId);
    mCategoryText.Construct(KAC_CATEGORY_TEXT_NAME, mpStateInterface, macName);
    mCollectedText.Construct(KAC_COLLECTED_TEXT_NAME, mpStateInterface, macName);
}

// @ 0x82419910 -- nothing to do until the selectable is dirtied; then the apt state
// mirrors highlighted / highlightable / hidden.
void ImageGallerySelectable::Update()
{
    if (!IsDirty())
        return;
    ClearFlag(E_FLAG_DIRTY);

    if (IsHighlighted())
    {
        AddOutputAptViewState("apt_state", "selected", false);
    }
    else if (IsHighlightable())
    {
        AddOutputAptViewState("apt_state", "unselected", false);
    }
    else
    {
        AddOutputAptViewState("apt_state", "invisible", false);
    }
}

// @ 0x82419998 -- two inlined strcmps against the children's names; a just-loaded
// child re-pushes its stored text (the ImageGalleryCarouselItem idiom).
bool ImageGallerySelectable::HandleLoadNotifications(const char* lpacComponentName)
{
    if (std::strcmp(mCategoryText.GetName(), lpacComponentName) == 0)
    {
        mCategoryText.SetText(mCategoryText.GetText());
        return true;
    }

    if (std::strcmp(mCollectedText.GetName(), lpacComponentName) == 0)
    {
        mCollectedText.SetText(mCollectedText.GetText());
        return true;
    }

    return false;
}

}
