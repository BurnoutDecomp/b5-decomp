#pragma once

#include "types.hpp"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"                            // CrashIO::InputBuffer_PreScene
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"       // RaceCarEntityModuleIO::OutputBuffer_PreScene

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
}
