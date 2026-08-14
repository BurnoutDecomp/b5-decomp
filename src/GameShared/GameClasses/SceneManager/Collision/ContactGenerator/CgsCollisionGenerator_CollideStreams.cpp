// =================================================================================================
// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/
// CgsCollisionGenerator_CollideStreams.cpp
//
// ⭐⭐ THE COLLIDE-STREAM FAMILY (walls leg 1, 2026-08-14) — six of the seven bodies the
// big-five #2 wave (2026-08-06) could only stub. This is the plumbing race-car WORLD contact
// generation posts into: DoRaceCarWorldContactGeneration @0x825EB140 posts one sphere-list (or
// swept-sphere-list) vs triangle-list command per live car per frame, and the three Run*
// dispatchers hand those commands to ContactGeneratorEntry.
//
//   CreateCollideSphereListWithTriangleListStream      @0x828113C8  (98)   REAL
//   CreateCollideSweptSphereListWithTriangleListStream @0x82811720  (98)   REAL
//   CreateCollideSphereListWithSphereListStream        @0x82811A78  (98)   REAL
//   AddSphereListWithTriangleListToStream              @0x82811340  (33)   REAL
//   AddSweptSphereListWithTriangleListToStream         @0x82811698  (33)   REAL
//   PrepareNewPrimitiveTestResultsList                 @0x82810798 (114)   REAL
//   RunCollideSphereListWithTriangleListStream         @0x82811550  (81)   REAL (dispatcher)
//   RunCollideSweptSphereListWithTriangleListStream    @0x828118A8  (81)   REAL (dispatcher)
//   RunCollideSphereListWithSphereListStream           @0x82811C00  (80)   REAL (dispatcher)
//
// ⭐ THE THREE Create* ARE ONE BODY. Diffed instruction-for-instruction: the only deltas are
// the assert line numbers (:1384/:1396 sphere-tri, :1567/:1579 swept, :1799/:1811 sphere-sphere).
// All three are the landed CreateLineWithTriangleListStream shape (2026-08-10) with command
// size 32 and NO result buffer (`li r5/r6, 0` into GetRequiredBufferSizes and Construct) —
// stream RESULTS go through the CollisionResultList each posted command carries, not through
// the producer's result lane.
//
// ⚠️⚠️ COMMAND SIZE WIDENS ON THIS HOST, AND THAT IS THE CONTRACT, NOT A DRIFT. The console's
// 32 == sizeof of a record holding two 4-byte-pointer list pairs; the host records widen to 48
// (static-asserted %16 against the poster's own tripwire). Poster and consumer both stride by
// the same sizeof, and these commands are runtime-carved (never serialized), so the standing
// serialized-slots rule says widen. The producer is constructed with the HOST size.
//
// ⭐ THE THREE Run* ARE THE RunLineWithTriangleListStream SHAPE (landed 2026-08-11), wiring
// ContactGeneratorEntry over descriptor types 6/14/8 — the three cases ContactGeneratorJob::
// Execute already dispatches to LOUD NAMED GATES (ExecuteSphereListWithTriangleListStream
// @0x829235C8 etc.). Landing the dispatchers with gated workers is NOT a silent drop: the
// moment a command flows, the gate names the missing kernel in the log. Divergences between
// the three, read out of the asm and reproduced:
//   * sphere-tri passes the caller's DebugRenderStreamReader through desc +0xF8; swept ditto;
//     sphere-sphere stores NULL (its DWARF Prepare takes no reader).
//   * sphere-tri brackets the AddTree site with the StartJobs perfmon (0x82811664..0x82811680);
//     swept + sphere-sphere bracket EACH batch's wiring instead (per-batch Start/Stop).
//   * none of the three writes mabUsedBatches explicitly (RunLine does); the landed
//     CreateNewBatch marks the slot itself.
//   * batch clamp is 3 on X360 for all three (`li r29, 3`).
//
// ⚠️⚠️ FLAG PC-platform leaf: THE DISPATCH, AND ONLY THE DISPATCH. `unk_830EA650` (the global
// EA::Jobs::JobScheduler) does not exist on this build (CgsHardwareInitPC.cpp:40), so the
// JobScheduler::AddTree call — and only that call — is replaced by running each batch's entry
// point inline, per the CgsLooseOctree.cpp:997 precedent and exactly as the two landed
// dispatchers in CgsCollisionGenerator_LineStream.cpp already do (inline inside the wiring
// loop, the precedent's own placement). The returned parent job was never submitted, so
// Job::WaitOn's own null-backend guard makes it safe to wait on — checked on the traction leg.
//
// PrepareNewPrimitiveTestResultsList @0x82810798: most of its 114 instructions are the two
// STREAMED assert-failure messages (BasePriorityQueue::Clear + operator<< bursts); the logic is
// ~15. The console allocates the list header with `li r4, 0x10` (its 16-byte sizeof — host
// sizeof(CollisionResultList) is wider; sizeof per the project rule) and the results buffer as
// 80 * maxResults (`5*n << 4`). 80 is the per-record memory size of result type 0 (the
// meResultType byte it writes; the DWARF names a 3-row KAI_RESULT_TYPE_MEMORY_SIZE table whose
// other rows no recovered function reads yet). The streamed messages are joined to literals per
// this tree's CGS_ASSERT convention (the DataStreamCommandPoster::End precedent).
// =================================================================================================

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"          // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"  // SimpleDataStreamProducer
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h" // CollisionResultList / CollisionResult
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsSphereList.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsSweptSphereList.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSphereListWithTriangleListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSweptSphereListWithTriangleListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSphereListWithSphereListJobDesc.h"

