// WheelList.cpp
// BrnResource::WheelList -- Construct / Destruct / AddListResource.
//
// Reconstructed from the X360 ARTIST build:
//   WheelList::Construct        @ 0x82677DB8  (executed in the boot trace)
//   WheelList::Destruct         @ 0x82677E30
//   WheelList::AddListResource  @ 0x8267BFC0
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

namespace BrnResource
{

namespace
{
    // X360: &dword_82FFB25C -- the invalid/default resource-handle sentinel used to
    // reset each ResourcePtr to "no resource". Modeled as a file-local default
    // (zero) ResourceHandle, matching the ChallengeList sibling's skInvalidHandle.
    // FLAG: the exact .rodata bytes of dword_82FFB25C are not recovered; an all-zero
    // / invalid handle is the modeled value.
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

} // namespace BrnResource
