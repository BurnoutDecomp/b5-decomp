#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include <new> // placement new (per-batch job construction in Prepare)

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h" // CollisionResultList (complete: by-value return)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h" // PrimitivePairList::GetNumTests
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"      // TriangleList (descriptor copy)
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsPrimitiveListWithTriangleListJobDesc.h" // the type-11 descriptor
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"        // SimpleDataStreamProducer (CreateStreamProducer)
#include "SDKs/EATech/eajobs/job.h"                                                       // EA::Jobs::Job (AllocateJob)
#include "SDKs/EATech/eajobs/job_types.h"                                                 // EA::Jobs::Param (the inline dispatch)

// The contact-generator job entry every CollisionBatch is wired to
// (GameShared/Jobs/ContactGenerator/ContactGenerator.cpp; X360 0x82920F10). GLOBAL scope, exactly
// as CgsCollisionGenerator_CollideStreams.cpp:84 declares it and as ContactGenerator.cpp:64
// defines it.
void ContactGeneratorEntry(EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param);

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

// =================================================================================================
// byte_82F310B0 -- the file-scope "optimised box tests are available" switch that
// CollidePrimitiveListAgainstTriangleList ANDs with its caller's request before seating the
// descriptor flag. AUTHORED NAME (0x82F310B0 carries no symbol and no DWARF candidate is
// identified), MEASURED VALUE.
//
// MEASURED 2026-08-19 with headless IDA 9.3 on a PRIVATE copy of IDA Files/BURNOUT_X360_ARTIST.XEX.i64
// (scratchpad/waveQ6/ida_worldc/): the 64 bytes at 0x82F310A0 read
//     00000000 00000000 00000000 82000B1C  01000000 FFFFFFFF 82000B1C 00000000 ...
// so 0x82F310B0 == 0x01, and an `idautils.XrefsTo` scan over the WHOLE IDB returns EXACTLY ONE
// reference to it -- the `lbz` at 0x828142A8 in that one function. No writer is known to IDA.
// ⚠️ WHY THE 0x82FB9xxx "PLACEHOLDER ZERO" TRAP (gotcha 13) DOES NOT APPLY HERE, stated so nobody
// has to re-derive it: that trap is zero-in-the-static-image + a dynamic-initialiser thunk that
// writes the real value at startup. This byte is NON-ZERO in the static image and sits inside an
// initialised .data run beside a live pointer word (0x82000B1C), so there is nothing for a thunk
// to supply. (I did NOT walk the 0x82CD0014..0x82CD3170 initialiser table for this address -- the
// static value plus the single-xref scan is the whole measurement, and it is stated as such.)
// So the shipped console build has the switch ON. ⚠️ The read is a RUNTIME load, not a folded
// constant, so the AND is reproduced at the use site rather than optimised away here.
// ⚠️ ITS HOME IS NOT RECOVERED. The DWARF names no such variable in CgsCollisionGenerator.cpp, so
// this is a file-local definition in the same MOVE-WHEN-IT-LANDS form the tree already uses for
// KF_TRIANGLE_CACHE_PADDING (PropManager_wQ_03.cpp:60) and for byte_82F2A39C's twin in
// PropManager_wQ2_03.cpp. If a real home appears, collapse it there.
// =================================================================================================
static const bool KB_OPTIMISED_BOX_TESTS_ENABLED = true;   // byte_82F310B0 == 0x01, measured

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

