// ============================================================================
// GameSource/World/Bridges/WorldBridgeSceneToPhysics.cpp
//   (original-source home confirmed by the assert strings the console bakes into
//    both bodies: "d:\p4\b5_main\burnout\main\code\gamesource\unity\../World/
//    Bridges/WorldBridgeSceneToPhysics.cpp")
//
// The scene -> physics half of WorldModule::Update's scene-query round trip.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, named members).
//
//   BridgeSceneQueryResultsToPhysics       @ 0x827A8E88   (88 insns)  [2026-08-11]
//   BridgeScenePotentialContactsToPhysics  @ 0x827ABD80   (41 insns)  [2026-08-19, wave Q5/F2]
//
// ⭐ WHY THIS FILE EXISTS NOW (2026-08-11, triangle-cache wiring wave). The traction-line
// leg was dying with "mpTriangleCacheManager != NULL" and then an AV inside
// TriangleCacheManager::GetTrianglesForCachedObject the moment the create drain put a race
// car in mUsedRaceCars. The cause was not the cache manager (it is live -- slots claimed,
// Prepare reached) but the HANDOFF: the physics side reads
//   lpInputBuffer->GetVehicleInputInterface()->GetTriangleCacheInterface()
// and the ONLY code in the whole program that ever writes that interface's manager pointer
// is the tail of this bridge. It was an inert one-shot-log gate in WorldLinkStubs.cpp:3298,
// so the pointer stayed NULL from boot.
//
// Full console chain, by address:
//   SceneManagerModule::ProcessSceneQueries @0x828D57D0   seeds the SCENE OUTPUT buffer
//       -> SceneManagerIO::OutputBuffer::GetTriangleCacheInterface() @0x828AFAF8 (write)
//       -> TriangleCacheInterface::SetTriangleCacheManager(&mTriangleCacheManager) [inlined]
//   THIS BRIDGE @0x827A8E88                               carries it to PHYSICS
//       -> OutputBuffer::GetTriangleCacheInterface() const @0x8279C1E8 (read)
//       -> VehicleInputInterface::AppendTriangleCacheInterface @0x8279B978
//   WorldModule::BridgeSceneModuleToOutput @0x827A5700     carries it to the WORLD OUTPUT
//       -> UpdateOutputBuffer::AppendTriangleCacheInterface @0x8279BAF8
// ============================================================================

#include "GameSource/World/Bridges/WorldBridgeSceneToPhysics.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                      // CgsDev::Log::gpDebugPrint ([DIAG])
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"        // OutEventLineTestNearestResult
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h" // VehicleInputInterface
#include <stdlib.h>                                                             // getenv ([DIAG] BRN_PROP_DIAG, host only)

