#pragma once

#include "types.hpp"
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"                      // BrnAI::AIModuleIO::OutputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"  // RaceCarEntityModuleIO::InputBuffer_PrePhysics / _PostPhysics
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"        // PropEntityIO::InputBuffer_PrePhysics
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                                        // BrnPhysics::PhysicsModuleIO::InputBuffer

// AI module -> entity-module / physics bridges -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeAIToEntityModules.h
//   (DWARF home: GameSource/Unity/../World/Bridges/WorldBridgeAIToEntityModules.cpp)
//
// The AI output fan-out WorldModule::Update @0x827D63E8 runs twice per frame:
// once before the pre-physics spine (race car + prop) and once after the physics
// update (race car), plus the AI -> physics staging. Each is bracketed by the
// AI-bridge CPU monitor (the X360 reads the monitor id straight out of the
// WorldModule context at +6167720). The leading lpWorldModule arg is the X360 r3.
// Bodies boot-gated in WorldLinkStubs.cpp until the AI module IO is homed.
namespace WorldModule
{
    // @ 0x827AD540
    void BridgeAIToEntityModules_PrePhysics(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
        BrnWorld::PropEntityIO::InputBuffer_PrePhysics* lpPropInputBuffer_PrePhysics,
        const BrnAI::AIModuleIO::OutputBuffer* lpAIOutputBuffer);

    // @ 0x827A4F58 -- latch the AI race-car interface into the race-car module's
    // post-physics input buffer.
    void BridgeAIToEntityModules_PostPhysics(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpRaceCarInputBuffer_PostPhysics,
        const BrnAI::AIModuleIO::OutputBuffer* lpAIOutputBuffer);

    // The AI -> physics staging run just before the physics post-scene update.
    void BridgeAIModuleToPhysicsModule(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnAI::AIModuleIO::OutputBuffer* lpAIOutputBuffer);
}
