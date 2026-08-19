#include "GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h" // BrnTrafficIO::InputBuffer_PrePhysics (the header only forward-declares it)


// @ 0x827ABAC8 -- append the scene manager's query-results ring (read-locked getter
// @0x823B1ED0) into the race-car module's pre-physics scene-result queue
// (write-locked getter @0x8279DB98) through the real instantiated
// VariableEventQueue<32768,16>::Append symbol.
// Both null tripwires are NON-gating (the X360 falls through after firing); the
// X360 tail returns the forwarded call's result as a register artifact -- the
// logical return type is void.
//
// ---- wave Q5 cluster F2 (2026-08-19) ------------------------------------------------------
// Two more of this TU's console bridges become landable once SceneManagerIO::OutputBuffer has
// its potential-contact / overlap-pair seats (wave Q5 round 2). ONE is landed below
// (BridgeSceneContactsToTrafficModule_PrePhysics @0x827ABC50); the other,
// BridgeSceneContactsToRaceCarModule_PrePhysics @0x827ABBD0, is PARKED -- see the block above
// it. A parked bridge is NOT defined here at all, so its one-shot gate in WorldLinkStubs.cpp
// keeps linking and no LNK2005 is possible.
namespace WorldModule
{
// @ 0x827ABAC8
void BridgeSceneQueryResultsToRaceCarModule_PrePhysics(
    void* lpWorldModule,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
    const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneModuleOutputBuffer)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpRaceCarInputBuffer_PrePhysics != 0, "lpRaceCarInputBuffer_PrePhysics != NULL");   // :98
    CGS_ASSERT(lpSceneModuleOutputBuffer != 0, "lpSceneModuleOutputBuffer != NULL");               // :99

    lpRaceCarInputBuffer_PrePhysics->GetSceneResultQueue()->Append(
        *lpSceneModuleOutputBuffer->GetSceneQueryResultsQueue());
}

// =================================================================================================
// ⛔ WorldModule::BridgeSceneContactsToRaceCarModule_PrePhysics  @ 0x827ABBD0  (32 insns)
//    PARKED -- NOT DEFINED HERE. Its one-shot gate at WorldLinkStubs.cpp:2300 stays.
//
// ---- The console body (0x827ABBD0..0x827ABC4C) --------------------------------------------
//   r4 = lpRaceCarInputBuffer_PrePhysics (dest), r5 = lpSceneModuleOutputBuffer (src);
//   r3 (the WorldModule `this`) is overwritten at 0x827ABC40 and never read.
//   if (!dest) FireAssert("lpRaceCarInputBuffer_PrePhysics != NULL", <this file>, 0xAB == 171)
//   if (!src)  FireAssert("lpSceneModuleOutputBuffer != NULL",       <this file>, 0xAC == 172)
//   0x827ABC38  bl 0x8279C098  SceneManagerIO::OutputBuffer::GetPotentialContactQueue() const
//   0x827ABC44  bl 0x827A9840  RaceCarEntityModuleIO::InputBuffer_PrePhysics::
//                              SetPotentialContactQueue          (write-lock; Clear + Append)
// Both ends exist and are MOUNTED (the setter is real at BrnRaceCarEntityModuleIO.cpp:786).
//
// ⛔ THE BLOCKER IS A TYPE FORK IN A FILE THIS TU DOES NOT OWN, and it is MEASURED, not
//    argued -- scratchpad/waveQ5/probe_f2/probe_racecar_setter.cpp compiles the one-line body
//    and MSVC rejects it:
//        error C2664: cannot convert argument 1 from
//          'const CgsSceneManager::SceneManagerIO::OutputBuffer::OutPotentialContactQueue *'
//          to 'const BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::
//              PotentialContactQueue *'
//        note: types pointed to are unrelated; conversion requires reinterpret_cast
//    The console has ONE type here: the DecFIGS DWARF spells
//    RaceCarEntityModuleIO::PotentialContactQueue (:89) as a TYPEDEF of
//    EventQueue<SceneManagerIO::PotentialContact,2048> -- the same type the scene's
//    OutPotentialContactQueue (:290) is -- which is exactly why the console can hand one
//    straight to the other with no conversion at all. This tree instead models it as
//        struct alignas(16) PotentialContactQueue : public EventQueue<PotentialContact,2048> {};
//    (BrnRaceCarEntityModuleIOQueues.h:130), a DISTINCT type. Base -> derived does not convert
//    implicitly, and static_cast'ing down onto an object that really is the base is UB, so
//    landing this leg today would need a reinterpret_cast the console does not have.
//
//    ONE-LINE UNBLOCK (reported, not applied -- BrnRaceCarEntityModuleIOQueues.h is another
//    owner's file): turn that struct back into the DWARF's typedef,
//        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact,2048>
//                PotentialContactQueue;
//    The stated reason for deriving ("so existing `struct PotentialContactQueue` forward
//    references stay valid", same file :126) no longer holds: grepped the whole tree, there is
//    NO forward declaration of that name anywhere. Its element is already alignas(16)
//    (CgsPotentialContact.h), so the queue keeps its 16-byte alignment and
//    InputBuffer_PrePhysics's layout does not move.
//
//    COST OF THE PARK: the race-car module's own pre-physics potential-contact queue stays
//    empty, so RaceCarEntityModule cannot see car-vs-anything broad-phase pairs. It does NOT
//    block the smash-gate goal -- the prop path (WorldBridgePropModule.cpp) and the physics
//    path (WorldBridgeSceneToPhysics.cpp) are both landed this wave and neither reads this
//    queue.
// =================================================================================================

