#pragma once

#include "types.hpp"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                                        // PhysicsModuleIO::InputBuffer
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"        // PropEntityIO::OutputBuffer_Prepare
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"  // BrnTrafficIO::OutputBuffer_Prepare
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                        // SceneManagerIO::InputBuffer_Update / InSceneUpdateInterface

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
namespace WorldModule
{
    // @ 0x827AB410
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
}
