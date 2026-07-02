#pragma once

#include "types.hpp"

// CgsGui popup vocabulary - the popup style/param/icon enums plus the single
// popup parameter record. Names/values from the DecFIGS DWARF
// (CgsGuiPopupResource.h:34/54/64/78); the X360 overlay states are gated on them
// (BrnGui::BaseOverlayState::SetupOverlay asserts meIcon against
// E_POPUPICONS_INVISIBLE/WARNING and indexes its icon-state and param-format
// tables with PopupIcons/PopupParamTypes, and the overlay flow instantiates one
// state per PopupStyle value).
//
// Only the popup vocabulary the in-scope GUI code consumes lives here; the full
// CgsGui::GuiPopup resource record (DWARF CgsGuiPopupResource.h:97) is out of
// scope and lands with its own resource TU.
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
}
