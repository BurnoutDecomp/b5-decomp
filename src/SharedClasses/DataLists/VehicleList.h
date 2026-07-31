#ifndef SHAREDCLASSES_DATALISTS_VEHICLELIST_H
#define SHAREDCLASSES_DATALISTS_VEHICLELIST_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // CgsID
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::ResourcePtr<T>

// ============================================================================
// BrnResource::VehicleList -- the runtime aggregation of loaded vehicle-list
// resources plus the per-vehicle "slot" table that maps a global vehicle index
// onto (which loaded list, which entry inside it) and tracks whether the car's
// content has been bought. Direct structural sibling of BrnResource::WheelList
// and BrnResource::ChallengeList (same maStaticDataLists[]/maSlots[]/miCount/
// miListCount shape, same Construct/Destruct/AddListResource trio).
//
// GROWN 2026-07-31 from the earlier two-accessor minimal slice into the real
// class. Shape is the DecFIGS DWARF (SharedClasses/DataLists/VehicleList.h:76),
// and every offset it implies is independently confirmed by the X360 asm:
//   GetVehicleData @0x822187E0
//       `lwz r11, 0x404(this + 12*i)`      -> maSlots[i].miListIndex  @ 0x400+12i+4
//       `slwi r11,r11,5; add r3,r11,this`  -> maStaticDataLists[]     @ 0x000, stride 32
//       `lwz r10, 4(resource)`             -> VehicleListResource::entry-array slot @+4
//       `lwzx r11, 12*(i+0x56), this`      -> maSlots[i].miEntryIndex @ 0x400+12i+8
//       `mulli r11,r11,0xF0`               -> sizeof(VehicleListEntry) == 240
//   GetVehicleIndex @0x822188C8            -> the loop bound is read at +0x3400 == miCount
//   Construct @0x82677850                  -> 32 ResourcePtr resets (stride 32) then
//                                             1024 slot resets (stride 12) then FOUR
//                                             counters at +0x3400/04/08/0C
//   AddListResource @0x8267B158            -> `>= 32` list guard @+0x3404, `> 1024`
//                                             vehicle guard @+0x3400
// so maStaticDataLists[32] @0x000, maSlots[1024] @0x400, the four counters @0x3400.
// ============================================================================

namespace BrnResource
{
struct VehicleListEntry;   // SharedClasses/DataLists/VehicleListEntry.h (full home)

// The SERIALISED vehicle-list resource (CgsResource type 65541), i.e. the payload of
// VEHICLELIST.BUNDLE's `B5VehicleList` resource. Single home; VehicleListResourceType.cpp
// includes this header rather than re-declaring the record.
//
// LAYOUT (X360-proven + measured against the shipped bundle):
//   +0x00 u32 muNumVehicles   -- GetSerialisedResourceDescriptor @0x8267B540 multiplies it
//                                by 240 and adds the 16-byte header; AddListResource reads it
//   +0x04 u32 muEntriesOffset -- the entry-array base, stored in a THIRTY-TWO-BIT slot and
//                                rebased by VehicleListResourceType::FixUp (+= load base).
//                                Kept a u32 + PointerFromU32 deliberately: the shipped
//                                payload really is 4 bytes wide here (16-byte header +
//                                240*430 == 103216 == the measured payload size exactly),
//                                so widening it to a host pointer would desynchronise the
//                                struct from the data. Safe because the GameData root is
//                                carved below 4 GB (BrnGameDataModule Construct logs it).
//   +0x08 u64 -- the 16-byte header's tail padding.
class VehicleListResource
{
public:
    u32 GetNumVehicles() const { return muNumVehicles; }

    // &entries[liEntryIndex]; stride 240 (X360 GetVehicleData's `mulli r11,r11,0xF0`).
    const VehicleListEntry* GetEntry(s32 liEntryIndex) const;

private:
    friend class VehicleListResourceType;   // FixUp/FixDown rebase muEntriesOffset