#include "SDKs/EATech/eajobs/job.h"        // EA::Jobs::Job
#include "SDKs/EATech/eajobs/job_types.h"  // EA::Jobs::Param / JOB_ENVIRONMENT_LOCAL

// The contact-generator job entry every collide-stream batch is wired to
// (GameShared/Jobs/ContactGenerator/ContactGenerator.cpp; X360 0x82920F10).
void ContactGeneratorEntry(EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param);

namespace CgsSceneManager
{
namespace CgsCollision
{
    namespace
    {
        // Per-record memory size of result type 0 (primitive-test results) — the `5*n << 4`
        // multiplier at 0x828108A0..0x828108A8. DWARF: KAI_RESULT_TYPE_MEMORY_SIZE[0]
        // (CgsCollisionResultList.h:39; the table's other rows are unread by any recovered
        // function, so only this row is named).
        const s32 KI_PRIMITIVE_TEST_RESULT_MEMORY_SIZE = 80;

        // The console's collide-stream command size is 32 (`li r4, 0x20` in all three Create*).
        // On this host each family's StreamCommand is its own (wider) sizeof — see the banner.

        // The shared Create* body (the three console functions are byte-identical bar assert
        // line numbers; both assert messages are the X360 rodata verbatim, same pair the landed
        // CreateStreamProducer / CreateLineWithTriangleListStream carry). Carve the producer and
        // ONE command buffer out of the result allocator at 128-byte alignment, no result lane.
        CgsMemory::SimpleDataStreamProducer* CreateCollideStreamProducer(
            CgsMemory::LinearMalloc& lrAllocator, s32 liMaxCommands, s32 liCommandSize)
        {
            const size_t lnSavedAlignment = lrAllocator.GetAlignment();   // lwzx @+0x123B0
            lrAllocator.SetAlignment(128);                                // li r4, 0x80

            CgsMemory::SimpleDataStreamProducer* lpProducer =
                static_cast<CgsMemory::SimpleDataStreamProducer*>(
                    lrAllocator.Malloc(sizeof(CgsMemory::SimpleDataStreamProducer)));  // li r4, 0x180 (console sizeof)
            CGS_ASSERT(lpProducer != nullptr, "Failed to allocate stream producer\n");

            u32 luCommandBufferSize = 0;
            u32 luResultBufferSize  = 0;
            CgsMemory::SimpleDataStreamProducer::GetRequiredBufferSizes(
                liMaxCommands, liCommandSize, 0, 0,                       // li r5, 0 / li r6, 0
                &luCommandBufferSize, &luResultBufferSize);

            void* lpCommandBuffer = lrAllocator.Malloc(luCommandBufferSize);
            CGS_ASSERT(lpCommandBuffer != nullptr, "Failed to allocate stream buffers\n");

            lpProducer->Construct(liMaxCommands, liCommandSize, lpCommandBuffer, 0, 0, 0);

            lrAllocator.SetAlignment(lnSavedAlignment);
            return lpProducer;
        }
    }

