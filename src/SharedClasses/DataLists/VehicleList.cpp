// VehicleList.cpp
// BrnResource::VehicleList -- GetVehicleIndex.
//
// Reconstructed from the X360 ARTIST build:
//   VehicleList::GetVehicleIndex @ 0x822188C8
//
// Linear scan over the vehicle list for the entry whose car id matches lCarId;
// returns its index, or -1 when not present. Structurally identical to the
// committed sibling ChallengeList::GetChallengeIndex @0x82326168.
//
// asm notes:
//   * The loop bound is read at VehicleList+0x3400 (lwz r11,0x3400(r30)) both
//     before the loop and each iteration -- i.e. GetVehicleCount(), the +0x3400
//     count accessor (NOT the offset-0 miCount slice-member; VehicleList.h flags
//     that the two count words differ).
//   * The compare is `ld r11,0(r3); cmpld cr6,r11,r29` -- an 8-byte (CgsID/u64)
//     load of the entry's leading id at entry+0x00 (== VehicleListEntry::GetId())
//     against the argument. The Hex-Rays `GetVehicleData(this,i)[1].field_0` is a
//     mis-sized-struct artifact; the real access is the entry's GetId().

#include "SharedClasses/DataLists/VehicleList.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"   // VehicleListEntry (complete: GetId())

namespace BrnResource
{

// VehicleList::GetVehicleIndex(CgsID) @ 0x822188C8
s32 VehicleList::GetVehicleIndex( CgsID lCarId ) const
{
    const s32 liCount = GetVehicleCount();

    for ( s32 liIndex = 0; liIndex < liCount; ++liIndex )
    {
        if ( GetVehicleData( liIndex )->GetId() == lCarId )
        {
            return liIndex;
        }
    }

    return -1;
}

} // namespace BrnResource
