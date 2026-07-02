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

    // ========================================================================
    // ADDITIVE GROW (BrnHudMessageController TU) -- MINIMAL slices of the loaded
    // HUD-message resource records, scoped to what
    // BrnResource::HudMessageController::GetIndexFromMessageHash @0x8267D4C8
    // reads. Full records land with their own TUs.

    // One loaded HUD-message data record. The only recovered field is the 64-bit
    // message-id hash the index lookup compares (X360 `ld r11,0x110(entry);
    // cmpld`); the 0x110 interior (priority/strings/params per the
    // BrnResource::HudMessageEvent analog) is opaque pad (FLAG: member name
    // borrowed from the HudMessageEvent analog; the record's own names land with
    // its TU).
    struct GuiHudMessageData
    {
        u8  maPad[0x110];         // +0x000 (record interior; own TU)
        u64 mHudMessageId;        // +0x110 (CgsID; the GetIndexFromMessageHash key)
    };

    // The HUD-message resource bundle payload a
    // CgsResource::ResourcePtr<GuiHudMessageResource> dereferences to.
    // X360 field offsets from GetIndexFromMessageHash: the data-pointer table at
    // +0 (`lwz r11,0(res); lwzx r11,r11,i*4`) and the loaded count at +8
    // (`lwz r11,8(res)`). FLAG: X360 packs a 4-byte pointer at +0 (+4 interior
    // unrecovered); on this host the 8-byte pointer occupies +0..+7 so the count
    // follows at its same +8 -- access is by name only.
    struct GuiHudMessageResource
    {
        GuiHudMessageData** mppMessages;    // +0x0 (per-message record pointers)
        s32                 miMessageCount; // +0x8 (loaded record count)
    };
}
