#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                                     // EActiveRaceCarIndex
#include "SharedClasses/BrnSharedConstants.h"                                                // BrnUpdateSet
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"          // WorldEntityIO::InputBuffer_PreScene
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"      // RaceCarEntityModuleIO::OutputBuffer_PreScene / InputBuffer_PrePhysics
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"      // TriggerEntityModuleIO::InputBuffer_PreScene
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"      // BrnTrafficIO::OutputBuffer_PreScene / OutputBuffer_PostScene
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"            // PropEntityIO::OutputBuffer_PrePhysics / InputBuffer_PreScene

// WorldModule entity-module -> entity-module bridges -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h
//   (DWARF home: WorldBridgeEntityModulesToEntityModules.cpp; decls BrnWorldModule.h:470/473/566)
//
// Per-frame cross-module data hand-offs run during the pre-scene / pre-physics phases.
// Signatures + parameter names verbatim from the PS3 DecFIGS DWARF (BrnWorldBridgesUnity.cpp
// dump); reconstructed against BURNOUT_X360_ARTIST.XEX @ 0x827AD788 / 0x827A52B0 / 0x827A51F0.
// On the consoles these are WorldModule methods (DWARF BrnWorldModule.h shows the 2-arg member
// shape, `this`==WorldModule implicit); per the committed bridge precedent they are modelled as
// namespace functions whose leading lpWorldModule arg is the X360 r3 (the WorldModule `this`).
// BridgeRaceCarModuleToWorldModule_PreScene DOES read through it (it writes the player index +
// per-car rival markers into WorldModule members at their X360 byte offsets, cited in the .cpp);
// the other two never dereference it.
namespace BrnTraffic { namespace BrnTrafficIO { class InputBuffer_PostScene; } }
namespace BrnWorld { namespace RaceCarEntityModuleIO { class OutputBuffer_PostScene; } }

namespace WorldModule
{
    // @ 0x827A52B0 (WorldBridgeEntityModulesToEntityModules.cpp:88; DWARF BrnWorldModule.h:473) --
    // latch the race-car module's active output interface into the world-entity input buffer, then
    // publish the player index + per-active-car rival markers into the WorldModule context.
    void BridgeRaceCarModuleToWorldModule_PreScene(
        void* lpWorldModule,
        BrnWorld::WorldEntityIO::InputBuffer_PreScene* lpWorldInputBuffer_PreScene,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene);

    // @ 0x827A51F0 (WorldBridgeEntityModulesToEntityModules.cpp:69; DWARF BrnWorldModule.h:566) --
    // latch the traffic module's post-scene traffic->race-car interface into the race-car
    // pre-physics input.
    // ---- ADDITIVE DECLS (attested callers in WorldModule::EntityModulePrePhysicsUpdate
    //      @0x827BD5B8; ledger-'reviewed' PHANTOMS. Reconstruct from @0x827A5270
    //      (race car -> traffic) and @0x827AEA70 (prop -> traffic). ----
    // PHANTOM (caller in EntityModulePostSceneUpdate @0x827C3C58) -- reconstruct
    // from @0x827AD828 (race-car post-scene state -> traffic post-scene input).
    void BridgeRaceCarModuleToTrafficModule_PostScene(
        void* lpWorldModule,
        BrnTraffic::BrnTrafficIO::InputBuffer_PostScene* lpTrafficInputBuffer_PostScene,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene* lpRaceCarOutputBuffer_PostScene);

    void BridgeRaceCarModuleToTrafficModule_PrePhysics(
        void* lpWorldModule,
        BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutputBuffer_PrePhysics);

    void BridgePropModuleToTrafficModule_PrePhysics(
        void* lpWorldModule,
        BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
        const BrnWorld::PropEntityIO::OutputBuffer_PrePhysics* lpPropOutputBuffer_PrePhysics);

    void BridgeTrafficToRaceCar_PrePhysics(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutputBuffer_PostScene);

    // @ 0x827AD788 (WorldBridgeEntityModulesToEntityModules.cpp:46; DWARF BrnWorldModule.h:470) --
    // merge the traffic module's pre-scene trigger-management output into the trigger pre-scene
    // input buffer.
    void BridgeTrafficToTrigger_PreScene(
        void* lpWorldModule,
        BrnWorld::TriggerEntityModuleIO::InputBuffer_PreScene* lpTriggerInputBuffer_PreScene,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutputBuffer_PreScene);

    // ---- ADDITIVE (world-drive wave 2026-07-27; same X360 TU): the three
    //      pre-scene hand-offs WorldModule::EntityModulePreSceneUpdate
    //      @0x827BD1F0 runs. ----

    // @ 0x827A50E0 -- prime the traffic module's pre-scene / post-scene /
    // post-physics inputs from the race-car pre-scene output.
    void BridgeRaceCarModuleToTrafficModule_PreScene(
        void* lpWorldModule,
        BrnTraffic::BrnTrafficIO::InputBuffer_PreScene* lpTrafficInputBuffer_PreScene,
        BrnTraffic::BrnTrafficIO::InputBuffer_PostScene* lpTrafficInputBuffer_PostScene,
        BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInputBuffer_PostPhysics,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene);

    // @ 0x827A5510 -- stage the race-car pre-scene output into the prop
    // module's pre-scene input.
    void BridgeRaceCarModuleToPropModule_PreScene(
        void* lpWorldModule,
        BrnWorld::PropEntityIO::InputBuffer_PreScene* lpPropInputBuffer_PreScene,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene,
        BrnUpdateSet lUpdateSet);

    // @ 0x827AACF8 -- stage the world-entity pre-scene output (the streamed
    // world's prop-relevant state) into the prop module's pre-scene input.
    void BridgeWorldModuleToPropModule_PreScene(
        void* lpWorldModule,
        BrnWorld::PropEntityIO::InputBuffer_PreScene* lpPropInputBuffer_PreScene,
        const BrnWorld::WorldEntityIO::OutputBuffer_PreScene* lpWorldOutputBuffer_PreScene);
}
