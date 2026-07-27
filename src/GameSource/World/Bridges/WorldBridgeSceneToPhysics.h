#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"   // SceneManagerIO::OutputBuffer
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                   // BrnPhysics::PhysicsModuleIO::InputBuffer

// Scene-manager -> physics bridges -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeSceneToPhysics.h
//   (DWARF home: GameSource/Unity/../World/Bridges/WorldBridgeSceneToPhysics.cpp --
//    the assert file strings baked into both bodies name that TU)
//
// The scene-query round trip in WorldModule::Update @0x827D63E8: after the scene
// manager processes the physics module's staged queries, the results and the
// potential-contact pairs are fed back into the physics module's input buffer.
// The leading lpWorldModule arg is the X360 r3 (the WorldModule `this`); neither
// bridge dereferences it. Bodies boot-gated in WorldLinkStubs.cpp.
namespace WorldModule
{
    // @ 0x827A8E88 -- scene query results -> physics input.
    void BridgeSceneQueryResultsToPhysics(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutputBuffer);

    // @ 0x827ABD80 -- the scene's potential-contact pairs -> physics input.
    void BridgeScenePotentialContactsToPhysics(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutputBuffer);
}
