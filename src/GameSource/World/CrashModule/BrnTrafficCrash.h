#pragma once
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"

namespace BrnWorld
{
    // DWARF BrnCrashModule.h:107. ARTIST Construct827B1608 and the inlined
    // Tick827C6798..827C67C4 pin this eight-byte traffic crash record.
    struct TrafficCrash
    {
        void Construct(s32 liOwner, u16 luVehicleIndex, f32 lfTimeTillClearup, bool lbNetwork);
        void Tick(f32 lfTimeStep);
        static void _AssertLayout();
        EActiveRaceCarIndex GetOwner() const { return static_cast<EActiveRaceCarIndex>(miOwner); }
        u32 GetVehicleIndex() const { return muVehicleIndex; }
        void OnOwnerDisconnected(); // inlined ARTIST827CD0A8..827CD0F0

    private:
        s8 miOwner;             // +0
        u8 mxFlags;             // +1: bit0 cleanup requested, bit1 unconfirmed network, bit2 confirmed
        u16 muVehicleIndex;     // +2
        f32 mfTimeTillClearup;  // +4: countdown, not a start timestamp
    };
}