    u32 muNumVehicles;      // +0x00
    u32 muEntriesOffset;    // +0x04  serialised 32-bit entry-array slot (FixUp-rebased)
    u64 muHeaderPad;        // +0x08  (the header is 16 bytes)
};

// VehicleList.h:45 (DWARF) -- one per-vehicle runtime slot. Namespace-scope sibling of
// VehicleList (the DWARF qualifies it BrnResource::VehicleSlot, not nested), exactly like
// WheelSlot/ChallengeSlot.
struct VehicleSlot
{
    bool mbBought;       // :47  @+0
    s32  miListIndex;    // :48  @+4
    s32  miEntryIndex;   // :49  @+8
};

// VehicleList.h:61 (DWARF) -- one entry of the sponsor/bonus-car table.
struct SponsorCarSlot
{
    CgsID mCarId;        // :63
    bool  mbAvailable;   // :64
};

// VehicleList.h:76 (DWARF)
// CLASS KEY: `struct`, matching the ten forward declarations across the tree
// (BrnGameStateModule.h, BrnCarSelectManager.h, BrnRaceCarEntityModule.h, ...).
// MSVC mangles the key, so a `class` definition against `struct` declarations makes
// two TUs emit different symbols for the same function -- it surfaced as an LNK2019
// on RaceCarStreamer::Prepare/GetVehicleList the moment two such TUs cross-called.
struct VehicleList
{
public:
    static const s32 KI_MAX_VEHICLE_LISTS = 32;    // :81
    static const s32 KI_MAX_VEHICLES      = 1024;  // :82
    static const u32 KI_SPONSOR_CAR_COUNT = 12;    // :84

    void Construct();   // :88  RECONSTRUCTED @0x82677850
    void Destruct();    // :91  RECONSTRUCTED @0x82677CC0

    // :94  RECONSTRUCTED @0x8267B158
    void AddListResource(CgsResource::ResourcePtr<VehicleListResource>& lrResource);

    s32 GetVehicleCount() const;                        // :97   RECONSTRUCTED
    s32 GetSelectableVehicleCount() const;              // :100  RECONSTRUCTED
    s32 GetSponsorVehicleCount() const;                 // :103  RECONSTRUCTED
    const VehicleListEntry* GetVehicleData(s32 liIndex) const;   // :106 RECONSTRUCTED @0x822187E0
    s32 GetVehicleIndex(CgsID lCarId) const;            // :112  RECONSTRUCTED @0x822188C8

    // ---- declared-only (each reconstructed in its own TU) ----
    const VehicleListEntry* GetVehicleData(CgsID lID) const;   // :109
    bool IsVehicleInList(CgsID lID) const;                     // :115
    bool IsVehicleSponsorVehicle(CgsID lID) const;             // :119
    bool IsVehicleContentBought(s32 liIndex) const;            // :122
    bool IsVehicleContentBought(CgsID lID) const;              // :125
    void SetVehicleContentBought(s32 liIndex, bool lbBought);  // :128
    void SetVehicleContentBought(CgsID lID, bool lbBought);    // :131

    // X360 0x82233A28 (unnamed export). Resolve a vehicle record straight from its car id --
    // GetVehicleData(GetVehicleIndex(lCarId)) -- returning nullptr when the id is not present.
    // This is the committed name for the DWARF's GetVehicleData(CgsID) overload at :109; kept
    // as its own declaration so the existing callers (BrnGui::LeaderboardTableComponent::SetCell)
    // are unaffected by the grow. Declaration-only.
    const VehicleListEntry* GetVehicleFromId(CgsID lCarId) const;

private:
    void SetSponserVehicleAvailable(CgsID lCarId);   // :136 [sic -- DWARF spelling]

    // ---- layout (DWARF offsets, every one confirmed by the X360 asm above) ----
    CgsResource::ResourcePtr<VehicleListResource> maStaticDataLists[KI_MAX_VEHICLE_LISTS]; // :138 @+0x0000
    VehicleSlot                                   maSlots[KI_MAX_VEHICLES];                // :139 @+0x0400
    s32                                           miCount;                                 // :140 @+0x3400
    s32                                           miListCount;                             // :141 @+0x3404
    s32                                           miSelectableVehicleCount;                // :144 @+0x3408
    s32                                           miSponsorVehicleCount;                   // :147 @+0x340C
};
}

#endif // SHAREDCLASSES_DATALISTS_VEHICLELIST_H
