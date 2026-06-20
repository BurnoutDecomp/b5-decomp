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
// FLAG: WheelListResource is modeled as a MINIMAL slice (only GetNumWheels()); the
// real class is reconstructed in its own TU and the exact accessor name is inferred
// from the asm (the symbol is truncated to BrnResource::WheelListResource_::()).
class WheelListResource
{
public:
    u32 GetNumWheels() const;   // X360: *BrnResource::WheelListResource_::(a2) -- count read
};

// WheelListEntry is referenced only by the declared-only accessors below (as a
// pointer return type). Forward-declared (its full layout lives in its own
// committed header, intentionally not pulled in here to keep this TU minimal).
struct WheelListEntry;

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
