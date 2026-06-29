#pragma once

// ===========================================================================
// SDKs/EATech/include/Apt/AptTextFormat.h
//
// AptTextFormat -- the ActionScript `TextFormat` object (font / size / colour /
// alignment / margins ...) an AS script attaches to a text field. It is an
// AptObject (a property-bearing GC value) wrapping an embedded TextFormat record;
// getNewTextFormat/getTextFormat/setTextFormat and the text render items read it.
// AptValue::isTextFormat @0x82AD4D60 type-tests for it.
//
// BASE: AptObject. The ctor @0x82AF1730 seeds the class-flags word at the X360
// +0x1C (mClassFlags, the AptObject member) -- masked `& 0xFF3FFFFF` over a freshly
// zeroed byte, i.e. mClassFlags = 0 -- and the dtor @0x82AF17F8 tail-calls
// AptObject::~AptObject, so AptTextFormat IS-A AptObject (not a bare
// AptValueWithHash). operator new/delete go through the GC value pool.
//
// LAYOUT from the ctor @0x82AF1730 + TextFormat::TextFormat @0x82AEC320:
//   AptObject base (vtable off_82145E18 + property hash + mClassFlags @ +0x1C),
//   then the embedded TextFormat record at the X360 +0x20 (a1+32).
//
// TextFormat record (X360 layout; the ctor seeds "inherit" sentinels then
// copyTextFormatObj overlays the source):
//   +0x00 mFontName  EAStringC  (default empty)
//   +0x04 mfSize     float  = -1.0   ("inherit")
//   +0x08 mnColor    s32    = -1      ("inherit")
//   +0x0C mnField0C  s32    =  3      FLAG: role TBD (objectMemberLookup's per-member
//   +0x10 mnField10  s32    =  2      FLAG: role TBD   jump table @0x82AF18E0 is not
//   +0x14 mnField14  s32    = -1      FLAG: margin/indent group (inherit sentinel)
//   +0x18 mnField18  s32    = -1      FLAG:  exported, so the precise member->field
//   +0x1C mnField1C  s32    = -1      FLAG:  mapping for the 16 gperf keywords (align/
//                                            bold/italic/url/leftMargin/... -- see
//                                     AptTextFormatMembersIndex) is resolved later.)
//
// Member access is BY NAME; X360 offsets are documentation (EAStringC + the value
// base widen on x64, so later offsets shift -- no size assert, per the parity rule).
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptObject.h"             // AptObject base (mClassFlags @ +0x1C)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC mFontName + AptNativeString typedef

class AptValue;            // SDKs/EATech/include/Apt/AptValue/AptValue.h (base, via AptObject)

// ---------------------------------------------------------------------------
// TextFormat -- the bare format record AptTextFormat embeds (TextFormat::TextFormat
// @0x82AEC320 / copyTextFormatObj). Distinct from the AS-object wrapper AptTextFormat.
// ---------------------------------------------------------------------------
struct TextFormat
{
    EAStringC mFontName;   // +0x00  empty == inherit
    float     mfSize;      // +0x04  -1.0 == inherit
    s32       mnColor;     // +0x08  -1   == inherit (24-bit RGB in the low bits)
    s32       mnAlign;     // +0x0C  align enum: 0 left / 1 center / 2 right / >=3 inherit
    // Packed style-flags word (resolved from objectMemberLookup @0x82AF18E0):
    //   bit 0  = bold value      bit 16 = bold defined
    //   bit 4  = italic value    bit 20 = italic defined
    //   bit 8  = underline value bit 24 = underline defined
    // A style is "inherit" when its defined-bit is clear.
    s32       mnStyleFlags; // +0x10
    s32       mnIndent;     // +0x14  -1 == inherit
    s32       mnLeftMargin; // +0x18  -1 == inherit
    s32       mnRightMargin;// +0x1C  -1 == inherit

    // ---- mnStyleFlags bit layout (X360 objectMemberLookup masks) -------------
    enum
    {
        KU_BOLD_VALUE        = 0x00000001u,
        KU_ITALIC_VALUE      = 0x00000010u,
        KU_UNDERLINE_VALUE   = 0x00000100u,
        KU_BOLD_DEFINED      = 0x00010000u,
        KU_ITALIC_DEFINED    = 0x00100000u,
        KU_UNDERLINE_DEFINED = 0x01000000u
    };

    // ctor @0x82AEC320 -- seed the inherit sentinels then copyTextFormatObj(this, pSrc).
    // FLAG: body its own TU (needs copyTextFormatObj). Declared for the embedders.
    explicit TextFormat(const TextFormat* pSource);   // FLAG: body its own TU
};

// ---------------------------------------------------------------------------
// AptTextFormat -- the GC'd AS TextFormat value (AptObject + the embedded record).
// ---------------------------------------------------------------------------
struct AptTextFormat : public AptObject
{
    // ---- GC pool new / delete (X360 operator new @0x82AE6F28 / delete @0x82AF17A0).
    // AptTextFormat is a garbage-collected value (AptObject -> ... -> AptValueGC), so
    // its block comes from the GC pool (gpGCPoolManager) with the AptValueGC_MemItem
    // "allocated" flag flipped -- the same GC-value pattern as AptError / AptArray.
    // Bodied in AptTextFormat.cpp.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // ctor @0x82AF1730 -- AptObject(AptVFT_TextFormat, 8) base (mClassFlags = 0) then
    // the embedded TextFormat(pSource) record. dtor @0x82AF17F8 releases mFontName and
    // chains ~AptObject (RAII + base, so the body is empty).
    explicit AptTextFormat(const TextFormat* pSource);   // @0x82AF1730
    virtual ~AptTextFormat() {}                          // @0x82AF17F8

    // objectMemberLookup @0x82AF18E0 -- resolve a scriptable TextFormat property name
    // (align / bold / color / font / indent / italic / leftMargin / rightMargin /
    // size / underline) to a fresh AptValue, or the shared `undefined` singleton when
    // the field is at its inherit sentinel; everything else -> null.
    virtual AptValue* objectMemberLookup(AptValue* const pThis,
                                         const AptNativeString* const pName) const;   // @0x82AF18E0

    TextFormat mFormat;   // +0x20 (X360) -- the embedded format record
};
