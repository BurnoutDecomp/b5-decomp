#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include <new>     // placement new (per-batch job construction in Prepare)
#include <cstdlib> // std::getenv (the BRN_PROP_DIAG latch -- diagnostics only, see the [DIAG] block)

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint (the [DIAG] block only)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h" // CollisionResultList (complete: by-value return)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h" // PrimitivePairList::GetNumTests
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"      // TriangleList (descriptor copy)
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsPrimitiveListWithTriangleListJobDesc.h" // the type-11 descriptor
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsPrimitiveListWithTriangleListStreamJobDesc.h" // the type-12 descriptor + its StreamCommand
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsPrimitivePairListJobDesc.h" // the type-10 descriptor (CollidePrimitivePairList)
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
// in CgsCollisionGenerator_CollideStreams.cpp.
//
// ⭐⭐ AND THE LAST GATE OF THAT FAMILY IS GONE TOO (2026-08-19, wave Q7 cluster `pairlist`). The
// sentence that used to close the paragraph above -- "only CollidePrimitivePairList keeps a gate
// (CgsCollisionGenerator_StreamStubs.cpp)" -- is retired: that body is BELOW, and the gate file it
// named now holds no definition at all (banner-only; reported for UNMOUNT, bat:1001).

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

// =================================================================================================
// BaseCollisionGenerator::CollidePrimitivePairList @0x82814138  (40 asm insns)
//
// ⭐⭐ BODIED 2026-08-19 (wave Q7, cluster `pairlist`) -- THE LAST GATE IN
// CgsCollisionGenerator_StreamStubs.cpp. That TU is banner-only now and is reported for UNMOUNT
// (build_game_exe.bat:1001); this body and that deletion are ONE change, because both TUs are
// mounted and an LNK2005 is invisible to `cl /c` (gotcha 7 / the pstream precedent).
//
// The SYNCHRONOUS primitive-pair leg: one already-built PrimitivePairList (a pair of primitive
// indices per test, no second list) posted as a single type-10 collision batch. It is the plainest
// member of the Collide* family -- the sibling CollidePrimitiveListAgainstTriangleList @0x828141D8
// above is the SAME 12 steps plus three non-gating asserts, a second (triangle) list and the
// optimised-box global. Diffed instruction-for-instruction against it; every delta is called out
// below rather than silently smoothed over.
//
// GROUNDING: the RAW `assembly` array of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82814138.json --
// 40 instructions, 0x82814138..0x828141D4, which is exactly (0x828141D8-0x82814138)/4, so the
// function is complete and abuts its sibling. (Hex-Rays pseudocode was read only AFTER the decode,
// as a cross-check; it agrees, including `Prepare(v10 + 976, a2, v9)` == batch +0x3D0.)
// ⚠️ The retired gate's own banner claimed "(92)" instructions. That number was wrong -- it is 40.
// A wrong comment is a real defect (gotcha 9); corrected here and in the StreamStubs banner.
//
// ---- SIGNATURE, from the prologue (asm arbitrates, DWARF names) ---------------------------------
// FOUR arguments, none of them a float, so no GPR slot is skipped (gotcha 3 does not bite):
//   0x82814144  mr r29, r4     ; r4 = lpPrimitiveList  (kept for the descriptor Prepare)
//   0x82814148  mr r4,  r5     ; r5 = lu16MaxNumCollisions  -> becomes arg 1 of the next call
//   0x8281414C  mr r5,  r6     ; r6 = luUserTagA            -> arg 2
//   0x82814150  mr r6,  r7     ; r7 = lu16UserTagB          -> arg 3
//   0x82814154  mr r31, r3     ; this
// which is PrepareNewPrimitiveTestResultsList(u16 maxResults, u32 tagA, u16 tagB) verbatim -- the
// same three-argument shuffle the two committed Add* posters make. DecFIGS confirms the shape and
// supplies the names: CgsCollisionGenerator.h:292 declares
//     uint16_t CollidePrimitivePairList(const PrimitivePairList *, uint16_t, uint32_t, uint16_t);
// and CgsCollisionGenerator.cpp:1662 names the parameters lpPrimitiveList / lu16MaxNumCollisions /
// luUserTagA / lu16UserTagB and the two locals lu16ResultListIndex / lu16BatchIndex (:1664/:1665).
// ⚠️ THE HEADER'S OLD PARAMETER NAMES WERE WRONG: it spelled the last two `u32 luFlags, u16
// lu16Tag`. They are the USER TAGS this family threads into the CollisionResultList, not a flag
// word -- PrepareNewPrimitiveTestResultsList stores them straight into the list header. Renamed to
// the DWARF's names with this body. The one recovered call site had the same misname in its
// literals and was corrected in the same wave: BrnVehicleManagerContactGeneration.cpp now spells
// them KU_COLLIDE_USER_TAG_A (11) / KU16_COLLIDE_USER_TAG_B (0).
//
// ---- DECODE, address by address ----------------------------------------------------------------
//   0x82814158  bl PrepareNewPrimitiveTestResultsList(maxResults, tagA, tagB) -> r30 = the index.
//               This is the value the function returns.
//   0x82814164  bl CreateNewBatch()                       -> r3 = the batch slot
//   0x82814168..0x82814178  `clrlwi r11,r30,16 ; addi r11,r11,0x4820 ; slwi r11,r11,2 ;
//               lwzx r5, r11, r31` -- 0x4820*4 == 0x12080 == offsetof(mapCollisionResultLists), so
//               r5 is mapCollisionResultLists[resultIndex], argument 2 of the descriptor Prepare.
//               ⚠️ 0x12080 IS A CONSOLE OFFSET (gotcha 1): the host array holds 8-byte pointers
//               behind a wider IOBuffer base. Reached BY NAME below. The `clrlwi` is why the index
//               is narrowed to u16 at the subscript here and nowhere else.
//   0x8281416C  mr r4, r29                                -- the pair list, argument 1
//   0x8281417C..0x8281418C  `clrlwi r11,r3,16 ; slwi r10,r11,3 ; add r11,r11,r10 ; slwi r11,r11,7`
//               == batchIndex * 9 * 128 == batchIndex * 1152 == the CONSOLE sizeof(CollisionBatch);
//               `add r31,r11,r31`.
//   0x82814190  `addi r3, r31, 0x3D0` == 0x80 (maCollisionBatches) + 0x350
//               (CollisionBatch::mJobDescription). Both CONSOLE numbers; the host indexes the typed
//               array and asks the batch for its descriptor buffer.
//   0x82814194  bl PrimitivePairListJobDesc::Prepare(pairList, resultList) -- TWO arguments (this
//               family has no optimised-box flag), and it seats muJobType = 10 ==
//               E_COLLISIONJOB_PRIMITIVE_PAIR_LIST (CgsPrimitivePairListJobDesc.cpp:25).
//   0x82814198..0x828141A0  PerfMonCpu::StartMonitor(dword_82F310B4 == _miStartJobsPerfMon)
//   0x828141A4..0x828141AC  `addi r31,r31,0x80` -> &maCollisionBatches[i]; bl CollisionBatch::SetupJob
//   0x828141B0..0x828141C0  `lis r11, unk_830EA650 ; li r5,1 ; mr r4,r31 ;
//               bl EA::Jobs::JobScheduler::AddJobs`  (r4 == the batch base == &batch.mJob, mJob
//               being CollisionBatch's first member, so this submits the one job).
//   0x828141C4/C8  PerfMonCpu::StopMonitor(_miStartJobsPerfMon)
//   0x828141CC  `mr r3, r30` -- the result-list index, NOT truncated on the return path.
//
// ⚠️ NO ASSERTS AT ALL. Unlike its sibling, this leg's `xrefs_from` set contains no
// BeginAssert/FireAssert/EndAssert (measured -- the JSON lists exactly seven callees:
// __savegprlr_29, PrepareNewPrimitiveTestResultsList, CreateNewBatch, PrimitivePairListJobDesc::
// Prepare, PerfMonCpu::Start/StopMonitor, CollisionBatch::SetupJob, JobScheduler::AddJobs). The
// null/GetNumTests() guards its sibling carries are NOT in this function; adding them would be
// invented behaviour. Both live call sites guard `GetNumTests() != 0` themselves.
//
// ⚠️⚠️ FLAG PC-platform leaf: THE DISPATCH, AND ONLY THE DISPATCH. `unk_830EA650` is the global
// EA::Jobs::JobScheduler singleton and it does not exist on this build (CgsHardwareInitPC.cpp:40
// has it commented out), so the AddJobs call -- and only that call -- is replaced by running the
// batch's entry point inline. Standing precedent of this subsystem (CgsLooseOctree.cpp:997, the
// five committed dispatchers in CgsCollisionGenerator_CollideStreams.cpp / _LineStream.cpp, and
// CollidePrimitiveListAgainstTriangleList directly above).
//
// ⚠️ AND BECAUSE THE DISPATCH IS INLINE, the _miStartJobsPerfMon bracket around it no longer
// measures the same thing on both platforms: on PC it times SUBMIT + EXECUTE (the whole narrow-
// phase walk runs inside StartMonitor/StopMonitor), on the console it times SUBMIT ONLY (AddJobs is
// ~a queue push). The counter is therefore NOT comparable to the console's. That became material
// only in wave Q7, when the type-10 worker stopped being a gate and started doing real work.
//
// ⭐ THE TYPE-10 WORKER IS A REAL BODY (wave Q7, 2026-08-19): ContactGeneratorJob::Execute case 10
// -> ExecutePrimitivePairList @0x82925798, reconstructed in ContactGeneratorJob.cpp (the Itterator
// walk: BuildGPInstance A/B -> CollideGPInstances -> the 16-byte header write-back). This leg is
// bodied end to end -- a posted primitive-pair batch now produces real contacts.
//
// ---- CALL SITES (xrefs_to, measured; both in another owner's file) ------------------------------
//   VehicleManager::StartVehicleContactGeneration @0x8262AEE8 -- the two simple-traffic pair lists,
//     ALREADY CALLING THIS (BrnVehicleManagerContactGeneration.cpp:344/:351) with
//     (200 results, tagA 11, tagB 0). Both are guarded by `GetNumTests() != 0`, and on the junkyard
//     path both builders are empty, so nothing posts YET -- it goes live with traffic.
//   VehicleManager::StartPartContactGeneration    @0x8262C220 -- three calls at 0x8262C28C /
//     0x8262C2D4 / 0x8262C31C (part / wheel / hinged builders; `li r5,0x64` == 100 results,
//     tagA 3 / 4 / 2, tagB 0), each followed by its marker AddEntry. That tail is a documented
//     PARTIAL (BrnVehicleManagerContactGeneration.cpp:868, owner `carcar`); nothing more is needed
//     FROM THIS FUNCTION for it -- the whole rest of its closure is already real too (the Create/Run
//     pair @0x82811DD0/@0x82811F58 landed wave Q6, DataStreamCommandPoster::Begin, and the two
//     DeformationManager Do*WorldContactGeneration drivers).
// =================================================================================================
u16 BaseCollisionGenerator::CollidePrimitivePairList(const PrimitivePairList* lpPrimitiveList,
                                                     u16                      lu16MaxNumCollisions,
                                                     u32                      luUserTagA,
                                                     u16                      lu16UserTagB)
{
    const s32 liResultListIndex =
        PrepareNewPrimitiveTestResultsList(lu16MaxNumCollisions, luUserTagA, lu16UserTagB); // 0x82814158

    // ⭐ [DIAG] NOT IN THE X360 BINARY -- wave Q7, behind BRN_PROP_DIAG, ONE-SHOT (the getenv is
    // latched once; this sits on the per-frame contact path). This leg is DEAD on the junkyard
    // route today -- both simple-traffic builders report GetNumTests() == 0 and their call sites
    // guard on it -- so the first line this ever prints is the proof that it went live, and the
    // user tag says WHICH caller woke it: 11 == StartVehicleContactGeneration's simple-traffic
    // pass, 3/4/2 == StartPartContactGeneration's part / wheel / hinged legs (that tail is still
    // a documented PARTIAL, owner `carcar`). Read it against [prop-diag] contact and against the
    // type-10 worker's own [Q7-pairs] line: the worker (ContactGeneratorJob::ExecutePrimitivePairList
    // @0x82925798) is a REAL body since wave Q7, so this line with no contacts behind it means the
    // pairs did not overlap, not that a gate ate the batch.
    {
        static const bool sbPropDiag  = (std::getenv("BRN_PROP_DIAG") != 0);
        static bool       sbFirstPass = true;
        if (sbPropDiag && sbFirstPass && CgsDev::Log::gpDebugPrint != 0)
        {
            sbFirstPass = false;
            *CgsDev::Log::gpDebugPrint
                << "[Q7-pairlist] first CollidePrimitivePairList: tests="
                << static_cast<s32>(lpPrimitiveList->GetNumTests())
                << " maxResults=" << static_cast<s32>(lu16MaxNumCollisions)
                << " tagA=" << static_cast<s32>(luUserTagA)
                << " resultList=" << liResultListIndex
                << "\n";
        }
    }

    CollisionBatch& lrBatch = maCollisionBatches[CreateNewBatch()];                         // 0x82814164

    PrimitivePairListJobDesc* lpDesc =
        reinterpret_cast<PrimitivePairListJobDesc*>(
            lrBatch.GetJobDescription().GetBuffer());                                       // batch +0x3D0

    // `clrlwi r11, r30, 16` @0x82814168 -- the index is narrowed to 16 bits for the subscript
    // only. Reached BY NAME; 0x12080 is a console offset (see the decode block).
    lpDesc->Prepare(lpPrimitiveList,
                    mapCollisionResultLists[static_cast<u16>(liResultListIndex)]);          // 0x82814194

    CgsDev::PerfMonCpu::StartMonitor(_miStartJobsPerfMon);                                  // 0x828141A0

    lrBatch.SetupJob();                                                                     // 0x828141AC
    // ---- FLAG PC-platform leaf: run the job body here (see the banner) ----
    // X360: EA::Jobs::JobScheduler::AddJobs(&unk_830EA650, &lrBatch.mJob, 1) at 0x828141C0.
    ContactGeneratorEntry(EA::Jobs::Param(static_cast<void*>(lpDesc)),
                          EA::Jobs::Param(static_cast<void*>(lpDesc)),
                          EA::Jobs::Param(),
                          EA::Jobs::Param());

    CgsDev::PerfMonCpu::StopMonitor(_miStartJobsPerfMon);                                   // 0x828141C8

    // 0x828141CC `mr r3, r30` -- no truncation on the return path; the u16 return type is the
    // DWARF's (CgsCollisionGenerator.h:292). All five measured call sites drop the value.
    return static_cast<u16>(liResultListIndex);
}