namespace WorldModule
{
    // ========================================================================
    // BridgeSceneQueryResultsToPhysics @ 0x827A8E88   (88 insns)
    //
    // Two null tripwires (WorldBridgeSceneToPhysics.cpp:40 / :41 -- the console's `li r5,
    // 0x28` / `li r5, 0x29`), then:
    //
    //   lpQueryResults     = lpSceneModuleOutputBuffer->GetSceneQueryResultsQueue();  // 0x823B1ED0, read-locked
    //   lpVehicleInterface = lpPhysicsModuleInputBuffer->GetVehicleInputInterface();  // 0x8279ED28, write-locked
    //
    //   walk the variable results queue with GetFirstEvent/GetNextEvent and switch on the
    //   record type (the console's 7-case jump table @0x827A8F44):
    //       cases 0, 1, 6  -> ignored here (coarse / fast-double-sided / the sixth result
    //                         kind; the race-car and traffic modules consume those)
    //       case 2         -> OutEventLineTestNearestResult: the console copies the whole
    //                         64-byte record off the queue into a stack temporary (the
    //                         `ld/std` x8 loop @0x827A8F74) and hands it to
    //                         AddLineTestResult BY VALUE, which inlines to
    //                         mLineTestResultsQueue.AddEvent (bl sub_827A5780 with r3 still
    //                         the interface base == &mLineTestResultsQueue at +0).
    //       default (3-5)  -> the "Unrecognised event type\n" tripwire at :92 (`li r5, 0x5C`)
    //
    //   THEN, after the loop (0x827A8FCC..0x827A8FDC -- the order is the asm's, not the
    //   DWARF listing's):
    //       lpVehicleInterface->AppendTriangleCacheInterface(
    //           lpSceneModuleOutputBuffer->GetTriangleCacheInterface() );
    //
    // The leading lpWorldModule arg is the console's r3 (the WorldModule `this`); the body
    // never dereferences it -- confirmed in the asm, r3 is overwritten at 0x827A8EEC before
    // any use.
    // ========================================================================
    void BridgeSceneQueryResultsToPhysics(
        void* /*lpWorldModule*/,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneModuleOutputBuffer)
    {
        CGS_ASSERT(lpPhysicsModuleInputBuffer != 0, "lpPhysicsModuleInputBuffer != NULL");  // :40
        CGS_ASSERT(lpSceneModuleOutputBuffer != 0,  "lpSceneModuleOutputBuffer != NULL");   // :41

        if (lpPhysicsModuleInputBuffer == 0 || lpSceneModuleOutputBuffer == 0)
        {
            return;
        }

        const CgsSceneManager::SceneManagerIO::OutputBuffer::SceneQueryResultsQueue* lpQueryResults =
            lpSceneModuleOutputBuffer->GetSceneQueryResultsQueue();
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface =
            lpPhysicsModuleInputBuffer->GetVehicleInputInterface();

        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        for (s32 liId = lpQueryResults->GetFirstEvent(&lpEvent, &liSize);
             liId >= 0;
             liId = lpQueryResults->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            // ⚠️ THE RECORD-TYPE IDS ARE THE CONSOLE'S RAW JUMP-TABLE CASES, quoted as
            // literals ON PURPOSE. They are NOT CgsSceneManager::EQueryResultType (that enum
            // spells 3/4 as the volume tests, and this switch sends 3-5 to the "Unrecognised
            // event type" arm) -- they are the OutEvent record ids the results queue stamps:
            // 0 == the coarse batch (SceneManagerIO::OutCoarseQueryResult::KI_EVENT_TYPE),
            // 2 == OutEventLineTestNearestResult (the producer's own
            // AddTriangleCollisionLineTestNearestResult passes `2` to AddEvent), 1 and 6 are
            // the two other kinds this bridge ignores. Naming 1 and 6 would be invention;
            // they get named when their producers land.
            switch (liId)
            {
            case 0:   // coarse batch
            case 1:
            case 6:
                // Not this bridge's business (the entity modules drain these).
                break;

            case 2:   // OutEventLineTestNearestResult
            {
                // reinterpret_cast, not static_cast: VariableEventQueue hands back a
                // CgsModule::Event* into its packed byte buffer, while this record derives
                // from the UNRELATED SceneManagerIO::Event base -- the two hierarchies do not
                // meet, and on the console the record is simply the bytes at the queue cursor
                // (the sanctioned external-byte-stream case, same as every other reader of
                // this queue in the tree).
                // BY VALUE at the call, exactly as the console does it (the 64-byte stack copy).
                const CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult* lpCommand =
                    reinterpret_cast<const CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult*>(lpEvent);
                lpVehicleInterface->AddLineTestResult(*lpCommand);
                break;
            }

            default:
                CGS_ASSERT(false, "Unrecognised event type\n");   // :92
                break;
            }
        }

        // The handoff. GetTriangleCacheInterface() picks the CONST (read-locked) overload
        // @0x8279C1E8 because the source buffer is the read side of the bridge's lock pair.
        lpVehicleInterface->AppendTriangleCacheInterface(
            lpSceneModuleOutputBuffer->GetTriangleCacheInterface());
    }

