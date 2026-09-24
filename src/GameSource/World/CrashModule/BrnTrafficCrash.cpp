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

    // Inlined ClearupCrashes827CE4E4..827CE518: postpone requested cleanup while visible/near.
    void TrafficCrash::MarkVehicleAsOnscreen()
    {
        CGS_ASSERT(WantsToBeClearedUp(), "WantsToBeClearedUp()");
        mfTimeTillClearup = 1.0f;
    }

    // DWARF BrnCrashModule.h:130, body BrnCrashModule.cpp:278 (crash parity FX-NETCRASH, G64-D2).
    // No out-of-line X360 symbol; CrashModule::HandleNetworkCrashingTraffic inlines it twice, once
    // on each arm of its new-crashing-traffic loop (0x827CC02C..0x827CC064 and 0x827CC248..
    // 0x827CC284):
    //   lbz 1 ; rlwinm 0,30,30 ; bne   else assert "IsUnconfirmedNetwork()" (:280, non-gating)
    //   stb leConfirmedOwner, 0        miOwner = the confirmed owner
    //   andi. 0xF9 ; ori 4 ; stb 1     mxFlags: drop "unconfirmed" (0x2) and "confirmed" (0x4),
    //                                  then set "confirmed" -- bit 0 (cleanup) is kept
    void TrafficCrash::ConfirmNetworkOwner(EActiveRaceCarIndex leConfirmedOwner)
    {
        CGS_ASSERT(IsUnconfirmedNetwork(), "IsUnconfirmedNetwork()");   // :280
        miOwner = static_cast<s8>(leConfirmedOwner);
        mxFlags = static_cast<u8>((mxFlags & 0xF9u) | 4u);
    }

    // DWARF BrnCrashModule.h:138, body BrnCrashModule.cpp:310 (crash parity FX-NETCRASH, G64-D2).
    // The network owner stopped sending this wreck: clear it up now. No out-of-line X360 symbol;
    // inlined by CrashModule::HandleNetworkCrashingTraffic's cleared-up loop (0x827CCAA0..
    // 0x827CCAE0):
    //   lbz 1 ; rlwinm 0,29,29 ; bne   else assert "IsConfirmedNetwork()" (:312, non-gating)
    //   stfs f31, 4                     mfTimeTillClearup = flt_820037C8 (x360rd 0xBF800000, -1.0)
    //   ori 1 ; stb 1                   mxFlags |= 1 (WantsToBeClearedUp)
    // Same two stores as OnOwnerDisconnected below, behind the confirmed-only tripwire.
    void TrafficCrash::SetNetworkVehicleClearedUp()
    {
        CGS_ASSERT(IsConfirmedNetwork(), "IsConfirmedNetwork()");   // :312
        mfTimeTillClearup = -1.0f;                                   // flt_820037C8
        mxFlags |= 1;
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
