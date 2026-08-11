// ============================================================================
// GameSource/World/Bridges/WorldBridgeSceneToOutput.cpp
//   (original-source home confirmed by the assert string the console bakes into the
//    body: "d:\p4\b5_main\burnout\main\code\gamesource\unity\../World/Bridges/
//    WorldBridgeSceneToOutput.cpp")
//
//   WorldModule::BridgeSceneModuleToOutput  @ 0x827A5700   (32 insns)
//
// ⭐ RECONSTRUCTED 2026-08-11 (triangle-cache wiring wave); RETIRES the inert boot gate
// that stood at WorldLinkStubs.cpp:3126.
//
// The world-output half of the triangle-cache handoff, and the ONLY caller of
// BrnWorldIO::UpdateOutputBuffer::AppendTriangleCacheInterface @0x8279BAF8 in the whole
// console image (xrefs_to of 0x8279BAF8 is exactly this one function) -- which is why that
// already-landed member had no live caller before this file existed.
//
// Its sibling is WorldModule::BridgeSceneQueryResultsToPhysics @0x827A8E88
// (WorldBridgeSceneToPhysics.cpp): the two run back to back inside the SAME
// LockBuffersForIO bracket in WorldModule::Update @0x827D63E8 (asm 0x827D7BE0 sits between
// the physics one and BridgeScenePotentialContactsToPhysics), reading the same
// write-published scene output and fanning it to the physics vehicle input and the world
// update output respectively. The world-output copy is what the EFFECTS module later reads
// through BrnEffects::EffectsIO::InputBuffer::SetTriangleCacheInterface @0x823BA928.
//
// The declaration's home in this tree is WorldBridgeEntityModulesToOutput.h (where the rest
// of the WorldModule::*ToOutput family is declared); only the DEFINITION mirrors the
// console's own file, per the mirror-original-paths rule.
// ============================================================================

#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"    // SceneManagerIO::OutputBuffer

namespace WorldModule
{
    // ========================================================================
    // BridgeSceneModuleToOutput @ 0x827A5700   (32 insns)
    //
    // Two null tripwires (WorldBridgeSceneToOutput.cpp:42 / :43 -- the console's
    // `li r5, 0x2A` / `li r5, 0x2B`), then a straight two-call forward:
    //   0x827A5768  bl SceneManagerIO::OutputBuffer::GetTriangleCacheInterface() const
    //                  @0x8279C1E8   (bit-4 read-lock tripwire, returns this+217164)
    //   0x827A5774  bl BrnWorldIO::UpdateOutputBuffer::AppendTriangleCacheInterface
    //                  @0x8279BAF8   (null tripwire + the inlined TriangleCacheInterface::
    //                                 Append, one pointer store at this+216112)
    // and TAIL-RETURNS its result (`b __restgprlr_29` -- the console's `int` return is the
    // r3 fallthrough of the assert helpers, not a value the callers read; DWARF spells the
    // bridge `void`).
    //
    // The leading lpWorldModule arg is the console's r3; the body never dereferences it
    // (r3 is overwritten at 0x827A5770 before any use).
    // ========================================================================
    void BridgeSceneModuleToOutput(
        void* /*lpWorldModule*/,
        BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutputBuffer)
    {
        CGS_ASSERT(lpOutputBuffer != 0,       "lpOutputBuffer != NULL");             // :42
        CGS_ASSERT(lpSceneOutputBuffer != 0,  "lpSceneModuleOutputBuffer != NULL");  // :43

        if (lpOutputBuffer == 0 || lpSceneOutputBuffer == 0)
        {
            return;
        }

        lpOutputBuffer->AppendTriangleCacheInterface(
            lpSceneOutputBuffer->GetTriangleCacheInterface());
    }
}
