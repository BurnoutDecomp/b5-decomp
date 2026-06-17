#pragma once

// BrnPhysics::Vehicle::E_DRIVER_TYPE -- which controller drives a vehicle.
// Recovered from references/DecFIGS/dwarfdump/.../SharedIO/BrnVehicleDriverControls.h
// (enum at line :40). The BrnPlayerDriverControls / BrnNetworkDriverControls /
// BrnAIDriverControls / BrnTrafficDriverControls classes that also live in this header are
// separate future TUs; only the enum (which RaceCarState::meDriverType needs) is here.
#include "types.hpp"   // s32

namespace BrnPhysics
{
namespace Vehicle
{
    enum E_DRIVER_TYPE : s32
    {
        E_DRIVER_TYPE_PLAYER       = 0,
        E_DRIVER_TYPE_AI           = 1,
        E_DRIVER_TYPE_NETWORK      = 2,
        E_DRIVER_TYPE_TRAFFIC      = 3,
        E_NUM_E_DRIVER_TYPE_EVENTS = 4
    };
}
}
