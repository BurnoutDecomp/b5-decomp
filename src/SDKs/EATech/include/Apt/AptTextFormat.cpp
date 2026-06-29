// ===========================================================================
// EATech Apt -- AptTextFormat: the ActionScript TextFormat object value.
//
// Decompiled from the X360 ARTIST.XEX (the authoritative spine):
//     AptTextFormat::operator new          @ 0x82AE6F28
//     AptTextFormat::operator delete       @ 0x82AF17A0
//     AptTextFormat::AptTextFormat (ctor)  @ 0x82AF1730
//     AptTextFormat::~AptTextFormat        @ 0x82AF17F8
//     AptTextFormat::objectMemberLookup    @ 0x82AF18E0
// The `vector deleting destructor' is a compiler thunk (~AptTextFormat ->
// conditional operator delete) and is dropped, not hand-written.
//
// AptTextFormat IS-A AptObject (a property-bearing GC value): the ctor seeds the
// AptObject class-flags word at the X360 +0x1C (`mClassFlags = 0`, the
// `& 0xFF3FFFFF` over a freshly-zeroed byte is the compiler's bitfield-store
// artifact) and the dtor tail-calls AptObject::~AptObject. See AptTextFormat.h.
//
// objectMemberLookup @0x82AF18E0 is a gperf-dispatched switch: the member name is
// resolved through TextFormatMembersIndex::in_word_set, then a per-member-id jump
// table (X360 byte_82145270[id-1], base loc_82AF1954) reads the matching embedded
// TextFormat field and boxes it -- align -> a string, color/indent/margins ->
// AptInteger, size -> AptFloat, bold/italic/underline -> AptBoolean, font -> a
// string -- returning the shared `undefined` singleton when the field is at its
// inherit sentinel. The remaining recognised names (blockIndent / bullet /
// leading / tabStops / target / url) have empty jump-table slots, so they fall
// through to null. The console's character-by-character name compares and the
// InitFromBuffer+operator=+DecreaseInternalRefCount string-temporary idiom are
// restored to their named EAStringC operations.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptTextFormat.h"

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"      // AptValue, AptVFT_TextFormat
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"     // AptString::Create / GetInternalString
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"    // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"      // AptFloat::Create
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"    // AptBoolean::Create
#include "SDKs/EATech/include/Apt/AptString/EAString.h"     // EAStringC / AptNativeString
#include "SDKs/EATech/include/Apt/AptDefine.h"              // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"          // gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"            // AptValueGC_MemItem
#include "SDKs/EATech/Apt/AptTextFormatMembersIndex.h"      // the gperf member recognizer

// FLAG (homed by the AS-globals layer): the shared `undefined` singleton
// (X360 off_8324D814). objectMemberLookup returns it for any field at its inherit
// sentinel. Declared extern here exactly as the sibling Apt TUs do (wired at AptInit).
extern AptValue* gpUndefinedValue;

// ---------------------------------------------------------------------------
// GC pool operator new / delete (X360 @0x82AE6F28 / @0x82AF17A0).
//
// AptTextFormat is a garbage-collected value, so its block comes from the GC value
// pool (off_8324D834 == gpGCPoolManager) with the AptValueGC_MemItem "allocated"
// flag flipped (byte_8324D804 == gAptValueGCSizeOffset selects the size-word
// offset) -- the identical external-allocator pattern as AptError. The
// cast-to-AptValueGC_MemItem is allocator bookkeeping on the raw pool block, not a
// member poke into a live C++ object.
// ---------------------------------------------------------------------------
void* AptTextFormat::operator new(size_t size)
{
    void* lpMem = gpGCPoolManager->Allocate(size);
    reinterpret_cast<AptValueGC_MemItem*>(lpMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    return lpMem;
}

void AptTextFormat::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager->Deallocate(p, size))
        reinterpret_cast<AptValueGC_MemItem*>(p)->SetIsAllocated(gAptValueGCSizeOffset, false);
}

// ---------------------------------------------------------------------------
// ctor @0x82AF1730
//
// AptValueWithHash(AptVFT_TextFormat, 8) base (r4=0x1C=AptVFT_TextFormat,
// r5=8=hash capacity), then the X360 stores 0 into the class-flags byte at +0x1C
// and masks it `& 0xFF3FFFFF` (a no-op over the zeroed byte) -- i.e. mClassFlags
// = 0, which the AptObject base ctor already does. Finally the embedded
// TextFormat record is constructed from pSource (TextFormat::TextFormat @0x82AEC320).
// ---------------------------------------------------------------------------
AptTextFormat::AptTextFormat(const TextFormat* pSource)
    : AptObject(AptVFT_TextFormat, 8)
    , mFormat(pSource)
{
}

