#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterDynamicText: the .apt character (asset-side, shared)
// for a DYNAMIC text field. It is a type-tagged AptCharacter subclass; its runtime
// per-instance state lives in AptRenderItemDynamicText / AptCharacterTextInst.
//
// The authored text-field defaults baked from the .apt at Fixup. The asset's
// DEFAULT text buffer (mpDefaultText) is read by SetText/UpdateText when the bound
// variable resolves to a non-string (the field's initial contents); the layout /
// formatting scalars (margins, colour, font size, the glyph index) are the
// authored defaults stamped into the shared default-text template by
// AptCharacterHelper::CreateTextCharacterInst (@0x82B010C0) and read by the
// text-field renderer. The X360 reads the default-text pointer at console offset
// 0x3C of the character object:
//   UpdateText: pDefault = *(mpRenderItem->mpCharacter + 0x3C); if(!pDefault) ""
//
// LAYOUT recovered from the X360 ARTIST AptCharacterHelper::CreateTextCharacterInst
// field-by-field stores (console offsets; total console sizeof = 0x44 / 68, pinned
// by that builder's DOGMA Allocate(68) + AptCharacterHelper::Shutdown's
// Deallocate(.,68)). The authored region is named so the builder writes it BY NAME
// (no offset hack); the few stores the builder leaves at zero are named with their
// observed role where clear and as reserved authored dwords otherwise. The x64
// layout differs (wider base pointers); members are accessed by name regardless.
//
// The font for a dynamic-text character is the FONT character it links to: the
// builder stores it in the inherited AptCharacter::mpFixupLink slot (the
// character-table back-link), so it is reached by that named base member rather
// than a per-subclass alias.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptCharacter.h"   // base AptCharacter

struct AptCharacterDynamicText : public AptCharacter
{
    // ---- authored layout defaults (console +0x10 .. +0x3C) --------------------
    // The four field-edge margins (the X360 stamps the template's to -2 / -2 / 2 / 2).
    float       mfMarginLeft;        // [c:0x10]
    float       mfMarginTop;         // [c:0x14]
    float       mfMarginRight;       // [c:0x18]
    float       mfMarginBottom;      // [c:0x1C]

    // The default glyph index into the linked font character's glyph list (the
    // builder seeds it to -1, then to the index of the first glyph tagged 3).
    int32_t     mnDefaultGlyphIndex; // [c:0x20]

    int32_t     mnAuthoredReserved0; // [c:0x24] (authored; template stamps 0)
    int32_t     mnAuthoredReserved1; // [c:0x28] (authored; template stamps 0)

    float       mfFontSize;          // [c:0x2C] default font size (template = 12.0)

    int32_t     mnAuthoredReserved2; // [c:0x30] (authored; template stamps 0)
    int32_t     mnAuthoredReserved3; // [c:0x34] (authored; template stamps 0)
    int32_t     mnAuthoredReserved4; // [c:0x38] (authored; template stamps 0)

    // [c:0x3C] the asset's default text contents (null -> the empty string ""). Read
    // by AptCharacterTextInst::SetText / UpdateText when the bound AS variable does
    // not resolve to a string. (Burnout wiki AptCharacterText.szInitialText: 4-byte
    // char* @c:0x2C, 8-byte char* @native:0x30 relative to the AptCharacterText region;
    // struct-absolute 0x50 on x64 -- the authored region starts at the 0x20-byte base end.)
    const char* mpDefaultText;

    // [c:0x40] the name of the AS VARIABLE this text field binds to (Burnout wiki
    // AptCharacterText.szVariable: "Name of the variable where the text is stored").
    // It is a POINTER, not a reserved dword -- an 8-byte char* on native-8 (struct-absolute
    // 0x58), the wiki's szVariable @ AptCharacterText+0x38. The X360 reads it as a pointer
    // too (instantiateCharacter @0x82B061D0: `v55 = *(char + 0x40); InitFromBuffer(&s, v55)`
    // -> RenderItem mVarValue @+56); the empty default is the shared "" byte (unk_82143A3E).
    // (Was mis-typed int32_t, which TRUNCATED the relocated 8-byte pointer to its low 32 bits
    // and AV'd in EAStringC::InitFromBuffer when a nested dynamic-text char was placed.)
    const char* mpVariableName;
};