// =================================================================================================
// ⭐⭐⭐ THE PRIMITIVE-PAIR-LIST vs TRIANGLE-LIST STREAM FAMILY -- LANDED 2026-08-19 (wave Q6,
// cluster pstream). Three functions, and this is the arm the console's runtime selector actually
// takes for prop/part-vs-world collision, so it is the LIVE half of "a smashed prop's parts land
// on the road instead of falling through it".
//
//   AddPrimitiveListWithTriangleListToStream         @0x82811D40  (35)   the command POSTER
//   CreateCollidePrimitiveListWithTriangleListStream @0x82811DD0  (98)   the producer FACTORY
//   RunCollidePrimitiveListWithTriangleListStream    @0x82811F58  (80)   the batch DISPATCHER
//
// WHY THEY LIVE IN THIS TU. The DWARF puts all three in CgsCollisionGenerator.cpp (source lines
// 1996 / 2028 / 2076 -- the same dumpfile that names their parameters and locals), which is this
// file. The tree's sibling partfile CgsCollisionGenerator_CollideStreams.cpp holds the three
// OTHER collide-stream families; it is not this owner's file, and its Create* helper has
// internal linkage, which is why the factory below is open-coded rather than calling it. See the
// factory's own banner.
//
// CONSUMERS (xrefs measured this wave with headless IDA 9.3 on a private .i64 copy):
//   Add  @0x82811D40 -- DeformableObject::DoBodyPartWorldContactGeneration x2 (0x82609720 /
//                       0x82609840), ::DoDetachedWheelWorldContactGeneration (0x82609AA4),
//                       PropManager::DoPartWorldContactGeneration (0x826120C0) and
//                       ::DoPropInstanceWorldContactGeneration (0x8261261C). The last two are
//                       LANDED AND MOUNTED (PropManager_wQ2_03.cpp), which is why this body had
//                       to exist this wave and not later: "LNK2019 resolves before /OPT:REF
//                       discards" (build_game_exe.bat:544).
//   Create/Run       -- PropManager::BeginPropWorldContactGeneration @0x82628CB0 (the Create at
//                       0x82628D00 with `li r4, 0x64` == 100 max commands, the Run at
//                       0x82628E00) and VehicleManager::StartPartContactGeneration @0x8262C344 /
//                       @0x8262C3DC.
//
// ⚠️ THE TYPE-12 WORKER IS REAL AS OF THIS WAVE (ContactGeneratorJob::
// ExecutePrimitiveListWithTriangleListStream @0x82926650, ContactGeneratorJob.cpp), so a command
// posted here is drained. ⭐ AND WHAT IT DRAINS INTO IS REAL TOO -- the 849-instruction non-stream
// kernel ExecutePrimitiveListWithTriangleList @0x82925908 has been a full body since wave Q6
// round 3 (2026-08-19), in ContactGeneratorJob.cpp, with ::BuildGPInstance @0x829222A0 and
// ::CollideGPInstances @0x829253C8 landing beside it in ContactGeneratorJob_wQ6_01.cpp. Nothing
// on this leg is gated any more (this paragraph called it a LOUD NAMED GATE for a wave after it
// landed).
// =================================================================================================