// =================================================================================================
// ⭐ WorldModule::BridgeSceneContactsToTrafficModule_PrePhysics  @ 0x827ABC50  (23 insns)
//
// ---- The console body (0x827ABC50..0x827ABCA8), instruction for instruction ---------------
//   r4 = lpTrafficInputBuffer_PrePhysics (dest), r5 = lpSceneModuleOutputBuffer (src);
//   r3 (the WorldModule `this`) is never even copied -- the prologue moves r5->r30, r4->r31.
//
//   ⚠️ NO ASSERTS. This bridge really has none: there is no `cmplwi` and no FireAssert
//   anywhere in the body. Do NOT add the null tripwires its three siblings carry (the same
//   disposition BridgeCrashModuleToPropModule_PostScene @0x827AAD78 already documents).
//
//   0x827ABC70  bl 0x8279C098  SceneManagerIO::OutputBuffer::GetPotentialContactQueue() const
//                              read-lock (bit 4), CgsSceneManagerModuleIO.h:625, this+32800
//   0x827ABC7C  bl 0x827A9DE0  BrnTrafficIO::InputBuffer_PrePhysics::SetPotentialContactQueue
//                              write-lock, BrnTrafficEntityModuleIO.h:282; Clear + Append
//   0x827ABC84  bl 0x8279C140  SceneManagerIO::OutputBuffer::GetOverlapPairsQueue() const
//                              read-lock (bit 4), CgsSceneManagerModuleIO.h:627, this+196656
//   0x827ABC90  bl 0x827A9E98  BrnTrafficIO::InputBuffer_PrePhysics::SetOverlapPairsQueue
//                              write-lock, BrnTrafficEntityModuleIO.h:285; Clear + Append
//   0x827ABCA8  blr            (the tail forwards the last call's register as an artifact;
//                               the logical return type is void)
//
// Both destination setters are REAL and MOUNTED (BrnTrafficEntityModuleIO.cpp:269 / :279) and
// both take exactly the types the two source getters return -- traffic's PotentialContactQueue
// (:91) and OverlapPairsQueue (:92) are direct typedefs of the same two EventQueue
// instantiations -- so there is no cast in this body. (That is the difference from the
// race-car sibling parked above, whose queue type this tree forked into a derived struct.)
// =================================================================================================
void BridgeSceneContactsToTrafficModule_PrePhysics(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
    const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneContactsFromWorld)
{
    (void)lpWorldModule;   // X360 r3 -- never read (not even copied out of the register)

    // Source getter, then destination setter, per leg -- the console's own order. Both getters
    // are lock tripwires, so the order is behaviour, not style.
    lpTrafficInputBuffer_PrePhysics->SetPotentialContactQueue(
        lpSceneContactsFromWorld->GetPotentialContactQueue());     // @0x8279C098 -> @0x827A9DE0

    lpTrafficInputBuffer_PrePhysics->SetOverlapPairsQueue(
        lpSceneContactsFromWorld->GetOverlapPairsQueue());         // @0x8279C140 -> @0x827A9E98
}
}
