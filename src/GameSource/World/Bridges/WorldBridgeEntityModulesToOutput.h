#pragma once

#include "types.hpp"
#include "GameSource/World/BrnWorldModuleIO.h"                                                // BrnWorldIO::UpdateOutputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"      // RaceCarEntityModuleIO::OutputBuffer_Prepare / OutputBuffer_PrePhysics
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"      // BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"            // PropEntityIO::OutputBuffer_Prepare
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"      // TriggerEntityModuleIO::OutputBuffer_PrePhysics
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"          // WorldEntityIO::OutputBuffer_PreScene / _PostPhysics

// WorldModule entity-modules -> update-output bridge -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h
//
// Per-frame bridges that drain each entity module's OWN output buffer into the
// WorldModule's shared UpdateOutputBuffer:
//   - Prepare phase: append the race-car / prop resource-request rings into the
//     world resource-request interface (BridgeRaceCar/PropResourceRequestsToOutput_Prepare).
//   - PrePhysics phase: forward race-car + traffic entity info and copy the trigger
//     module's overlap-output interface (BridgeEntityModulesToOutput_PrePhysics).
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Every module-IO buffer slice lives at its
// own home (see the includes). The leading lpWorldModule arg is the X360 r3 (the
// WorldModule context); the Prepare bridges never read through it.
namespace BrnWorld { namespace WorldEntityIO { struct OutputBuffer_Prepare; } }
namespace BrnTraffic { namespace BrnTrafficIO { class OutputBuffer_Prepare; } }
namespace BrnAI { namespace AIModuleIO { struct OutputBuffer; } }
namespace BrnPhysics { namespace PhysicsModuleIO { class OutputBuffer; } }
namespace CgsSceneManager { namespace SceneManagerIO { struct OutputBuffer; } }
namespace BrnWorld { namespace CrashIO { struct OutputBuffer_PostPhysics; } }

namespace WorldModule
{
    // @ 0x827AD950 -- append the race-car Prepare output's resource-request ring
    // (VariableEventQueue<8192,16>) into the world resource-request interface
    // (VariableEventQueue<4096,16>).
    // ---- ADDITIVE DECLS (attested callers in WorldModule::Prepare @0x827D53B0;
    //      the ledger marks these three 'reviewed' but no body was ever committed --
    //      the same phantom pattern as the WorldEntityModule drivers. Bodies follow
    //      the committed sibling append-forward pattern; reconstruct from
    //      @0x827ADA28 / @0x827AD9D8 / @0x827AD480 when this TU is next opened.) ----
    void BridgeWorldResourceRequestsToOutput_Prepare(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
        const BrnWorld::WorldEntityIO::OutputBuffer_Prepare* lpWorldEntityOutputBuffer_Prepare);

    void BridgeTrafficResourceRequestsToOutput(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare* lpTrafficOutputBuffer_Prepare);

    void BridgeAIModuleToOutput(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
        const BrnAI::AIModuleIO::OutputBuffer* lpAIOutputBuffer);

    void BridgeRaceCarResourceRequestsToOutput_Prepare(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_Prepare* lpRaceCarOutputBuffer_Prepare);

    // @ 0x827AF1D0 -- append the prop Prepare output's resource-request ring
    // (VariableEventQueue<1024,16>) into the world resource-request interface
    // (VariableEventQueue<4096,16>).
    void BridgePropResourceRequestsToOutput_Prepare(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
        const BrnWorld::PropEntityIO::OutputBuffer_Prepare* lpPropOutputBuffer_Prepare);

    // @ 0x827AEDE0 -- forward race-car + traffic entity info to output and copy the
    // trigger module's overlap-output interface into the world output buffer.
    void BridgeEntityModulesToOutput_PrePhysics(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutput_PrePhysics,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics* lpTrafficOutput_PrePhysics,
        const BrnWorld::TriggerEntityModuleIO::OutputBuffer_PrePhysics* lpTriggerOutput_PrePhysics);

    // Sibling pre-physics info bridges called by BridgeEntityModulesToOutput_PrePhysics.
    // They live in this same original TU but carry their own X360 addresses / ledger
    // entries; declared here (definitions land with their own reconstructions).
    void BridgeRaceCarEntityInfoToOutput_PrePhysics(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutput_PrePhysics);

    void BridgeTrafficCarEntityInfoToOutput_PrePhysics(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics* lpTrafficOutput_PrePhysics);

    // ====================================================================
    // ADDITIVE (world-drive wave 2026-07-27): the per-frame ...ToOutput set
    // WorldModule::Update @0x827D63E8 drives. Every one of these lives in
    // this same X360 TU (their addresses cluster in the 0x827AD..0x827AF
    // band next to the committed siblings). Signatures come from the Update
    // call sites; the leading lpWorldModule arg is the X360 r3.
    // ====================================================================

    // @ 0x827ADD78 -- the STREAMER-CRITICAL pre-scene world-entity flush:
    // append the world-entity module's out-event queue into the update
    // output's game-event queue, then forward its sound world-load events.
    // BODIED in WorldBridgeEntityModulesToOutput.cpp.
    void BridgeWorldEntityInfoToOutput(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnWorld::WorldEntityIO::OutputBuffer_PreScene* lpWorldEntityOutput_PreScene);

    // @ 0x827ADC38 -- ⭐ THE RACE-CAR OUTPUT-INTERFACE TRANSFER. Copies the race-car
    // module's four published interfaces (live + replay active-race-car, live + replay
    // global) out of its PostPhysics output buffer into the world update-output buffer,
    // then appends its game-event queue. This is the ONLY path by which a race car's
    // per-frame state leaves the race-car module, and therefore the middle link of the
    // chain that ends at the director's per-car VehicleInfo.
    // BODIED in WorldBridgeEntityModulesToOutput.cpp (called from
    // BridgeEntityModulesToOutput_PostPhysics, as on the console).
    void BridgeRaceCarEntityInfoToOutput_PostPhysics(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpRaceCarOutput_PostPhysics);

    // @ 0x827AF318
    void BridgeRaceCarEntityInfoToOutput_PreScene(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutput_PreScene);

    // (X360 sibling of the _PrePhysics traffic-info bridge; same TU.)
    void BridgeTrafficEntityInfoToOutput_PreScene(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutput_PreScene);

    // @ 0x827AF258
    void BridgePropToOutput_PreScene(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnWorld::PropEntityIO::OutputBuffer_PreScene* lpPropOutput_PreScene);

    // The post-physics output fan-in (the world-entity leg carries the
    // streamer's staged GameData resource requests + the status interface).
    void BridgeEntityModulesToOutput_PostPhysics(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* lpTrafficOutput_PostPhysics,
        const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpRaceCarOutput_PostPhysics,
        const BrnWorld::PropEntityIO::OutputBuffer_PostPhysics* lpPropOutput_PostPhysics,
        const BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics* lpWorldEntityOutput_PostPhysics);

    // @ 0x827AEB18
    void BridgePhysicsToOutput(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsOutputBuffer);

    void BridgeSceneModuleToOutput(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutputBuffer);

    void BridgeCrashModuleToOutput(
        void* lpWorldModule,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const BrnWorld::CrashIO::OutputBuffer_PostPhysics* lpCrashOutput_PostPhysics);
}
