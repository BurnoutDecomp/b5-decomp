#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)
#include "GameSource/Gui/BrnGuiTextField.h"                           // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"            // BrnGui::IconComponent (by value)
#include "GameSource/Gui/BrnGuiShared.h"                              // BrnGui::EGuiImageCategories

// BrnGui::ImageGalleryCarouselItem - one image slot of the image-gallery carousel
// screen: the picture frame plus its gamertag text field, a lock icon and a loading
// icon. DWARF home BrnImageGalleryCarouselItem.h:45. Construct and
// HandleLoadNotifications are bodied in BrnImageGalleryCarouselItem.cpp (this TU);
// the small setters are header-inline (the console inlines them into the gallery
// state's handlers, with no out-of-line copy).
namespace BrnGui
{
    struct ImageGalleryCarouselItem : public CgsGui::GuiComponent
    {
        // @0x82419B58 (this TU, DWARF cpp:68) -- base Construct, then build the three
        // children ("gamertag_mc" text field, "lockIcon_mc" / "ImageLoading_mc" icons,
        // each parented under this component's name) and clear the lock flag.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x82419BF8 (this TU, DWARF cpp:93) -- react to a component-load notification:
        // when it names the gamertag field, re-push its stored text; when it names the
        // lock icon, push the "locked"/"unlocked" state. Returns whether it was handled.
        bool HandleLoadNotifications(const char* lpacComponentName);

        // Store the lock flag and push the lock icon's matching state.
        void SetLocked(bool lbLocked)
        {
            mbLocked = lbLocked;
            mLockIcon.SetState(lbLocked ? "locked" : "unlocked");
        }

        void SetGamertag(const char* lpacGamertag) { mGamertag.SetText(lpacGamertag); }

        // Show the category's picture frame, or its invisible frame when lbInvisible
        // (the console materialises the flag and picks the invisible table on true).
        void SetImageType(EGuiImageCategories leCategory, bool lbInvisible)
        {
            AddOutputAptViewState("apt_state",
                                  lbInvisible ? KAPC_CAROUSEL_INVISIBLE_FRAMES[leCategory]
                                              : KAPC_CAROUSEL_FRAMES[leCategory],
                                  false);
        }

        void InvalidateImageType() { AddOutputAptViewState("apt_state", "invisible", false); }
        void ShowLoading();   // never reached by the console's gallery code; no body
        void HideLoading() { mLoadingIcon.SetState("invisible"); }

    private:
        // DWARF h:99-104 (X360 +0x8C / +0x1B4 / +0x248 / +0x2DC).
        TextField     mGamertag;      // +0x8C
        IconComponent mLockIcon;      // +0x1B4
        IconComponent mLoadingIcon;   // +0x248
        bool          mbLocked;       // +0x2DC

        // The per-category frame labels (defined in this TU's .cpp from the image) and the
        // three child names the Construct asm passes.
        static const char* const KAPC_CAROUSEL_FRAMES[4];            // cpp:23
        static const char* const KAPC_CAROUSEL_INVISIBLE_FRAMES[4];  // cpp:34
        static const char        KAC_GAMERTAG_NAME[12];              // cpp:45 "gamertag_mc"
        static const char        KAC_LOCK_ICON_NAME[12];             // cpp:46 "lockIcon_mc"
        static const char        KAC_LOADING_ICON_NAME[16];          // cpp:47 "ImageLoading_mc"
    };
}
