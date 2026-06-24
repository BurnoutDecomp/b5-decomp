#pragma once

// ===================================================================================
// WorldModule physics->entity-module bridges -- owning header
//   b5-decomp/src/GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.h
//
// Per-frame bridges that copy fields out of the physics module's output buffer into the
// downstream entity-module input buffers. This slice homes only the PostPhysics
// physics->AI bridge (the one function in this TU's ledger):
//   BridgePhysicsModuleToAIModule_PostPhysics @ 0x827A5680
// which latches one word from a physics-output sub-interface into the AI module's
// PostPhysics input buffer.
//
// FLAG: the two buffer payloads (AIModuleInputBuffer_PostPhysics / PhysicsModuleOutputBuffer)
// and the physics-output accessor the X360 calls (sub_8279F8E0, a read-locking getter in the
// PhysicsModuleIO::OutputBuffer read-accessor address band 0x8279F5xx..0x8279F8xx) are not
// fully homed in this slice. They are modelled here as minimal by-name sized types so the
// bridge's store-for-store data flow (copy one u32 from the physics sub-interface into the AI
// input buffer at +4) is reproduced; the X360 byte offsets are cited for provenance.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer base (FlagSet @ +0)

namespace WorldModule
{
    // The physics-module output buffer's PostPhysics sub-interface the bridge reads through.
    // FLAG: minimal stand-in -- the bridge only needs its leading u32 (the value it forwards).
    struct PhysicsToAIPostPhysicsInterface
    {
        u32 muValue;   // +0x0 -- first word; copied into the AI input buffer
    };

    // The physics-module output buffer (X360 BrnPhysics::PhysicsModuleIO::OutputBuffer). FLAG:
    // modelled minimally for this bridge -- a read-locked accessor returns the PostPhysics
    // sub-interface (the X360 sub_8279F8E0, a getter in the OutputBuffer read-accessor band).
    struct PhysicsModuleOutputBuffer : public CgsModule::IOBuffer
    {
        // @ ~0x8279F8E0 -- read-lock and return the PostPhysics->AI sub-interface.
        const PhysicsToAIPostPhysicsInterface* GetPhysicsToAIPostPhysicsInterface() const;

    private:
        PhysicsToAIPostPhysicsInterface mInterface;
    };

    // The AI module's PostPhysics input buffer. FLAG: modelled minimally -- the bridge writes
    // only the field at +0x4 (the first data member past the IOBuffer FlagSet base).
    struct AIModuleInputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        u32 muPhysicsValue;   // X360 +0x4 -- receives the physics sub-interface's leading word
    };

    // @ 0x827A5680 -- copy the physics-output PostPhysics word into the AI input buffer (+0x4).
    // Asserts both buffers non-null, then forwards the word. Returns the physics sub-interface.
    // The leading lpWorldModule arg is the X360 r3 (the WorldModule context); this bridge does
    // not read through it (the X360 body never touches r3), so it is accepted and ignored.
    const PhysicsToAIPostPhysicsInterface* BridgePhysicsModuleToAIModule_PostPhysics(
        void* lpWorldModule,
        AIModuleInputBuffer_PostPhysics* lpAIModuleInputBuffer_PostPhysics,
        const PhysicsModuleOutputBuffer* lpPhysicsModuleOutputBuffer);
}
