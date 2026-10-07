#pragma once

// ===================================================================================
// WorldModule physics->entity-module bridges -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.h
//   (DWARF home: GameSource/Unity/../World/Bridges/WorldBridgePhysicsToEntityModules.cpp)
//
// The post-physics fan-out WorldModule::EntityModulePostPhysicsUpdate @0x827D3F10
// runs: every entity module's post-physics input buffer is staged from the physics
// module's output buffer before that module ticks.
//
//   BridgePhysicsModuleToRaceCarModule_PostPhysics @0x827AE9D0
//   BridgePhysicsModuleToTrafficModule_PostPhysics @0x827AB910
//   BridgePhysicsModuleToPropModule_PostPhysics    @0x827AB998
//   BridgePhysicsModuleToCrashModule_PostPhysics   @0x827AB8B0
//   BridgePhysicsModuleToAIModule_PostPhysics      @0x827A5680  (WorldModule::Update
//       @0x827D63E8's AI post-physics leg; the X360 body copies the leading word of
//       the physics output's post-physics AI sub-interface -- the read-locking getter
//       sub_8279F8E0, PS3-inlined as a direct read of OutputBuffer +998192 -- into
//       the AI post-physics input buffer at +4)
//
// The declarations carry the REAL buffer types (BrnPhysicsModuleIO.h / BrnAIModuleIO.h);
// the drive call sites in BrnWorldModule.cpp bind to these signatures. Bodies: race car
// and traffic in WorldBridgePhysicsToEntityModules.cpp, prop in WorldBridgePropModule.cpp,
// crash in WorldBridgeCrashInputs.cpp, AI in WorldBridgeAIModule.cpp.
//
// The leading lpWorldModule arg is the X360 r3 (the WorldModule `this`); none of
// these bridges dereferences it.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                                        // BrnPhysics::PhysicsModuleIO::OutputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"  // RaceCarEntityModuleIO::InputBuffer_PostPhysics
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"  // BrnTrafficIO::InputBuffer_PostPhysics
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"        // PropEntityIO::InputBuffer_PostPhysics
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"                       // CrashIO::InputBuffer_PostPhysics
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"                                   // BrnAI::AIModuleIO::InputBuffer_PostPhysics

namespace WorldModule
{
    // @ 0x827AE9D0
    void BridgePhysicsModuleToRaceCarModule_PostPhysics(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpRaceCarInputBuffer_PostPhysics,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer);

    // @ 0x827AB910
    void BridgePhysicsModuleToTrafficModule_PostPhysics(
        void* lpWorldModule,
        BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInputBuffer_PostPhysics,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer);

    // @ 0x827AB998
    void BridgePhysicsModuleToPropModule_PostPhysics(
        void* lpWorldModule,
        BrnWorld::PropEntityIO::InputBuffer_PostPhysics* lpPropInputBuffer_PostPhysics,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer);

    // @ 0x827AB8B0
    void BridgePhysicsModuleToCrashModule_PostPhysics(
        void* lpWorldModule,
        BrnWorld::CrashIO::InputBuffer_PostPhysics* lpCrashInputBuffer_PostPhysics,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer);

    // @ 0x827A5680
    void BridgePhysicsModuleToAIModule_PostPhysics(
        void* lpWorldModule,
        BrnAI::AIModuleIO::InputBuffer_PostPhysics* lpAIModuleInputBuffer_PostPhysics,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer);
}
