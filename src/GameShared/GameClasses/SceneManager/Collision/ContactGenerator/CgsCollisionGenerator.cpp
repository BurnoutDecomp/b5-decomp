#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include <new> // placement new (per-batch job construction in Prepare)

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h" // CollisionResultList (complete: by-value return)
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"        // SimpleDataStreamProducer (CreateStreamProducer)
#include "SDKs/EATech/eajobs/job.h"                                                       // EA::Jobs::Job (AllocateJob)

// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.cpp
//
// BaseCollisionGenerator lifecycle + collision-batch ring management. Reconstructed from the
// X360 ARTIST asm (ground truth for offsets/counts) with member names/shape from the DecFIGS
// DWARF. The batch ring is 64 wide on X360 (KU16_MAX_NUM_BATCHES); mabUsedBatches[i] tracks
// which of the 64 embedded jobs is in flight.
//
// NOT reconstructed here (blocked): the CollideLine*/TestLine* VMX kernels (0x82812AE0,
// 0x828131C0 CollideLineAgainstPolySoupListNea, 0x82813978 TestLineAgainstPolySoupListDouble)
// -- large hand-written VMX pipelines that need the un-homed
// CgsGeometric::IntersectLinePolygonSoupNearestSingleSided, the PolygonSoupListSpatialMap query
// path and permute-table constants (vpermwi128 0x4B/0x87, sub_82843E98).
//
// ⭐⭐ STALE-BLOCKED PARAGRAPH RETIRED 2026-08-14 (walls leg 1). This banner used to list the
// Run*/Add*/PrepareNewPrimitiveTestResultsLi family as blocked on (a) the JobScheduler singleton,
// (b) the un-exported job entry symbols, (c) an "un-homed descriptor variant" and (d)
// "un-attested CollisionResultList header fields". Every one of those blockers had already
// fallen by the time it was re-read: (a) the CgsLooseOctree.cpp:997 inline-dispatch precedent
// (used by both dispatchers in CgsCollisionGenerator_LineStream.cpp since 2026-08-10/11),
// (b) ContactGeneratorEntry is homed in GameShared/Jobs/ContactGenerator/, (c) the descriptor
// "variant" is the per-family StreamCommand/Data the DWARF names verbatim, and (d) the DWARF
// names every CollisionResultList field (CgsCollisionResultList.h:163-169). The family is REAL
// in CgsCollisionGenerator_CollideStreams.cpp; only CollidePrimitivePairList keeps a gate
// (CgsCollisionGenerator_StreamStubs.cpp).