    // =============================================================================================
    // PrepareNewPrimitiveTestResultsList @0x82810798 (114) — see the file banner.
    //   index = mu16NumUsedResultLists++            (lhz/sth @+0x123BC; stored BEFORE the assert)
    //   assert index+1 <= 200                       "Ran out of result lists"            (:251)
    //   header  = allocator.Malloc(16-console)  -> mapCollisionResultLists[index]  (stwx BEFORE
    //             the null assert, exactly as the console stores then tests)
    //   results = allocator.Malloc(80 * maxResults)
    //   fill the six DWARF-named header fields; return index.
    // =============================================================================================
    s32 BaseCollisionGenerator::PrepareNewPrimitiveTestResultsList(u16 lu16MaxResults,
                                                                   u32 lu32UserTagA,
                                                                   u16 lu16UserTagB)
    {
        const s32 liIndex = mu16NumUsedResultLists;
        mu16NumUsedResultLists = static_cast<u16>(liIndex + 1);

        CGS_ASSERT(static_cast<u16>(liIndex + 1) <= KU16_MAX_NUM_RESULT_LISTS,
                   "Ran out of result lists");                                     // :251

        CollisionResultList* lpList = static_cast<CollisionResultList*>(
            mCollisionResultsAllocator.Malloc(sizeof(CollisionResultList)));       // li r4, 0x10 (console sizeof)
        mapCollisionResultLists[liIndex] = lpList;                                 // stwx (before the test)
        CGS_ASSERT(lpList != nullptr,
                   "Failed to allocate memory for collision results list");        // :254 (streamed form joined)

        void* lpResults = mCollisionResultsAllocator.Malloc(
            KI_PRIMITIVE_TEST_RESULT_MEMORY_SIZE * static_cast<s32>(lu16MaxResults)); // 5*n << 4
        CGS_ASSERT(lpResults != nullptr,
                   "Failed to allocate memory for collision result containing  collisions"); // :257 (count streamed on console)

        lpList->mpResults         = static_cast<CollisionResult*>(lpResults);      // stw  +0x00
        lpList->mu32UserTagA      = lu32UserTagA;                                  // stw  +0x04
        lpList->mu16UserTagB      = lu16UserTagB;                                  // sth  +0x08
        lpList->mu16MaxNumResults = lu16MaxResults;                                // sth  +0x0A
        lpList->mu16NumResults    = 0;                                             // sth  +0x0C
        lpList->meResultType      = 0;                                             // stb  +0x0E

        return liIndex;
    }

    // =============================================================================================
    // AddSphereListWithTriangleListToStream @0x82811340 (33) — allocate the result list, build
    // the StreamCommand, post it. The console copies each list as one 8-byte {ptr,count} `ld`;
    // the host copies the same pair through the named structs. Returns the result-list index
    // (`mr r3, r29`).
    // =============================================================================================
    s32 BaseCollisionGenerator::AddSphereListWithTriangleListToStream(
        const SphereList* lpSphereList, const TriangleList* lpTriangleList,
        u16 lu16MaxResults, f32 lfPadding, u32 lu32UserTagA, u16 lu16UserTagB,
        CgsMemory::SimpleDataStreamProducer* lpProducer)
    {
        const s32 liResultListIndex =
            PrepareNewPrimitiveTestResultsList(lu16MaxResults, lu32UserTagA, lu16UserTagB);

        SphereListWithTriangleListStreamJobDesc::StreamCommand lCommand;
        lCommand.mSphereList  = *lpSphereList;                                     // ld r11, 0(r28) / std
        lCommand.mTriList     = *lpTriangleList;                                   // ld r11, 0(r27) / std
        lCommand.mfPadding    = lfPadding;                                         // stfs f31
        lCommand.mpResultList =
            mapCollisionResultLists[static_cast<u16>(liResultListIndex)];          // clrlwi 16 + mapCollisionResultLists[i]

        lpProducer->AddCommand(&lCommand);   // the inlined producer AddCommand (poster + count store)

        return liResultListIndex;
    }

