#include "GameSource/Gui/Flow/Screen/Components/BrnImageGalleryCarouselItem.h"

#include <cstring>   // std::strcmp (the X360 inlines both name compares)

// BrnGui::ImageGalleryCarouselItem -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/Components/BrnImageGalleryCarouselItem.cpp):
//   ImageGalleryCarouselItem::Construct               @0x82419B58
//   ImageGalleryCarouselItem::HandleLoadNotifications @0x82419BF8
//     (called by BrnGui::ImageGalleryState::HandleLoadNotification)
//
// Construct (asm walk): base GuiComponent::Construct, then
//   mGamertag.Construct("gamertag_mc", mpStateInterface, macName)   -- dispatched through
//     the text field's vtable slot 0 (TextField overrides the component Construct);
//   mLockIcon.Construct("lockIcon_mc", mpStateInterface, NULL table, macName);
//   mLoadingIcon.Construct("ImageLoading_mc", mpStateInterface, NULL table, macName);
//   mbLocked = false.
// (Each child is parented under this component's own name -- the X360 passes this+0x04,
// the base macName buffer.)
//
// HandleLoadNotifications (asm walk): two inlined strcmps against the children's names:
//   name == mGamertag's   -> mGamertag.SetText(mGamertag.GetText())  (re-push the stored
//                            text now the component is loaded), handled;
//   name == mLockIcon's   -> mLockIcon.SetState(mbLocked ? "locked" : "unlocked")
//                            (the string-keyed SetState @0x824E2B90), handled;
//   otherwise             -> not handled.

namespace BrnGui
{
    const char ImageGalleryCarouselItem::KAC_GAMERTAG_NAME[12]     = "gamertag_mc";
    const char ImageGalleryCarouselItem::KAC_LOCK_ICON_NAME[12]    = "lockIcon_mc";
    const char ImageGalleryCarouselItem::KAC_LOADING_ICON_NAME[16] = "ImageLoading_mc";

    // @ 0x82419B58
    void ImageGalleryCarouselItem::Construct(const char* lpacName,
                                             CgsGui::StateInterface* lpStateInterface,
                                             const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        mGamertag.Construct(KAC_GAMERTAG_NAME, mpStateInterface, GetName());
        mLockIcon.Construct(KAC_LOCK_ICON_NAME, mpStateInterface, 0, GetName());
        mLoadingIcon.Construct(KAC_LOADING_ICON_NAME, mpStateInterface, 0, GetName());

        mbLocked = false;
    }

    // @ 0x82419BF8
    bool ImageGalleryCarouselItem::HandleLoadNotifications(const char* lpacComponentName)
    {
        if (std::strcmp(lpacComponentName, mGamertag.GetName()) == 0)
        {
            // The gamertag field just loaded: re-push its stored text.
            mGamertag.SetText(mGamertag.GetText());
            return true;
        }

        if (std::strcmp(lpacComponentName, mLockIcon.GetName()) == 0)
        {
            mLockIcon.SetState(mbLocked ? "locked" : "unlocked");
            return true;
        }

        return false;
    }
}
