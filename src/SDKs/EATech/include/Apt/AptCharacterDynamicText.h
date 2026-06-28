#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterDynamicText: the .apt character (asset-side, shared)
// for a DYNAMIC text field. It is a type-tagged AptCharacter subclass; its runtime
// per-instance state lives in AptRenderItemDynamicText / AptCharacterTextInst.
//
// MINIMAL reconstruction: only the one field the AptCharacterTextInst text path
// needs is recovered -- the asset's DEFAULT text buffer, read by SetText/UpdateText
// when the bound variable resolves to a non-string (the field's initial contents).
// The X360 reads it at console offset 0x3C of the character object:
//   UpdateText: pDefault = *(mpRenderItem->mpCharacter + 0x3C); if(!pDefault) ""
//
// FLAG: the remaining dynamic-text character fields (the authored bounds, font,
// colours, flags baked from the .apt at Fixup) are NOT yet recovered -- there is no
// header for them in b5-decomp/src, no Feb-2007 partial source, and no DecFIGS
// dwarfdump for this class. They are represented as an opaque padding region so the
// one recovered member (mpDefaultText) is reachable BY NAME without an offset hack;
// access is by name, not by the console byte offset (the x64 base width differs).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptCharacter.h"   // base AptCharacter

struct AptCharacterDynamicText : public AptCharacter
{
    // FLAG: unrecovered authored dynamic-text fields between the AptCharacter base
    // (console +0x10) and the default-text pointer (console +0x3C) -- 0x3C-0x10 =
    // 0x2C bytes on the console. Kept as an opaque blob (no offset hack: the one
    // recovered field below is named). The x64 layout differs (wider base); the
    // member is accessed by name regardless.
    uint8_t mPadAuthoredFields[0x3C - 0x10];

    // [c:0x3C] the asset's default text contents (null -> the empty string ""). Read
    // by AptCharacterTextInst::SetText / UpdateText when the bound AS variable does
    // not resolve to a string.
    const char* mpDefaultText;
};
