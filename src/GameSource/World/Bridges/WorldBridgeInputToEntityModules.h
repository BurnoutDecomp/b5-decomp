#pragma once

#include "types.hpp"
#include "GameSource/World/BrnWorldModuleIO.h"                                                   // BrnWorldIO::UpdateInputBuffer
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"        // TriggerEntityModuleIO::InputBuffer_PreScene/_PostScene
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"        // BrnTrafficIO::InputBuffer_PreScene
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"        // RaceCarEntityModuleIO::InputBuffer_PreScene/_PrePhysics
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"            // WorldEntityIO::InputBuffer_PreScene
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"              // PropEntityIO::InputBuffer_PreScene
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"                             // CrashIO::InputBuffer_PreScene
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                                              // BrnPhysics::PhysicsModuleIO::InputBuffer

// WorldModule GUI/game-input -> entity-modules bridge -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeInputToEntityModules.h
//   (DWARF home: WorldBridgeInputToEntityModules.cpp; decls BrnWorldModule.h:440)
//
// Per-frame: fan the world module's Update input buffer out into the trigger /
// traffic / race-car / world-entity / prop entity-module input buffers.
// Signature + parameter names verbatim from the PS3 DecFIGS DWARF
// (BrnWorldBridgesUnity.cpp dump); reconstructed against BURNOUT_X360_ARTIST.XEX
// @ 0x827ADF88 / 0x827A8C58. On the consoles these are WorldModule methods; per
// the committed bridge precedent they are modelled as namespace functions whose
// leading lpWorldModule arg is the X360 r3 (the WorldModule `this`) -- this
// bridge DOES read through it (the director camera member + the hard-stop flag,
// X360 offsets cited in the .cpp).
namespace WorldModule
{
    // @ 0x827A8C58 (this TU, WorldBridgeInputToEntityModules.cpp:183) -- scan the
    // world input's vehicle-driver update queue and latch the per-race-car
    // "received network driver controls" flags.
    void CheckForNetworkDriverControlsReceived(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene* lpRaceCarInputBuffer_PreScene,
        const BrnWorldIO::UpdateInputBuffer* lpWorldInput);

    // @ 0x827ADF88 (this TU, WorldBridgeInputToEntityModules.cpp:46)
    void BridgeInputToEntityModules(
        void* lpWorldModule,
        BrnWorld::TriggerEntityModuleIO::InputBuffer_PreScene*  lpTriggerInput_PreScene,
        BrnWorld::TriggerEntityModuleIO::InputBuffer_PostScene* lpTriggerInput_PostScene,
        BrnTraffic::BrnTrafficIO::InputBuffer_PreScene*         lpTrafficInputBuffer_PreScene,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene*  lpRaceCarInputBuffer_PreScene,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
        BrnWorld::WorldEntityIO::InputBuffer_PreScene*          lpWorldEntityInputBuffer_PreScene,
        BrnWorld::PropEntityIO::InputBuffer_PreScene*           lpPropEntityInputBuffer_PreScene,
        const BrnWorldIO::UpdateInputBuffer*                    lpWorldInput);

    // ---- ADDITIVE (world-drive wave 2026-07-27; same X360 TU -- their
    //      addresses sit immediately below BridgeInputToEntityModules) -------

    // WorldModule::Update @0x827D63E8's first input bridge: stage the world
    // input's physics-facing state into the physics module's input buffer.
    void BridgeInputToPhysicsModule(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnWorldIO::UpdateInputBuffer* lpWorldInput);

    // @ 0x827ADEE8 -- stage the crash module's pre-scene input: the player
    // vehicle-controls word (+59), the crash network input interface, the game
    // action queue, the vehicle-driver interface and the timer status.
    void BridgeInputToCrashModule(
        void* lpWorldModule,
        BrnWorld::CrashIO::InputBuffer_PreScene* lpCrashInputBuffer_PreScene,
        const BrnWorldIO::UpdateInputBuffer* lpWorldInput);
}
