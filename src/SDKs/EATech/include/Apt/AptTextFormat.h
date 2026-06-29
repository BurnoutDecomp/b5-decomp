#pragma once

// ===========================================================================
// SDKs/EATech/include/Apt/AptTextFormat.h
//
// AptTextFormat -- the ActionScript `TextFormat` object (font / size / colour /
// alignment / margins ...) an AS script attaches to a text field. It is an
// AptValueWithHash (a GC value with a named-member property table) wrapping an
// embedded TextFormat record; getNewTextFormat/getTextFormat/setTextFormat and the
// text render items read it. AptValue::isTextFormat @0x82AD4D60 type-tests for it.
//
// LAYOUT from the ctor @0x82AF1730 + TextFormat::TextFormat @0x82AEC320:
//   AptTextFormat : AptValueWithHash (vtable off_82145E18, bitfield type-masked
//   0xFF3FFFFF), then the embedded TextFormat record at the X360 +0x20 (a1+32).
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

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptValueWithHash.h"      // AptValueWithHash base
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC mFontName

// ---------------------------------------------------------------------------
// TextFormat -- the bare format record AptTextFormat embeds (TextFormat::TextFormat
// @0x82AEC320 / copyTextFormatObj). Distinct from the AS-object wrapper AptTextFormat.
// ---------------------------------------------------------------------------
struct TextFormat
{
    EAStringC mFontName;   // +0x00
    float     mfSize;      // +0x04  -1.0 == inherit
    s32       mnColor;     // +0x08  -1   == inherit
    s32       mnField0C;   // +0x0C  FLAG: role TBD (ctor default 3)
    s32       mnField10;   // +0x10  FLAG: role TBD (ctor default 2)
    s32       mnField14;   // +0x14  FLAG: margin/indent group (ctor default -1)
    s32       mnField18;   // +0x18  FLAG: margin/indent group (ctor default -1)
    s32       mnField1C;   // +0x1C  FLAG: margin/indent group (ctor default -1)

    // ctor @0x82AEC320 -- seed the inherit sentinels then copyTextFormatObj(this, pSrc).
    // FLAG: body its own TU (needs copyTextFormatObj). Declared for the embedders.
    explicit TextFormat(const TextFormat* pSource);   // FLAG: body its own TU
};

// ---------------------------------------------------------------------------
// AptTextFormat -- the GC'd AS TextFormat value (AptValueWithHash + the record).
// ---------------------------------------------------------------------------
struct AptTextFormat : public AptValueWithHash
{
    TextFormat mFormat;   // +0x20 (X360) -- the embedded format record

    // ctor @0x82AF1730 / dtor @0x82AF17F8 / operator new @0x82AE6F28 /
    // operator delete @0x82AF17A0 / objectMemberLookup @0x82AF18E0 -- FLAG: bodies are
    // the AptTextFormat TU (objectMemberLookup needs the un-exported per-member jump
    // table). Declared so the type + member access compile for the text subsystem.
    explicit AptTextFormat(const TextFormat* pSource);   // FLAG: body its own TU
};