// =================================================================================================
// BaseCollisionGenerator::CollidePrimitiveListAgainstTriangleList @0x828141D8  (86 asm insns)
//
// ⭐ BODIED 2026-08-19 (wave Q6, cluster B). Declaration + full evidence: CgsCollisionGenerator.h.
// The SYNCHRONOUS twin of AddPrimitiveListWithTriangleListToStream -- one primitive-pair list
// against one triangle list, posted as a single type-11 collision batch. Its two PROP call sites
// (PropManager::DoPart/DoPropInstanceWorldContactGeneration, landed this wave) are the arm the
// runtime selector byte_82F2A39C does NOT take, but the selector is a RUNTIME load so both arms
// are emitted and both must exist; its other five call sites (DeformableObject x3, VehicleManager
// traffic x2) are ordinary reconstructed functions.
//
// GROUNDING: the RAW `assembly` array of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x828141D8.json
// (Hex-Rays pseudocode NOT consulted), plus headless IDA 9.3 on a PRIVATE copy of the .i64 for
// the boundaries (0x828141D8..0x82814330 == 86 insns), the xrefs, and the two data globals below.
//
// ---- DECODE, address by address ----------------------------------------------------------------
//   0x828141E4..0x82814200  the six parameters spill to r28/r25/r29/r27/r26/r24 in that order.
//   0x82814204..0x82814228  assert "lpPrimitiveList != NULL"                              (:1940)
//   0x8281422C..0x8281424C  assert "lpTriangleList != NULL"                                (:1941)
//   0x82814250  lhz r11, 6(r28)  == PrimitivePairList::mu16NumTests, then
//               assert "lpPrimitiveList->GetNumTests() > 0"                                (:1942)
//               ⚠️ All three are NON-GATING -- the asm falls straight through each one.
//   0x82814288  bl PrepareNewPrimitiveTestResultsList(r4=maxResults, r5=tagA, r6=tagB)
//               -> r29 = the result-list index. This is the value the function returns.
//   0x82814294  bl CreateNewBatch()                       -> r3 = the batch slot
//   0x82814298..0x828142B0  `clrlwi r10,r29,16 ; addi r10,r10,0x4820 ; slwi r10,r10,2 ;
//               lwzx r6, r10, r31` -- 0x4820*4 == 0x12080 == offsetof(mapCollisionResultLists),
//               so r6 is mapCollisionResultLists[resultIndex].
//               ⚠️ 0x12080 IS A CONSOLE OFFSET (gotcha 1): the host array holds 8-byte pointers
//               behind a wider IOBuffer base. Reached by NAME below.
//   0x828142A8..0x828142C8  `lbz byte_82F310B0` AND the caller's bool -> the descriptor flag.
//               ⭐ MEASURED THIS WAVE, not assumed (gotcha 13): the 64 bytes at 0x82F310A0 read
//                  00000000 00000000 00000000 82000B1C | 01000000 FFFFFFFF 82000B1C 00000000 ...
//               so the byte at 0x82F310B0 is 0x01 -- it sits INSIDE an initialised .data run next
//               to a live pointer word, not on a zero page. A whole-image xref scan returns
//               EXACTLY ONE reference to 0x82F310B0 (this `lbz`): no writer, and no MSVC
//               dynamic-initialiser thunk. So on the shipped console build the global arm is
//               ENABLED and the flag is just the caller's bool -- but the read is a RUNTIME load,
//               so the AND is reproduced rather than folded away.
//               (The neighbouring word 0x82F310B4 is this class's own _miStartJobsPerfMon, whose
//                static image value is 0xFFFFFFFF == -1; the tree initialises it to 0. Behaviour-
//                neutral -- Construct() overwrites it with AddMonitor's handle before any use, and
//                PerfMonCpu ignores a negative handle -- reported, not changed, because the static
//                initialiser is not this function's to move.)
//   0x828142CC..0x828142E8  `clrlwi r11,r3,16 ; slwi r10,r11,3 ; add r11,r11,r10 ; slwi r11,r11,7`
//               == batchIndex * 9 * 128 == batchIndex * 1152 == the CONSOLE sizeof(CollisionBatch),
//               then `add r31,r11,r31 ; addi r3,r31,0x3D0`. 0x3D0 == 0x80 (maCollisionBatches) +
//               0x350 (CollisionBatch::mJobDescription). Both are CONSOLE numbers; the host
//               indexes the typed array and asks the batch for its descriptor buffer.
//   0x828142EC  bl PrimitiveListWithTriangleListJobDesc::Prepare(pairList, triList, resultList,
//               flag) -- which seats muJobType = 11 == E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST.
//   0x828142F0..0x828142F8  PerfMonCpu::StartMonitor(dword_82F310B4 == _miStartJobsPerfMon)
//   0x828142FC..0x82814304  `addi r31,r31,0x80` -> &maCollisionBatches[i]; bl CollisionBatch::SetupJob
//   0x82814308..0x82814318  `lis r11, unk_830EA650 ; li r5,1 ; mr r4,r31 ;
//               bl EA::Jobs::JobScheduler::AddJobs`  (r4 == the batch base == &batch.mJob, mJob
//               being CollisionBatch's first member, so this submits the one job).
//   0x8281431C  PerfMonCpu::StopMonitor(_miStartJobsPerfMon)
//   0x82814324  `mr r3, r29` -- the result-list index, NOT truncated on the return path.
//
// ⚠️⚠️ FLAG PC-platform leaf: THE DISPATCH, AND ONLY THE DISPATCH. `unk_830EA650` is the global
// EA::Jobs::JobScheduler singleton and it does not exist on this build (CgsHardwareInitPC.cpp:40
// has it commented out), so the AddJobs call -- and only that call -- is replaced by running the
// batch's entry point inline. That is the standing precedent of this subsystem
// (CgsLooseOctree.cpp:997, and the five committed dispatchers in
// CgsCollisionGenerator_CollideStreams.cpp / _LineStream.cpp).
//
// ⚠️ THE TYPE-11 WORKER IS A LOUD NAMED GATE, NOT A BODY: ContactGeneratorJob::Execute case 11 ->
// ExecutePrimitiveListWithTriangleList (ContactGeneratorJob.cpp:229/:992). Landing this dispatcher
// over a named gate is the same precedent the committed Run* dispatchers rest on -- the moment a
// command flows, the gate names the missing kernel in the log. It is NOT a silent drop.
// =================================================================================================
u16 BaseCollisionGenerator::CollidePrimitiveListAgainstTriangleList(
    const PrimitivePairList* lpPrimitiveList,
    const TriangleList*      lpTriangleList,
    u16                      lu16MaxResults,
    u32                      luUserTagA,
    u16                      lu16UserTagB,
    bool                     lbUseOptimisedBoxTests)
{
    // :1940 / :1941 / :1942 -- all three non-gating (the asm falls through every one).
    CGS_ASSERT(lpPrimitiveList != NULL, "lpPrimitiveList != NULL");
    CGS_ASSERT(lpTriangleList != NULL, "lpTriangleList != NULL");
    CGS_ASSERT(lpPrimitiveList->GetNumTests() > 0, "lpPrimitiveList->GetNumTests() > 0");

    const s32 liResultListIndex =
        PrepareNewPrimitiveTestResultsList(lu16MaxResults, luUserTagA, lu16UserTagB);  // 0x82814288

    CollisionBatch& lrBatch = maCollisionBatches[CreateNewBatch()];                    // 0x82814294

    PrimitiveListWithTriangleListJobDesc* lpDesc =
        reinterpret_cast<PrimitiveListWithTriangleListJobDesc*>(
            lrBatch.GetJobDescription().GetBuffer());                                  // batch +0x3D0

    // 0x828142A8..0x828142C8 -- the global enable ANDed with the caller's request. See the decode
    // block: byte_82F310B0 is a MEASURED 0x01 with no writer anywhere in the image.
    const bool lbOptimisedBoxTests = KB_OPTIMISED_BOX_TESTS_ENABLED && lbUseOptimisedBoxTests;

    lpDesc->Prepare(lpPrimitiveList, lpTriangleList,
                    mapCollisionResultLists[liResultListIndex],                        // by NAME
                    lbOptimisedBoxTests);                                              // 0x828142EC

    CgsDev::PerfMonCpu::StartMonitor(_miStartJobsPerfMon);                             // 0x828142F8

    lrBatch.SetupJob();                                                                // 0x82814304
    // ---- FLAG PC-platform leaf: run the job body here (see the decode block) ----
    // X360: EA::Jobs::JobScheduler::AddJobs(&unk_830EA650, &lrBatch.mJob, 1) at 0x82814318.
    ContactGeneratorEntry(EA::Jobs::Param(static_cast<void*>(lpDesc)),
                          EA::Jobs::Param(static_cast<void*>(lpDesc)),
                          EA::Jobs::Param(),
                          EA::Jobs::Param());

    CgsDev::PerfMonCpu::StopMonitor(_miStartJobsPerfMon);                              // 0x8281431C

    // 0x82814324 `mr r3, r29` -- the console does NOT truncate on the return path; the u16 return
    // type is the DWARF's (CgsCollisionGenerator.h:257). Every call site drops the value.
    return static_cast<u16>(liResultListIndex);
}

}
}
