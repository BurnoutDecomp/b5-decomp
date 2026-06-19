#ifndef CGS_DICTIONARY_H
#define CGS_DICTIONARY_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ============================================================================
// GameShared/GameClasses/Containers/CgsDictionary.h
//
// CgsContainers::DictEntry / DictionaryBase / SimpleDictionary<T> / Dictionary<T>
// -- the project's key->data dictionary container family.
//
// This is a MINIMAL owning home reconstructed from the DecFIGS DWARF
// (GameShared/GameClasses/Containers/CgsDictionary.h) plus the X360 pseudocode of
// the in-place inline accessors the ARTIST build emitted into its callers (the
// linear-scan Find/GetAt/GetNumEntries seen in BrnResource::ICEList). It carries
// the full member layout (proven by ICEList's pseudocode pointer arithmetic) but
// provides INLINE bodies ONLY for the three accessors ICEList actually needs --
// GetNumEntries() const, GetAt(int) const, Find(DictionaryKey) const -- as
// faithful linear scans matching the inlined X360 code, PLUS the generic
// Dictionary<T>::FixUp/FixDown(const Resource&) relocation pass (grown in when the
// DictionaryResourceType<ICE::ICETakeData> TU landed -- the X360 inlines those two
// generic template bodies into each per-T resource-type wrapper). Everything else
// (GetIndexByKey/GetKeyByIndex/GetSize/FindByValue and the non-const Find/GetAt) is
// declaration-only; future Dictionary TUs EXTEND this header rather than redefining it.
//
// LAYOUT (32-bit X360, proven by ICEList's pseudocode):
//   DictEntry            : mKey(int64 @0), mpData(char* @8), mxUserFlags(int32 @12)
//                          -> 16-byte stride (pseudocode steps `+= 16`).
//   DictionaryBase       : miNumEntries(@0), miDictionarySize(@4), mpaIndex(@8)
//                          -> pseudocode reads *dict (miNumEntries) and dict[2]
//                          (mpaIndex), and indexes entries as 16*i + dict[2].
// ============================================================================

namespace CgsResource
{
    // The Dictionary<T>::FixUp/FixDown fix-up pass takes the owning resource by
    // reference. Forward-declared (declaration-only methods; no body needs the
    // layout here). Tag must be `struct` to match the canonical CgsResourceType.h
    // declaration (avoids MSVC C4099 when a future CgsDictionary.cpp includes both).
    struct Resource;
}

namespace CgsContainers
{

// CgsDictionary.h:51 (DWARF). One key/data slot. POD; 16-byte stride proven by the
// ICEList pseudocode (`v11 += 16`, `16 * v10 + ...`).
struct DictEntry
{
    // CgsDictionary.h:39 (DWARF). The dictionary key is a 64-bit value.
    typedef s64 DictionaryKey;

    DictionaryKey mKey;        // :53  @+0
    char*         mpData;      // :54  @+8
    s32           mxUserFlags; // :55  @+12
};

// CgsDictionary.h:40 (DWARF). Invalid-key sentinel (all-ones 32-bit value widened
// to the 64-bit key; DWARF value 4294967295 == 0xFFFFFFFF).
static const DictEntry::DictionaryKey KI_DICTIONARY_INVALID_KEY = 0xFFFFFFFF;

// CgsDictionary.h:33 (DWARF). User-flag bit marking a duplicate data reference
// (top bit set: DWARF byte image [255,255,255,255,128,0,0,0] -> 0x80000000).
static const s32 kxDictFlag_DuplicateDataReference = static_cast<s32>(0x80000000);

// CgsDictionary.h:73 (DWARF). Untyped base: entry count, capacity, and the entry
// array. Members at their proven X360 offsets.
struct DictionaryBase
{
public:
    // ---- declaration-only (each reconstructed in its own TU) ----
    void                     FixUp();                                   // :76
    void                     FixDown();                                 // :79
    s32                      GetIndexByKey(DictEntry::DictionaryKey lKey) const; // :87
    DictEntry::DictionaryKey GetKeyByIndex(s32 liIndex) const;          // :91
    s32                      GetSize() const;                           // :94

