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
// InputBuffer_Query's twelve queues + its leading SceneQueryInterface (scene-query wave 1):
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h"                    // SceneQueryInterface (:540) -- pulls the nine fine/tri-collision element homes
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestQuery.h"                     // InEventLineTest (:168 element, 0x30)

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

    // ------------------------------------------------------------------------
    // SceneManagerIO::InputBuffer_Query -- the per-module scene-query input buffer
    // the entity-module spines stack-allocate for each query round-trip
    // (WorldModule::EntityModulePostSceneUpdate @0x827C3C58 does three of them, and
    // WorldModule::Update's physics round trip a fourth -- the one that carries the race
    // car's above-ground down-rays).
    //
    // ⭐ FULL LAYOUT (scene-query wave 1, 2026-09-02). The "minimal-complete slice, size NOT
    // X360-attested" that stood here -- the coarse queue alone, 16 KB -- is RETIRED. The
    // aggregate is the DWARF's (CgsSceneManagerModuleIO.h:540..:555) and every member's
    // console offset is pinned by the buffer's own Construct @0x828C7BC0, which brings the
    // twelve queues up in member order and then writes the nine fine/tri-collision queue
    // addresses into the leading SceneQueryInterface (this+4..+36):
    //
    //   console  member                                   DWARF   console span
    //   +0       IOBuffer status byte
    //   +4       mSceneQueryInterface   (9 x 4-byte ptr)   :540    36
    //   +40      mCoarseQueryQueue      VEQ<16384,16>      :543    16408  Construct @0x828C7BD4
    //   +16448   mCoarseLineTestQueue   EQ<LineTest,256>   :546    12304  (0x4040)
    //   +28752   mFineLineTestQueue     EQ<LineTestFine,256>          :547 16400 (0x7050)
    //   +45152   mFineLineTestNearestQueue EQ<LineTestNearest,256>    :548 16400 (0xB060)
    //   +61552   mFineLineTestFastDoubleSidedQueue EQ<...,16>         :549  1040 (0xF070)
    //   +62592   mFineSphereTestFastQueue EQ<SphereTestFast,16>       :550   784 (0xF480)
    //   +63376   mFineVolumeTestDeepestQueue EQ<VolumeTestDeepest,256> :551 57360 (0xF790)
    //   +120736  mFineVolumeTestQueue   EQ<VolumeTestFine,64>         :552 14352 (0x1D7A0)
    //   +135088  mTriangleCollisionLineTestQueue EQ<...,256>          :553 12304 (0x20FB0)
    //   +147392  mTriangleCollisionLineTestNearestQueue EQ<...,256>   :554 12304 (0x23FC0)
    //   +159696  mTriangleCollisionSphereTestQueue EQ<...,256>        :555  2060 (0x26FD0)
    //   = 161756 bytes on the console (every span == 16-byte header + N * element, except
    //     the 8-byte-element sphere queue whose header stays at 12 -- see the element homes).
    //
    // Those numbers are provenance, not code: the host reaches every seat by NAME. (The
    // console's per-queue read getters, 0x828AF270..0x828AF858, return exactly these seats;
    // WorldModule::BridgePhysicsSceneQueriesToScene @0x827A8D20 AddEvents into +45152 /
    // +63376 -- the nearest-line and deepest-volume seats -- and ProcessFineQueriesDirectly
    // @0x828D4F80 walks +28752 / +45152 / +61552 / +62592 / +63376 / +120736.)
    //
    // Element homes: CgsSceneManagerIO_EventLineTestQuery.h (InEventLineTest, 0x30),
    // _EventLineTest.h (Fine + FastDoubleSided, 0x40), _EventLineTestNearest.h (0x40),
    // _EventSphereTest.h (0x30), _EventVolumeTestDeepest.h (0xE0), _EventVolumeTestFine.h
    // (0xE0), the three _EventTriangleCollision*.h (0x30 / 0x30 / 0x08).
    // ------------------------------------------------------------------------
    struct InputBuffer_Query : public CgsModule::IOBuffer
    {
        // The DWARF's queue typedefs (CgsSceneManagerModuleIO.h:165..:178).
        typedef InCoarseQueryQueue<16384>                                               InSmCoarseQueryQueue;                     // :165
        typedef CgsModule::EventQueue<InEventLineTest, 256>                             InCoarseLineTestQueue;                    // :168
        typedef CgsModule::EventQueue<InEventLineTestFine, 256>                         InFineLineTestQueue;                      // :169
        typedef CgsModule::EventQueue<InEventLineTestNearest, 256>                      InFineLineTestNearestQueue;               // :170
        typedef CgsModule::EventQueue<InEventLineTestFastDoubleSided, 16>               InFineLineTestFastDoubleSidedQueue;       // :171
        typedef CgsModule::EventQueue<InEventSphereTestFast, 16>                        InFineSphereTestFastQueue;                // :172
        typedef CgsModule::EventQueue<InEventVolumeTestDeepest, 256>                    InFineVolumeTestDeepestQueue;             // :173
        typedef CgsModule::EventQueue<InEventVolumeTestFine, 64>                        InFineVolumeTestQueue;                    // :174
        typedef CgsModule::EventQueue<InEventTriangleCollisionLineTest, 256>            InTriangleCollisionLineTestQueue;         // :176
        typedef CgsModule::EventQueue<InEventTriangleCollisionLineTestNearest, 256>     InTriangleCollisionLineTestNearestQueue;  // :177
        typedef CgsModule::EventQueue<InEventTriangleCollisionSphereTest, 256>          InTriangleCollisionSphereTestQueue;       // :178

        // @ 0x828C7BC0 (DWARF :486). Bodied in CgsSceneManagerIO_InputBuffer_Query.cpp.
        void Construct();
        // DWARF :490. The console body is ICF-folded onto the bare `b CgsModule::IOBuffer::Destruct`
        // (DestroyIOBuffer<InputBuffer_Query> @0x823AF240 bl's the PropEntityIO::OutputBuffer_PreScene
        // fold @0x823AF2E4; the EventQueue/VariableEventQueue members have no Destruct work).
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

        // The producer entry points (DWARF :494/:498/:502/:506). None is emitted out of line on
        // the console: every producer inlines `m<Queue>.AddEvent(event)` on the member seat --
        // e.g. BridgePhysicsSceneQueriesToScene @0x827A8E58 `add r3, r26, 0xB060 ; bl
        // BaseEventQueue<InEventLineTestNearest>::AddEvent` is AddLineTestNearestQuery, and
        // @0x827A8E4C `add r3, r26, 0xF790 ; bl ...VolumeTestDeepest...AddEvent` is
        // AddVolumeTestDeepestQuery. Header-inline here, matching that.
        void AddLineTestFineQuery(const InEventLineTestFine& lrQuery)             { mFineLineTestQueue.AddEvent(lrQuery); }
        void AddLineTestNearestQuery(const InEventLineTestNearest& lrQuery)       { mFineLineTestNearestQueue.AddEvent(lrQuery); }
        void AddVolumeTestDeepestQuery(const InEventVolumeTestDeepest& lrQuery)   { mFineVolumeTestDeepestQueue.AddEvent(lrQuery); }
        void AddVolumeTestFineQuery(const InEventVolumeTestFine& lrQuery)         { mFineVolumeTestQueue.AddEvent(lrQuery); }

        // READ-locked getters (DWARF :509..:519). Each console body is the same 41-insn shape:
        // test status bit 4, stream "Not locked for reading\n" + FireAssert(...ModuleIO.h, <line>)
        // when clear, return the member seat. Bodied in CgsSceneManagerIO_InputBuffer_Query.cpp.
        const InSmCoarseQueryQueue*                    GetCoarseQueryQueue() const;                     // :509  @0x828AF270 (+40)
        const InCoarseLineTestQueue*                   GetCoarseLineTestQueue() const;                  // :510  (no out-of-line X360 emission)
        const InFineLineTestQueue*                     GetFineLineTestQueue() const;                    // :511  @0x828AF318 (+28752)
        const InFineLineTestNearestQueue*              GetFineLineTestNearestQueue() const;             // :512  @0x828AF3C0 (+45152)
        const InFineLineTestFastDoubleSidedQueue*      GetFineLineTestFastDoubleSidedQueue() const;     // :513  @0x828AF468 (+61552)
        const InFineSphereTestFastQueue*               GetFineSphereTestFastQueue() const;              // :514  @0x828AF510 (+62592)
        const InFineVolumeTestDeepestQueue*            GetFineVolumeTestDeepestQueue() const;           // :515  @0x828AF5B8 (+63376)
        const InFineVolumeTestQueue*                   GetFineVolumeTestQueue() const;                  // :516  @0x828AF660 (+120736)
        const InTriangleCollisionLineTestQueue*        GetTriangleCollisionLineTestQueue() const;       // :517  @0x828AF708 (+135088)
        const InTriangleCollisionLineTestNearestQueue* GetTriangleCollisionLineTestNearestQueue() const;// :518  @0x828AF7B0 (+147392)
        const InTriangleCollisionSphereTestQueue*      GetTriangleCollisionSphereTestQueue() const;     // :519  @0x828AF858 (+159696)

        // WRITE-side getters (DWARF :522..:526). The producers reach these while the buffer is
        // write-locked. None of the five is emitted out of line in the X360 IDB, so no lock
        // tripwire is attested for them; header-inline, no assert (the pre-existing
        // GetInCoarseQueryQueue() alias below kept that same contract).
        InSmCoarseQueryQueue*                    GetCoarseQueryQueue()                      { return &mCoarseQueryQueue; }                    // :522
        InFineLineTestQueue*                     GetFineLineTestQueue()                     { return &mFineLineTestQueue; }                   // :523
        InTriangleCollisionLineTestQueue*        GetTriangleCollisionLineTestQueue()        { return &mTriangleCollisionLineTestQueue; }      // :524
        InTriangleCollisionLineTestNearestQueue* GetTriangleCollisionLineTestNearestQueue() { return &mTriangleCollisionLineTestNearestQueue; } // :525
        InTriangleCollisionSphereTestQueue*      GetTriangleCollisionSphereTestQueue()      { return &mTriangleCollisionSphereTestQueue; }    // :526

        // DWARF :528 -- the producer-facing pointer table (filled by Construct).
        SceneQueryInterface* GetSceneQueryInterface() { return &mSceneQueryInterface; }

        // Pre-existing spelling of the write-side coarse-queue getter (X360 this+0x28), kept
        // for the mounted frustum-query producers in BrnWorldModule.cpp / the frustum job
        // dispatcher in CgsSceneManagerModule.cpp.
        InSmCoarseQueryQueue* GetInCoarseQueryQueue() { return &mCoarseQueryQueue; }

    private:
        SceneQueryInterface                     mSceneQueryInterface;                    // :540  console +4
        InSmCoarseQueryQueue                    mCoarseQueryQueue;                       // :543  console +40
        InCoarseLineTestQueue                   mCoarseLineTestQueue;                    // :546  console +16448
        InFineLineTestQueue                     mFineLineTestQueue;                      // :547  console +28752
        InFineLineTestNearestQueue              mFineLineTestNearestQueue;               // :548  console +45152
        InFineLineTestFastDoubleSidedQueue      mFineLineTestFastDoubleSidedQueue;       // :549  console +61552
        InFineSphereTestFastQueue               mFineSphereTestFastQueue;                // :550  console +62592
        InFineVolumeTestDeepestQueue            mFineVolumeTestDeepestQueue;             // :551  console +63376
        InFineVolumeTestQueue                   mFineVolumeTestQueue;                    // :552  console +120736
        InTriangleCollisionLineTestQueue        mTriangleCollisionLineTestQueue;         // :553  console +135088
        InTriangleCollisionLineTestNearestQueue mTriangleCollisionLineTestNearestQueue;  // :554  console +147392
        InTriangleCollisionSphereTestQueue      mTriangleCollisionSphereTestQueue;       // :555  console +159696
    };

    // ------------------------------------------------------------------------
    // SceneManagerIO::TriCacheQueryBuffer (DWARF CgsSceneManagerModuleIO.h:664) -- the
    // per-pass buffer SceneManagerModule::ProcessFineQueries @0x828D5608 stacks
    // ("TriCacheQuery") to COLLECT the triangle-collision tests of one query pass: the
    // fine-query dispatchers push the world-only tests here (ProcessLineTestNearest
    // @0x828D38C0 AddEvents the world-flag nearest line tests into +12320), then
    // ProcessFineQueriesDirectly appends the input buffer's own three tri-collision queues
    // onto these and hands each to its ProcessTriangleCollision* pass.
    //
    // Console layout, from CreateIOBuffer<TriCacheQueryBuffer> @0x828CC940 (26688 bytes):
    //   +0      status byte  (`*v8 = 1`)
    //   +16     mTriangleCollisionLineTestQueue         EQ<...LineTest,256>::Construct(v8 + 16)
    //   +12320  mTriangleCollisionLineTestNearestQueue  EQ<...Nearest,256>::Construct(v9 + 12320)
    //   +24624  mTriangleCollisionSphereTestQueue       EQ<...SphereTest,256>::Construct(v9 + 24624)
    // DestroyIOBuffer<TriCacheQueryBuffer> @0x828C57A8 bl's CgsModule::IOBuffer::Destruct
    // directly -- the Destruct is the base's.
    // ------------------------------------------------------------------------
    struct TriCacheQueryBuffer : public CgsModule::IOBuffer
    {
        // DWARF :669 -- inlined into the CreateIOBuffer instantiation @0x828CC940.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mTriangleCollisionLineTestQueue.Construct();
            mTriangleCollisionLineTestNearestQueue.Construct();
            mTriangleCollisionSphereTestQueue.Construct();
        }
        // DWARF :673 -- the base Destruct (DestroyIOBuffer @0x828C57A8 calls it directly).
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

        // READ-locked getters (DWARF :675..:677; no out-of-line X360 emission).
        const InputBuffer_Query::InTriangleCollisionLineTestQueue*        GetTriangleCollisionLineTestQueue() const;
        const InputBuffer_Query::InTriangleCollisionLineTestNearestQueue* GetTriangleCollisionLineTestNearestQueue() const;
        const InputBuffer_Query::InTriangleCollisionSphereTestQueue*      GetTriangleCollisionSphereTestQueue() const;

        // WRITE-locked getters (DWARF :678..:680). Console bodies: @0x828AFBA0 (:678, +16),
        // @0x828AFC48 (:679, +12320), @0x828AFCF0 (:680, +24624) -- each tests status bit 3 and
        // streams "Not locked for writing\n". Bodied in CgsSceneManagerIO_InputBuffer_Query.cpp.
        InputBuffer_Query::InTriangleCollisionLineTestQueue*        GetTriangleCollisionLineTestQueue();
        InputBuffer_Query::InTriangleCollisionLineTestNearestQueue* GetTriangleCollisionLineTestNearestQueue();
        InputBuffer_Query::InTriangleCollisionSphereTestQueue*      GetTriangleCollisionSphereTestQueue();

    private:
        InputBuffer_Query::InTriangleCollisionLineTestQueue        mTriangleCollisionLineTestQueue;         // :684  console +16
        InputBuffer_Query::InTriangleCollisionLineTestNearestQueue mTriangleCollisionLineTestNearestQueue;  // :685  console +12320
        InputBuffer_Query::InTriangleCollisionSphereTestQueue      mTriangleCollisionSphereTestQueue;       // :686  console +24624
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
