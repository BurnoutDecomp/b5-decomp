// VehicleList_wS34_00.cpp -- VehicleList::GetVehicleFromId (VehicleList.cpp family),
// reconstructed from the console image.

#include "SharedClasses/DataLists/VehicleList.h"

namespace BrnResource
{
    // the vehicle record for a car id, or null when the id is not listed.
    const VehicleListEntry* VehicleList::GetVehicleFromId(CgsID lCarId) const
    {
        const s32 liVehicleIndex = GetVehicleIndex(lCarId);
        if (liVehicleIndex < 0)
            return 0;
        return GetVehicleData(liVehicleIndex);
    }
}
