#ifndef SHAREDCLASSES_DATALISTS_VEHICLELIST_H
#define SHAREDCLASSES_DATALISTS_VEHICLELIST_H

#include "types.hpp"
#include "BrnCommonTypes.h"                              // CgsID

// ============================================================================
// MINIMAL FLAGGED SLICE for BrnResource::VehicleList.
//
// The serialised vehicle-list resource: a count of vehicles plus a remap/index table onto the
// embedded VehicleListEntry[] records. Callers (BrnDriveThruManager::HandleDriveThru, CarSelect)
// resolve a car by id through GetVehicleIndex() and fetch the VehicleListEntry through
// GetVehicleData(). Only those two accessors + the leading miCount are reconstructed here; the
// full record/remap-table layout (X360 GetVehicleData @0x822187E0 walks a `gap24`+`gap408`
// 12-byte-stride index table into a 240-byte VehicleListEntry array via a per-slot
// VehicleListResourceType pointer) is owned by the VehicleList TU.
//
// FLAG: this is a minimal slice. `miCount` is the only member modelled (it is the X360
// `this->miCount` GetVehicleIndex/GetVehicleData read); the remaining table members are NOT laid
// out here. GROW this into the real DWARF layout when the VehicleList TU lands; do NOT fork it.
// ============================================================================

namespace BrnResource
{
struct VehicleListEntry;   // SharedClasses/DataLists/VehicleListEntry.h (full home)

struct VehicleList
{
    // X360 0x822188C8. Linear search for the vehicle whose model id matches lCarId; returns its
    // index into the list, or -1 when not present. Declaration-only (body in the VehicleList TU).
    s32 GetVehicleIndex(CgsID lCarId) const;

    // X360 0x822187E0. Return the VehicleListEntry record at liIndex (asserts liIndex in range with
    // the "Index out of range" / VehicleList.h:191 assert). Declaration-only (body in the
    // VehicleList TU). FLAG: the entry resolution walks a remap table not modelled in this slice.
    const VehicleListEntry* GetVehicleData(s32 liIndex) const;

    // X360 GetVehicleIndex/GetVehicleData read `this->miCount`. FLAG: leading-member offset assumed
    // 0; the real preamble (resource header) lands with the full TU.
    s32 miCount;
};
}

#endif // SHAREDCLASSES_DATALISTS_VEHICLELIST_H
