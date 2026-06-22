#ifndef SHAREDCLASSES_DATALISTS_WHEELLIST_H
#define SHAREDCLASSES_DATALISTS_WHEELLIST_H

// WheelList.h
// BrnResource::WheelList -- the runtime aggregation of loaded wheel-list resources
// plus the per-wheel "slot" table the profile uses to track which wheels exist and
// whether their content has been bought. Direct structural sibling of
// BrnResource::ChallengeList (same maStaticDataLists[]/maSlots[]/miCount/miListCount
// shape, same Construct/Destruct/AddListResource trio).
//
// Shape recovered from the DecFIGS DWARF (SharedClasses/DataLists/WheelList.h);
// behaviour of the three reconstructed methods (Construct/Destruct/AddListResource)
// verified against the X360 pseudocode/asm
//   Construct        @ 0x82677DB8  (executed in the boot trace)
//   Destruct         @ 0x82677E30
//   AddListResource  @ 0x8267BFC0
// Every other method is declared-only and lands in its own TU.

#include "types.hpp"                                                        // s32, bool widths
#include "BrnCommonTypes.h"                                                 // CgsID (typedef u64)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"          // CgsResource::ResourcePtr<T>, BaseResourcePtr, ResourceHandle

namespace BrnResource
{

// WheelListResource is the element type of the static-data-list ResourcePtr array.
// It is NOT reconstructed in this TU; ResourcePtr<WheelListResource> embeds it by
// value but Construct/Destruct never dereference it, so a forward declaration
// suffices for them.
//
// AddListResource DOES need the wheel count of the resource being added (X360:
// the truncated accessor BrnResource::WheelListResource_::() returns a pointer
// whose first dword is the wheel count). To keep this TU minimal we model that
// single accessor below as GetNumWheels() rather than pull in the full (deferred)
// WheelListResource layout.
// FLAG: WheelListResource is modeled as a MINIMAL slice (count + entry-array base);
// the real class is reconstructed in its own TU and the exact accessor name is
// inferred from the asm (the symbol is truncated to BrnResource::WheelListResource_::()).
//
// LAYOUT (X360-proven, minimal): +0x00 muNumWheels (count, read by AddListResource),
// +0x04 mpaEntries (pointer to the WheelListEntry[] array; this is the field
// rebased by WheelListResourceType::FixUp @ 0x8267DF28 and dereferenced by
// WheelList::GetWheelData via `*(resource+4) + 72*entryIndex`). Only these two
// fields are attested here; the rest of the record is owned by the dedicated
// WheelListResource TU and intentionally not modeled.
struct WheelListEntry;

class WheelListResource
{
public:
    u32 GetNumWheels() const;   // X360: *BrnResource::WheelListResource_::(a2) -- count @+0x00

    // X360 (BrnResource::WheelList::GetWheelData @ 0x822CD3E8): the entry array base
    // is the pointer at +0x04 and the per-entry stride is 72 (sizeof WheelListEntry);
    // returns &mpaEntries[liEntryIndex]. Const accessor; the base is read directly.
    const WheelListEntry* GetEntry(s32 liEntryIndex) const;

private:
    u32                   muNumWheels;   // +0x00  wheel count
    const WheelListEntry* mpaEntries;    // +0x04  entry array base (FixUp-rebased)
};

// WheelListEntry -- one wheel record inside a WheelListResource. Owned here in the
// DataLists family (no other TU defines it). sizeof == 72 (0x48) is X360-proven by
// the GetWheelData @ 0x822CD3E8 stride (`base + 72*index`); the name C-string at
// +0x08 is proven by FindWheelIndexFromName @ 0x822CD4D8 (`stricmp(entry+8, name)`).
// FLAG: only the +0x08 name field and the 72-byte footprint are attested by the asm
// in this group's TUs. The leading 8 bytes (modeled as the wheel's CgsID hash, the
// canonical first member of these *Entry records -- cf. ChallengeListEntry) and the
// trailing bytes after the name are HONEST opaque padding sized to the proven 72-byte
// record; replace with the fully-decoded members when a WheelListEntry-detail TU lands.
struct WheelListEntry
{
    CgsID mID;                  // +0x00  wheel id/hash (modeled; sibling-pattern)
    char  macName[72 - 8];      // +0x08  name C-string (stricmp target); fills to sizeof 72
};

// WheelList.h:45 -- one per-wheel runtime slot. POD; DWARF offsets:
// mbBought @+0, miListIndex @+4, miEntryIndex @+8. Namespace-scope sibling of
// WheelList (DWARF qualifies it BrnResource::WheelSlot, not nested), exactly like
// ChallengeSlot.
struct WheelSlot
{
    bool mbBought;       // :47  @+0
    s32  miListIndex;    // :48  @+4
    s32  miEntryIndex;   // :49  @+8
};

// WheelList.h:61
class WheelList
{
public:
    static const s32 KI_MAX_WHEEL_LISTS = 32;  // :66
    static const s32 KI_MAX_WHEELS      = 256; // :67

    void Construct();   // :70  RECONSTRUCTED @0x82677DB8
    void Destruct();    // :73  RECONSTRUCTED @0x82677E30

    // :76  RECONSTRUCTED @0x8267BFC0
    void AddListResource(CgsResource::ResourcePtr<WheelListResource>& lrResource);

    // ---- declared-only (each reconstructed in its own TU) ----
    s32                    GetWheelCount() const;                       // :79
    const WheelListEntry*  GetWheelData(s32 liIndex) const;            // :82
    const WheelListEntry*  GetWheelData(CgsID lID) const;             // :85
    s32                    GetWheelIndex(CgsID lID) const;            // :88
    s32                    FindWheelIndexFromName(const char* lpcName) const; // :91
    bool                   IsWheelContentBought(s32 liIndex) const;    // :94
    bool                   IsWheelContentBought(CgsID lID) const;     // :97
    void                   SetWheelContentBought(s32 liIndex, bool lbBought); // :100
    void                   SetWheelContentBought(CgsID lID, bool lbBought);   // :103

private:
    // ---- layout (DWARF offsets, proven by X360 pseudocode) ----
    CgsResource::ResourcePtr<WheelListResource> maStaticDataLists[KI_MAX_WHEEL_LISTS]; // :106  @+0x000 (32-byte stride -> 0x400 total)
    WheelSlot                                   maSlots[KI_MAX_WHEELS];                 // :107  @+0x400 (12-byte stride)
    s32                                         miCount;                                // :108  @+0x1000 total wheels registered
    s32                                         miListCount;                            // :109  @+0x1004 num lists added (<= KI_MAX_WHEEL_LISTS)
};

} // namespace BrnResource

#endif // SHAREDCLASSES_DATALISTS_WHEELLIST_H
