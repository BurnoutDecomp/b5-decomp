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
        bool WantsToBeClearedUp() const { return (mxFlags & 1) != 0; }
        bool IsAllowedToBeClearedUp() const { return WantsToBeClearedUp() && mfTimeTillClearup <= 0.0f; }
        void MarkVehicleAsOnscreen();
        void OnOwnerDisconnected(); // inlined ARTIST827CD0A8..827CD0F0
        // DWARF BrnCrashModule.h:150 (body BrnCrashModule.cpp:361): flag bit 0x4 (`lbz 1 ;
        // rlwinm 0,29,29` at OnContactFromNetworkPlayer 0x827C642C).
        bool IsConfirmedNetwork() const { return (mxFlags & 4) != 0; }
        // DWARF BrnCrashModule.h:134 (body BrnCrashModule.cpp:295). Inlined into
        // CrashModule::OnContactFromNetworkPlayer @0x827C6444..0x827C6478: assert
        // IsConfirmedNetwork() (:297) then store KF_NETWORK_CRASH_TIMEOUT (20.0f) at +4.
        // Body in BrnCrashModule_RaceCarCrashes.cpp.
        void ResetNetworkTimeout();

    private:
        s8 miOwner;             // +0
        u8 mxFlags;             // +1: bit0 cleanup requested, bit1 unconfirmed network, bit2 confirmed
        u16 muVehicleIndex;     // +2
        f32 mfTimeTillClearup;  // +4: countdown, not a start timestamp
    };
}