    // =============================================================================================
    // AddSweptSphereListWithTriangleListToStream @0x82811698 (33) — byte-identical to the sphere
    // sibling over a SweptSphereList (diffed; only the list type differs at the call sites).
    // =============================================================================================
    s32 BaseCollisionGenerator::AddSweptSphereListWithTriangleListToStream(
        const SweptSphereList* lpSweptSphereList, const TriangleList* lpTriangleList,
        u16 lu16MaxResults, f32 lfPadding, u32 lu32UserTagA, u16 lu16UserTagB,
        CgsMemory::SimpleDataStreamProducer* lpProducer)
    {
        const s32 liResultListIndex =
            PrepareNewPrimitiveTestResultsList(lu16MaxResults, lu32UserTagA, lu16UserTagB);

        SweptSphereListWithTriangleListStreamJobDesc::StreamCommand lCommand;
        lCommand.mSphereList  = *lpSweptSphereList;
        lCommand.mTriList     = *lpTriangleList;
        lCommand.mfPadding    = lfPadding;
        lCommand.mpResultList =
            mapCollisionResultLists[static_cast<u16>(liResultListIndex)];

        lpProducer->AddCommand(&lCommand);

        return liResultListIndex;
    }

    // =============================================================================================
    // The three Create* factories — the shared body above with each family's HOST command size
    // and its own console assert lines (:1384/:1396, :1567/:1579, :1799/:1811 — messages
    // identical, recorded here so the addresses stay greppable).
    // =============================================================================================
    CgsMemory::SimpleDataStreamProducer*
    BaseCollisionGenerator::CreateCollideSphereListWithTriangleListStream(s32 liMaxCommands)
    {
        // @0x828113C8 (exported as the unnamed sub_828113C8; seat mpSphereTriangleStreamProducer)
        return CreateCollideStreamProducer(
            mCollisionResultsAllocator, liMaxCommands,
            static_cast<s32>(sizeof(SphereListWithTriangleListStreamJobDesc::StreamCommand)));
    }

    CgsMemory::SimpleDataStreamProducer*
    BaseCollisionGenerator::CreateCollideSweptSphereListWithTriangleListStream(s32 liMaxCommands)
    {
        // @0x82811720 (sub_82811720; seat mpSweptSphereTriangleStreamProducer)
        return CreateCollideStreamProducer(
            mCollisionResultsAllocator, liMaxCommands,
            static_cast<s32>(sizeof(SweptSphereListWithTriangleListStreamJobDesc::StreamCommand)));
    }

    CgsMemory::SimpleDataStreamProducer*
    BaseCollisionGenerator::CreateCollideSphereListWithSphereListStream(s32 liMaxCommands)
    {
        // @0x82811A78 (sub_82811A78; seat mpSphereSphereStreamProducer)
        return CreateCollideStreamProducer(
            mCollisionResultsAllocator, liMaxCommands,
            static_cast<s32>(sizeof(SphereListWithSphereListStreamJobDesc::StreamCommand)));
    }

