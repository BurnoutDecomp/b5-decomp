#pragma once

#include "types.hpp"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                                        // PhysicsModuleIO::InputBuffer
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"        // PropEntityIO::OutputBuffer_Prepare
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"  // BrnTrafficIO::OutputBuffer_Prepare
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                        // SceneManagerIO::InputBuffer_Update / InSceneUpdateInterface
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"  // RaceCarEntityModuleIO::OutputBuffer_PreScene / _PostPhysics
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"  // TriggerEntityModuleIO::OutputBuffer_PreScene
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"      // WorldEntityIO::OutputBuffer_PreScene / _PostPhysics

// WorldModule entity-modules -> scene/physics prepare-phase bridges -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeEntityModulesToScene.h
//
// Per-frame Prepare phase: merge each source module's prepare-phase output interface into the
// destination module's input buffer:
//   * prop  output -> physics prop-manager input   (PropInputInterface::Append)         @0x827AB410
//   * prop  output -> scene-manager update input    (InSceneUpdateInterface::Append)     @0x827AB388
//   * traffic output -> scene-manager update input  (InSceneUpdateInterface::Append)     @0x827AB300
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The leading lpWorldModule arg is the X360 r3
// (the WorldModule context); these bridges never read through it. The module-IO buffer slices
// live at their own homes (see the includes). Each X360 body tail-forwards the merge call's
// result; the logical return type is void.
namespace CgsSceneManager { namespace SceneManagerIO { struct InputBuffer_Query; } }
namespace BrnWorld { namespace RaceCarEntityModuleIO { class OutputBuffer_PostScene; }
                     namespace TriggerEntityModuleIO { class OutputBuffer_PostScene; } }
namespace BrnTraffic { namespace BrnTrafficIO { class OutputBuffer_PostScene; } }

namespace WorldModule
{
    // @ 0x827AB410
    // ---- ADDITIVE DECLS (callers in WorldModule::EntityModulePostSceneUpdate
    //      @0x827C3C58; PHANTOMS -- reconstruct from @0x827ADDC8 / @0x827ADE70 /
    //      @0x827A8B70: each stages the module's scene queries into the scene
    //      query input buffer. ----
    void BridgeRaceCarModuleToSceneModule_PostScene(
        void* lpWorldModule,
        CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneQueryInput,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene* lpRaceCarOutputBuffer_PostScene);

    void BridgeTrafficModuleToSceneModule_PostScene(
        void* lpWorldModule,
        CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneQueryInput,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutputBuffer_PostScene);

    void BridgeTriggerModuleToSceneModule_PostScene(
        void* lpWorldModule,
        CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneQueryInput,
        const BrnWorld::TriggerEntityModuleIO::OutputBuffer_PostScene* lpTriggerOutputBuffer_PostScene);

    void BridgePropModuleToPhysicsModule_Prepare(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnWorld::PropEntityIO::OutputBuffer_Prepare* lpPropOutputBuffer_Prepare);

    // @ 0x827AB388
    void BridgePropModuleToSceneModule_Prepare(
        void* lpWorldModule,
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
        const BrnWorld::PropEntityIO::OutputBuffer_Prepare* lpPropOutputBuffer_Prepare);

    // @ 0x827AB300
    void BridgeTrafficModuleToSceneModule_Prepare(
        void* lpWorldModule,
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare* lpTrafficOutputBuffer_Prepare);

    // ---- ADDITIVE (world-drive wave 2026-07-27; same X360 TU): the two
    //      per-FRAME entity-modules -> scene merges WorldModule::Update
    //      @0x827D63E8 runs (pre-scene staging + the post-physics restage). ----

    // @ 0x827AB490 -- merge every entity module's pre-scene scene-update output
    // (trigger / traffic / race car / prop / world entity) into the scene
    // manager's update input buffer.
    void BridgeEntityModulesToSceneModule_PreScene(
        void* lpWorldModule,
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
        const BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene* lpTriggerOutputBuffer_PreScene,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutputBuffer_PreScene,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene,
        const BrnWorld::PropEntityIO::OutputBuffer_PreScene* lpPropOutputBuffer_PreScene,
        const BrnWorld::WorldEntityIO::OutputBuffer_PreScene* lpWorldEntityOutputBuffer_PreScene);

    // @ 0x827AB608 -- the post-physics restage (traffic / race car / prop /
    // world entity) into the scene manager's update input buffer.
    void BridgeEntityModulesToScene_PostPhysics(
        void* lpWorldModule,
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* lpTrafficOutputBuffer_PostPhysics,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpRaceCarOutputBuffer_PostPhysics,
        const BrnWorld::PropEntityIO::OutputBuffer_PostPhysics* lpPropOutputBuffer_PostPhysics,
        const BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics* lpWorldEntityOutputBuffer_PostPhysics);
}
