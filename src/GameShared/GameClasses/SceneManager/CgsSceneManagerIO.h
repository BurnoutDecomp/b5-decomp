#pragma once

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_CoarseQueryQueue.h" // InCoarseQueryQueue<N>
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                    // CgsModule::IOBuffer
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT (lock tripwires)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<32768,16>
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"  // InSceneUpdateInterface (canonical home)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h" // TriangleCacheInterface (OutputBuffer::mTriangleCacheInterface)

// CgsSceneManager::SceneManagerIO - the scene-manager module's IO buffer slices
// the world bridges drive. FLAG: MINIMAL slices -- only the accessors the bridges
// call are modelled (per the BrnPhysicsModuleIO.h / BrnCrashModuleIO.h precedent);
// the full buffer payloads are owned by the scene-manager IO TUs. X360 anchors:
//   InputBuffer_Update::GetInSceneUpdateInterface() @ 0x825BD8C0
//     write-lock (bit 3, "Not locked for writing\n", assert line 463) -> this + 16
//   OutputBuffer::GetSceneQueryResultsQueue() const @ 0x823B1ED0
//     read-lock (bit 4, "Not locked for reading\n") -> this + 4
//     (FLAG: the getter's IDA symbol is truncated to "CgsSceneManager::SceneM";
//      named here after the queue the scene->race-car bridge Appends from)
// InputBuffer_Update's Construct/Destruct are real committed symbols
// (@0x828C7B80 / @0x828BAD00, their own TUs).
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

    struct OutputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::VariableEventQueue<32768, 16> SceneQueryResultsQueue;

        // The Construct CreateIOBuffer<T> runs after the alloc (X360 instantiation
        // @0x823AF668): raise the IOBuffer status base + bring up the query-results ring.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mSceneQueryResultsQueue.Construct();
            // ⭐ 2026-08-15 (IO-buffer zero-fill removal audit) -- MISSING STORE.
            // *(this+217164) = 0 @0x828C7CA0 is this pointer. It was omitted, and it only
            // ever survived because the old PC CreateIOBuffer<T> value-initialised the whole
            // buffer; with default-init the slot holds the previous IO-stack tenant's bytes,
            // which SURVIVES GetCache()'s "mpTriangleCacheManager != NULL" tripwire and is
            // then dereferenced. Same defect class as SimpleDataStreamProducer's cursor.
            mTriangleCacheInterface.mpTriangleCacheManager = 0;
            // The console's tail `VariableEventQueue<32768,16>::Clear(this+4)` @0x828C7CA0.
            mSceneQueryResultsQueue.Clear();
            // [FLAG] the console Construct also runs PotentialContact<2048>/ErrorEvent<128>/
            // OutOverlapPair<128> Constructs at +32800/+199744/+196656 -- those three queues
            // are not members of this documented MINIMAL slice, so they cannot be reached by
            // name yet.
            // ⚠️ CORRECTED 2026-08-15: the assert on `mResultsQueue.Prepare()` that used to be
            // listed alongside them as "not a member" IS a member -- mResultsQueue is this
            // buffer's own mSceneQueryResultsQueue. It is not omitted, it is already covered:
            // VariableEventQueue<...>::Prepare is nothing but the "Not Constructed" assert plus
            // Clear() and `return true` (CgsVariableEventQueue.h), and both the Construct and
            // the Clear above are made. Nothing to restore for that leg.
        }

        // @ 0x823B1ED0 -- read-lock tripwire, then the query-results ring. (Was a
        // declaration-only accessor whose WorldLinkStubs body asserted; every scene-query
        // round trip in WorldModule::EntityModulePostSceneUpdate reads the results
        // through it, so the trap stopped the world drive. The member IS committed, so
        // this is the real body.)
        const SceneQueryResultsQueue* GetSceneQueryResultsQueue() const
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
            return &mSceneQueryResultsQueue;
        }

        // The producer-side handle (X360 0x828AF900, guarded on the WRITE lock):
        // SceneManagerModule::ProcessFrustumTestJobResults @0x828C7838 allocates one
        // variable event per coarse-result batch through it.
        SceneQueryResultsQueue* GetSceneQueryResultsQueueForWrite()
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mSceneQueryResultsQueue;
        }

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
        u8                     maStatusPad[3];            // +1..+3 (force +4)
        SceneQueryResultsQueue mSceneQueryResultsQueue;   // +4
        // DWARF CgsSceneManagerModuleIO.h:652. Console seat +217164; placed by NAME after the
        // results ring because this whole buffer is the documented MINIMAL slice above (the
        // overlap-pair / error queues that fill the gap belong to their own TUs). Nothing
        // addresses it by offset -- both accessors take its address.
        TriangleCacheInterface mTriangleCacheInterface;   // X360 +217164 (0x3504C)
    };
}
}
