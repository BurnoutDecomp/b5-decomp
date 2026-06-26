// ============================================================================
// b5-decomp/src/GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.cpp
//
// WorldModule::BridgePhysicsModuleToAIModule_PostPhysics (X360 0x827A5680).
//
// A per-frame bridge run from WorldModule::Update: it copies one word out of the physics
// module's PostPhysics output sub-interface into the AI module's PostPhysics input buffer.
//
// Reconstructed store-for-store from the X360 asm; cross-checked against the PS3 DecFIGS
// body WorldModule::BridgePhysicsModuleToAIModule_PostPhysics (DecFIGS 0xA2B84C):
//   if (!aiInput)       assert "lpAIModuleInputBuffer_PostPhysics != NULL" (:142)
//   if (!physicsOutput) assert "lpPhysicsModuleOutputBuffer != NULL"       (:143)
//   if (!(physicsOutput->Flags & 0x10)) assert "Not locked for reading"
//                                              (Physics/BrnPhysicsModuleIO.h:369)
//   *(aiInput + 4) = *(physicsOutput + 998192)        ; the read-locked sub-interface word
//
// The PS3 body INLINES the X360's `bl sub_8279F8E0` getter: it is a read-lock check (bit 0x10,
// "Not locked for reading") followed by a direct read of the PostPhysics sub-interface word at
// OutputBuffer +998192 (0xF3CB0), not a call returning a small offset-0 sub-interface. The two
// real buffer types are BrnAI::AIModuleIO::InputBuffer_PostPhysics and
// BrnPhysics::PhysicsModuleIO::OutputBuffer.
//
// FLAG: the two buffer payloads are not fully homed in this slice; they are modelled by name
// (see WorldBridgePhysicsToEntityModules.h) so the read-locked word copy is reproduced. The
// real +998192 read offset and the BrnAI/BrnPhysics buffer types are header-level and left
// deferred. The baked assert file/line are discarded per project convention; the stringized
// conditions match the X360/PS3 assert text.
// ============================================================================

#include "GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace WorldModule
{

// FLAG: placeholder for the X360 read-locking getter sub_8279F8E0. The real getter
// read-locks the OutputBuffer and returns &<sub-interface>; here it returns the modelled
// member so the bridge's word copy is well defined.
const PhysicsToAIPostPhysicsInterface*
PhysicsModuleOutputBuffer::GetPhysicsToAIPostPhysicsInterface() const
{
    LockForRead();
    return &mInterface;
}

// @ 0x827A5680
const PhysicsToAIPostPhysicsInterface* BridgePhysicsModuleToAIModule_PostPhysics(
    void* lpWorldModule,
    AIModuleInputBuffer_PostPhysics* lpAIModuleInputBuffer_PostPhysics,
    const PhysicsModuleOutputBuffer* lpPhysicsModuleOutputBuffer)
{
    (void)lpWorldModule;   // X360 r3 -- never read by this bridge

    CGS_ASSERT(lpAIModuleInputBuffer_PostPhysics != 0, "lpAIModuleInputBuffer_PostPhysics != NULL");
    CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL");

    const PhysicsToAIPostPhysicsInterface* lpInterface =
        lpPhysicsModuleOutputBuffer->GetPhysicsToAIPostPhysicsInterface();

    // X360: lwz r11, 0(result) ; stw r11, 4(aiInput)  -- copy the leading word.
    lpAIModuleInputBuffer_PostPhysics->muPhysicsValue = lpInterface->muValue;

    return lpInterface;
}

} // namespace WorldModule
