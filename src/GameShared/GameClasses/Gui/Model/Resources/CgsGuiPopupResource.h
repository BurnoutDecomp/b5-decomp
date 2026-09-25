#pragma once

#include "types.hpp"
#include <cstdint>   // uintptr_t (the relocation delta)

// CgsGui popup vocabulary - the popup style/param/icon enums plus the single
// popup parameter record. Names/values from the DecFIGS DWARF
// (CgsGuiPopupResource.h:34/54/64/78); the X360 overlay states are gated on them
// (BrnGui::BaseOverlayState::SetupOverlay asserts meIcon against
// E_POPUPICONS_INVISIBLE/WARNING and indexes its icon-state and param-format
// tables with PopupIcons/PopupParamTypes, and the overlay flow instantiates one
// state per PopupStyle value).
//
// The popup records themselves (GuiPopup / GuiPopupResource) live at the end of this
// header; their FixUp/FixDown bodies are in CgsGuiPopupResource.cpp.
namespace CgsGui
{
    // DWARF CgsGuiPopupResource.h:34.
    enum PopupStyle
    {
        E_POPUPSTYLE_CRASHNAV_WAIT            = 0,
        E_POPUPSTYLE_CRASHNAV_OK              = 1,
        E_POPUPSTYLE_CRASHNAV_OKCANCEL        = 2,
        E_POPUPSTYLE_CRASHNAV_ONLINE_WAIT     = 3,
        E_POPUPSTYLE_CRASHNAV_ONLINE_OK       = 4,
        E_POPUPSTYLE_CRASHNAV_ONLINE_OKCANCEL = 5,
        E_POPUPSTYLE_INGAME_WAIT              = 6,
        E_POPUPSTYLE_INGAME_OK                = 7,
        E_POPUPSTYLE_INGAME_OKCANCEL          = 8,
        E_POPUPSTYLE_INGAME_ONLINE_WAIT       = 9,
        E_POPUPSTYLE_INGAME_ONLINE_OK         = 10,
        E_POPUPSTYLE_INGAME_ONLINE_OKCANCEL   = 11,
        E_POPUPSTYLE_INGAME_ONLINE_ENTER_FREEBURN = 12,
        E_POPUPSTYLE_CUSTOM                   = 13,
        E_POPUPSTYLE_COUNT                    = 14,
    };

    // DWARF CgsGuiPopupResource.h:54.
    enum PopupParamTypes
    {
        E_POPUPPARAMTYPES_UNUSED    = 0,
        E_POPUPPARAMTYPES_STRING    = 1,
        E_POPUPPARAMTYPES_STRING_ID = 2,
        E_POPUPPARAMTYPES_COUNT     = 3,
    };

    // DWARF CgsGuiPopupResource.h:64.
    enum PopupIcons
    {
        E_POPUPICONS_INVISIBLE = 0,
        E_POPUPICONS_WARNING   = 1,
        E_POPUPICONS_COUNT     = 2,
    };

    // DWARF CgsGuiPopupResource.h:78 - one formatted popup message parameter.
    // X360-attested by BrnGui::BaseOverlayState::SetupOverlay @0x824B1690, which
    // walks GuiOverlayFullInfoResponse::maMessageParams with a 0x44-byte stride
    // (meParamType @ +0x00, macParameter @ +0x04).
    struct GuiPopupParameter
    {
        static const s32 KI_MAX_PARAM_STRING_LENGTH = 64;   // DWARF h:81

        PopupParamTypes meParamType;                           // +0x00
        char            macParameter[KI_MAX_PARAM_STRING_LENGTH]; // +0x04
    };

    // One popup record inside the Popups.pup resource (member names/order from the
    // type information). The record holds no pointer, so the console
    // offsets are the host offsets: mNameId +0x00, macName +0x08, meStyle +0x18,
    // meIcon +0x1C, macTitleId +0x20, macMessageId +0x40, maeMessageParams +0x60,
    // miMessageParamsUsed +0x68, macButton1Id +0x6C, macButton2Id +0x91 (the loads in
    // PopupController::GetPopup), 0xC0-byte stride (the shipped POPUPS.PUP spaces its 77
    // record offsets 0xC0 apart).
    struct GuiPopup
    {
        static const s32 MKI_MAX_LENGTH_OF_STRING_ID   = 32;
        static const s32 MKI_MAX_LENGTH_OF_FLASH_FRAME = 32;
        static const s32 MKI_MAX_MESSAGE_PARAM_COUNT   = 2;

        u64             mNameId;                                       // +0x00 (CgsID)
        char            macName[13];                                   // +0x08
        PopupStyle      meStyle;                                       // +0x18
        PopupIcons      meIcon;                                        // +0x1C
        char            macTitleId[MKI_MAX_LENGTH_OF_STRING_ID];       // +0x20
        char            macMessageId[MKI_MAX_LENGTH_OF_STRING_ID];     // +0x40
        PopupParamTypes maeMessageParams[MKI_MAX_MESSAGE_PARAM_COUNT]; // +0x60
        s32             miMessageParamsUsed;                           // +0x68
        char            macButton1Id[MKI_MAX_LENGTH_OF_STRING_ID];     // +0x6C
        PopupParamTypes meButton1Param;                                // +0x8C
        bool            mbButton1ParamUsed;                            // +0x90
        char            macButton2Id[MKI_MAX_LENGTH_OF_STRING_ID];     // +0x91
        PopupParamTypes meButton2Param;                                // +0xB4
        bool            mbButton2ParamUsed;                            // +0xB8
    };

    // The loaded popup table. The console slot at +0x00
    // is a 4-byte pointer; the shipped POPUPS.PUP is transcoded to 8-byte slots (the table
    // pointer reads 0x40 and the 77 record offsets that follow are 8 bytes apart), so the
    // host pointer width is the file's width and the two 16-bit fields follow it.
    struct GuiPopupResource
    {
        GuiPopup** mppPopupData;          // +0x00
        s16        miPopupCount;          // console +0x04
        s16        miSizeOfPopupResource; // console +0x06

        GuiPopupResource* FixUp(uintptr_t luDelta);
        GuiPopupResource* FixDown(uintptr_t luDelta, bool lbDeep);
    };
}
