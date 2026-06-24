#include "GameSource/World/CrashModule/BrnTrafficCrash.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::TrafficCrash::Construct, reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   Construct @ 0x827B1608  (World/CrashModule/BrnCrashModule.cpp)
//
// Initialises one tracked crashing-traffic entry. The X360 body asserts three non-gating
// tripwires, then writes the four fields store-for-store:
//   bge  liOwner,0 / blt liOwner,8  -> CGS_ASSERT(0 <= liOwner < 8)  (BrnCrashModule.cpp:211/212)
//   cmplwi muVehicleIndex,0x258     -> CGS_ASSERT(muVehicleIndex < 600) (BrnCrashModule.cpp:216)
//   stb  liOwner,        0(this)    -> mliOwner       (byte @0)
//   sth  muVehicleIndex, 2(this)    -> muVehicleIndex (halfword @2)
//   stfs f31,            4(this)    -> mfStartTime    (the double ctor arg as f32, @4)
//   stb  0,              1(this)    -> meCrashState = 0   (byte @1)
//   if (lbFlag) stb 2,   1(this)    -> meCrashState = 2   (byte @1)
//
// Note the Hex-Rays render collapses all four stores onto `*(v8 + 1)` -- the ASSEMBLY store
// widths/offsets (stb @0, stb @1, sth @2, stfs @4) are authoritative. The fifth Hex-Rays arg
// (a5 / liUnused) is loaded into r28 but the body never stores it. The owner bound is the
// active-race-car index range (E_ACTIVE_RACE_CAR_INDEX_0 .. _COUNT == 0..8); the vehicle bound
// is BrnTraffic::KU_MAX_TOTAL_TRAFFIC == 600 (0x258). Called by
// CrashModule::HandleNetworkCrashingTraffic and CrashModule::AddCrashingTrafficVehicle.

namespace BrnWorld
{
    void TrafficCrash::Construct(s32 liOwner, u16 luVehicleIndex, f64 lfStartTime, s32 /*liUnused*/, bool lbFlag)
    {
        CGS_ASSERT(liOwner >= 0, "liOwner >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(liOwner < 8,  "liOwner < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        mliOwner       = static_cast<s8>(liOwner);   // stb @0
        muVehicleIndex = luVehicleIndex;             // sth @2

        CGS_ASSERT(luVehicleIndex < 0x258u, "muVehicleIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");

        mfStartTime  = static_cast<f32>(lfStartTime); // stfs @4
        meCrashState = 0;                             // stb @1
        if (lbFlag)
            meCrashState = 2;                         // stb @1
    }

    // Layout pins (X360 Construct store offsets/widths).
    void TrafficCrash::_AssertLayout()
    {
        static_assert(offsetof(TrafficCrash, mliOwner)       == 0, "mliOwner @0");
        static_assert(offsetof(TrafficCrash, meCrashState)   == 1, "meCrashState @1");
        static_assert(offsetof(TrafficCrash, muVehicleIndex) == 2, "muVehicleIndex @2");
        static_assert(offsetof(TrafficCrash, mfStartTime)    == 4, "mfStartTime @4");
        static_assert(sizeof(TrafficCrash) == 8, "TrafficCrash size 8");
    }
}
