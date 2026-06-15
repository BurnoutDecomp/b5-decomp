#pragma once

// Crash-module network IO event payloads. Reconstructed from the DecFIGS DWARF.
#include "BrnCommonTypes.h"   // Matrix44Affine

namespace BrnWorld
{
namespace CrashIO
{
    // Network event: a crashing traffic vehicle's updated transform.
    struct alignas(16) CrashingTrafficUpdateEvent
    {
        u16            muVehicleId;
        Matrix44Affine mTransform;
    };
}
}
