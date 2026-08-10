// =================================================================================================
// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator_LineStream.cpp
//
// ⭐ THE FLOOR OF THE VEHICLE TRACTION-LINE CHAIN (ground wave, 2026-08-10).
//   BaseCollisionGenerator::CreateLineWithTriangleListStream @0x82810B98 (96 insns)  -- REAL
//   BaseCollisionGenerator::RunLineWithTriangleListStream    @0x82810E80 (90 insns)  -- boot gate
//
// A Burnout car does not rest on contacts -- contacts are the body-shell/crash path. It rests on
// TRACTION LINE TESTS, and this producer is the stream those tests are posted into:
//   DoVehicleTractionLineAllocations -> CreateLineWithTriangleListStream -> mpTractionLineStream-
//   Producer; AddRaceCarTractionLineTests posts one 176-byte command per live car; RunTraction-
//   LineTestJobs -> RunLineWithTriangleListStream -> the job tree; EndVehicleTractionLineTests
//   harvests 192-byte results and feeds RaceCarPhysics::AddTractionPoint.
//
// ⚠️ NEITHER FUNCTION CARRIES AN IDA SYMBOL, AND THE TWO ABSENCES ARE DIFFERENT KINDS.
//  * Create is exported as the unnamed `sub_82810B98`. Identity is not guessed: its two asserts
//    are `CgsCollisionGenerator.cpp` :533 "Failed to allocate stream producer\n" and :550
//    "Failed to allocate stream buffers\n" (the same pair CreateStreamProducer @0x828109F8 carries
//    at its own lines), its only caller is VehicleManager::DoVehicleTractionLineAllocations, and
//    its result is stored straight into mpTractionLineStreamProducer.
//  * Run @0x82810E80 is a GENUINE export-set hole -- the export dir goes 0x82810D38
//    (RunFillTriangleCacheStream, last insn 0x82810E7C) -> 0x82810FE8 with nothing between.
//    Proved the brief's way rather than assumed: the `bl` word at the call site 0x825B5248 is
//    0x4825BC39, which decodes to 0x82810E80, and the same decoder was proved in the same run on
//    four independently-named neighbours (0x825B5228 -> DataStreamCommandPoster::Begin @0x82867AE8,
//    0x825B51E0 -> PerfMonCpu::StartMonitor @0x821F1198, 0x82810E6C -> JobScheduler::AddTree
//    @0x82BCB540, 0x82633DAC -> ReadRaceCarTractionLineTestResults @0x82618058).
//
// ⛔ WHY Run IS A GATE AND NOT A BODY. Its 90 instructions were read out of the image and are the
// exact call sequence of its exported twin RunFillTriangleCacheStream @0x82810D38 -- AllocateJob,
// then per batch { CreateNewBatch, Job::Clear, EntryPoint::SetName, EntryPoint::SetCode(<entry>),
// Job::SetData, Job::DependsOn }, then JobScheduler::AddTree. What it installs is a JOB ENTRY
// POINT, and the worker behind it (ContactGeneratorJob::ExecuteLineWithTriangleListStream
// @0x82921968, 589 insns, on top of CgsGeometric::IntersectLinePolygonSoupNearestSingleSided 575 /
// PolygonSoupListSpatialMap::RunQuery 261 / the PolygonSoupTesterJob family) is not in this tree.
// Writing the dispatcher without the worker would produce a job that returns silently empty
// results, which the harvest would then read as "no wheel touched anything" -- a silent-drop stub
// of exactly the class this project keeps getting burned by. It is a LOUD one-shot gate instead.
// =================================================================================================

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint (boot gate)

#include "SDKs/EATech/eajobs/job.h"                         // EA::Jobs::Job (return type)
#include "SDKs/EATech/eajobs/job_types.h"                   // EA::Jobs::Param / JOB_ENVIRONMENT_LOCAL
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsFillTriangleCacheStreamJobDesc.h" // the descriptor RunFillTriangleCacheStream prepares

// The polygon-soup tester job entry every fill batch is wired to
// (GameShared/Jobs/PolygonSoupTester/PolygonSoupTester.cpp; X360 rodata PolygonSoupTesterEntry).
void PolygonSoupTesterEntry(EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param);