    // CgsDictionary.h:83 (DWARF). Entry count. Inlined by the X360 build as a plain
    // load of miNumEntries (`*dict`); reconstructed as the trivial accessor.
    s32 GetNumEntries() const
    {
        return miNumEntries;
    }

protected:
    s32        miNumEntries;     // :99   @+0
    s32        miDictionarySize; // :100  @+4
    DictEntry* mpaIndex;         // :101  @+8
};

// CgsDictionary.h:123 (DWARF). Adds typed key->data lookup over the base.
template <class Type>
struct SimpleDictionary : public DictionaryBase
{
public:
    // ---- declaration-only (reconstructed in their own TUs) ----
    Type*                    Find(DictEntry::DictionaryKey lKey);            // :263
    Type*                    GetAt(s32 liIndex);                             // :354
    DictEntry::DictionaryKey FindByValue(const Type* lpData) const;         // :324

    // CgsDictionary.h:294 (DWARF). Const key lookup. The X360 inlined this as a
    // linear scan over mpaIndex comparing the full 64-bit mKey field (asm ld/cmpd
    // at DictEntry+0); restored as the logical const Find. Returns the matching
    // entry's data, or null.
    const Type* Find(DictEntry::DictionaryKey lKey) const
    {
        for ( s32 liIndex = 0; liIndex < miNumEntries; ++liIndex )
        {
            if ( mpaIndex[ liIndex ].mKey == lKey )
            {
                return reinterpret_cast<const Type*>( mpaIndex[ liIndex ].mpData );
            }
        }
        return 0;
    }

    // CgsDictionary.h:375 (DWARF). Const indexed access. The X360 asserts the index
    // bound inline before returning mpaIndex[i].mpData; reconstructed with the same
    // bound via CGS_ASSERT (the baked CgsDictionary.h:377 path/line is discarded).
    const Type* GetAt(s32 liIndex) const
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < miNumEntries,
            "lnIndex>=0 && lnIndex < miNumEntries\n");
        return reinterpret_cast<const Type*>( mpaIndex[ liIndex ].mpData );
    }
};

// CgsDictionary.h:168 (DWARF). Adds the resource fix-up/fix-down pass.
template <class Type>
struct Dictionary : public SimpleDictionary<Type>
{
public:
    // CgsDictionary.h:396 (DWARF). Relocate-on-load: first run the base fix-up
    // (DictionaryBase::FixUp relocates the entry index), then fix up every entry's
    // payload by calling Type::FixUp(entry.mpData, lrResource). The X360 ARTIST build
    // INLINES this generic body into each per-T DictionaryResourceType<T>::FixUp
    // wrapper (e.g. @0x82665FF8 for ICE::ICETakeData) -- restored here as the logical
    // template body the wrapper forwards to. Base-FIRST, entries-second (matches the
    // asm: bl DictionaryBase::FixUp precedes the entry loop).
    void FixUp(const CgsResource::Resource& lrResource)
    {
        DictionaryBase::FixUp();

        const s32 liNumEntries = this->miNumEntries;
        for ( s32 liIndex = 0; liIndex < liNumEntries; ++liIndex )
        {
            // Per-entry bound assert (the X360 re-tests it each iteration;
            // CgsDictionary.h:356 in the baked image -- path/line dropped here).
            CGS_ASSERT(liIndex >= 0 && liIndex < this->miNumEntries,
                "lnIndex>=0 && lnIndex < miNumEntries\n");

            Type* lpEntryData = reinterpret_cast<Type*>( this->mpaIndex[ liIndex ].mpData );
            lpEntryData->FixUp(lrResource);
        }
    }

    // CgsDictionary.h:421 (DWARF). Relocate-for-save: the inverse order -- fix DOWN
    // every entry's payload FIRST (Type::FixDown(entry.mpData, lrResource)), THEN run
    // the base fix-down (DictionaryBase::FixDown relocates the entry index last). The
    // X360 INLINES this into DictionaryResourceType<T>::FixDown (@0x82666090 for
    // ICE::ICETakeData); entries-FIRST, base-second (matches the asm: the entry loop
    // precedes the trailing bl DictionaryBase::FixDown).
    void FixDown(const CgsResource::Resource& lrResource)
    {
        const s32 liNumEntries = this->miNumEntries;
        for ( s32 liIndex = 0; liIndex < liNumEntries; ++liIndex )
        {
            CGS_ASSERT(liIndex >= 0 && liIndex < this->miNumEntries,
                "lnIndex>=0 && lnIndex < miNumEntries\n");

            Type* lpEntryData = reinterpret_cast<Type*>( this->mpaIndex[ liIndex ].mpData );
            lpEntryData->FixDown(lrResource);
        }

        DictionaryBase::FixDown();
    }
};

} // namespace CgsContainers

#endif // CGS_DICTIONARY_H
