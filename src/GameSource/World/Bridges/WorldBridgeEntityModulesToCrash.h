#pragma once

#include "types.hpp"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"                            // CrashIO::InputBuffer_PreScene
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"       // RaceCarEntityModuleIO::OutputBuffer_PreScene
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"       // BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics

// WorldModule race-car -> crash bridge -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeEntityModulesToCrash.h
//
// Per-frame: latch the race-car module's pre-scene active-car view into the crash
// module's input buffer.
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827A5060; the module-IO buffer slices
// live at their own homes (see the includes). The leading lpWorldModule arg is the
// X360 r3 (the WorldModule context); the bridge never reads through it.
namespace WorldModule
{
    // @ 0x827A5060
    void BridgeEntityModulesToCrashModule_PreScene(
        void* lpWorldModule,
        BrnWorld::CrashIO::InputBuffer_PreScene* lpCrashInputBuffer_PreScene,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene);

    // ---- ADDITIVE (world-drive wave 2026-07-27; same X360 TU) --------------
    // @ 0x827AD708 -- latch the traffic module's post-physics crash view into
    // the crash module's post-physics input buffer (the crash leg of
    // WorldModule::EntityModulePostPhysicsUpdate @0x827D3F10).
    void BridgeTrafficToCrashModule_PostPhysics(
        void* lpWorldModule,
        BrnWorld::CrashIO::InputBuffer_PostPhysics* lpCrashInputBuffer_PostPhysics,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* lpTrafficOutputBuffer_PostPhysics);
}
