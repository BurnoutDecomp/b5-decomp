#pragma once

#include "types.hpp"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                                        // BrnPhysics::PhysicsModuleIO::InputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"  // RaceCarEntityModuleIO::OutputBuffer_PreScene / _PrePhysics
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"        // PropEntityIO::OutputBuffer_PreScene / _PrePhysics
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"  // BrnTrafficIO::OutputBuffer_PrePhysics

// WorldModule entity-modules -> physics bridges -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeEntityModulesToPhysics.h
//   (DWARF home: GameSource/Unity/../World/Bridges/WorldBridgeEntityModulesToPhysics.cpp
//    -- the assert file strings baked into both bodies name that TU)
//
// The two per-frame stages WorldModule::Update @0x827D63E8 runs into the physics
// module's input buffer:
//   * pre-scene   @0x827AADB8 -- race-car + prop pre-scene outputs,
//   * pre-physics @0x827AAEC0 -- traffic + race-car + prop pre-physics outputs.
// The leading lpWorldModule arg is the X360 r3 (the WorldModule `this`); neither
// bridge dereferences it. Bodies are boot-gated in WorldLinkStubs.cpp until the
// physics-input staging interfaces are homed.
namespace WorldModule
{
    // @ 0x827AADB8
    void BridgeEntityModulesToPhysicsModule_PreScene(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene,
        const BrnWorld::PropEntityIO::OutputBuffer_PreScene* lpPropOutputBuffer_PreScene);

    // @ 0x827AAEC0
    void BridgeEntityModulesToPhysicsModule_PrePhysics(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics* lpTrafficOutputBuffer_PrePhysics,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutputBuffer_PrePhysics,
        const BrnWorld::PropEntityIO::OutputBuffer_PrePhysics* lpPropOutputBuffer_PrePhysics);
}
