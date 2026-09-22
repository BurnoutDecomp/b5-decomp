#include "GameSource/World/CrashModule/BrnTrafficCrash.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstddef>

namespace BrnWorld
{
    // ARTIST827B1608: owner r4, vehicle r5, float f1 (argument slot r6), bool r7.
    // DecFIGS confirms four explicit arguments; Hex-Rays invented a double and extra integer.
    void TrafficCrash::Construct(s32 liOwner, u16 luVehicleIndex, f32 lfTimeTillClearup, bool lbNetwork)
    {
        CGS_ASSERT(liOwner >= 0, "liOwner >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(liOwner < 8, "liOwner < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        miOwner = static_cast<s8>(liOwner);
        muVehicleIndex = luVehicleIndex;
        CGS_ASSERT(luVehicleIndex < 600u, "muVehicleIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");
        mfTimeTillClearup = lfTimeTillClearup;
        mxFlags = lbNetwork ? 2 : 0;
    }

    // Inlined ARTIST TickCrashes827C6798..827C67C4; DWARF TrafficCrash::Tick(float).
    void TrafficCrash::Tick(f32 lfTimeStep)
    {
        mfTimeTillClearup -= lfTimeStep;
        // fcmpu/bgt: unordered also takes the request path, as on the console.
        if (!(mxFlags & 1) && !(mfTimeTillClearup > 0.0f))
        {
            mxFlags |= 1;
            mfTimeTillClearup = 1.0f; // flt_82001C98: one grace period, never restarted here
        }
    }

    void TrafficCrash::OnOwnerDisconnected()
    {
        CGS_ASSERT((mxFlags & 4) != 0 || (mxFlags & 2) != 0,
                   "IsConfirmedNetwork() || IsUnconfirmedNetwork()");
        mfTimeTillClearup = -1.0f;
        mxFlags |= 1;
    }

    void TrafficCrash::_AssertLayout()
    {
        static_assert(offsetof(TrafficCrash, miOwner) == 0, "miOwner @0");
        static_assert(offsetof(TrafficCrash, mxFlags) == 1, "mxFlags @1");
        static_assert(offsetof(TrafficCrash, muVehicleIndex) == 2, "muVehicleIndex @2");
        static_assert(offsetof(TrafficCrash, mfTimeTillClearup) == 4, "mfTimeTillClearup @4");
        static_assert(sizeof(TrafficCrash) == 8, "TrafficCrash size 8");
    }
}
