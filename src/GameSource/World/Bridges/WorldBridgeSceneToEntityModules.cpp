#include "GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint  ([DIAG] only)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h" // BrnTrafficIO::InputBuffer_PrePhysics (the header only forward-declares it)

#include <cstdlib>   // getenv  ([DIAG] latch only -- not in the X360 binary)


// @ 0x827ABAC8 -- append the scene manager's query-results ring (read-locked getter
// @0x823B1ED0) into the race-car module's pre-physics scene-result queue
// (write-locked getter @0x8279DB98) through the real instantiated
// VariableEventQueue<32768,16>::Append symbol.
// Both null tripwires are NON-gating (the X360 falls through after firing); the
// X360 tail returns the forwarded call's result as a register artifact -- the
// logical return type is void.
//
// ---- wave Q5 cluster F2 (2026-08-19) ------------------------------------------------------
// Two more of this TU's console bridges became landable once SceneManagerIO::OutputBuffer had
// its potential-contact / overlap-pair seats (wave Q5 round 2).
// ---- wave Q6 cluster C5 (2026-08-19) ------------------------------------------------------
// BOTH are now LIVE: BridgeSceneContactsToTrafficModule_PrePhysics @0x827ABC50 (Q5) and
// BridgeSceneContactsToRaceCarModule_PrePhysics @0x827ABBD0 (Q6 -- the typedef fork that parked
// it is collapsed, see that body's banner). Every bridge this TU declares is therefore defined
// here, and BOTH WorldLinkStubs.cpp gates must be gone in the same integration step or the
// link is an LNK2005.
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
// ⭐ WorldModule::BridgeSceneContactsToRaceCarModule_PrePhysics  @ 0x827ABBD0  (32 insns)
//    LANDED 2026-08-19 (wave Q6 cluster C5). Was PARKED on a typedef fork through wave Q5.
//
// ---- The console body (0x827ABBD0..0x827ABC4C), instruction for instruction ---------------
//   prologue mr r30,r4 / mr r29,r5:
//     r4 = lpRaceCarInputBuffer_PrePhysics (dest), r5 = lpSceneModuleOutputBuffer (src);
//     r3 (the WorldModule `this`) is overwritten at 0x827ABC40 and never read.
//   0x827ABBE8  cmplwi r30,0 ; bne -> if (!dest) FireAssert(
//                 "lpRaceCarInputBuffer_PrePhysics != NULL", <this file>, 0xAB == 171)
//   0x827ABC10  cmplwi r29,0 ; bne -> if (!src)  FireAssert(
//                 "lpSceneModuleOutputBuffer != NULL",       <this file>, 0xAC == 172)
//     Both are NON-gating (the console falls through after firing), so no early return.
//   0x827ABC34  mr r3,r29
//   0x827ABC38  bl 0x8279C098  SceneManagerIO::OutputBuffer::GetPotentialContactQueue() const
//                              read-lock (bit 4), CgsSceneManagerModuleIO.h:625, this+32800
//   0x827ABC3C  mr r4,r3 ; 0x827ABC40 mr r3,r30
//   0x827ABC44  bl 0x827A9840  RaceCarEntityModuleIO::InputBuffer_PrePhysics::
//                              SetPotentialContactQueue   (write-lock; Clear + Append,
//                              BrnRaceCarEntityModuleIO.cpp:839)
//   0x827ABC4C  b __restgprlr_29   (the tail forwards the setter's register as an artifact;
//                                   the logical return type is void)
//
//   ⚠ THE SOURCE PARAMETER IS NAMED FROM THE CONSOLE, NOT FROM THE SIBLING BELOW: the assert
//   string at 0x827ABC24 is literally "lpSceneModuleOutputBuffer != NULL". The traffic twin
//   @0x827ABC50 has NO asserts, so its `lpSceneContactsFromWorld` spelling is a tree
//   convention with no console attestation; this one's name is attested.
//
// ---- WHY IT WAS PARKED, AND WHAT UNPARKED IT ----------------------------------------------
// The blocker was a TYPE FORK, measured as a compiler diagnostic (wave Q5,
// scratchpad/waveQ5/probe_f2/probe_racecar_setter.cpp):
//     error C2664: cannot convert argument 1 from
//       'const CgsSceneManager::SceneManagerIO::OutputBuffer::OutPotentialContactQueue *'
//       to 'const BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::
//           PotentialContactQueue *'
// The console has ONE type here -- DWARF BrnRaceCarEntityModuleIO.h:89 is
//   typedef OutputBuffer::OutPotentialContactQueue PotentialContactQueue;
// -- but this tree modelled it as a DERIVED struct
// (BrnRaceCarEntityModuleIOQueues.h), which is a distinct type: base -> derived does not
// convert, and static_cast'ing down onto an object that really is the base is UB. That fork is
// COLLAPSED to the DWARF typedef as of this wave. It is offset-neutral by MEASUREMENT
// (scratchpad/waveQ6/probe_rcfork/): PotentialContactQueue stays 163856 / align 16 and
// InputBuffer_PrePhysics stays 212160 / align 16, before and after. The `alignas(16)` the
// struct carried was redundant -- the element PotentialContact is itself alignas(16).
//
// ---- ⛔⛔ MANDATORY COMPANION EDIT -- THIS BODY IS A BOOT KILLER WITHOUT IT ⛔⛔ ------------
// `BrnRaceCarEntityModuleIO.h` (READ-ONLY to this cluster; reported to the conductor) needs
// ONE line added to RaceCarEntityModuleIO::InputBuffer_PrePhysics::Construct():
//     mPotentialContactQueue.Construct();          // console: PotentialContact<2048>, +16
// The console's own Construct @0x822EA6F0 does it -- that very call is already NAMED in the
// comment directly above that method ("PotentialContact<2048>::Construct(+16)") -- but the PC
// slice never made the call, because while nothing wrote the member it did not matter.
// It matters now: SetPotentialContactQueue does Clear() + Append(), and Append memcpy's into
// mpEvents. CreateIOBuffer<T> stopped zero-filling on 2026-08-15, so mpEvents is UNINITIALISED
// GARBAGE, not NULL -- the 'mpEvents != NULL' tripwire is non-gating and would pass on garbage,
// and the memcpy then writes up to 2048*80 bytes through it.
// THIS IS NOT A HYPOTHESIS: the traffic twin hit exactly this on the wave Q5 round-3
// integration and the fix is recorded in BrnTrafficEntityModuleIO.h:212-215 ("without it the
// base IOBuffer::Construct ran instead and the first BridgeSceneContactsToTrafficModule_
// PrePhysics ... died on 'mpEvents != NULL' inside SetOverlapPairsQueue -- the
// never-Constructed-queue IO trap").
//
// ---- WHAT LANDING THIS BUYS, HONESTLY -----------------------------------------------------
// The buffer's mPotentialContactQueue has been empty every frame since the module was mounted,
// because this bridge was its ONLY producer. It is filled now. It is NOT yet consumed:
// `InputBuffer_PrePhysics::GetPotentialContactQueue() const` (BrnRaceCarEntityModuleIO.h:547,
// DWARF :412) is declaration-only in this tree -- no body, and no WorldLinkStubs gate standing
// in for one -- so the first race-car reader will need that body written before it links. This
// leg is the producer half, and only the producer half.
// It is OFF the smashed-prop motion path (that runs through WorldBridgePropModule.cpp), so it
// neither helps nor hinders wave Q6's headline goal.
// =================================================================================================
void BridgeSceneContactsToRaceCarModule_PrePhysics(
    void* lpWorldModule,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
    const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneModuleOutputBuffer)
{
    (void)lpWorldModule;   // X360 r3 -- overwritten at 0x827ABC40 and never read

    CGS_ASSERT(lpRaceCarInputBuffer_PrePhysics != 0, "lpRaceCarInputBuffer_PrePhysics != NULL"); // :171
    CGS_ASSERT(lpSceneModuleOutputBuffer != 0, "lpSceneModuleOutputBuffer != NULL");             // :172

    // Source getter (read-lock), then destination setter (write-lock) -- the console's own
    // order, and its own register dance (bl getter ; mr r4,r3 ; mr r3,r30 ; bl setter).
    // Both getters are lock tripwires, so the order is behaviour, not style. Exactly ONE
    // getter call, as on the console.
    const CgsSceneManager::SceneManagerIO::OutputBuffer::OutPotentialContactQueue* lpSceneQueue =
        lpSceneModuleOutputBuffer->GetPotentialContactQueue();      // @0x8279C098

    lpRaceCarInputBuffer_PrePhysics->SetPotentialContactQueue(lpSceneQueue);   // @0x827A9840

    // ---- [DIAG] NOT IN THE X360 BINARY -------------------------------------------------
    // One-shot, behind BRN_PROP_DIAG. The race-car pre-physics potential-contact queue has
    // been empty every frame since the module was mounted (this bridge was an inert stub),
    // so "did anything actually cross?" has never been answerable. Reads the SOURCE queue
    // through the pointer already fetched above -- no second getter, no second lock
    // tripwire, and no read of the destination (whose const getter has no body in the tree).
    {
        static const bool sbPropDiag = (getenv("BRN_PROP_DIAG") != 0);
        static bool sbLoggedFirst = false;
        if ( sbPropDiag && !sbLoggedFirst && lpSceneQueue != 0
             && lpSceneQueue->GetLength() > 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLoggedFirst = true;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-rc] first scene->race-car potential contacts: "
                << lpSceneQueue->GetLength() << " [DIAG]\n";
        }
    }
}

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
// instantiations -- so there is no cast in this body. (⚠️ CORRECTED wave Q6 round 1, rcfork #2:
// this parenthetical used to read "That is the difference from the race-car sibling parked above,
// whose queue type this tree forked into a derived struct." BOTH halves are now false and the
// sentence was going stale in the helpful direction -- a later reader could have re-parked or
// re-forked on the strength of it. The race-car sibling directly above is BODIED at :116, and its
// queue type is no longer a derived struct: BrnRaceCarEntityModuleIOQueues.h:175 is a typedef of
// the same EventQueue instantiation as of this same wave, which is exactly what unparked it. The
// two legs now behave identically.)
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
