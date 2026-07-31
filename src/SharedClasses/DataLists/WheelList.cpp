// WheelList.cpp
// BrnResource::WheelList -- Construct / Destruct / AddListResource / GetWheelData /
// FindWheelIndexFromName.
//
// Reconstructed from the X360 ARTIST build:
//   WheelList::Construct              @ 0x82677DB8  (executed in the boot trace)
//   WheelList::Destruct               @ 0x82677E30
//   WheelList::AddListResource        @ 0x8267BFC0
//   WheelList::GetWheelData(s32)      @ 0x822CD3E8
//   WheelList::FindWheelIndexFromName @ 0x822CD4D8
//   WheelListResource::GetEntry       (inlined as `*(resource+4) + 72*idx`)
//
// Direct structural sibling of BrnResource::ChallengeList; Construct/Destruct mirror
// that TU exactly (re-rolled marching-pointer loops over named members -- semantic
// parity). The X360 "return" of each function is the last assignment result -- a
// fastcall register artifact, not a real return value; all three methods are void.
//
// The X360 inlined BaseResourcePtr::CreateFromHandle(&maStaticDataLists[i],
// &sentinel) at each Construct/Destruct iteration; the DecFIGS DWARF renders the
// pre-inline call as ResourcePtr<WheelListResource>::operator=(...), i.e. the source
// was `maStaticDataLists[i] = skInvalidHandle;` (assign-from-ResourceHandle, which
// resets the ResourcePtr). That public assignment is the faithful source-level form
// and the exact observable operation; reconstructed as such rather than calling the
// protected CreateFromHandle.

#include "SharedClasses/DataLists/WheelList.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle (used by value below)
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include <string.h>                                                    // _stricmp

