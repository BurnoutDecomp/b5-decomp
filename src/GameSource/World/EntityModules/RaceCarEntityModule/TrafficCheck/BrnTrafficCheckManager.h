// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/TrafficCheck/BrnTrafficCheckManager.h
//
// Canonical (DWARF) home for BrnWorld::TrafficCheckManager (DWARF home
// BrnTrafficCheckManager.h:44). MINIMAL-COMPLETE slice: it homes the two data members
// the X360 Update body touches and declares the lifecycle/Update entry points. The
// remaining lifecycle bodies (Construct/Prepare/Release/Destruct) are declared-only here
// and bodied by their own TUs if/when they land.
//
// LAYOUT (DWARF :73/:74 + X360 Update @0x822F8F20, authoritative):
//   +0  s32  miCurrentCheckChain      (stw/lwz 0(r31); the "current chain" counter)
//   +4  f32  mfTimeSinceLastCheck     (lfs/stfs 4(r31); seconds since the last check)
// => sizeof 8. The X360 Update treats +0 as a 32-bit int and +4 as an IEEE f32, matching
// the DWARF member types.
#pragma once

#include "types.hpp"   // s32, f32, bool

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<1536,16>

namespace BrnWorld
{
    // BrnTrafficCheckManager.h:44 -- per-frame "is the local player meeting traffic-check
    // criteria" accumulator. Update consumes a 1536-byte input event queue, tracks a
    // current-chain counter and a time-since-last-check timer, and publishes a chain event
    // to the output queue when the dry-spell stays under the cap.
    class TrafficCheckManager
    {
    public:
        // The input/output event queues are the shared 1536-byte / 16-align variable event
        // queue instantiation (CgsModule::VariableEventQueue<1536,16>). The X360 Update calls
        // GetFirstEvent/GetNextEvent on the input and AddEvent on the output by that name.
        typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;

        // BrnTrafficCheckManager.h:49 / :53 / :57 / :61 -- declared-only (own TUs).
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();

        // BrnTrafficCheckManager.h:69 -- this TU. @ X360 0x822F8F20.
        // Accumulates lfTimeStep into the dry-spell timer; while the local player is
        // crashing it pins the timer to the cap and zeroes the chain. Otherwise it walks the
        // input queue, and for every event of type KI_TRAFFIC_CHECK_PASSED (73) it bumps the
        // chain and resets the timer. If the dry-spell timer is still under the cap it
        // publishes the current chain count to the output as a KI_TRAFFIC_CHECK_CHAIN (74)
        // event; otherwise it zeroes the chain.
        void Update(const GameEventQueue* lpInput,
                    GameEventQueue* lpOutput,
                    bool lbLocalPlayerCrashing,
                    f32 lfTimeStep);

        static void _AssertLayout();

    private:
        s32 miCurrentCheckChain;    // :73  (+0)
        f32 mfTimeSinceLastCheck;   // :74  (+4)
    };
}