namespace CgsSceneManager
{
namespace CgsCollision
{
    // dword_82F310B4 -- the perf-monitor id RunFillTriangleCacheStream brackets its per-batch
    // wiring with (0x82810DE4 StartMonitor / 0x82810E4C StopMonitor). Same shape and same
    // "-1 means unregistered" convention as CgsTriangleCacheManager_Update.cpp's three ids.
    static s32 s_miFillTriangleCacheStreamPerfMon = -1;   // dword_82F310B4

    // The stream's fixed record geometry, read out of the X360 asm at 0x82810C5C..0x82810D1C
    // (`li r4, 0xB0` / `li r6, 0xC0` into GetRequiredBufferSizes, then `li r5, 0xB0` / `li r8, 0xC0`
    // into Construct). Both are DATA-LAYOUT constants of the line-test command/result records, not
    // sizeofs of any type this tree homes yet, so they are named here rather than derived:
    //   command 176 bytes: 4x Vector4 line START (+0x00) | 4x Vector4 line END (+0x50) |
    //                      Triangle4* (+0xA0) | s32 numTriangles (+0xA4) | s32 numLines (+0xA8)
    //                      -- the seats AddRaceCarTractionLineTests @0x825E9640 writes.
    //   result 192 bytes:  4x Vector4 hit POSITION (+0x00) | 4x Vector4 hit NORMAL (+0x50) |
    //                      4x u32 surface tag (+0xA0) | 4x u8 hit flag (+0xB4)
    //                      -- the seats ReadRaceCarTractionLineTestResults @0x82618058 reads.
    static const s32 KI_LINE_STREAM_COMMAND_SIZE = 176;
    static const s32 KI_LINE_STREAM_RESULT_SIZE  = 192;

    // X360 0x82810B98 (`sub_82810B98`; CgsCollisionGenerator.cpp :533/:550).
    // Structurally identical to CreateStreamProducer @0x828109F8 -- carve the producer and its two
    // backing buffers out of the collision-result bump allocator at 128-byte alignment, size the
    // buffers with the producer's own requirement helper, construct the producer over them, and
    // restore the allocator's previous alignment -- differing only in the record geometry above.
    //
    // ⭐ The console's producer allocation is a literal `li r4, 0x180` (384). That is the X360
    // sizeof: the struct's 128-byte alignment rounds its 0x108 of members (mbIsStreaming @+0x100,
    // miNumAddedCommands @+0x104) up to 0x180. Per the project rule a console size literal is
    // reproduced as a `sizeof`, not as the number -- the host struct is wider.
    CgsMemory::SimpleDataStreamProducer*
    BaseCollisionGenerator::CreateLineWithTriangleListStream(s32 liMaxCommands)
    {
        const size_t lnSavedAlignment = mCollisionResultsAllocator.GetAlignment();
        mCollisionResultsAllocator.SetAlignment(128);

        CgsMemory::SimpleDataStreamProducer* lpProducer =
            static_cast<CgsMemory::SimpleDataStreamProducer*>(
                mCollisionResultsAllocator.Malloc(sizeof(CgsMemory::SimpleDataStreamProducer)));
        CGS_ASSERT(lpProducer != nullptr, "Failed to allocate stream producer\n");

        u32 luCommandBufferSize = 0;
        u32 luResultBufferSize  = 0;
        CgsMemory::SimpleDataStreamProducer::GetRequiredBufferSizes(
            liMaxCommands, KI_LINE_STREAM_COMMAND_SIZE,
            liMaxCommands, KI_LINE_STREAM_RESULT_SIZE,
            &luCommandBufferSize, &luResultBufferSize);

        void* lpCommandBuffer = mCollisionResultsAllocator.Malloc(luCommandBufferSize);
        void* lpResultBuffer  = mCollisionResultsAllocator.Malloc(luResultBufferSize);
        CGS_ASSERT(lpCommandBuffer != nullptr && lpResultBuffer != nullptr,
                   "Failed to allocate stream buffers\n");

        lpProducer->Construct(liMaxCommands, KI_LINE_STREAM_COMMAND_SIZE, lpCommandBuffer,
                              liMaxCommands, KI_LINE_STREAM_RESULT_SIZE, lpResultBuffer);

        mCollisionResultsAllocator.SetAlignment(lnSavedAlignment);
        return lpProducer;
    }

