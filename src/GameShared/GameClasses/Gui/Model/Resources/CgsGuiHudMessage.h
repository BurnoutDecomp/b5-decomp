#pragma once

#include "types.hpp"

// CgsGui HUD-message vocabulary - the per-parameter type enum and the single
// message parameter record. Names/values from the DecFIGS DWARF
// (CgsGuiHudMessage.h:38/:59/:95); consumed by BrnGui::GuiHudMessage (the wire
// record the HUD-message analyzer builds) and the HUD-message resource/director
// TUs. Only the vocabulary the in-scope code consumes lives here; the full
// GuiHudMessageResource/GuiHudMessageData records land with their own TUs.
namespace CgsGui
{
    // DWARF CgsGuiHudMessage.h:29 -- which message group a HUD message belongs to.
    enum HudMessageGroups
    {
        E_HUDMESSAGEGROUP_ALL                  = 0,
        E_HUDMESSAGEGROUP_ONLINE_LIVEREVENGE   = 1,
        E_HUDMESSAGEGROUP_ONLINE_DIRTY_TRICKS  = 2,
        E_HUDMESSAGEGROUP_INGAMEMESSAGES       = 3,
        E_HUDMESSAGEGROUP_COUNT                = 4,
    };

    // DWARF CgsGuiHudMessage.h:59.
    enum HudMessageParamTypes
    {
        E_HUDMESSAGEPARAMTYPES_UNUSED   = 0,
        E_HUDMESSAGEPARAMTYPES_STRING   = 1,
        E_HUDMESSAGEPARAMTYPES_INT      = 2,
        E_HUDMESSAGEPARAMTYPES_FLOAT    = 3,
        E_HUDMESSAGEPARAMTYPES_MONEY    = 4,
        E_HUDMESSAGEPARAMTYPES_TIME     = 5,
        E_HUDMESSAGEPARAMTYPES_STRINGID = 6,
        E_HUDMESSAGEPARAMTYPES_COUNT    = 7,
    };

    // DWARF CgsGuiHudMessage.h:95 -- one formatted HUD-message parameter.
    struct HudMessageParameter
    {
        static const s32 KI_MAX_PARAM_STRING_LENGTH = 64;   // DWARF h:97

        HudMessageParamTypes meParamType;                        // +0x00 (DWARF h:99)
        char                 macParameter[KI_MAX_PARAM_STRING_LENGTH]; // +0x04 (DWARF h:100)
    };
}
