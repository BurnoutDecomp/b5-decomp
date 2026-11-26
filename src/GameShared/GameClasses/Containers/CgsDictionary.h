#ifndef CGS_DICTIONARY_H
#define CGS_DICTIONARY_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT_AT

// ARTIST's spelling of this header in its inlined asserts (0x820A1A70: the GameShared include
// directory plus "Containers/CgsDictionary.h").
#define CGS_ARTIST_CGSDICTIONARY_FILE "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsDictionary.h"

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

    // ADDITIVE (member fn, no storage). The serialised dictionary's total stored byte
    // size (miDictionarySize @+4). DictionaryResourceTypeBase::GetSerialisedResourceDescriptor
    // (X360 0x828158E0) reads this `*(resource+4)` to size the main resource block; exposed
    // by name so that handler does not reach the protected field via raw-offset arithmetic.
    s32 GetDictionarySize() const
    {
        return miDictionarySize;
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
    DictEntry::DictionaryKey FindByValue(const Type* lpData) const;         // :324

    // CgsDictionary.h:354 (DWARF). Indexed access; the bound assert sits on line 356 with the
    // message ARTIST bakes (0x820A1A48), inlined into Dictionary<T>::FixUp (0x82666048).
    Type* GetAt(s32 lnIndex)
    {
        CGS_ASSERT_AT(lnIndex>=0 && lnIndex < miNumEntries, "lnIndex>=0 && lnIndex < miNumEntries",
            CGS_ARTIST_CGSDICTIONARY_FILE, 356);
        return reinterpret_cast<Type*>( mpaIndex[ lnIndex ].mpData );
    }

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

    // CgsDictionary.h:375 (DWARF). Const indexed access: the bound assert on line 377
    // (ICEList::GetICETakeDataFromGuid 0x8267BF60), then mpaIndex[i].mpData.
    const Type* GetAt(s32 lnIndex) const
    {
        CGS_ASSERT_AT(lnIndex>=0 && lnIndex < miNumEntries, "lnIndex>=0 && lnIndex < miNumEntries",
            CGS_ARTIST_CGSDICTIONARY_FILE, 377);
        return reinterpret_cast<const Type*>( mpaIndex[ lnIndex ].mpData );
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

        // DWARF h:398 scopes only liIndex: ARTIST re-reads the count each pass (0x82666070)
        // and asserts through the inlined GetAt (h:356).
        for ( s32 liIndex = 0; liIndex < this->GetNumEntries(); ++liIndex )
        {
            this->GetAt(liIndex)->FixUp(lrResource);
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
        // DWARF h:423 scopes only liIndex (as FixUp).
        for ( s32 liIndex = 0; liIndex < this->GetNumEntries(); ++liIndex )
        {
            this->GetAt(liIndex)->FixDown(lrResource);
        }

        DictionaryBase::FixDown();
    }
};

} // namespace CgsContainers

#endif // CGS_DICTIONARY_H
