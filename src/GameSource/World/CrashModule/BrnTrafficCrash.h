#pragma once

// Canonical (DWARF) home for BrnWorld::TrafficCrash (World/CrashModule/BrnCrashModule.cpp).
// A TrafficCrash records one crashing-traffic entry the crash module tracks per active race
// car: who owns the crash (the active-race-car slot), which traffic vehicle is crashing, when
// it started, and the crash's state. Member layout is pinned store-for-store by the X360
// Construct @ 0x827B1608 (see BrnTrafficCrash.cpp); field *meanings* come from that Construct's
// asserts ("liOwner >= E_ACTIVE_RACE_CAR_INDEX_0", "liOwner < E_ACTIVE_RACE_CAR_INDEX_COUNT",
// "muVehicleIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC" against BrnCrashModule.cpp:211/212/216).
//
// LAYOUT (X360 Construct store widths/offsets, authoritative):
//   +0x00  s8   mliOwner        (stb @0 -- active-race-car index, 0..E_ACTIVE_RACE_CAR_INDEX_COUNT)
//   +0x01  s8   meCrashState    (stb @1 -- 0 by default, 2 when the bool ctor arg is set)
//   +0x02  u16  muVehicleIndex  (sth @2 -- traffic vehicle index, < BrnTraffic::KU_MAX_TOTAL_TRAFFIC)
//   +0x04  f32  mfStartTime     (stfs @4 -- the ctor's double arg stored as float)
// => sizeof 8 (f32 @4 naturally ends at +8; 4-byte alignment from the u16/f32 fields).
#include "types.hpp"   // s8, u16, f32

namespace BrnWorld
{
    // BrnCrashModule.cpp -- one tracked crashing-traffic entry.
    struct TrafficCrash
    {
        // X360 0x827B1608. liOwner = active-race-car slot, luVehicleIndex = crashing traffic
        // vehicle, lfStartTime = the crash start time (ctor double arg, stored as f32),
        // lbFlag selects meCrashState (false -> 0, true -> 2). The fifth Hex-Rays arg (a5) is
        // unused by the body. Asserts the owner/vehicle-index bounds (non-gating tripwires).
        void Construct(s32 liOwner, u16 luVehicleIndex, f64 lfStartTime, s32 liUnused, bool lbFlag);

        // Layout pins (X360 Construct store offsets/widths); defined in BrnTrafficCrash.cpp.
        static void _AssertLayout();

    private:
        s8  mliOwner;         // +0x00
        s8  meCrashState;     // +0x01  (0 or 2 -- see header comment; interior enum opaque)
        u16 muVehicleIndex;   // +0x02
        f32 mfStartTime;      // +0x04
    };
}