// ---------------------------------------------------------------------------
// objectMemberLookup @0x82AF18E0
//
// Resolve a scriptable TextFormat property. The console gate is `if (pThis)` --
// when the context value is null the lookup is skipped entirely.
// ---------------------------------------------------------------------------
AptValue* AptTextFormat::objectMemberLookup(AptValue* const pThis,
                                            const AptNativeString* const pName) const
{
    // X360: in_word_set is only called when pThis (a2) is non-null.
    const TextFormatMembersIndex::Entry* pEntry =
        pThis ? TextFormatMembersIndex::in_word_set(pName->GetBuffer(), pName->GetLength())
              : nullptr;
    if (pEntry == nullptr)
        return nullptr;

    // X360: v4 = pEntry->miData - 1; if (v4 > 0xE) return 0; else jump byte_82145270[v4].
    // The jump table (base loc_82AF1954) maps each member id to its field accessor;
    // ids whose slot is the table's default (blockIndent / bullet / leading /
    // tabStops / target / url) land on the `return 0` block.
    switch (pEntry->miData)
    {
    case 1:   // "align" -> mnAlign enum boxed as a string ("left"/"center"/"right")
    {
        // X360: AptString::Create("") then operator=(StaticStringHelperT&) the
        // alignment keyword; >=3 is the inherit sentinel -> undefined.
        // FLAG: the X360 assigns a precompiled StaticStringHelperT constant
        // (unk_8324E638/6A4/60C); restored to the equivalent literal assignment.
        if (mFormat.mnAlign >= 3)
            return gpUndefinedValue;

        AptString* pStr = AptString::Create("");
        if (mFormat.mnAlign < 1)
            *pStr->GetInternalString() = EAStringC("left");    // mnAlign == 0
        else if (mFormat.mnAlign == 1)
            *pStr->GetInternalString() = EAStringC("center");
        else /* mnAlign == 2 */
            *pStr->GetInternalString() = EAStringC("right");
        return pStr;
    }

    case 3:   // "bold" -> AptBoolean (bit 16 = defined, bit 0 = value)
        if ((mFormat.mnStyleFlags & TextFormat::KU_BOLD_DEFINED) == 0)
            return gpUndefinedValue;
        return AptBoolean::Create((mFormat.mnStyleFlags & TextFormat::KU_BOLD_VALUE) != 0);

    case 5:   // "color" -> AptInteger (24-bit RGB); -1 == inherit
        if (mFormat.mnColor == -1)
            return gpUndefinedValue;
        return AptInteger::Create(mFormat.mnColor & 0x00FFFFFF);

    case 6:   // "font" -> AptString of mFontName; empty == inherit
    {
        // X360 compares mFontName.m_pData against the empty-string sentinel
        // (&s_EmptyInternalData, unk_82F72FF8). The empty font has an empty buffer.
        // FLAG: pointer-identity sentinel test restored to the equivalent
        // empty-buffer test (s_EmptyInternalData is private to EAStringC).
        if (mFormat.mFontName.GetBuffer()[0] == '\0')
            return gpUndefinedValue;

        AptString* pStr = AptString::Create("");
        *pStr->GetInternalString() = EAStringC(mFormat.mFontName.GetBuffer());
        return pStr;
    }

    case 7:   // "indent" -> AptInteger; -1 == inherit
        if (mFormat.mnIndent == -1)
            return gpUndefinedValue;
        return AptInteger::Create(mFormat.mnIndent);

    case 8:   // "italic" -> AptBoolean (bit 20 = defined, bit 4 = value)
        if ((mFormat.mnStyleFlags & TextFormat::KU_ITALIC_DEFINED) == 0)
            return gpUndefinedValue;
        return AptBoolean::Create((mFormat.mnStyleFlags & TextFormat::KU_ITALIC_VALUE) != 0);

    case 10:  // "leftMargin" -> AptInteger; -1 == inherit
        if (mFormat.mnLeftMargin == -1)
            return gpUndefinedValue;
        return AptInteger::Create(mFormat.mnLeftMargin);

    case 11:  // "rightMargin" -> AptInteger; -1 == inherit
        if (mFormat.mnRightMargin == -1)
            return gpUndefinedValue;
        return AptInteger::Create(mFormat.mnRightMargin);

    case 14:  // "size" -> AptFloat; -1.0 == inherit
        if (mFormat.mfSize == -1.0f)   // X360 fcmpu vs flt_820037C8 (== -1.0)
            return gpUndefinedValue;
        return AptFloat::Create(mFormat.mfSize);

    case 15:  // "underline" -> AptBoolean (bit 24 = defined, bit 8 = value)
        if ((mFormat.mnStyleFlags & TextFormat::KU_UNDERLINE_DEFINED) == 0)
            return gpUndefinedValue;
        return AptBoolean::Create((mFormat.mnStyleFlags & TextFormat::KU_UNDERLINE_VALUE) != 0);

    default:  // miData 2/4/9/12/13/16 (blockIndent/bullet/leading/tabStops/target/url)
        return nullptr;   // X360 loc_82AF1AF0: empty jump-table slots -> return 0
    }
}
