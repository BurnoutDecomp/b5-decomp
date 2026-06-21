#include "GameSource/Director/Utils/BrnVehicleRef.h"

// Reconstructed BrnDirector::VehicleRef::Construct.
//
// Behaviour-faithful:
//     *(result + 12) = 0;
//     return result;
//
// Zeroes the single owned reference at +0x0C and returns `this`.

namespace BrnDirector
{
    VehicleRef* VehicleRef::Construct()
    {
        mpRef = nullptr;
        return this;
    }
}
