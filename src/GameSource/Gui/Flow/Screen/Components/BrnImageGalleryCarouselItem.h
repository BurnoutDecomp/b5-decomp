#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)
#include "GameSource/Gui/BrnGuiTextField.h"                           // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"            // BrnGui::IconComponent (by value)

// BrnGui::ImageGalleryCarouselItem - one image slot of the image-gallery carousel
// screen: the picture frame plus its gamertag text field, a lock icon and a loading
// icon. DWARF home BrnImageGalleryCarouselItem.h:45. Construct and
// HandleLoadNotifications are bodied in BrnImageGalleryCarouselItem.cpp (this TU);
// the other members are their own ledger functions (declaration-only here, X360-gated).
namespace BrnGui
{
    // DWARF parameter type of SetImageType; the enum's own home (the gallery image
    // categories) is not yet reconstructed -- forward-declared enum per the pointer/
    // value-only use here. FLAG: enumerators land with the gallery-state TU.
    enum EGuiImageCategories : s32;

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

        // DWARF h:121/h:145/h:162/h:184/h:199/h:214 -- declaration-only (own ledger TUs).
        void SetLocked(bool lbLocked);
        void SetGamertag(const char* lpacGamertag);
        void SetImageType(EGuiImageCategories leCategory, bool lbFlag);
        void InvalidateImageType();
        void ShowLoading();
        void HideLoading();

    private:
        // DWARF h:99-104 (X360 +0x8C / +0x1B4 / +0x248 / +0x2DC).
        TextField     mGamertag;      // +0x8C
        IconComponent mLockIcon;      // +0x1B4
        IconComponent mLoadingIcon;   // +0x248
        bool          mbLocked;       // +0x2DC

        // DWARF cpp:23-47 statics (.rdata values not recovered for the frame tables;
        // resolved at link time). The three child names are the literals the Construct
        // asm passes.
        static const char* const KAPC_CAROUSEL_FRAMES[4];            // cpp:23
        static const char* const KAPC_CAROUSEL_INVISIBLE_FRAMES[4];  // cpp:34
        static const char        KAC_GAMERTAG_NAME[12];              // cpp:45 "gamertag_mc"
        static const char        KAC_LOCK_ICON_NAME[12];             // cpp:46 "lockIcon_mc"
        static const char        KAC_LOADING_ICON_NAME[16];          // cpp:47 "ImageLoading_mc"
    };
}
