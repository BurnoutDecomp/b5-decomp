// VehicleList.cpp
// BrnResource::VehicleList -- Construct / Destruct / AddListResource / GetVehicleData /
// GetVehicleCount / GetSelectableVehicleCount / GetSponsorVehicleCount / GetVehicleIndex,
// plus VehicleListResource::GetEntry.
//
// Reconstructed from the X360 ARTIST build:
//   VehicleList::Construct       @ 0x82677850
//   VehicleList::Destruct        @ 0x82677CC0
//   VehicleList::AddListResource @ 0x8267B158
//   VehicleList::GetVehicleData  @ 0x822187E0
//   VehicleList::GetVehicleIndex @ 0x822188C8
//   VehicleListResource::GetEntry (inlined into GetVehicleData as `*(res+4) + 240*idx`)
//   the KAPC_SPONSOR_CAR dynamic initialiser @ 0x82C60928 (12 CgsIDCompress stores into
//     the zero-initialised table at 0x82FFB290, stride 16)
//
// Direct structural sibling of BrnResource::WheelList; Construct/Destruct/AddListResource
// mirror that TU's shape exactly (re-rolled marching-pointer loops over named members --
// semantic parity). The X360 "return" of each of those is the last assignment result, a
// fastcall register artifact, not a real return value; all three are void.
//
// The X360 inlined BaseResourcePtr::CreateFromHandle(&maStaticDataLists[i], &sentinel) at
// each Construct/Destruct iteration; the DecFIGS DWARF renders the pre-inline call as
// ResourcePtr<VehicleListResource>::operator=(...), i.e. the source was
// `maStaticDataLists[i] = skInvalidHandle;`. That public assignment is the faithful
// source-level form and the exact observable operation; reconstructed as such.

#include "SharedClasses/DataLists/VehicleList.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"   // VehicleListEntry (complete)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // ResourceHandle by value
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                         // CgsIDCompress

namespace BrnResource
{

namespace
{
    // X360: &dword_82FFB25C -- the invalid/default resource-handle sentinel each slot is
    // reset to. Same sentinel the WheelList/ChallengeList siblings use; modelled here as a
    // file-local default-constructed (zero) ResourceHandle, which is that null handle's
    // value. (CgsResource::NULLResourcePtr is not yet a defined global in this port.)
    const CgsResource::ResourceHandle skInvalidHandle = {};

    // The serialised entry stride the X360 multiplies by (`mulli r11,r11,0xF0`).
    const u32 KU_VEHICLE_LIST_ENTRY_STRIDE = 240;

    // ---- KAPC_SPONSOR_CAR (DWARF VehicleList.h:85, SponsorCarSlot[12]) ------------------
    // The X360 table at 0x82FFB290 is ZERO in the image and filled by a dynamic initialiser
    // (@0x82C60928): twelve `mCarId = CgsIDCompress("<id>"); mbAvailable = false;` pairs, in
    // this order. Recovered verbatim from that initialiser -- eleven of the twelve ids also
    // appear in the shipped 430-entry vehicle list (CARBEAGT does not).
    //
    // [PC boot-safety deviation] the console runs the initialiser at static-init time; here
    // it is a function-local static so nothing calls CgsIDCompress before main (the project's
    // pre-main hazard: gGameModule constructs subsystems before gpDebugPrint exists).
    const u32 KU_SPONSOR_CAR_COUNT = VehicleList::KI_SPONSOR_CAR_COUNT;

    SponsorCarSlot* GetSponsorCarTable()
    {
        static SponsorCarSlot saSponsorCars[KU_SPONSOR_CAR_COUNT];
        static bool sbInitialised = false;
        if (!sbInitialised)
        {
            sbInitialised = true;
            static const char* const skapcSponsorCarIds[KU_SPONSOR_CAR_COUNT] =
            {
                "PSPBEST", "PSPBZ",  "PSPCIR",  "PSPGAS",
                "PSPT",    "PSPMETL", "PSPTS",  "PSPWAL",
                "PSPYODO", "PSPMICR", "PSPCHRO", "CARBEAGT"
            };
            for (u32 luCar = 0; luCar < KU_SPONSOR_CAR_COUNT; ++luCar)
            {
                saSponsorCars[luCar].mCarId      = CgsIDCompress(skapcSponsorCarIds[luCar]);
                saSponsorCars[luCar].mbAvailable = false;
            }
        }
        return saSponsorCars;
    }

