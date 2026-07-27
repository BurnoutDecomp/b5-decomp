#pragma once

#include "types.hpp"
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"                                        // AIModuleIO::InputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"       // RaceCarEntityModuleIO::OutputBuffer_PreScene / _PostScene
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"       // BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene

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

    // ---- ADDITIVE (world-drive wave 2026-07-27; same X360 TU -- the AI input
    //      staging block of WorldModule::Update @0x827D63E8) ------------------

    // @ 0x827AD688 (WorldBridgeEntityModulesToAI.cpp:60) -- append the race-car
    // post-scene output's AI-module request interface into the AI input buffer.
    void BridgeRaceCarModuleToAIModule_PostScene(
        void* lpWorldModule,
        BrnAI::AIModuleIO::InputBuffer* lpAIInputBuffer,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene* lpRaceCarOutputBuffer_PostScene);

    // @ 0x827A5020 -- latch the traffic post-scene output's traffic-AI interface
    // into the AI input buffer.
    void BridgeTrafficModuleToAIModule_Update(
        void* lpWorldModule,
        BrnAI::AIModuleIO::InputBuffer* lpAIInputBuffer,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutputBuffer_PostScene);
}