namespace BrnResource
{

namespace
{
    // X360: &dword_82FFB25C -- the invalid/default resource-handle sentinel used to
    // reset each ResourcePtr to "no resource". PS3 DecFIGS resolves this sentinel:
    // WheelList::Construct/Destruct (0x811CA4 / 0x811B9C) call
    // BaseResourcePtr::CreateFromHandle(slot, &CgsResource::NULLResourcePtr.mHandle),
    // i.e. the X360 dword_82FFB25C IS CgsResource::NULLResourcePtr.mHandle -- the
    // engine's canonical null/invalid ResourceHandle (same sentinel the ChallengeList
    // sibling uses). Modeled here as a file-local default-constructed (zero)
    // ResourceHandle, which is that null handle's value. (CgsResource::NULLResourcePtr
    // is not yet a defined global in this port; reference it directly once it lands.)
    const CgsResource::ResourceHandle skInvalidHandle = {};
}

// WheelList::Construct @ 0x82677DB8
void WheelList::Construct()
{
    // X360: 32x reset of each static-data-list ResourcePtr to the invalid handle
    // (marching pointer v2 += 8 dwords == 32-byte ResourcePtr stride).
    for ( s32 liIndex = 0; liIndex < KI_MAX_WHEEL_LISTS; ++liIndex )
    {
        maStaticDataLists[ liIndex ] = skInvalidHandle;
    }

    // X360: each slot's bought-flag <- 0, both indices <- -1 (marching pointer v5
    // walks from the middle: *(v5-4)=mbBought, *v5=miListIndex, v5[1]=miEntryIndex,
    // stride 3 dwords == 12-byte WheelSlot stride).
    for ( s32 liIndex = 0; liIndex < KI_MAX_WHEELS; ++liIndex )
    {
        maSlots[ liIndex ].mbBought     = false;
        maSlots[ liIndex ].miListIndex  = -1;
        maSlots[ liIndex ].miEntryIndex = -1;
    }

    // X360: a1[1024] = 0; a1[1025] = 0;  (miCount @+0x1000, miListCount @+0x1004)
    miCount     = 0;
    miListCount = 0;
}

// WheelList::Destruct @ 0x82677E30
void WheelList::Destruct()
{
    // X360: 32x reset of each static-data-list ResourcePtr to the invalid handle
    // (marching pointer a1 += 32 bytes per iteration).
    for ( s32 liIndex = 0; liIndex < KI_MAX_WHEEL_LISTS; ++liIndex )
    {
        maStaticDataLists[ liIndex ] = skInvalidHandle;
    }
}

// WheelList::AddListResource @ 0x8267BFC0
// Assert there is room for another list and for all of the resource's wheels, store
// the resource in the next list slot, register each of its wheels into the slot
// table (entry index = wheel ordinal, list index = this list), and advance the
// counts.
void WheelList::AddListResource(CgsResource::ResourcePtr<WheelListResource>& lrResource)
{
    // X360 @0x8267BFE0: if (miListCount >= 32) fire assert (line 63).
    CGS_ASSERT(miListCount < KI_MAX_WHEEL_LISTS, "No space for more wheel lists\n");

    // X360 @0x8267C064: numWheels = *WheelListResource_(a2); if (numWheels + miCount > 256)
    // fire assert (line 64).
    const u32 luNumWheels = lrResource->GetNumWheels();
    CGS_ASSERT(static_cast<s32>(luNumWheels) + miCount <= KI_MAX_WHEELS,
               "Not enough space for that many more wheels\n");

    // X360 @0x8267C0D4: CreateFromHandle(&maStaticDataLists[miListCount], a2 + 0x14) --
    // the inlined form of the public ResourcePtr assignment (a2+0x14 is the handle
    // inside the passed ResourcePtr). Restored as operator= (DWARF shows
    // ResourcePtr::operator=), the faithful source-level write.
    maStaticDataLists[ miListCount ] = lrResource;

    // X360 @0x8267C0F8..0x8267C14C: register each wheel of the resource into the next
    // free slots. Per the asm offsets the WHEEL ORDINAL goes into miEntryIndex
    // (*(12*(miCount+86)+a1) == &maSlots[miCount].miEntryIndex, since
    // 12*(miCount+86) = 0x400 + 12*miCount + 8) and the LIST INDEX goes into
    // miListIndex (*(12*miCount + a1 + 0x404) == &maSlots[miCount].miListIndex, since
    // 0x404 = 0x400 + 4). mbBought is left untouched here (only Construct zeroes it).
    for ( u32 luWheel = 0; luWheel < luNumWheels; ++luWheel )
    {
        maSlots[ miCount ].miEntryIndex = static_cast<s32>(luWheel);
        maSlots[ miCount ].miListIndex  = miListCount;
        ++miCount;
    }

    // X360 @0x8267C150: ++miListCount.
    ++miListCount;
}

// WheelListResource::GetNumWheels -- the count word at +0x00 (X360 reads it directly
// inside AddListResource through the truncated accessor BrnResource::WheelListResource_::()).
u32 WheelListResource::GetNumWheels() const
{
    return muNumWheels;
}

// WheelListResource::GetEntry -- inlined inside WheelList::GetWheelData on X360
// (no standalone symbol; the body is the `*(resource+4) + 72*index` tail at
// 0x822CD4A8..0x822CD4CC). The entry array base is the serialised 32-bit slot at +0x04
// (FixUp-rebased) and the per-entry stride is sizeof(WheelListEntry) == 72. No bounds
// check here -- the caller (GetWheelData) is the one that asserts the slot index range.
const WheelListEntry* WheelListResource::GetEntry(s32 liEntryIndex) const
{
    const u8* lpBase = reinterpret_cast<const u8*>(static_cast<uintptr_t>(muEntriesOffset));
    return reinterpret_cast<const WheelListEntry*>(
        lpBase + sizeof(WheelListEntry) * static_cast<u32>(liEntryIndex));
}

// WheelList::GetWheelCount @ WheelList.h:79 (inlined on the console; the count word at
// +0x1000 that GetWheelData/FindWheelIndexFromName bound their loops with).
s32 WheelList::GetWheelCount() const
{
    return miCount;
}

// WheelList::GetWheelData(s32) @ 0x822CD3E8
// Bounds-assert the wheel index against miCount, then resolve the wheel record:
// look up the slot's owning list + entry ordinal, instance that list's resource,
// and return the entry at that ordinal.
const WheelListEntry* WheelList::GetWheelData(s32 liIndex) const
{
    // X360 @0x822CD3FC..0x822CD40C: if (liIndex < 0 || liIndex >= miCount) fire
    // assert "Index out of range" (WheelList.h line 141). The rich stream message is
    // the file-local debug builder; the stringized-condition CGS_ASSERT carries the
    // same guard.
    CGS_ASSERT(liIndex >= 0 && liIndex < miCount, "liIndex >= 0 && liIndex < miCount");

    // X360 @0x822CD488..0x822CD4CC: the slot at liIndex names which static data list
    // (miListIndex) owns the wheel and the wheel's ordinal within that list's
    // resource (miEntryIndex). Instance that list's resource (the WheelListResou
    // accessor == ResourcePtr<WheelListResource>::operator-> reading offset 0) and
    // return its entry at miEntryIndex (stride 72).
    const WheelSlot& lrSlot = maSlots[ liIndex ];
    return maStaticDataLists[ lrSlot.miListIndex ]->GetEntry( lrSlot.miEntryIndex );
}

// WheelList::FindWheelIndexFromName @ 0x822CD4D8
// Linear scan of all registered wheels; return the index of the first whose name
// matches lpcName case-insensitively, or -1 if none.
s32 WheelList::FindWheelIndexFromName(const char* lpcName) const
{
    // X360 @0x822CD4F4: loop liIndex 0..miCount (the count at +0x1000). For each,
    // GetWheelData(liIndex) then _stricmp(entry->macName, lpcName) -- the name field
    // sits at +0x08 of the WheelListEntry (asm `addi r3, r3, 8` before the compare).
    // The early `if (miCount <= 0) return -1` is the loop guard.
    for ( s32 liIndex = 0; liIndex < miCount; ++liIndex )
    {
        const WheelListEntry* lpEntry = GetWheelData( liIndex );
        if ( _stricmp( lpEntry->macName, lpcName ) == 0 )
        {
            return liIndex;
        }
    }

    return -1;
}

} // namespace BrnResource
