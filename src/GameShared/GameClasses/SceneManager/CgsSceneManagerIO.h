#pragma once

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_CoarseQueryQueue.h" // InCoarseQueryQueue<N>
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                    // CgsModule::IOBuffer
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT (lock tripwires)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<32768,16>
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"  // InSceneUpdateInterface (canonical home)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h" // TriangleCacheInterface (OutputBuffer::mTriangleCacheInterface)
// ---- OutputBuffer's four output queues (all DWARF CgsSceneManagerModuleIO.h members) ----
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                        // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"        // SceneManagerIO::OutErrorQueue<N> (:141)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryResultsQueue.h" // OutSceneQueryResultsQueue<32768> (:303)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"   // SceneManagerIO::PotentialContact (:290 element)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h" // SceneManagerIO::OutOverlapPair (:300 element)

// CgsSceneManager::SceneManagerIO - the scene-manager module's IO buffers the world
// bridges drive. X360 anchors:
//   InputBuffer_Update::GetInSceneUpdateInterface() @ 0x825BD8C0
//     write-lock (bit 3, "Not locked for writing\n", assert line 463) -> this + 16
//   OutputBuffer::GetResultsQueue() const @ 0x823B1ED0
//     read-lock (bit 4, "Not locked for reading\n", line 624) -> this + 4
//     (the getter's IDA symbol is truncated to "CgsSceneManager::SceneM"; the DWARF
//      names it GetResultsQueue -- CgsSceneManagerModuleIO.h:624)
// InputBuffer_Update's Construct/Destruct are real committed symbols
// (@0x828C7B80 / @0x828BAD00, their own TUs).
//
// FLAG (still MINIMAL, and which one): InputBuffer_Query is a documented sized slice
// (see its own banner). OutputBuffer is NO LONGER a slice as of wave Q5 -- its five
// DWARF members are all present and its Construct/Destruct are the console bodies.
// InputBuffer_Update carries only the scene-update aggregate the X360 Construct brings up.
namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // InSceneUpdateInterface's canonical home is CgsSceneManagerIO_SceneUpdate.h
    // (included above); its whole-interface Append (the merge the physics->scene
    // bridge drives) is declared there.

    // The scene-manager update-phase input buffer.
    struct InputBuffer_Update : public CgsModule::IOBuffer
    {
        // The Construct CreateIOBuffer<T> runs after the alloc (X360 instantiation
        // @0x827B59F0): raise the IOBuffer status base, then bring up the embedded
        // scene-update aggregate (the X360 InputBuffer_Update::Construct constructs the
        // aggregate's 25 queues -- see InSceneUpdateInterface::Construct @0x822E6550).
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mInSceneUpdateInterface.Construct();
        }

        // @ 0x825BD8C0 -- write-lock tripwire, then the in-scene-update aggregate.
        InSceneUpdateInterface* GetInSceneUpdateInterface();   // +16, write

        // @ 0x828AF1C8 -- the CONST overload, and it is a DIFFERENT FUNCTION with a
        // DIFFERENT tripwire. Added 2026-08-10 (spatial-partition wave) after the
        // write-locked one above cost a whole verification run.
        //
        // The two are a matched pair in the console and differ in exactly the bit they
        // test and the message they fire:
        //   0x828AF1C8  `((*a1 >> 4) & 1) == 0` -> "Not locked for reading\n"
        //               CgsSceneManagerModuleIO.h:462     <- CONST, read lock  (bit 4)
        //   0x825BD8C0  `((*a1 >> 3) & 1) == 0` -> "Not locked for writing\n"
        //               CgsSceneManagerModuleIO.h:463     <- non-const, write lock (bit 3)
        // Both `return a1 + 16`.
        //
        // ⭐ WHICH ONE A CALLER WANTS IS DECIDED BY ITS LOCK, NOT BY ITS CONSTNESS OF
        // INTENT: SceneManagerModule::StartUpdateTriangleCache @0x828C73D8 takes a READ
        // lock and therefore calls 0x828AF1C8, while UpdateScene takes a WRITE lock and
        // calls 0x825BD8C0. Calling the write one under a read lock fires 927 asserts and
        // wedges the boot in FLYBY -- measured, not hypothesised.
        const InSceneUpdateInterface* GetInSceneUpdateInterface() const;   // +16, read

        // DWARF CgsSceneManagerModuleIO.h:164/:462/:463 -- the nested alias and the plain
        // (unlocked) accessor pair for the same embedded member.
        // ⚠️ MOVED HERE 2026-08-03 (task #123) FROM A DUPLICATE DEFINITION OF THIS WHOLE STRUCT.
        // SharedIO/CgsInputBufferUpdate.h used to define a SECOND, DIFFERENT
        // CgsSceneManager::SceneManagerIO::InputBuffer_Update -- same namespace, same name, but
        // WITHOUT the maStatusPad[15] that puts the interface on its asm-attested +16, and with
        // this accessor pair instead of GetInSceneUpdateInterface(). The two never met in one TU
        // until PhysicsModule embedded the deformation manager, at which point it was a hard
        // C2011. This home wins because it is the asm-attested one (+16 and the out-of-line
        // symbol @0x825BD8C0); the duplicate's layout was simply wrong, so folding it here also
        // retires a latent wrong-offset bug on the deformation path. The accessor is kept under
        // its DWARF name because that is what the two call sites in
        // BrnDeformationManager_Contacts.cpp use, and it is carried over verbatim -- a plain
        // getter with no lock tripwire, exactly as the retired header had it.
        typedef InSceneUpdateInterface InSmSceneUpdateInterface;                                   // DWARF :164
        const InSmSceneUpdateInterface* GetSceneUpdateInterface() const { return &mInSceneUpdateInterface; } // :462
        InSmSceneUpdateInterface*       GetSceneUpdateInterface()       { return &mInSceneUpdateInterface; } // :463

    private:
        u8                     maStatusPad[15];           // +1..+15 (force +16)
        InSceneUpdateInterface mInSceneUpdateInterface;   // +16
    };

    // The scene-manager output buffer (query results side).
    // ------------------------------------------------------------------------
    // SceneManagerIO::InputBuffer_Query -- the per-module scene-query input buffer
    // the entity-module spines stack-allocate for each query round-trip
    // (WorldModule::EntityModulePostSceneUpdate @0x827C3C58 does three of them).
    //
    // FLAG (minimal-complete slice, size NOT X360-attested): this buffer's real
    // aggregate is the coarse/line-test/etc. query queues (see the sibling
    // CgsSceneManagerIO_*Query.h element homes); that layout belongs to this
    // buffer's own TU and is NOT recovered here. The slice exists so the spines'
    // CreateIOBuffer<InputBuffer_Query> instantiates against a real type. GROW it
    // (and re-check every CreateIOBuffer call site) when the query-buffer TU lands
    // -- do NOT treat maDeferredPayload's size as fact.
    // ------------------------------------------------------------------------
    struct InputBuffer_Query : public CgsModule::IOBuffer
    {
        // X360 0x828C7BC0 -- IOBuffer status, then VariableEventQueue<16384,16>::Construct
        // on the coarse-query queue (this+40) followed by the nine typed line/volume/sphere
        // test queues and the nine cached pointers to them (this+4..+36). Only the coarse
        // queue is committed on this side, so the PARTIAL SLICE below runs the base status +
        // that queue; the nine typed sub-queues land with their own TUs [marked deviation].
        // (Was a declaration-only accessor whose WorldLinkStubs body asserted; every
        // per-frame scene-query block Locks this buffer, so the trap stopped the drive.)
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mInCoarseQueryQueue.Construct();
        }
        void Destruct();

        // @ 0x828AF270 -- the read-locked handle to the embedded coarse-query queue
        // (X360 this+0x28). SceneManagerModule::ProcessFrustumTestJobRequests walks it;
        // the query PRODUCERS reach it write-locked, so the lock bit is not asserted here
        // (the X360 getter has one overload per lock state and the IDB keeps only one).
        InCoarseQueryQueue<16384>* GetInCoarseQueryQueue() { return &mInCoarseQueryQueue; }

        InCoarseQueryQueue<16384> mInCoarseQueryQueue;   // (see FLAG: position not attested)
    };

    // ------------------------------------------------------------------------
    // The coarse (frustum/sphere/volume) query RESULT record, as it is written into
    // OutputBuffer::mSceneQueryResultsQueue.
    //
    // Producer: SceneManagerModule::ProcessFrustumTestJobResults @0x828C7838 --
    //   Event* e = queue.AllocateEvent(0, 4 * (numResults + 3));
    //   e[0] = the query id;  e[1] = numResults;  e[2] = numResults;
    //   e[3..] = one EntityId per result (the octree hands back pool INDICES; the
    //            producer resolves each through the entity manager first).
    // Consumer: WorldModule::FilterFrustumTestResults @0x827BDA60 -- reads the count
    //   at +4 and walks the ids from +12 in 4-byte steps, and the dispatch producer
    //   reads the leading SceneQueryId to assert the batches arrive in query order.
    // Record TYPE ID is 0 (the X360 passes `li r4, 0` to AllocateEvent).
    //
    // The two counts are written from the same register; the second is the count of
    // ids ATTEMPTED vs WRITTEN in the coarse buffer's batch header sense (they are
    // equal on every path the producer can take -- it asserts that in the octree
    // drain), so both are recorded rather than collapsed.
    // ------------------------------------------------------------------------
    struct OutCoarseQueryResult : public CgsModule::Event
    {
        static const s32 KI_EVENT_TYPE = 0;

        SceneQueryId mQueryId;              // +0x00
        s32          miNumResults;          // +0x04
        s32          miNumResultsAttempted; // +0x08
        // EntityId maEntityIds[miNumResults] follows at +0x0C.

        CgsSceneManager::EntityId*       GetEntityIds()
        { return reinterpret_cast<CgsSceneManager::EntityId*>(this + 1); }
        const CgsSceneManager::EntityId* GetEntityIds() const
        { return reinterpret_cast<const CgsSceneManager::EntityId*>(this + 1); }
    };

    // ========================================================================
    // SceneManagerIO::OutputBuffer -- the scene manager's whole output payload.
    //
    // ⭐ THE THREE MISSING SEATS LANDED 2026-08-18 (wave Q5, scene-collision middle).
    // The struct used to be a documented MINIMAL slice (results ring + triangle-cache
    // interface only) and the FLAG in its Construct admitted the gap. The full member
    // set is now DWARF-shaped and console-sized:
    //
    //   DWARF CgsSceneManagerModuleIO.h:612 (struct), members :640/:643/:646/:649/:652
    //   in that order, with typedefs :303 / :290 / :300 / :304.
    //
    //   console offset   member                    type                                  extent
    //   +0               IOBuffer status byte      (base)                                1 (+3 pad)
    //   +4        (:640) mResultsQueue             OutSceneQueryResultsQueue<32768>       32796
    //   +32800    (:643) mPotentialContactQueue    EventQueue<PotentialContact,2048>     163856 = 16 + 2048*80
    //   +196656   (:646) mOverlapPairsQueue        EventQueue<OutOverlapPair,128>          3088 = 16 + 128*24
    //   +199744   (:649) mErrorQueue               OutErrorQueue<128>                     17420 = 12 + 128*136
    //   +217164   (:652) mTriangleCacheInterface   TriangleCacheInterface                     4
    //                                                                          sizeof = 217168
    // The three queue offsets come straight out of OutputBuffer::Construct @0x828C7CA0
    // (`addis r31,1 / addi -0x7FE0` = +0x8020, `addis 3 / addi 0x30` = +0x30030,
    // `addis 3 / addi 0xC40` = +0x30C40) and sizeof from the CreateIOBuffer<OutputBuffer>
    // instantiation @0x823AF668 (`IOBufferStack::Alloc(this, 0x35050, name)`).
    //
    // ⚠️ CONSOLE OFFSETS ARE COMMENTS, NOT LAYOUT. On this LLP64 host the queues are wider
    // (VolumeInstanceId/pointers), so the members land wherever the compiler puts them --
    // every access here and downstream is BY NAME. Do NOT re-pad to hit +32800.
    // ========================================================================
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // ---- DWARF typedefs (all four are the member types, in DWARF order) ----
        typedef OutSceneQueryResultsQueue<32768>                             OutSmSceneQueryResultsQueue;  // :303
        typedef CgsModule::EventQueue<PotentialContact, 2048>                OutPotentialContactQueue;     // :290
        typedef CgsModule::EventQueue<OutOverlapPair, 128>                   OutOverlapPairsQueue;         // :300
        typedef OutErrorQueue<128>                                           OutSmErrorQueue;              // :304

        // Pre-DWARF spelling of the results-queue type, kept because the existing call
        // sites (WorldBridgeSceneToEntityModules/ToPhysics, BrnWorldModule, the frustum
        // producer) name the accessors below by it. It is the BASE of the DWARF type --
        // OutSceneQueryResultsQueue<32768> : VariableEventQueue<32768,16> -- so the two
        // spellings describe one member, not two.
        typedef CgsModule::VariableEventQueue<32768, 16> SceneQueryResultsQueue;

        // X360 0x828C7CA0 / 0x828BAD38. Both are OUT-OF-LINE in the console's own
        // CgsSceneManagerModuleIO.cpp (their asserts bake that path at lines 224 and 256),
        // and that file is this tree's CgsSceneManagerModuleIO.cpp -- where the bodies live.
        void Construct();   // DWARF :617
        void Destruct();    // DWARF :621

        // ---- the four const (READ-locked) getters, DWARF :624-:628 ----

        // @ 0x823B1ED0 -- read-lock tripwire (bit 4, "Not locked for reading"), `this+4`,
        // baked line 624. Every scene-query round trip in
        // WorldModule::EntityModulePostSceneUpdate reads the results through it.
        const OutSmSceneQueryResultsQueue* GetResultsQueue() const           // :624
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
            return &mResultsQueue;
        }

        // ⭐ @ 0x8279C098 -- read-lock (bit 4), `this+32800`, baked line 625. THE SEAT THE
        // WHOLE PROP-CONTACT PATH DRAINS: WorldModule::BridgeSceneContactsToPropModule_
        // PrePhysics @0x827ABCB0, BridgeSceneContactsToRaceCarModule_PrePhysics @0x827ABBD0,
        // BridgeSceneContactsToTrafficModule_PrePhysics @0x827ABC50 and
        // BridgeScenePotentialContactsToPhysics @0x827ABD80 all call exactly this function.
        const OutPotentialContactQueue* GetPotentialContactQueue() const     // :625
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
            return &mPotentialContactQueue;
        }

        // :626. The console emits NO out-of-line body for this overload (no caller in the
        // ARTIST image -- the error queue is filled but never drained on the shipped path),
        // so the lock bit is taken from the DWARF constness + the family's proven
        // const-reads / non-const-writes pattern rather than from an asm tripwire. FLAG:
        // lock bit inferred, offset and existence are DWARF-attested. There is deliberately
        // NO non-const overload -- the DWARF declares none.
        const OutSmErrorQueue* GetErrorQueue() const                        // :626
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
            return &mErrorQueue;
        }

        // @ 0x8279C140 -- read-lock (bit 4), `this+196656`, baked line 627. Drained by
        // WorldModule::BridgeSceneContactsToTrafficModule_PrePhysics @0x827ABC50 and
        // BridgeScenePotentialContactsToPhysics @0x827ABD80 (the traffic overlap feed).
        const OutOverlapPairsQueue* GetOverlapPairsQueue() const             // :627
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
            return &mOverlapPairsQueue;
        }

        // ---- the non-const (WRITE-locked) getters, DWARF :631-:634 ----

        // X360 0x828AF900, guarded on the WRITE lock: SceneManagerModule::
        // ProcessFrustumTestJobResults @0x828C7838 allocates one variable event per
        // coarse-result batch through it.
        OutSmSceneQueryResultsQueue* GetResultsQueue()                       // :631
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mResultsQueue;
        }

        // ⭐ @ 0x828AF9A8 -- write-lock (bit 3, "Not locked for writing"), `this+32800`,
        // baked line 632. Its ONE caller is SceneManagerModule::
        // BridgeOverlapCullerToOutputBuffer @0x828BA8C8 -- the producer that turns each
        // culler Contact into a PotentialContact. Round 3/4 writes through this.
        OutPotentialContactQueue* GetPotentialContactQueue()                 // :632
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mPotentialContactQueue;
        }

        // @ 0x828AFA50 -- write-lock (bit 3), `this+196656`, baked line 633. Its ONE caller
        // is SceneManagerModule::BridgeOverlapGenerationToOutputBuffer @0x828BA6A0.
        OutOverlapPairsQueue* GetOverlapPairsQueue()                         // :633
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mOverlapPairsQueue;
        }

        // ---- pre-DWARF accessor spellings kept for the existing call sites ----
        // Same member, same tripwire, same console symbol as GetResultsQueue()/() const
        // above; they return the BASE type because that is what the call sites bind to.
        const SceneQueryResultsQueue* GetSceneQueryResultsQueue() const { return GetResultsQueue(); }
        SceneQueryResultsQueue*       GetSceneQueryResultsQueueForWrite() { return GetResultsQueue(); }

        // ⭐ ADDED 2026-08-11 (triangle-cache wiring wave). THE HANDOFF SEAT OF THE WHOLE
        // TRIANGLE-CACHE CHAIN: the scene manager publishes &mTriangleCacheManager here, and
        // every physics/effects consumer downstream reaches the cache only through a copy of
        // this pointer. Both overloads are DWARF-declared (CgsSceneManagerModuleIO.h:628 const,
        // :634 non-const) and both have real out-of-line X360 symbols whose baked assert LINE
        // is the identification:
        //   const:     0x8279C1E8  bit-4 test -> "Not locked for reading\n"  :628 (0x274)
        //   non-const: 0x828AFAF8  bit-3 test -> "Not locked for writing\n"  :634 (0x27A)
        // Both `return this + 217164` (0x3504C) on the console.
        //
        // ⚠️ WHICH OVERLOAD A CALLER GETS IS DECIDED BY ITS LOCK, exactly like the
        // InputBuffer_Update::GetInSceneUpdateInterface pair above: the SEEDERS
        // (SceneManagerModule::ProcessSceneQueries @0x828D57D0 / UpdateScene @0x828D4C28) hold
        // a WRITE lock, the CARRIERS (WorldModule::BridgeSceneQueryResultsToPhysics @0x827A8E88
        // / BridgeSceneModuleToOutput @0x827A5700) hold a READ lock.
        const TriangleCacheInterface* GetTriangleCacheInterface() const
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
            return &mTriangleCacheInterface;
        }
        TriangleCacheInterface* GetTriangleCacheInterface()
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mTriangleCacheInterface;
        }

    private:
        u8                          maStatusPad[3];           // +1..+3 (force +4)
        OutSmSceneQueryResultsQueue mResultsQueue;            // :640  console +4
        OutPotentialContactQueue    mPotentialContactQueue;   // :643  console +32800  (0x8020)
        OutOverlapPairsQueue        mOverlapPairsQueue;       // :646  console +196656 (0x30030)
        OutSmErrorQueue             mErrorQueue;              // :649  console +199744 (0x30C40)
        TriangleCacheInterface      mTriangleCacheInterface;  // :652  console +217164 (0x3504C)
    };
}
}
