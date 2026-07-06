#pragma once

// BrnPopupController.h
// BrnResource::PopupController -- the loaded popup-message table (a
// CgsGui::GuiPopupResource bundle) and the accessors the overlays director uses
// to resolve a popup by its name-id hash and stamp it into a GuiOverlayFullInfoResponse.
//
// Shape / member names / method set verbatim from the DecFIGS DWARF
// (SharedClasses/DataLists/BrnPopupController.h:43); gated on the X360 ledger.
// X360-pinned offsets: mPopupsPtr @+0x00 (the ResourcePtr base -- CreateFromHandle
// takes `this` directly; operator-> reads its +0 word), mbIsPopupLoaded @+0x20
// (lbz/stb 0x20). This TU bodies GetIndexFromPopupHash (@0x8267D6C0); Construct,
// GetPopup and AddPopupResource are their own ledger functions (declaration-only here).
//
// The full CgsGui::GuiPopup / CgsGui::GuiPopupResource record types (DWARF
// CgsGuiPopupResource.h:97/139) are authored here because the committed
// CgsGuiPopupResource.h currently defines only the popup enums + GuiPopupParameter
// (the .cpp has a private FixUp-only u32 layout). Offsets X360-attested by
// GetIndexFromPopupHash (mppPopupData@+0, miPopupCount@+4 s16; GuiPopup::mNameId@+0)
// and by the GetPopup copy path (macName@+8, meStyle@+0x18, meIcon@+0x1C,
// macTitleId@+0x20, macMessageId@+0x40, macButton1Id@+0x6C, macButton2Id@+0x91).

#include "types.hpp"                                                    // s32/u32/f32/s16
#include "BrnCommonTypes.h"                                             // CgsID (u64)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"      // CgsResource::ResourcePtr<T>
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiPopupResource.h" // PopupStyle/PopupIcons/PopupParamTypes

namespace BrnGui { struct GuiOverlayFullInfoResponse; }                // GameSource/Gui/BrnGuiEventTypeDefs.h
namespace CgsResource { namespace Events { struct AcquireResourceResponse; } } // CgsResourceIOEvents.h

namespace CgsGui
{
    // DWARF CgsGuiPopupResource.h:97 -- one popup record inside the resource bundle.
    // Only the fields the controller copy path touches are pinned by name/offset; the
    // interior (maeMessageParams/miMessageParamsUsed/button-param fields) is DWARF-named
    // and sits between the attested id spans (macButton1Id@+0x6C, macButton2Id@+0x91).
    struct GuiPopup
    {
        static const s32 MKI_MAX_LENGTH_OF_STRING_ID   = 32;   // DWARF h:100
        static const s32 MKI_MAX_LENGTH_OF_FLASH_FRAME = 32;   // DWARF h:101
        static const s32 MKI_MAX_MESSAGE_PARAM_COUNT   = 2;    // DWARF h:102

        CgsID           mNameId;                                  // +0x00
        char            macName[13];                              // +0x08
        PopupStyle      meStyle;                                  // +0x18
        PopupIcons      meIcon;                                   // +0x1C
        char            macTitleId[MKI_MAX_LENGTH_OF_STRING_ID];  // +0x20
        char            macMessageId[MKI_MAX_LENGTH_OF_STRING_ID];// +0x40
        PopupParamTypes maeMessageParams[MKI_MAX_MESSAGE_PARAM_COUNT]; // +0x60
        s32             miMessageParamsUsed;                      // +0x68
        char            macButton1Id[MKI_MAX_LENGTH_OF_STRING_ID];// +0x6C
        PopupParamTypes meButton1Param;                           // +0x8C
        bool            mbButton1ParamUsed;                       // +0x90
        char            macButton2Id[MKI_MAX_LENGTH_OF_STRING_ID];// +0x91
        PopupParamTypes meButton2Param;                           // +0xB4
        bool            mbButton2ParamUsed;                       // +0xB8

        void FixDown(bool);                                       // DWARF h:126 (own ledger fn)
    };

    // DWARF CgsGuiPopupResource.h:139 -- the loaded popup table.
    struct GuiPopupResource
    {
        GuiPopup** mppPopupData;          // +0x00
        s16        miPopupCount;          // +0x04
        s16        miSizeOfPopupResource; // +0x06

        void FixUp(u32);                  // DWARF h:148 (own ledger fn)
    };
}

namespace BrnResource
{
    // BrnPopupController.h:43 -- wraps the loaded popup resource bundle.
    struct PopupController
    {
        // DWARF :48-:68 -- declared-only here except the this-TU body.
        void Construct();                                                       // :48 (own ledger fn)
        bool GetPopup(BrnGui::GuiOverlayFullInfoResponse* lpOverlayInfo) const;  // :53 (@0x8267EB98)
        void AddPopupResource(const CgsResource::Events::AcquireResourceResponse* lpResource); // :58 (@0x82678D88)

    private:
        s32  GetIndexFromPopupHash(CgsID lPopupId) const;                       // :68 (@0x8267D6C0)

    private:
        CgsResource::ResourcePtr<CgsGui::GuiPopupResource> mPopupsPtr;          // :62 @+0x00
        bool mbIsPopupLoaded;                                                    // :63 @+0x20
    };
}