    // X360 0x82810E80 -- export-set hole, body NOT reconstructed (see the file banner).
    // ⛔ NEVER make this silent. Returning null is the console's own "nothing to dispatch" answer
    // (its exported twin returns null when the producer holds zero commands), and
    // EndVehicleTractionLineTests' `if (job) WaitOn(job)` handles null exactly as shipped -- but a
    // null returned because the DISPATCHER IS MISSING is not the same fact, so it says so once.
    EA::Jobs::Job*
    BaseCollisionGenerator::RunLineWithTriangleListStream(CgsMemory::SimpleDataStreamProducer*)
    {
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: BaseCollisionGenerator::RunLineWithTriangleListStream "
                       "@0x82810E80 (90; export hole, address PROVED by bl decode 2026-08-10) "
                       "inert -- the job entry + ContactGeneratorJob::ExecuteLineWithTriangleList"
                       "Stream @0x82921968 (589) are not in the tree [FLAG PC boot gate]\n";
        }
        return 0;
    }

    // =============================================================================================
    // ⭐⭐⭐ BaseCollisionGenerator::RunFillTriangleCacheStream @0x82810D38 (82) -- LANDED
    // 2026-08-10 (fill-worker wave 2). This is the FIRST EA::Jobs dispatch this PC port has ever
    // performed: `grep AddTree` over the whole tree outside SDKs/EATech/eajobs found ZERO call
    // sites before this one.
    //
    // The console body, read instruction for instruction:
    //   0x82810D54  lwz  r11, 0x104(producer)   -> miNumAddedCommands; 0 => return null
    //   0x82810D70  li   r21, 3 ; clamp numBatches = min(numCommands, 3)
    //   0x82810D88  AllocateJob()                                       -> the parent job
    //   per batch:
    //   0x82810DB4  CreateNewBatch()             -> index into maCollisionBatches
    //   0x82810DDC  stw r22, 0x3D0(batch)        -> desc.mpSpatialMap
    //   0x82810DE0  stw r23, 0x3D4(batch)        -> desc.mpStreamProducer
    //   0x82810DD4  stw r27, 0x4C0 / 0x82810DCC stfs f31, 0x4C4 (flt_82001CC0 == 0.0f)
    //   0x82810DD8  stw r27, 0x4C8 / 0x82810DD0 stb r21, 0x4CF  -> the CollisionJobDescription
    //                                                              bookkeeping, type = 3
    //   0x82810DF4  Job::Clear(batch + 0x80)
    //   0x82810E04  EntryPoint::SetName(job.mEntryPoint, "CollisionBatch")
    //   0x82810E1C  EntryPoint::SetCode(job.mEntryPoint, 0, PolygonSoupTesterEntry, 0)
    //   0x82810E2C  Job::SetData(job, batch + 0x3D0, 256)
    //   0x82810E38  <STUB>(job.mEntryPoint, 1)   -> SetCodeRecycle(CODE_RECYCLE_ON), the same
    //                                              ICF-folded call CollisionBatch::SetupJob makes
    //   0x82810E48  Job::DependsOn(parent, job, 1)
    //   0x82810E6C  JobScheduler::AddTree(&unk_830EA650, parent)
    //   0x82810E70  return parent
    //
    // ⚠️⚠️ FLAG PC-platform leaf: THE DISPATCH, AND ONLY THE DISPATCH.
    // `unk_830EA650` is the global EA::Jobs::JobScheduler singleton, and on this build it DOES
    // NOT EXIST -- CgsHardwareInitPC.cpp:40 has it commented out
    // (`//JobScheduler CgsSystem::HardwareInit::mJobManager; // TODO: Implement HardwareInit`),
    // while the PS3 leaf declares it for real (CgsHardwareInitPS3.cpp:66). There is no scheduler
    // to AddTree to and no job thread to run the tree on. So the AddTree call -- and ONLY that
    // call -- is replaced by running the batch's entry point inline, which is precisely the
    // precedent CgsLooseOctree::StartFrustumTestJobs @0x828B23E0 set for the same reason
    // (CgsLooseOctree.cpp:997: "the JobScheduler::AddJobs call is replaced by running the job's
    // queries inline ... and only that call is replaced").
    //
    // ⭐ AND THE RETURNED JOB IS STILL SAFE TO WaitOn -- CHECKED, NOT ASSUMED.
    // TriangleCacheManager::EndUpdateTriangleCaches calls `mpUpdateTriangleCacheJob->WaitOn()`.
    // EA::Jobs::Job::WaitOn @0x82BCB238 opens with the console's own liveness guard
    // (`if (mJobInstanceHandle.mSchedulerBackend != 0 && ...)`), and a job that was never
    // submitted has a null backend, so it returns immediately. No spin, no hang.
    //
    // ⚠️ ONE CONSEQUENCE, STATED PLAINLY: running inline means the batches execute SEQUENTIALLY
    // inside StartUpdateTriangleCaches instead of concurrently before EndUpdateTriangleCaches.
    // The results are identical (each batch drains from the same command stream and posts into
    // the same result buffer, and both walks are ordered by the same slot scan), but the COST
    // lands in a different place in the frame. Measured, not assumed -- see the wave's fps table.
    // =============================================================================================
    EA::Jobs::Job*
    BaseCollisionGenerator::RunFillTriangleCacheStream(
        const CgsGeometric::PolygonSoupListSpatialMap* lpPolySoupListSpacialMap,
        CgsMemory::SimpleDataStreamProducer*           lpProducer)
    {
        // 0x82810D54: an empty stream dispatches nothing. This is the console's own null.
        if (lpProducer->GetNumCommands() == 0)
        {
            return 0;
        }

        // 0x82810D70..0x82810D80: at most KI_MAX_FILL_BATCHES batches, one per command below that.
        const s32 KI_MAX_FILL_BATCHES = 3;

        s32 liNumBatches = lpProducer->GetNumCommands();
        if (liNumBatches > KI_MAX_FILL_BATCHES)
        {
            liNumBatches = KI_MAX_FILL_BATCHES;
        }

        EA::Jobs::Job* lpParentJob = AllocateJob();

        for (s32 liBatch = 0; liBatch < liNumBatches; ++liBatch)
        {
            CollisionBatch& lrBatch = maCollisionBatches[CreateNewBatch()];

            // The batch's 256-byte descriptor slot, filled through the named Prepare rather than
            // at the console's byte offsets (see CgsFillTriangleCacheStreamJobDesc.h).
            FillTriangleCacheStreamJobDesc* lpDesc =
                reinterpret_cast<FillTriangleCacheStreamJobDesc*>(lrBatch.GetJobDescription().GetBuffer());
            lpDesc->Prepare(lpPolySoupListSpacialMap, lpProducer);

            if (s_miFillTriangleCacheStreamPerfMon > -1)
            {
                CgsDev::PerfMonCpu::StartMonitor(s_miFillTriangleCacheStreamPerfMon);
            }

            EA::Jobs::Job* lpJob = lrBatch.GetJob();
            lpJob->Clear();
            lpJob->SetName("CollisionBatch");
            lpJob->SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                           reinterpret_cast<const void*>(&PolygonSoupTesterEntry), 0);
            lpJob->SetData(lpDesc,
                           static_cast<int>(CollisionJobDescriptionStorage::KU_CONSOLE_BYTES));
            lpJob->SetCodeRecycle(EA::Jobs::EntryPoint::CODE_RECYCLE_ON);
            lpParentJob->DependsOn(*lpJob, EA::Jobs::Event::EVENT_WHEN_JOB_END);   // 0x82810E3C `li r5, 1`

            if (s_miFillTriangleCacheStreamPerfMon > -1)
            {
                CgsDev::PerfMonCpu::StopMonitor(s_miFillTriangleCacheStreamPerfMon);
            }

            // ---- FLAG PC-platform leaf: run the job body here ----
            // X360: JobScheduler::AddTree(&unk_830EA650, lpParentJob) after the loop.
            PolygonSoupTesterEntry(EA::Jobs::Param(static_cast<void*>(lpDesc)),
                                   EA::Jobs::Param(static_cast<void*>(lpDesc)),
                                   EA::Jobs::Param(),
                                   EA::Jobs::Param());
        }

        return lpParentJob;
    }
}
}
