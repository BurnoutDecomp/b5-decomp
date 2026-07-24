#pragma once

#include "types.hpp"
#include "GameSource/World/BrnWorldModuleIO.h"                                                // BrnWorldIO::UpdateOutputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"      // RaceCarEntityModuleIO::OutputBuffer_Prepare / OutputBuffer_PrePhysics
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"      // BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"            // PropEntityIO::OutputBuffer_Prepare
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"      // TriggerEntityModuleIO::OutputBuffer_PrePhysics

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
}