// -------------------------------------------------------------------------------------------------
// BaseCollisionGenerator::AddPrimitiveListWithTriangleListToStream @0x82811D40 (35 asm insns)
//
// ⚠️ EXPORT-SET HOLE: there is no .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82811D40.json. All 35
// instructions (raw words included) were read out of a PRIVATE copy of the .i64 with headless
// IDA 9.3 this wave -- scratchpad/waveQ6/ida_pstream/{dump_ps.py,out.json}. Hex-Rays pseudocode
// was not available and was not used.
//
// ---- REGISTER MAP, read off the prologue --------------------------------------------------------
//   r3 this  r4 lpSPrimList  r5 lpTriangleList  r6 lu16MaxNumCollisions  r7 lbUseOptimisedBoxTests
//   r8 luUserTagA  r9 lu16UserTagB  r10 lpPrimitiveTriangleStream
// SEVEN GPR arguments and NO float -- this family carries a bool where the sphere families carry
// lfPadding, so AGENTS.md gotcha 3 (a float arg skipping a GPR slot) does NOT bite here. The
// spills at 0x82811D4C..0x82811D68 are r31=r4, r27=r5, r26=r7, r29=r10, r30=r3, and the
// re-shuffle r4=r6 / r5=r8 / r6=r9 is the argument set for the call on the next line.
//
// ---- DECODE, address by address -----------------------------------------------------------------
//   0x82811D6C  bl PrepareNewPrimitiveTestResultsList(maxCollisions, tagA, tagB) -> r28 = index.
//   0x82811D7C  stb r26, sp+0x68   -> command +0x18   the bool (stored FIRST on the console)
//   0x82811D70/84, 88/8C, 90/94    lwz/stw pairList words 0,4,8 -> command +0x00/+0x04/+0x08
//   0x82811D98/9C  ld/std triangle list (one 8-byte console pair) -> command +0x0C
//   0x82811DA0..0x82811DB0  `clrlwi r11,r28,16 ; addi r11,r11,0x4820 ; slwi r11,r11,2 ;
//               lwzx r11,r11,r30` == this + 0x12080 + index*4 == mapCollisionResultLists[index],
//               stored to command +0x14.
//               ⚠️ 0x12080 IS A CONSOLE OFFSET (gotcha 1) -- the host array holds 8-byte pointers
//               behind a wider IOBuffer base. Reached BY NAME below.
//   0x82811D80/0x82811DB4  `addi r3, r29, 0x80 ; bl DataStreamCommandPoster::AddCommand` --
//               producer+0x80 is SimpleDataStreamProducer::mCommandPoster.
//   0x82811DC0  stw r3, 0x104(r29) -> the producer's miNumAddedCommands.
//               Those two together are exactly the committed
//               SimpleDataStreamProducer::AddCommand (CgsSimpleDataStreamProducer.cpp:121,
//               `miNumAddedCommands = mCommandPoster.AddCommand(lpCommand);`), so the pair is
//               written as that one call -- the same reduction the two committed Add* siblings
//               in CgsCollisionGenerator_CollideStreams.cpp already make.
//   0x82811DBC  `mr r3, r28` -- the result-list index, NOT truncated on the return path, which is
//               why this tree types the two Add* siblings (and this one) s32 while the DWARF
//               spells them uint16_t. Every measured call site drops the value.
// -------------------------------------------------------------------------------------------------
s32 BaseCollisionGenerator::AddPrimitiveListWithTriangleListToStream(
    const PrimitivePairList*             lpSPrimList,
    const TriangleList*                  lpTriangleList,
    u16                                  lu16MaxNumCollisions,
    bool                                 lbUseOptimisedBoxTests,
    u32                                  luUserTagA,
    u16                                  lu16UserTagB,
    CgsMemory::SimpleDataStreamProducer* lpPrimitiveTriangleStream)
{
    const s32 liResultListIndex =
        PrepareNewPrimitiveTestResultsList(lu16MaxNumCollisions, luUserTagA, lu16UserTagB); // 0x82811D6C

    PrimitiveListWithTriangleListStreamJobDesc::StreamCommand lCommand;
    lCommand.mbUseOptimisedBoxTests = lbUseOptimisedBoxTests;                  // 0x82811D7C (+0x18)
    lCommand.mPairList              = *lpSPrimList;                            // +0x00..+0x08
    lCommand.mTriangleList          = *lpTriangleList;                         // +0x0C
    lCommand.mpResultsList =
        mapCollisionResultLists[static_cast<u16>(liResultListIndex)];          // +0x14, by NAME

    lpPrimitiveTriangleStream->AddCommand(&lCommand);                          // 0x82811DB4 + 0x82811DC0

    return liResultListIndex;                                                  // 0x82811DBC
}