    // =============================================================================================
    // RunCollideSphereListWithTriangleListStream @0x82811550 (81) — the RunLine dispatcher shape,
    // desc type 6 (-> ExecuteSphereListWithTriangleListStream, a loud named gate until the
    // sphere-vs-Triangle4 contact kernel lands). Empty stream returns null (0x82811578), clamp 3,
    // AddTree site bracketed by the StartJobs perfmon (0x82811664..0x82811680).
    // =============================================================================================
    EA::Jobs::Job*
    BaseCollisionGenerator::RunCollideSphereListWithTriangleListStream(
        CgsMemory::SimpleDataStreamProducer* lpProducer,
        CgsDev::DebugRenderStreamReader*     lpDebugReader)
    {
        if (lpProducer->GetNumCommands() == 0)      // lwz r11, 0x104(producer)
        {
            return 0;
        }

        const s32 KI_MAX_BATCHES = 3;               // li r29, 3
        s32 liNumBatches = lpProducer->GetNumCommands();
        if (liNumBatches > KI_MAX_BATCHES)
        {
            liNumBatches = KI_MAX_BATCHES;
        }

        EA::Jobs::Job* lpParentJob = AllocateJob();

        for (s32 liBatch = 0; liBatch < liNumBatches; ++liBatch)
        {
            CollisionBatch& lrBatch = maCollisionBatches[CreateNewBatch()];

            SphereListWithTriangleListStreamJobDesc* lpDesc =
                reinterpret_cast<SphereListWithTriangleListStreamJobDesc*>(
                    lrBatch.GetJobDescription().GetBuffer());
            lpDesc->Prepare(lpProducer, lpDebugReader);

            EA::Jobs::Job* lpJob = lrBatch.GetJob();
            lpJob->Clear();
            lpJob->SetName("CollisionBatch");
            lpJob->SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                           reinterpret_cast<const void*>(&ContactGeneratorEntry), 0);
            lpJob->SetData(lpDesc,
                           static_cast<int>(CollisionJobDescriptionStorage::KU_CONSOLE_BYTES));
            lpJob->SetCodeRecycle(EA::Jobs::EntryPoint::CODE_RECYCLE_ON);
            lpParentJob->DependsOn(*lpJob, EA::Jobs::Event::EVENT_WHEN_JOB_END);   // li r5, 1

            // ---- FLAG PC-platform leaf: run the job body here (see the file banner) ----
            // X360: JobScheduler::AddTree(&unk_830EA650, lpParentJob) after the loop.
            ContactGeneratorEntry(EA::Jobs::Param(static_cast<void*>(lpDesc)),
                                  EA::Jobs::Param(static_cast<void*>(lpDesc)),
                                  EA::Jobs::Param(),
                                  EA::Jobs::Param());
        }

        // The console brackets the AddTree call with the StartJobs perfmon; the dispatch itself
        // ran inline above, so on PC the bracket closes over the (replaced) AddTree site only.
        CgsDev::PerfMonCpu::StartMonitor(_miStartJobsPerfMon);   // 0x82811668
        CgsDev::PerfMonCpu::StopMonitor(_miStartJobsPerfMon);    // 0x82811680

        return lpParentJob;
    }

    // =============================================================================================
    // RunCollideSweptSphereListWithTriangleListStream @0x828118A8 (81) — desc type 14
    // (-> ExecuteSweptSphereListWithTriangleListStream, loud named gate). Same shape; the
    // perfmon brackets EACH batch's wiring on this one (the console's own placement).
    // =============================================================================================
    EA::Jobs::Job*
    BaseCollisionGenerator::RunCollideSweptSphereListWithTriangleListStream(
        CgsMemory::SimpleDataStreamProducer* lpProducer,
        CgsDev::DebugRenderStreamReader*     lpDebugReader)
    {
        if (lpProducer->GetNumCommands() == 0)
        {
            return 0;
        }

        const s32 KI_MAX_BATCHES = 3;
        s32 liNumBatches = lpProducer->GetNumCommands();
        if (liNumBatches > KI_MAX_BATCHES)
        {
            liNumBatches = KI_MAX_BATCHES;
        }

        EA::Jobs::Job* lpParentJob = AllocateJob();

        for (s32 liBatch = 0; liBatch < liNumBatches; ++liBatch)
        {
            CollisionBatch& lrBatch = maCollisionBatches[CreateNewBatch()];

            SweptSphereListWithTriangleListStreamJobDesc* lpDesc =
                reinterpret_cast<SweptSphereListWithTriangleListStreamJobDesc*>(
                    lrBatch.GetJobDescription().GetBuffer());
            lpDesc->Prepare(lpProducer, lpDebugReader);

            CgsDev::PerfMonCpu::StartMonitor(_miStartJobsPerfMon);   // per-batch on this twin

            EA::Jobs::Job* lpJob = lrBatch.GetJob();
            lpJob->Clear();
            lpJob->SetName("CollisionBatch");
            lpJob->SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                           reinterpret_cast<const void*>(&ContactGeneratorEntry), 0);
            lpJob->SetData(lpDesc,
                           static_cast<int>(CollisionJobDescriptionStorage::KU_CONSOLE_BYTES));
            lpJob->SetCodeRecycle(EA::Jobs::EntryPoint::CODE_RECYCLE_ON);
            lpParentJob->DependsOn(*lpJob, EA::Jobs::Event::EVENT_WHEN_JOB_END);

            CgsDev::PerfMonCpu::StopMonitor(_miStartJobsPerfMon);

            // ---- FLAG PC-platform leaf: run the job body here (see the file banner) ----
            ContactGeneratorEntry(EA::Jobs::Param(static_cast<void*>(lpDesc)),
                                  EA::Jobs::Param(static_cast<void*>(lpDesc)),
                                  EA::Jobs::Param(),
                                  EA::Jobs::Param());
        }

