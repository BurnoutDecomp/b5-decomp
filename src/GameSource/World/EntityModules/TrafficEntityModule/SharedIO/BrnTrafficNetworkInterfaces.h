#pragma once

// Traffic network IO event payloads. Reconstructed from the DecFIGS DWARF.
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"   // EActiveRaceCarIndex

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // Network event: activate a new collision hull for an active race car.
    struct ActivateHullEvent
    {
        EActiveRaceCarIndex meActiveRaceCarIndex;
        u16                 muNewActiveHull;
        u32                 muUpdateFrame;
    };
}
}
