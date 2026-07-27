#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"          // SceneManagerIO::InputBuffer_Update / InSceneUpdateInterface
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                          // PhysicsModuleIO::OutputBuffer

// WorldModule physics -> scene bridge -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgePhysicsToScene.h
//
// Per-frame: append the physics module's staged scene-update events into the scene
// manager's update input buffer.
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827ABA40; the module-IO buffer slices
// live at their own homes (see the includes). The leading lpWorldModule arg is the
// X360 r3 (the WorldModule context); the bridge never reads through it.
namespace WorldModule
{
    // @ 0x827ABA40
    void BridgePhysicsSceneUpdateToScene(
        void* lpWorldModule,
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer);

    // ---- ADDITIVE (world-drive wave 2026-07-27; same X360 TU) --------------
    // @ 0x827A8D20 -- stage the physics module's generated scene queries into the
    // scene manager's query input buffer (WorldModule::Update @0x827D63E8's
    // physics/scene round trip). Body boot-gated in WorldLinkStubs.cpp.
    void BridgePhysicsSceneQueriesToScene(
        void* lpWorldModule,
        CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneInputBuffer_Query,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer);
}
