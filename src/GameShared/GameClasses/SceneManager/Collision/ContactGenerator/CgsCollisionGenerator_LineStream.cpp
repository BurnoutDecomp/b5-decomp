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

namespace CgsSceneManager
{
namespace CgsCollision
{
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

    // X360 0x82810D38 (82 insns, EXPORTED -- unlike its Line sibling this one is not a hole; the
    // body was read, and it is the same dispatcher shape). ⛔ NOT RECONSTRUCTED: see the
    // declaration note in CgsCollisionGenerator.h. Returning null is the console's own "nothing
    // dispatched" answer (its first act is `lwz r11, 260(producer) ; cmpi 0 ; li r3, 0` -- an
    // empty stream returns null), and TriangleCacheManager::EndUpdateTriangleCaches' `if (job)
    // WaitOn(job)` handles null exactly as shipped. But a null returned because THE WORKER IS
    // MISSING is not the same fact, so it says so once.
    EA::Jobs::Job*
    BaseCollisionGenerator::RunFillTriangleCacheStream(
        const CgsGeometric::PolygonSoupListSpatialMap*, CgsMemory::SimpleDataStreamProducer*)
    {
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: BaseCollisionGenerator::RunFillTriangleCacheStream "
                       "@0x82810D38 (82) inert -- the triangle-cache FILL WORKER is absent: "
                       "PolygonSoupTesterEntry @0x829157B8 (80) / PolygonSoupTesterJob::Execute "
                       "@0x82915930 (107) / ExecuteFillTriangleCacheStream @0x82915D88 (145) / "
                       "FillTriangleCache @0x82915FD0 (219) / AllocateMemory @0x82916B98 (99) / "
                       "RunBoxQuery @0x82916D28 (46) / LoadPrimitive @0x82916AB8 (8) / "
                       "PolygonSoupListSpatialMap::RunQuery @0x82843A80 (261) / "
                       "ExtractTriangle4ListIntersectingSphere @0x82844C80 (602) "
                       "[FLAG PC boot gate]\n";
        }
        return 0;
    }
}
}
