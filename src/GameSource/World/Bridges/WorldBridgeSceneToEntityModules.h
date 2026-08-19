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
namespace BrnTraffic { namespace BrnTrafficIO { class InputBuffer_PostPhysics; class InputBuffer_PrePhysics; } }
namespace BrnWorld { namespace TriggerEntityModuleIO { class InputBuffer_PrePhysics; }
                     namespace PropEntityIO { class InputBuffer_PrePhysics; }
                     namespace RaceCarEntityModuleIO { class InputBuffer_PrePhysics; } }

namespace WorldModule
{
    // @ 0x827ABAC8
    // ---- ADDITIVE DECLS (attested callers in WorldModule::EntityModulePrePhysicsUpdate
    //      @0x827BD5B8; ledger-'reviewed' PHANTOMS -- no body was ever committed.
    //      Reconstruct from @0x827ABBD0 / @0x827ABCB0 / @0x827ABC50 (each appends the
    //      scene output's contact queues into the module's pre-physics input). ----
    // PHANTOMS (callers in EntityModulePostSceneUpdate @0x827C3C58):
    //   @0x827ABB50 traffic (3-buffer form), @0x827ABD30 trigger.
    //
    // ---- STATUS 2026-08-19 (wave Q5 cluster F2) -----------------------------------------
    //   @0x827ABCB0 prop     BODIED -- WorldBridgePropModule.cpp (the split-out TU that owns
    //                        all five prop bridges); its WorldLinkStubs gate is already retired.
    //   @0x827ABC50 traffic  BODIED -- WorldBridgeSceneToEntityModules.cpp (this header's TU).
    //   @0x827ABBD0 race car PARKED -- the destination setter's parameter type is forked in
    //                        BrnRaceCarEntityModuleIOQueues.h (a derived struct where the DWARF
    //                        has a typedef), so the scene's queue pointer does not convert.
    //                        Measured, with the exact one-line unblock, in that TU's banner.
    void BridgeSceneQueryResultsToTrafficModule_PrePhysics(
        void* lpWorldModule,
        BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInputBuffer_PostPhysics,
        BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneQueryOutput);

    void BridgeSceneQueryResultsToTriggerModule_PrePhysics(
        void* lpWorldModule,
        BrnWorld::TriggerEntityModuleIO::InputBuffer_PrePhysics* lpTriggerInputBuffer_PrePhysics,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneQueryOutput);

    void BridgeSceneContactsToRaceCarModule_PrePhysics(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneContactsFromWorld);

    void BridgeSceneContactsToPropModule_PrePhysics(
        void* lpWorldModule,
        BrnWorld::PropEntityIO::InputBuffer_PrePhysics* lpPropInputBuffer_PrePhysics,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneContactsFromWorld);

    void BridgeSceneContactsToTrafficModule_PrePhysics(
        void* lpWorldModule,
        BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneContactsFromWorld);

    void BridgeSceneQueryResultsToRaceCarModule_PrePhysics(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneModuleOutputBuffer);
}
