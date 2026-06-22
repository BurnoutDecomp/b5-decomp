// Embed-check / compile tripwire for BrnTraffic::StaticTrafficParam (8 funcs).
// Pins the X360 byte layout by offset and forces every body to instantiate. Not a unit
// test -- a compile/link guard for this TU.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h"
#include <cstddef>

namespace
{
    using BrnTraffic::StaticTrafficParam;

    // Layout pins (asm STORE/LOAD offsets).
    static_assert(offsetof(StaticTrafficParam, muHull) == 0x00, "muHull @0x00 (lhz)");
    static_assert(offsetof(StaticTrafficParam, muStaticTrafficIndexOnHull) == 0x02, "index @0x02 (lbz)");
    static_assert(offsetof(StaticTrafficParam, mxFlags) == 0x03, "mxFlags @0x03 (flag byte)");
    static_assert(offsetof(StaticTrafficParam, muVehicleType) == 0x04, "muVehicleType @0x04");

    int ExerciseStaticTrafficParam()
    {
        StaticTrafficParam lParam;
        lParam.Construct();
        lParam.Initialise(7u, 42u, 3u);          // makes it alive (mxFlags = E_FLAG_ALIVE)

        const u16 luHull = lParam.GetHull();
        const u8  luIndex = lParam.GetIndexInHull();

        lParam.SetZombie();                       // requires alive && !shouldBeRemoved
        lParam.SetDivorced();                     // requires alive && !shouldBeRemoved
        lParam.SetShouldBeRemoved();              // requires alive && !zombie -> reset zombie first
        return static_cast<int>(luHull + luIndex);
    }
}

int g_BrnTrafficStaticParamEmbedCheck = ExerciseStaticTrafficParam();
