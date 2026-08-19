// b5-decomp/src/GameSource/World/Bridges/WorldBridgeToEntityModules.h
#pragma once

#include "types.hpp"
#include "GameSource/World/BrnWorldModuleIO.h"                                            // BrnWorldIO::UpdateInputBuffer / GameActionQueue
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                                        // BrnPhysics::PhysicsModuleIO::InputBuffer
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h" // BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h" // RaceCarEntityModuleIO::InputBuffer_PreScene
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"     // WorldEntityIO::InputBuffer_PostPhysics

// WorldModule game-action -> physics / traffic module bridges -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeToEntityModules.cpp
//   (DWARF home: GameSource/Unity/../World/Bridges/WorldBridgeToEntityModules.cpp)
//
// Per game-action apply (WorldModule::HandleGameActions @0x827C44D8): walk the world
// Update input buffer's GameActionQueue (VariableEventQueue<13312,16>) and forward the
// game-action events a given destination module cares about, verbatim (same payload
// pointer / type id / size), into that module's own GameActionQueue. Each destination
// filters to a fixed allowlist of game-action type ids (the X360 switch jump tables).
//
// Return shape (X360 asm, BURNOUT_X360_ARTIST.XEX): both are int(this, dstBuffer,
// srcWorldInput); the returned int is the last GetNextEvent/GetFirstEvent result left
// in r3 as a register artifact -- the logical return type is void. On the consoles these
// are WorldModule methods; per the committed bridge precedent
// (WorldBridgeInputToEntityModules.cpp) they are modelled as namespace functions whose
// leading lpWorldModule arg is the X360 r3 (the WorldModule `this`), which neither bridge
// reads through.
// ---- ADDITIVE 2026-08-19 (wave Q6, cluster C2) ------------------------------
// The owning .cpp ALSO homes BrnWorld::WorldModule::BridgeWorldModuleToEntityModules_Render
// @0x827ABE28 (DWARF WorldBridgeToEntityModules.cpp:47 -- the FIRST function of the console
// TU). That one is a real WorldModule METHOD, so it is declared where its class lives,
// GameSource/World/BrnWorldModule.h:642, NOT in this header; the .cpp includes
// BrnWorldModule.h for it. Listed here only so the next reader of this pair knows the .cpp
// carries a fifth bridge that this header does not declare.
namespace WorldModule
{
    // @ 0x827AC568 -- forward the physics-relevant game actions (type ids 7,11,23,34,37,
    // 39,42,43,65,97,98,99,116,135,138,146,176,198; X360 switch @0x827AC61C).
    void BridgeActionsToPhysicsModule(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnWorldIO::UpdateInputBuffer* lpWorldInput);

    // @ 0x827ABFF0 -- forward the traffic-relevant game actions (type ids 13,23,28,29,30,
    // 34,39,47,73,75,77,97,98,99,100,110,143,192,225,226,236,244; X360 switch @0x827AC0A4).
    void BridgeActionsToTrafficModule(
        void* lpWorldModule,
        BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficModuleInputBuffer,
        const BrnWorldIO::UpdateInputBuffer* lpWorldInput);

    // ---- ADDITIVE (world-drive wave 2026-07-27; same X360 TU) --------------

    // @ 0x827ABF40 -- forward the race-car-relevant game actions into the
    // race-car module's pre-scene input buffer (WorldModule::Update's input
    // fan-out, before the pre-scene spine).
    void BridgeActionsToRaceCarModule(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene* lpRaceCarModuleInputBuffer,
        const BrnWorldIO::UpdateInputBuffer* lpWorldInput);

    // @ 0x827AC488 (WorldBridgeToEntityModules.cpp:178) -- clear the world-entity
    // module's post-physics game-action queue, then forward every type-192 game
    // action (the world/streamer action) verbatim into it. BODIED in
    // WorldBridgeToEntityModules.cpp.
    void BridgeActionsToWorldModule(
        void* lpWorldModule,
        BrnWorld::WorldEntityIO::InputBuffer_PostPhysics* lpWorldEntityInputBuffer_PostPhysics,
        const BrnWorldIO::UpdateInputBuffer* lpWorldInput);
}
