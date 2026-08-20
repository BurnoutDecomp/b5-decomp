#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (GuiHudMessageData::mMessageIdHash)

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
    // The loaded HUD-message resource records (full DWARF layout, CgsGuiHudMessage.h).
    // The BrnHudMessageController accessors read named fields (mMessageIdHash @+0x110,
    // muAvailabilityBitSet @+0x118, mfTimeToWait @+0x120, meMessageGroup @+0x12C,
    // maiParamCount[3] @+0x130, maaeParams[3][4] @+0x13C). All access is BY NAME (the
    // X360 32-bit member offsets are not host-assertable across the widened pointers).

    // DWARF CgsGuiHudMessage.h:113 -- one loaded HUD-message record.
    struct GuiHudMessageData
    {
        static const s32 KI_MAX_NUM_STRINGS          = 3;   // DWARF h:116
        static const s32 KI_STRING_ID_LENGTH         = 64;  // DWARF h:117
        static const s32 KI_FLASH_FRAME_LABEL_LENGTH = 32;  // DWARF h:118
        static const s32 KI_MESSAGE_ID_LENGTH        = 13;  // DWARF h:119
        static const s32 KI_MAX_PARAMS_PER_STRING    = 4;   // DWARF h:120

        char  maacStringId[KI_MAX_NUM_STRINGS][KI_STRING_ID_LENGTH]; // +0x000 (DWARF h:122)
        char  macMessageStyle[KI_FLASH_FRAME_LABEL_LENGTH];          // +0x0C0 (DWARF h:124)
        char  macDefaultIcon[KI_FLASH_FRAME_LABEL_LENGTH];           // +0x0E0 (DWARF h:125)
        char  macMessageId[KI_MESSAGE_ID_LENGTH];                    // +0x100 (DWARF h:127)
        // (3 bytes tail pad to 8-byte align the CgsID hash)
        CgsID mMessageIdHash;                                        // +0x110 (DWARF h:128)
        u32   muAvailabilityBitSet;                                  // +0x118 (DWARF h:130)
        f32   mfDuration;                                            // +0x11C (DWARF h:132)
        f32   mfTimeToWait;                                          // +0x120 (DWARF h:133)
        s32   miPriority;                                            // +0x124 (DWARF h:134)
        s32   miForceRemoveThreshold;                                // +0x128 (DWARF h:135)
        HudMessageGroups     meMessageGroup;                         // +0x12C (DWARF h:136)
        s32                  maiParamCount[KI_MAX_NUM_STRINGS];      // +0x130 (DWARF h:138)
        HudMessageParamTypes maaeParams[KI_MAX_NUM_STRINGS][KI_MAX_PARAMS_PER_STRING]; // +0x13C (DWARF h:140)
    };

    // DWARF CgsGuiHudMessage.h:161 -- the HUD-message resource bundle a
    // CgsResource::ResourcePtr<GuiHudMessageResource> dereferences to.
    // X360: pointer table @+0 (lwz r11,0(res)), record count @+8 (lwz r11,8(res)).
    // FLAG: X360 packs the 4-byte pointer at +0 with miSizeOfHudMessageResource at +4 and
    // miHudMessageCount at +8; on this 64-bit host the pointer widens to +0..+7, so the two
    // trailing s32s no longer land at their X360 offsets -- access is BY NAME only.
    struct GuiHudMessageResource
    {
        GuiHudMessageData** mppHudMessageData;         // +0x0 (DWARF h:164)
        s32                 miSizeOfHudMessageResource; // +0x4 (DWARF h:165)
        s32                 miHudMessageCount;          // +0x8 (DWARF h:166)

        // X360 CgsGui::GuiHudMessageResource::FixUp @0x82846528 -- the load-time
        // relocation the type handler forwards to (CgsResource::HudMessageResourceType::
        // FixUp @0x828465D8 is a single tail call to it). Body in CgsGuiHudMessage.cpp.
        //
        // [gateui r4] CE-3: the delta is `uintptr_t`, not the console's `int`. The console
        // spells it 32-bit only because its pointers are; the shipped .HM bundle is
        // transcoded to native-8 slots, so the offsets this relocates are host-width and a
        // truncated (GetLoadBase) delta would rebase them into the low 4 GB. Same treatment
        // and same reason as BrnStreetData::StreetData::FixUp and
        // CgsLanguage::LanguageResourceType::FixUp/FixDown.
        GuiHudMessageResource* FixUp(uintptr_t luDelta);
    };
}
