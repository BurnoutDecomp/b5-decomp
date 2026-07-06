#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                                     // EActiveRaceCarIndex
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"          // WorldEntityIO::InputBuffer_PreScene
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"      // RaceCarEntityModuleIO::OutputBuffer_PreScene / InputBuffer_PrePhysics
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"      // TriggerEntityModuleIO::InputBuffer_PreScene
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"      // BrnTrafficIO::OutputBuffer_PreScene / OutputBuffer_PostScene

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
}