namespace CgsSceneManager
{
namespace CgsCollision
{

// Static perfmon latch (X360 byte_83011D70 / dword_82F310B4).
bool BaseCollisionGenerator::_mbInitializedPerfMons = false;
s32  BaseCollisionGenerator::_miStartJobsPerfMon    = 0;

// X360 0x828105F8. Register the shared "StartJobs" CPU perfmon exactly once (guarded by the
// static latch). Only the two static side effects are grounded in the asm; the AddMonitor
// call's parent-handle argument (r6) is an un-set live-in in this frame -- passed as -1 ("no
// parent"), matching the identical SceneManagerModule perfmon call site.
void BaseCollisionGenerator::Construct()
{
    if (!_mbInitializedPerfMons)
    {
        _miStartJobsPerfMon = CgsDev::PerfMonCpu::AddMonitor("StartJobs", 12, 0, 10.0, -1, 1);
        _mbInitializedPerfMons = true;
    }
}

// X360 0x8284CB38. Empty (a single blr; ICF-folded with the generic empty-teardown body many
// other classes share). The console body genuinely does nothing.
void BaseCollisionGenerator::Destruct()
{
}

// X360 0x82810660. Reset the batch/result bookkeeping, adopt the caller's result buffer into
// the bump allocator (16-byte aligned), and construct all 64 collision batches (each batch's
// embedded job in its empty state). The generator lives in raw IOBuffer-stack memory, so the
// sub-object constructors are run here rather than by an aggregate ctor. Always returns true.
bool BaseCollisionGenerator::Prepare(void* lpResultBuffer, s32 liResultBufferSize)
{
    mu16NumUsedResultLists = 0;
    mu16NumUsedBatches     = 0;
    for (s32 liBatch = 0; liBatch < KU16_MAX_NUM_BATCHES; ++liBatch)
        mabUsedBatches[liBatch] = false;

    mCollisionResultsAllocator.Construct();
    mCollisionResultsAllocator.Create(lpResultBuffer, static_cast<size_t>(liResultBufferSize));
    mCollisionResultsAllocator.SetAlignment(16);

    for (s32 liBatch = 0; liBatch < KU16_MAX_NUM_BATCHES; ++liBatch)
        new (&maCollisionBatches[liBatch]) CollisionBatch();

    return true;
}

// X360 0x828128D8. Drain the ring: wait on every in-flight batch job, then clear its used flag.
void BaseCollisionGenerator::Finish()
{
    for (u16 lu16BatchIndex = 0; lu16BatchIndex < KU16_MAX_NUM_BATCHES; ++lu16BatchIndex)
    {
        if (mabUsedBatches[lu16BatchIndex])
        {
            maCollisionBatches[lu16BatchIndex].WaitOn();
            mabUsedBatches[lu16BatchIndex] = false;
        }
    }
}

// X360 0x82810718. Wait on one specific batch's job and release its slot. Asserts the slot was
// actually in use first.
void BaseCollisionGenerator::FinishBatch(u16 lu16BatchIndex)
{
    CGS_ASSERT(mabUsedBatches[lu16BatchIndex],
               "Attempting to finsh a batch that hasn't been started");
    maCollisionBatches[lu16BatchIndex].WaitOn();
    mabUsedBatches[lu16BatchIndex] = false;
}

// X360 0x82810960. Claim the next slot in the 64-wide batch ring: if that slot is still busy,
// finish it first, then bump the running count and mark the slot in use. Returns the slot index.
u16 BaseCollisionGenerator::CreateNewBatch()
{
    const u16 lu16BatchIndex = static_cast<u16>(mu16NumUsedBatches % KU16_MAX_NUM_BATCHES);

    if (mabUsedBatches[lu16BatchIndex])
        FinishBatch(lu16BatchIndex);

    ++mu16NumUsedBatches;

    CGS_ASSERT(!mabUsedBatches[lu16BatchIndex], "Trying to use a job thats already in use");

    mabUsedBatches[lu16BatchIndex] = true;
    return lu16BatchIndex;
}

// X360 0x825B2AE0. Return a by-value copy of the luIndex'th result list. Bounds-checked
// against mu16NumUsedResultLists first, then the CollisionResultList the slot points to is
// copied into the caller's sret storage (the asm loads the 4-word list header from
// mapCollisionResultLists[luIndex] and stores it into the return buffer). The assert
// message is verbatim X360 rodata (CgsCollisionGenerator.h:303; file/line dropped).
CollisionResultList BaseCollisionGenerator::GetResultList(u16 luIndex) const
{
    CGS_ASSERT(luIndex < mu16NumUsedResultLists, "luIndex < mu16NumUsedResultLists");
    return *mapCollisionResultLists[luIndex];
}

// X360 0x82810588. Allocate sizeof(Job) bytes from the result bump allocator at 128-byte
// alignment, placement-construct one empty (null-named) EA::Jobs::Job in it, then restore the
// allocator's previous alignment. Returns null when the bump allocator overflows (the asm's
// `beq` short-circuits the Job ctor to a null result). The allocator's live alignment is read
// back via GetAlignment() (X360 reads the allocator's alignment member directly, +0x10) so the
// burst is alignment-neutral to the caller.
EA::Jobs::Job* BaseCollisionGenerator::AllocateJob()
{
    const size_t lnSavedAlignment = mCollisionResultsAllocator.GetAlignment();
    mCollisionResultsAllocator.SetAlignment(128);

    EA::Jobs::Job* lpJob = nullptr;
    void* lpvJobMemory = mCollisionResultsAllocator.Malloc(sizeof(EA::Jobs::Job));
    if (lpvJobMemory)
        lpJob = new (lpvJobMemory) EA::Jobs::Job(nullptr);

    mCollisionResultsAllocator.SetAlignment(lnSavedAlignment);
    return lpJob;
}

// X360 0x828109F8 (ledger identity "Crea" -- IDA-truncated; reconstructed under its descriptive
// name). Carve a SimpleDataStreamProducer and its command + result buffers out of the result
// allocator at 128-byte alignment, size the two buffers with the producer's own requirement
// helper, construct the producer over them, then restore the allocator alignment. The stream
// carries fixed-geometry commands (32-byte command records, 16-byte result records), so the
// command/result counts both come from liMaxCommands (r4). The two allocation-failure asserts
// are the X360 streamed-message form (BasePriorityQueue::Clear + operator<< + FireAssert); their
// messages are string literals here, matching this file's CGS_ASSERT convention. The
// GetRequiredBufferSizes result-size out-slot is a single u32 on X360 (the frame reserved three
// words but only the first is read).
CgsMemory::SimpleDataStreamProducer* BaseCollisionGenerator::CreateStreamProducer(s32 liMaxCommands)
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
        liMaxCommands, 32, liMaxCommands, 16, &luCommandBufferSize, &luResultBufferSize);

    void* lpCommandBuffer = mCollisionResultsAllocator.Malloc(luCommandBufferSize);
    void* lpResultBuffer  = mCollisionResultsAllocator.Malloc(luResultBufferSize);
    CGS_ASSERT(lpCommandBuffer != nullptr && lpResultBuffer != nullptr,
               "Failed to allocate stream buffers\n");

    lpProducer->Construct(liMaxCommands, 32, lpCommandBuffer, liMaxCommands, 16, lpResultBuffer);

    mCollisionResultsAllocator.SetAlignment(lnSavedAlignment);
    return lpProducer;
}

}
}