    // AddListResource's sponsor test: linear scan of the 12-entry table for the car id
    // (X360 @0x8267B2xx -- stride 16, byte limit 0xC0 == 12*16).
    bool IsSponsorCarId(CgsID lCarId)
    {
        const SponsorCarSlot* lpTable = GetSponsorCarTable();
        for (u32 luCar = 0; luCar < KU_SPONSOR_CAR_COUNT; ++luCar)
        {
            if (lpTable[luCar].mCarId == lCarId)
                return true;
        }
        return false;
    }
}

// VehicleListResource::GetEntry -- inlined inside VehicleList::GetVehicleData on the X360
// (no standalone symbol; the body is the `*(resource+4) + 240*index` tail at
// 0x822188A4..0x822188BC). The entry-array base is the FixUp-rebased 32-bit slot at +0x04.
// No bounds check here -- the caller (GetVehicleData) owns the index assert.
const VehicleListEntry* VehicleListResource::GetEntry(s32 liEntryIndex) const
{
    const u8* lpBase = reinterpret_cast<const u8*>(static_cast<uintptr_t>(muEntriesOffset));
    return reinterpret_cast<const VehicleListEntry*>(
        lpBase + KU_VEHICLE_LIST_ENTRY_STRIDE * static_cast<u32>(liEntryIndex));
}

// VehicleList::Construct @ 0x82677850
void VehicleList::Construct()
{
    // X360: 32x reset of each static-data-list ResourcePtr to the invalid handle
    // (marching pointer v2 += 8 dwords == 32-byte ResourcePtr stride).
    for ( s32 liIndex = 0; liIndex < KI_MAX_VEHICLE_LISTS; ++liIndex )
    {
        maStaticDataLists[ liIndex ] = skInvalidHandle;
    }

    // X360: each slot's bought-flag <- 0, both indices <- -1 (marching pointer v4 walks the
    // 12-byte VehicleSlot stride over 1024 slots).
    for ( s32 liIndex = 0; liIndex < KI_MAX_VEHICLES; ++liIndex )
    {
        maSlots[ liIndex ].mbBought     = false;
        maSlots[ liIndex ].miListIndex  = -1;
        maSlots[ liIndex ].miEntryIndex = -1;
    }

    // X360: a1[3328..3331] = 0 -- the four counters at +0x3400/04/08/0C.
    miCount                  = 0;
    miListCount              = 0;
    miSelectableVehicleCount = 0;
    miSponsorVehicleCount    = 0;

    // Touch the sponsor table so its (console static-init) contents exist before any
    // AddListResource lookup. Cheap and keeps the lazy init off the streaming path.
    GetSponsorCarTable();

    // [FLAG PC leaf -- SKU sponsor-car availability, DELIBERATELY NOT RUN]
    // The X360 tail is `switch (CgsSystem::HardwareSku::GetSku())`, marking a per-territory
    // subset of KAPC_SPONSOR_CAR available via SetSponserVehicleAvailable:
    //     sku 0 : PSPBEST, PSPCIR, PSPGAS, PSPMETL, PSPWAL
    //     sku 1 : PSPMETL, PSPGAS
    //     sku 3 : PSPBZ, PSPMETL
    //     sku 6 : PSPBEST, PSPBZ, PSPCIR, PSPGAS, PSPMETL, PSPMICR, PSPWAL
    //     sku 2 / 4 / 5 : none
    //     default : assert "Unrecognised SKU code" (VehicleList.cpp:127)
    // CgsSystem::HardwareSku::GetSku() has NO PC leaf (CgsHardwareSkuPC.cpp defines only
    // FindLanguage), and picking a SKU number for the PC build would be inventing the
    // platform's identity. Every sponsor slot therefore stays mbAvailable == false, which is
    // exactly the sku 2/4/5 behaviour. miSponsorVehicleCount is UNAFFECTED (AddListResource
    // counts membership of the table, not availability). RESTORE this switch verbatim the
    // moment HardwareSku::GetSku() lands on PC.
}

// VehicleList::Destruct @ 0x82677CC0
void VehicleList::Destruct()
{
    // X360: 32x reset of each static-data-list ResourcePtr to the invalid handle.
    for ( s32 liIndex = 0; liIndex < KI_MAX_VEHICLE_LISTS; ++liIndex )
    {
        maStaticDataLists[ liIndex ] = skInvalidHandle;
    }
}

// VehicleList::SetSponserVehicleAvailable [sic -- DWARF spelling, VehicleList.h:136]
// Mark the sponsor slot whose car id matches as available. The X360 inlines this at each of
// the SKU switch's call sites (stride-16 scan of KAPC_SPONSOR_CAR, `if (slot.mCarId == id)
// slot.mbAvailable = 1;` with NO early-out).
void VehicleList::SetSponserVehicleAvailable(CgsID lCarId)
{
    SponsorCarSlot* lpTable = GetSponsorCarTable();
    for ( u32 luCar = 0; luCar < KI_SPONSOR_CAR_COUNT; ++luCar )
    {
        if ( lpTable[ luCar ].mCarId == lCarId )
        {
            lpTable[ luCar ].mbAvailable = true;
        }
    }
}

// VehicleList::AddListResource @ 0x8267B158
// Assert there is room for another list and for all of the resource's vehicles, store the
// resource in the next list slot, register each of its vehicles into the slot table (entry
// index = vehicle ordinal, list index = this list), and maintain the two derived counters.
void VehicleList::AddListResource(CgsResource::ResourcePtr<VehicleListResource>& lrResource)
{
    // X360 @0x8267B1F0: if (miListCount >= 32) fire assert (VehicleList.cpp:148).
    CGS_ASSERT(miListCount < KI_MAX_VEHICLE_LISTS, "No space for more vehicle lists\n");

    // X360 @0x8267B200: if (numVehicles + miCount > 1024) fire assert (VehicleList.cpp:149).
    const u32 luNumVehicles = lrResource->GetNumVehicles();
    CGS_ASSERT(static_cast<s32>(luNumVehicles) + miCount <= KI_MAX_VEHICLES,
               "Not enough space for that many more vehicles\n");

    // X360 @0x8267B268: CreateFromHandle(&maStaticDataLists[miListCount], a2 + 0x14) -- the
    // inlined ResourcePtr copy-assign (the {mpThis, muThreadId} pair at +0x14 IS the handle
    // CreateFromHandle stored there when the source was bound; see CgsResourcePtr.cpp).
    // Restored as the source-level operator=.
    maStaticDataLists[ miListCount ] = lrResource;

    // X360 @0x8267B294..0x8267B330: register each vehicle of the resource into the next free
    // slots, then maintain the two derived counters. Per the asm offsets the VEHICLE ORDINAL
    // goes into miEntryIndex (*(12*(miCount+86)+a1) == &maSlots[miCount].miEntryIndex, since
    // 12*(miCount+86) == 0x400 + 12*miCount + 8) and the LIST INDEX into miListIndex
    // (*(12*miCount + a1 + 0x404)). mbBought is left untouched (only Construct zeroes it).
    for ( u32 luVehicle = 0; luVehicle < luNumVehicles; ++luVehicle )
    {
        maSlots[ miCount ].miEntryIndex = static_cast<s32>(luVehicle);
        maSlots[ miCount ].miListIndex  = miListCount;

        // The X360 bumps miCount FIRST and then reads the vehicle back through
        // GetVehicleData(oldCount) -- which is why the bump has to precede the read (that
        // accessor asserts index < miCount).
        const s32 liVehicleIndex = miCount;
        ++miCount;

        const VehicleListEntry* lpEntry = GetVehicleData( liVehicleIndex );
        if ( lpEntry == 0 )
            continue;   // [marked deviation] see GetVehicleData's range guard

        // X360: `(*(entry+0x94) & 1) == 1` AND the +0xE9 livery-type byte is NOT one of
        // {1, 3, 4} -- i.e. the car is a car-select entry and not a derived livery variant.
        const u8 luLiveryType = lpEntry->GetLiveryType();
        const bool lbSelectable =
            lpEntry->IsTrophyCar() &&
            (luLiveryType != 1 && luLiveryType != 3 && luLiveryType != 4);

        if ( lbSelectable )
        {
            ++miSelectableVehicleCount;
            if ( IsSponsorCarId( GetVehicleData( liVehicleIndex )->GetId() ) )
            {
                ++miSponsorVehicleCount;
            }
        }
    }

    // X360 @0x8267B334: ++miListCount.
    ++miListCount;
}

// VehicleList::GetVehicleCount @ VehicleList.h:97 (inlined on the console; the count word
// GetVehicleIndex reads at +0x3400).
s32 VehicleList::GetVehicleCount() const
{
    return miCount;
}

// VehicleList.h:100 / :103 -- the two derived counters AddListResource maintains.
s32 VehicleList::GetSelectableVehicleCount() const
{
    return miSelectableVehicleCount;
}

s32 VehicleList::GetSponsorVehicleCount() const
{
    return miSponsorVehicleCount;
}

// VehicleList::GetVehicleData(s32) @ 0x822187E0
// Bounds-assert the vehicle index against miCount, then resolve the record: the slot names
// which loaded list owns the vehicle (miListIndex) and its ordinal within that list's
// resource (miEntryIndex); instance that list's resource and return its entry.
const VehicleListEntry* VehicleList::GetVehicleData(s32 liIndex) const
{
    // X360 @0x822187F4: if (liIndex < 0 || liIndex >= miCount) fire assert
    // "Index out of range\n" (VehicleList.h line 191).
    CGS_ASSERT(liIndex >= 0 && liIndex < miCount, "Index out of range\n");

    // [marked deviation] the console assert is log-and-continue and then indexes maSlots
    // anyway; on the PC host that is an out-of-bounds read of the module object. Guard.
    if (liIndex < 0 || liIndex >= miCount)
        return 0;

    const VehicleSlot& lrSlot = maSlots[ liIndex ];
    if (lrSlot.miListIndex < 0 || lrSlot.miListIndex >= KI_MAX_VEHICLE_LISTS)
        return 0;   // [marked deviation] unregistered slot (Construct seeds -1)

    return maStaticDataLists[ lrSlot.miListIndex ]->GetEntry( lrSlot.miEntryIndex );
}

// VehicleList::GetVehicleIndex(CgsID) @ 0x822188C8
// Linear scan over the registered vehicles for the entry whose car id matches lCarId;
// returns its index, or -1 when not present. Structurally identical to the committed
// sibling ChallengeList::GetChallengeIndex @0x82326168.
//
// asm notes:
//   * The loop bound is read at VehicleList+0x3400 (lwz r11,0x3400(r30)) both before the
//     loop and each iteration -- i.e. GetVehicleCount().
//   * The compare is `ld r11,0(r3); cmpld cr6,r11,r29` -- an 8-byte (CgsID) load of the
//     entry's leading id at entry+0x00 (== VehicleListEntry::GetId()) against the argument.
//     The Hex-Rays `GetVehicleData(this,i)[1].field_0` is a mis-sized-struct artifact.
s32 VehicleList::GetVehicleIndex( CgsID lCarId ) const
{
    const s32 liCount = GetVehicleCount();

    for ( s32 liIndex = 0; liIndex < liCount; ++liIndex )
    {
        const VehicleListEntry* lpEntry = GetVehicleData( liIndex );
        if ( lpEntry != 0 && lpEntry->GetId() == lCarId )
        {
            return liIndex;
        }
    }

    return -1;
}

// X360 0x82233A28 (exported unnamed; the DWARF's GetVehicleData(CgsID) overload at
// VehicleList.h:109). Literally `i = GetVehicleIndex(id); return i < 0 ? NULL :
// GetVehicleData(i);`. Both GameDataModule vehicle handlers resolve their request id
// through this; ProcessLoadVehicleRequest's asm shows the same pair not inlined.
const VehicleListEntry* VehicleList::GetVehicleData( CgsID lID ) const
{
    const s32 liIndex = GetVehicleIndex( lID );
    if ( liIndex < 0 )
    {
        return 0;
    }
    return GetVehicleData( liIndex );
}

} // namespace BrnResource