        return lpParentJob;
    }

    // =============================================================================================
    // RunCollideSphereListWithSphereListStream @0x82811C00 (80) — desc type 8
    // (-> ExecuteSphereListWithSphereListStream, loud named gate). No debug reader on this one;
    // per-batch perfmon bracket like the swept twin. Dead at runtime this wave: its poster
    // (AddSphereListWithSphereListToStream @0x828119F0, the car-car leg) is not reconstructed,
    // so the stream always carries zero commands and this returns null at the top.
    // =============================================================================================
    EA::Jobs::Job*
    BaseCollisionGenerator::RunCollideSphereListWithSphereListStream(
        CgsMemory::SimpleDataStreamProducer* lpProducer)
    {
        if (lpProducer->GetNumCommands() == 0)
        {
            return 0;
        }

        const s32 KI_MAX_BATCHES = 3;
        s32 liNumBatches = lpProducer->GetNumCommands();
        if (liNumBatches > KI_MAX_BATCHES)
        {
            liNumBatches = KI_MAX_BATCHES;
        }

        EA::Jobs::Job* lpParentJob = AllocateJob();

        for (s32 liBatch = 0; liBatch < liNumBatches; ++liBatch)
        {
            CollisionBatch& lrBatch = maCollisionBatches[CreateNewBatch()];

            SphereListWithSphereListStreamJobDesc* lpDesc =
                reinterpret_cast<SphereListWithSphereListStreamJobDesc*>(
                    lrBatch.GetJobDescription().GetBuffer());
            lpDesc->Prepare(lpProducer);

            CgsDev::PerfMonCpu::StartMonitor(_miStartJobsPerfMon);

            EA::Jobs::Job* lpJob = lrBatch.GetJob();
            lpJob->Clear();
            lpJob->SetName("CollisionBatch");
            lpJob->SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                           reinterpret_cast<const void*>(&ContactGeneratorEntry), 0);
            lpJob->SetData(lpDesc,
                           static_cast<int>(CollisionJobDescriptionStorage::KU_CONSOLE_BYTES));
            lpJob->SetCodeRecycle(EA::Jobs::EntryPoint::CODE_RECYCLE_ON);
            lpParentJob->DependsOn(*lpJob, EA::Jobs::Event::EVENT_WHEN_JOB_END);

            CgsDev::PerfMonCpu::StopMonitor(_miStartJobsPerfMon);

            // ---- FLAG PC-platform leaf: run the job body here (see the file banner) ----
            ContactGeneratorEntry(EA::Jobs::Param(static_cast<void*>(lpDesc)),
                                  EA::Jobs::Param(static_cast<void*>(lpDesc)),
                                  EA::Jobs::Param(),
                                  EA::Jobs::Param());
        }

        return lpParentJob;
    }
}
}