// -------------------------------------------------------------------------------------------------
// BaseCollisionGenerator::CreateCollidePrimitiveListWithTriangleListStream @0x82811DD0 (98 insns)
//
// DWARF h:271; CgsCollisionGenerator.cpp:2028 names the parameter liMaxTests and the locals
// (liOrigAlignment / lpStreamProducer / liCommandBufferSize / liResultBufferSize /
// lpCommandBuffer). Grounding: the RAW `assembly` array of
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82811DD0.json (dumped to
// scratchpad/waveQ6/asm_82811DD0.txt; 98 lines, matching (0x82811F58-0x82811DD0)/4).
//
// The 98 instructions are the SAME BODY as the three committed Create* in
// CgsCollisionGenerator_CollideStreams.cpp -- the only deltas are the two assert line numbers,
// :2037 (`li r5, 0x7F5`) and :2049 (`li r5, 0x801`). Decode:
//   0x82811DE0/E8  `addis r25,r3,1 ; addi r25,r25,0x23A0` -> &mCollisionResultsAllocator
//   0x82811DF4     `lwzx r20, r3, 0x123B0`  -> the allocator's live alignment (GetAlignment)
//   0x82811DFC     SetAlignment(0x80)
//   0x82811E08     Malloc(0x180)            -> the producer (0x180 is the CONSOLE sizeof)
//   0x82811EA4/AC  GetRequiredBufferSizes(liMaxTests, 0x20, 0, 0, &cmdSize, &resSize)
//   0x82811EB8     Malloc(cmdSize)          -> the command buffer
//   0x82811F30/3C  Construct(liMaxTests, 0x20, cmdBuffer, 0, 0, 0)
//   0x82811F48     SetAlignment(saved) ; 0x82811F4C `mr r3, r22` -> the producer
// NO RESULT LANE (`li r5,0 / li r6,0` into GetRequiredBufferSizes and three zeros into
// Construct): stream results travel through the CollisionResultList each posted command carries.
//
// ⚠️ COMMAND SIZE: the console passes 32; this host passes sizeof(StreamCommand) == 48. That is
// the standing widen-the-runtime-carved-record rule, gated by the static_asserts in
// CgsPrimitiveListWithTriangleListStreamJobDesc.h -- see its banner.
//
// ⚠️ WHY THIS IS OPEN-CODED RATHER THAN CALLING A SHARED HELPER. The three committed Create*
// share an ANONYMOUS-NAMESPACE helper `CreateCollideStreamProducer` in
// CgsCollisionGenerator_CollideStreams.cpp:105. That helper has internal linkage and lives in a
// TU outside this owner's scope, so it cannot be called from here and duplicating a second
// static of the same name in this TU would be a worse fork than open-coding the console's own
// body. FOLLOW-UP: whoever owns _CollideStreams.cpp can promote that helper (file-scope in a
// shared header, or a private member) and collapse all four factories onto it.
// -------------------------------------------------------------------------------------------------
CgsMemory::SimpleDataStreamProducer*
BaseCollisionGenerator::CreateCollidePrimitiveListWithTriangleListStream(s32 liMaxTests)
{
    const size_t lnSavedAlignment = mCollisionResultsAllocator.GetAlignment();  // 0x82811DF4
    mCollisionResultsAllocator.SetAlignment(128);                               // 0x82811DF0 li r4, 0x80

    CgsMemory::SimpleDataStreamProducer* lpProducer =
        static_cast<CgsMemory::SimpleDataStreamProducer*>(
            mCollisionResultsAllocator.Malloc(sizeof(CgsMemory::SimpleDataStreamProducer))); // li r4, 0x180 (console sizeof)
    CGS_ASSERT(lpProducer != nullptr, "Failed to allocate stream producer\n");  // :2037

    // The producer's stride, and the poster's. NEVER the console's literal 32 -- see the banner.
    const s32 liCommandSize = static_cast<s32>(
        sizeof(PrimitiveListWithTriangleListStreamJobDesc::StreamCommand));

    u32 luCommandBufferSize = 0;
    u32 luResultBufferSize  = 0;
    CgsMemory::SimpleDataStreamProducer::GetRequiredBufferSizes(
        liMaxTests, liCommandSize, 0, 0,                                        // 0x82811E9C/EA0 li r5,0 / li r6,0
        &luCommandBufferSize, &luResultBufferSize);

    void* lpCommandBuffer = mCollisionResultsAllocator.Malloc(luCommandBufferSize);
    CGS_ASSERT(lpCommandBuffer != nullptr, "Failed to allocate stream buffers\n"); // :2049

    lpProducer->Construct(liMaxTests, liCommandSize, lpCommandBuffer, 0, 0, 0);  // 0x82811F20..F3C

    mCollisionResultsAllocator.SetAlignment(lnSavedAlignment);                  // 0x82811F48
    return lpProducer;                                                          // 0x82811F4C mr r3, r22
}

// -------------------------------------------------------------------------------------------------
// BaseCollisionGenerator::RunCollidePrimitiveListWithTriangleListStream @0x82811F58 (80 insns)
//
// DWARF h:275; CgsCollisionGenerator.cpp:2076 names the locals liNumJobs / lpNULLJob / liJobIndex
// / lu16BatchIndex and the calls GetNumCommands / rw::core::stdc::Min / AllocateJob /
// PerfMonCpu::Start+StopMonitor -- i.e. exactly this dispatcher shape. Grounding: the RAW
// `assembly` of 0x82811F58.json (scratchpad/waveQ6/asm_82811F58.txt, 80 lines).
//
// Divergences from its three committed twins, read off the asm and reproduced:
//   * NO DebugRenderStreamReader parameter (DWARF :275 has one argument, and the asm loads only
//     r3=this, r4=producer); the descriptor's mpDebugStream is stored NULL (0x82811FF4).
//   * descriptor type 12 (`li r23, 0xC` @0x82811FC0 -> `stb r23, 0x4CF` @0x82811FEC).
//   * the perfmon brackets EACH batch's wiring (Start @0x82812000 after the descriptor stores,
//     Stop @0x82812068 after DependsOn) -- the swept / sphere-sphere placement, not the
//     sphere-triangle one.
//   * mabUsedBatches is NOT written here (no store to +0x123C0 anywhere in the 80 instructions);
//     the landed CreateNewBatch marks the slot itself.
//   * batch clamp is 3 (`li r29, 3` @0x82811F90), like all three twins. The console runs the
//     loop as a countdown (`addi r29,r29,-1` @0x8281206C); re-rolled forward here.
//   * the empty-stream early-out returns NULL (0x82811F7C `li r3, 0`) BEFORE AllocateJob.
//
// ⚠️⚠️ FLAG PC-platform leaf: THE DISPATCH, AND ONLY THE DISPATCH. `unk_830EA650` (the global
// EA::Jobs::JobScheduler) does not exist on this build (CgsHardwareInitPC.cpp:40), so the
// JobScheduler::AddTree call at 0x82812084 -- and only that call -- is replaced by running each
// batch's entry point inline, exactly as the five already-committed dispatchers in
// CgsCollisionGenerator_CollideStreams.cpp / _LineStream.cpp and CollidePrimitiveListAgainst-
// TriangleList above already do.
// -------------------------------------------------------------------------------------------------
EA::Jobs::Job*
BaseCollisionGenerator::RunCollidePrimitiveListWithTriangleListStream(
    CgsMemory::SimpleDataStreamProducer* lpStream)
{
    const s32 liNumCommands = lpStream->GetNumCommands();   // 0x82811F70 lwz r11, 0x104(producer)
    if (liNumCommands == 0)
    {
        return 0;                                           // 0x82811F7C li r3, 0
    }

    const s32 KI_MAX_BATCHES = 3;                           // 0x82811F90 li r29, 3
    s32 liNumBatches = liNumCommands;
    if (liNumBatches > KI_MAX_BATCHES)                      // 0x82811F8C cmpwi 3 / bgt
    {
        liNumBatches = KI_MAX_BATCHES;
    }

    EA::Jobs::Job* lpParentJob = AllocateJob();              // 0x82811FA0

    for (s32 liBatch = 0; liBatch < liNumBatches; ++liBatch)
    {
        CollisionBatch& lrBatch = maCollisionBatches[CreateNewBatch()];        // 0x82811FD0

        PrimitiveListWithTriangleListStreamJobDesc* lpDesc =
            reinterpret_cast<PrimitiveListWithTriangleListStreamJobDesc*>(
                lrBatch.GetJobDescription().GetBuffer());                      // batch +0x350
        lpDesc->Prepare(lpStream);                 // the five inlined stores, 0x82811FE8..0x82811FF8

        CgsDev::PerfMonCpu::StartMonitor(_miStartJobsPerfMon);                 // 0x82812000

        EA::Jobs::Job* lpJob = lrBatch.GetJob();                               // batch +0x00
        lpJob->Clear();                                                        // 0x8281200C
        lpJob->SetName("CollisionBatch");                                      // 0x8281201C
        lpJob->SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                       reinterpret_cast<const void*>(&ContactGeneratorEntry), 0);  // 0x82812034
        lpJob->SetData(lpDesc,
                       static_cast<int>(CollisionJobDescriptionStorage::KU_CONSOLE_BYTES)); // li r5, 0x100
        lpJob->SetCodeRecycle(EA::Jobs::EntryPoint::CODE_RECYCLE_ON);          // 0x82812050 (ICF-folded blr)
        lpParentJob->DependsOn(*lpJob, EA::Jobs::Event::EVENT_WHEN_JOB_END);   // 0x82812060, li r5, 1

        CgsDev::PerfMonCpu::StopMonitor(_miStartJobsPerfMon);                  // 0x82812068

        // ---- FLAG PC-platform leaf: run the job body here (see the banner) ----
        // X360: JobScheduler::AddTree(&unk_830EA650, lpParentJob) at 0x82812084, after the loop.
        ContactGeneratorEntry(EA::Jobs::Param(static_cast<void*>(lpDesc)),
                              EA::Jobs::Param(static_cast<void*>(lpDesc)),
                              EA::Jobs::Param(),
                              EA::Jobs::Param());
    }

    return lpParentJob;                                     // 0x82812088 mr r3, r24
}

}
}