    // ========================================================================
    // ⭐⭐ BridgeScenePotentialContactsToPhysics @ 0x827ABD80   (41 insns)
    //
    // The PHYSICS half of the scene's broad-phase output: the potential-contact pairs the
    // overlap culler produced, and the raw overlap pairs the generator produced, handed to
    // rw::physics through PhysicsModuleIO::InputBuffer. Its consumers are already real and
    // mounted -- PhysicsModule::Update's PotentialContactInterface::SetConstQueue
    // (BrnPhysicsModuleUpdateFunctions.cpp:304) reads mPotentialContactQueue and the two
    // GetOverlapPairsQueue() sites at :343/:389 read mOverlapPairsQueue -- so while this
    // bridge was the boot gate at WorldLinkStubs.cpp both queues read length 0 every frame.
    //
    // ---- The console body (0x827ABD80..0x827ABE20), instruction for instruction ----------
    //   r4 = lpPhysicsModuleInputBuffer (dest), r5 = lpSceneModuleOutputBuffer (src);
    //   r3 (the WorldModule `this`) is overwritten at 0x827ABDE4 and never read.
    //
    //   if (!dest) FireAssert("lpPhysicsModuleInputBuffer != NULL", <this file>, 0x75 == 117)
    //   if (!src)  FireAssert("lpSceneModuleOutputBuffer != NULL",  <this file>, 0x76 == 118)
    //
    //   0x827ABDE8  bl 0x8279C098  SceneManagerIO::OutputBuffer::GetPotentialContactQueue() const
    //                              read-lock (bit 4), CgsSceneManagerModuleIO.h:625, this+32800
    //   0x827ABDF4  bl 0x8279EFD8  PhysicsModuleIO::InputBuffer::GetPotentialContactQueue()
    //                              WRITE-lock (bit 3), BrnPhysicsModuleIO.h:290, this+160208
    //   0x827ABDFC  bl 0x827A6EF8  BaseEventQueue<PotentialContact>::Append(dest, src)
    //
    //   0x827ABE04  bl 0x8279C140  SceneManagerIO::OutputBuffer::GetOverlapPairsQueue() const
    //                              read-lock (bit 4), CgsSceneManagerModuleIO.h:627, this+196656
    //   0x827ABE10  bl 0x8279F080  PhysicsModuleIO::InputBuffer::GetOverlapPairsQueue()
    //                              WRITE-lock (bit 3), BrnPhysicsModuleIO.h:293, this+324064
    //   0x827ABE18  bl 0x827A6FE8  BaseEventQueue<OutOverlapPair>::Append(dest, src)
    //
    // ⚠️ IT IS `Append`, NOT `Set`. The two sibling bridges into the race-car and traffic
    // buffers call those buffers' Set*Queue setters, which Clear() the destination first
    // (X360 0x827A9840 / 0x827A9DE0 store 0 to miLength before appending). This one calls the
    // queue's Append DIRECTLY on both legs -- there is no `stw 0, 8(queue)` anywhere in the
    // body -- because PhysicsModule::Update drains and clears its own input queues. Adding a
    // Clear() here to "match the siblings" would be an invented behaviour.
    //
    // ⚠️ WHICH OVERLOAD: both destination getters are the NON-const (write-locked) ones. That
    // is not a style choice -- the console's tripwires are `extrwi r11,r11,1,28` (bit 3,
    // "Not locked for writing"). The bridge's caller, WorldModule::Update @0x827D63E8, holds
    // the physics input buffer write-locked and the scene output read-locked, which is exactly
    // what makes const-on-the-source / non-const-on-the-destination the right pair here.
    //
    // The X360 tail forwards the second Append's result as a register artifact; the logical
    // return type is void.
    //
    // ⭐ UNBLOCKED 2026-08-19 (wave Q5 cluster F2): the two source seats landed in wave Q5
    // round 2 (SceneManagerIO::OutputBuffer's mPotentialContactQueue/+32800 and
    // mOverlapPairsQueue/+196656 with their read accessors), and the two write-locked
    // destination accessors (:290/:293) are added additively to BrnPhysicsModuleIO.h by this
    // wave. All four types are the same two EventQueue instantiations, so there is no cast
    // anywhere in this body.
    // ========================================================================
    void BridgeScenePotentialContactsToPhysics(
        void* /*lpWorldModule*/,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneModuleOutputBuffer)
    {
        CGS_ASSERT(lpPhysicsModuleInputBuffer != 0, "lpPhysicsModuleInputBuffer != NULL");  // :117
        CGS_ASSERT(lpSceneModuleOutputBuffer != 0,  "lpSceneModuleOutputBuffer != NULL");   // :118

        // ---- leg 1: the culler's potential contacts ------------------------------------
        // Source getter first, destination getter second -- both are lock tripwires, so the
        // console's evaluation order is behaviour and the locals pin it.
        const CgsSceneManager::SceneManagerIO::OutputBuffer::OutPotentialContactQueue* const
            lpScenePotentialContacts = lpSceneModuleOutputBuffer->GetPotentialContactQueue();  // @0x8279C098
        CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>* const
            lpPhysicsPotentialContacts = lpPhysicsModuleInputBuffer->GetPotentialContactQueue(); // @0x8279EFD8

        // [DIAG] NOT IN THE X360 BINARY. Opt-in one-shot (BRN_PROP_DIAG), the physics-side twin
        // of the probe in WorldBridgePropModule.cpp: it answers "did the scene middle's output
        // reach rw::physics at all, or only the world modules?". Fires on the first non-empty
        // frame and never again. The latch is evaluated ONCE (getenv per frame is a syscall).
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static bool       sbLoggedFirstContacts = false;
            if ( sbPropDiag && !sbLoggedFirstContacts
                 && lpScenePotentialContacts->GetLength() > 0
                 && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedFirstContacts = true;
                *CgsDev::Log::gpDebugPrint
                    << "[Q5-world] first " << lpScenePotentialContacts->GetLength()
                    << " potential contacts -> physics\n";
            }
        }

        lpPhysicsPotentialContacts->Append(*lpScenePotentialContacts);                     // @0x827A6EF8

        // ---- leg 2: the generator's raw overlap pairs -----------------------------------
        const CgsSceneManager::SceneManagerIO::OutputBuffer::OutOverlapPairsQueue* const
            lpSceneOverlapPairs = lpSceneModuleOutputBuffer->GetOverlapPairsQueue();        // @0x8279C140
        CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>* const
            lpPhysicsOverlapPairs = lpPhysicsModuleInputBuffer->GetOverlapPairsQueue();     // @0x8279F080

        lpPhysicsOverlapPairs->Append(*lpSceneOverlapPairs);                               // @0x827A6FE8
    }
}
