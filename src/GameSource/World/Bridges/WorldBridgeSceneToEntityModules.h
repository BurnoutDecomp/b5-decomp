#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                             // SceneManagerIO::OutputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"       // RaceCarEntityModuleIO::InputBuffer_PrePhysics

// WorldModule scene -> race-car bridge -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h
//
// Per-frame: append the scene manager's query results into the race-car module's
// pre-physics scene-result queue.
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827ABAC8; the module-IO buffer slices
// live at their own homes (see the includes). The leading lpWorldModule arg is the
// X360 r3 (the WorldModule context); the bridge never reads through it.
namespace WorldModule
{
    // @ 0x827ABAC8
    void BridgeSceneQueryResultsToRaceCarModule_PrePhysics(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneModuleOutputBuffer);
}
