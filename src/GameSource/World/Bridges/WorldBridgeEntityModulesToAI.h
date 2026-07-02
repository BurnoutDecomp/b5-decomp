#pragma once

#include "types.hpp"
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"                                        // AIModuleIO::InputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"       // RaceCarEntityModuleIO::OutputBuffer_PreScene

// WorldModule race-car -> AI bridge -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeEntityModulesToAI.h
//
// Per-frame: latch the race-car module's pre-scene AI view into the AI module's input
// buffer.
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827A4FA0; the module-IO buffer slices
// live at their own homes (see the includes). The leading lpWorldModule arg is the
// X360 r3 (the WorldModule context); the bridge never reads through it.
namespace WorldModule
{
    // @ 0x827A4FA0
    void BridgeRaceCarModuleToAIModule_PreScene(
        void* lpWorldModule,
        BrnAI::AIModuleIO::InputBuffer* lpAIInputBuffer,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene);
}
