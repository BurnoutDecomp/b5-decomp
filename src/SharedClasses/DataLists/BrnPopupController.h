#pragma once

// BrnPopupController.h
// BrnResource::PopupController -- the loaded popup-message table (a
// CgsGui::GuiPopupResource bundle, "Popups.pup") and the accessors the overlays director
// uses to resolve a popup by its name-id hash and stamp it into a
// GuiOverlayFullInfoResponse.
//
// Member names and method set from the type information. Offsets: mPopupsPtr
// +0x00 (the ResourcePtr base; operator-> reads its +0 word), mbIsPopupLoaded console +0x20.
// The popup records are the real CgsGui::GuiPopup / GuiPopupResource from
// CgsGuiPopupResource.h.

#include "types.hpp"                                                    // s32/u32/f32/s16
#include "BrnCommonTypes.h"                                             // CgsID (u64)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"      // CgsResource::ResourcePtr<T>
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiPopupResource.h" // CgsGui::GuiPopup / GuiPopupResource

namespace BrnGui { struct GuiOverlayFullInfoResponse; }                // GameSource/Gui/BrnGuiEventTypeDefs.h
namespace CgsResource { namespace Events { struct AcquireResourceResponse; } } // CgsResourceIOEvents.h

namespace BrnResource
{
    // BrnPopupController.h:43 -- wraps the loaded popup resource bundle.
    struct PopupController
    {
        // Declaration only: the image carries no body and no caller (the owning
        // GameDataModule's constructor builds mPopupsPtr and leaves mbIsPopupLoaded clear).
        void Construct();
        bool GetPopup(BrnGui::GuiOverlayFullInfoResponse* lpOverlayInfo) const;  // :53
        void AddPopupResource(const CgsResource::Events::AcquireResourceResponse* lpResource); // :58

    private:
        s32  GetIndexFromPopupHash(CgsID lPopupId) const;                       // :68

    private:
        CgsResource::ResourcePtr<CgsGui::GuiPopupResource> mPopupsPtr;          // :62 +0x00
        bool mbIsPopupLoaded;                                                    // :63 console +0x20
    };
}
