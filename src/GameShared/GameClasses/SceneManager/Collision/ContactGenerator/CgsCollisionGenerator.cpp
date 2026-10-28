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
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSweptSphereListWithTriangleListJobDesc.h" // the type-13 descriptor (CollideSweptSphereListWithTriangleList)
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"        // SimpleDataStreamProducer (CreateFillTriangleCacheStream)
#include "SDKs/EATech/eajobs/job.h"                                                       // EA::Jobs::Job (AllocateJob)
#include "SDKs/EATech/eajobs/job_types.h"                                                 // EA::Jobs::Param (the inline dispatch)

// includes folded in from the CgsCollisionGenerator_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"            // AxisAlignedBox
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"                      // CgsGeometric::Line
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h" // PolygonSoupListSpatialMap
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h" // PolygonSoupLeafNode
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"       // IntersectLinePolygonSoup{Nearest,}SingleSided
#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"              // TestLineStartEndAxisAlignedBox (the long arm's per-leaf slab test)
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                    // CgsGeometric::Sphere (TestSphereAgainstPolySoupList)
#include "rw/math/vpu/vector4_operation.h"                                            // vpu::operator+ / operator- (the sphere's box)
#include <cstring>   // std::memcpy (the 14-qword result copy)
#include <cmath>     // std::signbit (the line box's vmaxfp128 / vminfp128 zero ordering)

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
// The synchronous line drivers (CollideLineAgainstPolySoupList, CollideLineAgainstPolySoupList-
// Nearest, TestLineAgainstPolySoupListDoubleSided) are in the folded scene-query section at the
// end of this file.
//
// The streamed Run*/Add*/Create* family is in CgsCollisionGenerator_CollideStreams.cpp and
// CgsCollisionGenerator_LineStream.cpp; the synchronous primitive legs are below.

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
// static latch).
void BaseCollisionGenerator::Construct()
{
    if (!_mbInitializedPerfMons)
    {
        _miStartJobsPerfMon = CgsDev::PerfMonCpu::AddMonitor("StartJobs", CgsDev::E_PMP_12, false, 10.0f, true);
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

    EA::Jobs::Job* lpJob = 0;
    void* lpvJobMemory = mCollisionResultsAllocator.Malloc(sizeof(EA::Jobs::Job));
    if (lpvJobMemory)
        lpJob = new (lpvJobMemory) EA::Jobs::Job(0);

    mCollisionResultsAllocator.SetAlignment(lnSavedAlignment);
    return lpJob;
}

// CreateFillTriangleCacheStream (the declaration's name; the console symbol is truncated to
// "Crea"). Carve a SimpleDataStreamProducer and its command + result buffers out of
// the result allocator at 128-byte alignment, size the two buffers with the producer's own requirement
// helper, construct the producer over them, then restore the allocator alignment. The stream
// carries fixed-geometry commands (32-byte command records, 16-byte result records), so the
// command/result counts both come from liMaxCommands (r4). The two allocation-failure asserts
// are the X360 streamed-message form (BasePriorityQueue::Clear + operator<< + FireAssert); their
// messages are string literals here, matching this file's CGS_ASSERT convention. The
// GetRequiredBufferSizes result-size out-slot is a single u32 on X360 (the frame reserved three
// words but only the first is read).
CgsMemory::SimpleDataStreamProducer* BaseCollisionGenerator::CreateFillTriangleCacheStream(s32 liMaxCommands)
{
    const size_t lnSavedAlignment = mCollisionResultsAllocator.GetAlignment();
    mCollisionResultsAllocator.SetAlignment(128);

    CgsMemory::SimpleDataStreamProducer* lpProducer =
        static_cast<CgsMemory::SimpleDataStreamProducer*>(
            mCollisionResultsAllocator.Malloc(sizeof(CgsMemory::SimpleDataStreamProducer)));
    CGS_ASSERT(lpProducer != 0, "Failed to allocate stream producer\n");

    u32 luCommandBufferSize = 0;
    u32 luResultBufferSize  = 0;
    CgsMemory::SimpleDataStreamProducer::GetRequiredBufferSizes(
        liMaxCommands, 32, liMaxCommands, 16, &luCommandBufferSize, &luResultBufferSize);

    void* lpCommandBuffer = mCollisionResultsAllocator.Malloc(luCommandBufferSize);
    void* lpResultBuffer  = mCollisionResultsAllocator.Malloc(luResultBufferSize);
    CGS_ASSERT(lpCommandBuffer != 0 && lpResultBuffer != 0,
               "Failed to allocate stream buffers\n");

    lpProducer->Construct(liMaxCommands, 32, lpCommandBuffer, liMaxCommands, 16, lpResultBuffer);

    mCollisionResultsAllocator.SetAlignment(lnSavedAlignment);
    return lpProducer;
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
    CGS_ASSERT(lpProducer != 0, "Failed to allocate stream producer\n");  // :2037

    // The producer's stride, and the poster's. NEVER the console's literal 32 -- see the banner.
    const s32 liCommandSize = static_cast<s32>(
        sizeof(PrimitiveListWithTriangleListStreamJobDesc::StreamCommand));

    u32 luCommandBufferSize = 0;
    u32 luResultBufferSize  = 0;
    CgsMemory::SimpleDataStreamProducer::GetRequiredBufferSizes(
        liMaxTests, liCommandSize, 0, 0,                                        // 0x82811E9C/EA0 li r5,0 / li r6,0
        &luCommandBufferSize, &luResultBufferSize);

    void* lpCommandBuffer = mCollisionResultsAllocator.Malloc(luCommandBufferSize);
    CGS_ASSERT(lpCommandBuffer != 0, "Failed to allocate stream buffers\n"); // :2049

    lpProducer->Construct(liMaxTests, liCommandSize, lpCommandBuffer, 0, 0, 0);  // 0x82811F20..F3C

    mCollisionResultsAllocator.SetAlignment(lnSavedAlignment);                  // 0x82811F48
    return lpProducer;                                                          // 0x82811F4C mr r3, r22
}

}
}

// ============================================================================
// FOLDED FROM CgsCollisionGenerator_wSQ1.cpp (wave SQ1) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =================================================================================================
// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator_wSQ1.cpp
//
// BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest @0x828131C0 (494 insns) --
// THE RAY-vs-STATIC-WORLD KERNEL DRIVER of the scene-query pipeline. Scene-query wave 1 landed it
// as a loud trap (2026-09-02, morning); wave 1b (same day) bodies the short-line arm on top of
// PolygonSoupListSpatialMap::RunQuery @0x82843A80 and CgsGeometric::
// IntersectLinePolygonSoupNearestSingleSided @0x8283BC98, both reconstructed in the same commit.
//
// Reachability: SceneManagerModule::ProcessTriangleCollisionLineTestNearests (direct arm) and
// ProcessLineTestNearest (the world-race arm) -- every race car's above-ground ray, every frame.
//
// ---- DECODE, address by address ----------------------------------------------------------------
//   0x828131E4..0x82813244  CGS_ASSERT(Line::IsValid(line), "Invalid line") -- :1023
//   0x8281324C  bl PrepareNewPrimitiveTestResultsList(1, tagA, tagB) -> r3 = the index (returned)
//   0x82813254  lvx128 v127 <- line+0x00 (start, all four lanes); v123 <- line+0x10 (end)
//   0x82813260  vsubfp128 v126 = end - start
//   0x82813268  `vspltisw v0, 2 ; vcfsx v0, v0, 0` = {2,2,2,2}
//   0x82813278  r16 = 4*(idx + 0x4820) -> mapCollisionResultLists[idx] (0x12080 == the console
//               offsetof; reached BY NAME here) ; r15 = list->mpResults ; r14 = r15 + 0x50
//   0x82813290  stvx128 v0 -> [r14]  : the single CollisionResult's +0x50 lane seeded to 2.0
//   0x82813298  vmsum3fp128 v12 = |end-start|^2 (xyz) ; vcmpgtfp. 400.0 > v12
//   -- short arm (under 20 m) --
//   0x828132B4  vmaxfp/vminfp of start,end -> AxisAlignedBox {min,max} on the stack
//   0x828132C8  bl PolygonSoupListSpatialMap::RunQuery(map, &box) -> r3 = leaf count
//   loop        idx = map->mpOutputQueryBuffer[i] (+0x58) ; leaf = map->mpLeafNodes (+0x48) +
//               48*idx ; six vcmpgefp lanes (leaf.max >= box.min, box.max >= leaf.min), xyz
//               reduced by vpermwi 0x4B/0x87 ; skip on no overlap ;
//               bl IntersectLinePolygonSoupNearestSingleSided(leaf.mpPolygonSoup (+0x20), tmp)
//               with v1 = start, v2 = end ; `vcmpeqfp128. v0, v1, zero` -> skip on no hit ;
//               `vcmpgtfp. best+0x50, tmp+0x50` -> copy 14 qwords tmp -> best when strictly nearer
//   -- long arm (20 m and over) -- (crash parity FX-GEOMETRIC, 2026-09-24; a named trap before)
//   0x82813440  n = sub_82843E98(map, &line) == PolygonSoupListSpatialMap::RunQuery(const Line&),
//               the segment's leaf gather ; n <= 0 -> the count store (0x82813918)
//   per leaf (0x8281348C, count-down r17, byte cursor r19 over mpOutputQueryBuffer (+0x58)):
//     leaf (r26) = mpLeafNodes (+0x48) + 48 * mpOutputQueryBuffer[i]
//     0x8281348C..0x82813870  CgsGeometric::TestLineStartEndAxisAlignedBox(start, end, leaf box)
//                 INLINED WHOLE, as in the all-hits twin: vrefp128 + three Newton-Raphson steps
//                 (0x828134D8..0x8281350C), the "Line reciprocal X / Y / Z is 0\n" tripwires
//                 (0x1B9 / 0x1BA / 0x1BB == CgsLineTests.cpp:441..443, strings 0x820DB18C /
//                 0x820DB174 / 0x820DB15C), the endpoint-inside and six face terms
//     0x82813874  `vcmpeqfp128. mask, 0` -> no hit -> next leaf
//     0x8281389C  IntersectLinePolygonSoupNearestSingleSided(*leaf.mpPolygonSoup (+0x20), &tmp
//                 (sp+0x120), v1 = start, v2 = end) ; `vcmpeqfp128. v1, 0` -> no hit -> next leaf
//     0x828138C4  `vcmpgtfp. best+0x50, tmp+0x50` -> copy 14 qwords tmp -> best when strictly
//                 nearer -- the short arm's leaf body exactly
//   0x8281390C  reload this / the list offset / idx from the stack and join the count store
//   0x82813930  `vcfsx 1.0 ; vcmpgefp. 1.0 >= best+0x50` ; `sth -> list+0x0C` (mu16NumResults)
//               = 1 on a hit, 0 otherwise. This is the ONLY write of the count on this leg.
//   return      the result-list index.
//
// ⚠️ CONSOLE QUIRK, REPRODUCED, NOT FIXED: PrepareNewPrimitiveTestResultsList(1, ...) allocates
// KI_PRIMITIVE_TEST_RESULT_MEMORY_SIZE * 1 == 80 bytes for the result, but the record this leg
// keeps in it is 112 bytes (+0x50 t lane, +0x60 tag lane -- exactly the bytes past the block). The
// console writes them into the linear allocator's not-yet-claimed tail; both consumers read the
// record BEFORE the next Prepare* claims anything, so it works by ordering. Same here (the
// allocator is CgsMemory::LinearMalloc, a bump allocator with slack). Widening the allocation
// would be an invented arm and would move every later block.
// =================================================================================================

namespace CgsSceneManager
{
namespace CgsCollision
{
    namespace
    {
        // 0x82813298: `vmsum3fp128` vs the 400.0 splat (flt at 0x828131C0's literal pool; 20 m^2).
        const f32 KF_SHORT_LINE_LENGTH_SQ = 400.0f;

        // 0x82813268: the "no hit yet" line parameter seed.
        const f32 KF_LINE_PARAM_NO_HIT = 2.0f;

        // vmaxfp128 / vminfp128 per lane, for the two line boxes (CollideLineAgainstPolySoupListNearest
        // 0x82813318 / 0x82813320, CollideLineAgainstPolySoupList 0x82812C20 / 0x82812C2C; both `vA = v127 = the
        // start, vB = v123 = the end`). The VMX result, not a C select's: a NaN operand gives a NaN (vA's when both
        // are), and +0 orders above -0. (Until 2026-09-25 the boxes were built with `a < b ? a : b` selects, which
        // answer the end for a NaN start lane.)
        inline f32 VmxMaxFp(f32 lfA, f32 lfB)
        {
            if (lfA != lfA) return lfA;
            if (lfB != lfB) return lfB;
            if (lfA == lfB) return std::signbit(lfA) ? lfB : lfA;   // +0 is the larger zero
            return (lfA > lfB) ? lfA : lfB;
        }
        inline f32 VmxMinFp(f32 lfA, f32 lfB)
        {
            if (lfA != lfA) return lfA;
            if (lfB != lfB) return lfB;
            if (lfA == lfB) return std::signbit(lfA) ? lfA : lfB;   // -0 is the smaller zero
            return (lfA < lfB) ? lfA : lfB;
        }

        // The six-lane box overlap the loop does inline (leaf.max >= box.min && box.max >= leaf.min,
        // xyz only -- `vpermwi128 0x4B / 0x87 ; vspltw 0` drop the w lane).
        inline bool LeafOverlapsBoxXYZ(const CgsGeometric::AxisAlignedBox& lrLeaf,
                                       const CgsGeometric::AxisAlignedBox& lrBox)
        {
            if (!(lrLeaf.mMax.x >= lrBox.mMin.x)) return false;
            if (!(lrLeaf.mMax.y >= lrBox.mMin.y)) return false;
            if (!(lrLeaf.mMax.z >= lrBox.mMin.z)) return false;
            if (!(lrBox.mMax.x >= lrLeaf.mMin.x)) return false;
            if (!(lrBox.mMax.y >= lrLeaf.mMin.y)) return false;
            if (!(lrBox.mMax.z >= lrLeaf.mMin.z)) return false;
            return true;
        }

        // [DIAG] NOT IN THE X360 BINARY -- CollideLineAgainstPolySoupList's result-block overrun tripwire
        // (crash parity FX-TAILS-A item 7, REVIEW-E risk 2). Default on, one line per process, changes nothing.
        // PrepareNewPrimitiveTestResultsList @0x82810798 sizes a list's block for 80-byte PrimitiveTestResult
        // records (`slwi ; add ; slwi` 0x828108A0..0x828108A8: Malloc((5 * max) << 4) @0x828108AC), but the
        // all-hits line driver fills it with 112-byte soup records and neither of its arms tests
        // `found < max` before calling the kernel (0x82812CE4 / 0x82813178 pass `max - found` as it is,
        // and the kernel writes a record before it tests its limit). So from the
        // 23rd hit at max 32 -- or any hit once the list is full -- the console writes past the block into
        // the linear allocator's unclaimed tail. That behaviour is KEPT (no clamp, no guard); this only makes
        // it visible on PC, where a dense line near the end of the arena would corrupt the next block silently.
        void NoteLineSoupListOverrun(s32 liNumFound, u16 lu16MaxNumResults)
        {
            const u32 lu32BytesWritten = static_cast<u32>(liNumFound) *
                                         static_cast<u32>(sizeof(CgsGeometric::PolySoupLineNearestResult));
            const u32 lu32BlockBytes   = static_cast<u32>(lu16MaxNumResults) *
                                         static_cast<u32>(sizeof(PrimitiveTestResult));
            static bool sbReported = false;
            if (lu32BytesWritten <= lu32BlockBytes || sbReported || CgsDev::Log::gpDebugPrint == 0)
            {
                return;
            }
            sbReported = true;
            *CgsDev::Log::gpDebugPrint
                << "[scene] CollideLineAgainstPolySoupList overran its result block: " << liNumFound
                << " records x 112 bytes = " << lu32BytesWritten << " bytes into a block of "
                << static_cast<s32>(lu16MaxNumResults) << " x 80 = " << lu32BlockBytes
                << " bytes (console behaviour kept; NOT IN THE X360 BINARY tripwire, reported once)\n";
        }
    }

    // @ 0x828131C0
    u16 BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest(
        const CgsGeometric::Line&                 lrLine,
        CgsGeometric::PolygonSoupListSpatialMap*  lpPolySoupListSpacialMap,
        u32                                       lu32UserTagA,
        u16                                       lu16UserTagB)
    {
        CGS_ASSERT(lrLine.IsValid(), "Invalid line\n");   // :1023

        const s32 liResultListIndex = PrepareNewPrimitiveTestResultsList(1, lu32UserTagA, lu16UserTagB);

        // 0x82813278..0x82813290: the list's single record, its +0x50 lane seeded to 2.0. The rest
        // of the record is whatever the allocator held until the first strictly-nearer copy.
        CollisionResultList* lpList = mapCollisionResultLists[static_cast<u16>(liResultListIndex)];
        CgsGeometric::PolySoupLineNearestResult* lpBest =
            reinterpret_cast<CgsGeometric::PolySoupLineNearestResult*>(lpList->mpResults);
        lpBest->mLineParam.x = KF_LINE_PARAM_NO_HIT;
        lpBest->mLineParam.y = KF_LINE_PARAM_NO_HIT;
        lpBest->mLineParam.z = KF_LINE_PARAM_NO_HIT;
        lpBest->mLineParam.w = KF_LINE_PARAM_NO_HIT;

        // v127 / v123: the two 16-byte lanes of the line, w included (the soup test's +0x40
        // position lane multiplies all four).
        const Vector3& lrStart = reinterpret_cast<const Vector3&>(lrLine.mStart);
        const Vector3& lrEnd   = reinterpret_cast<const Vector3&>(lrLine.mEnd);

        const f32 lfDx = lrEnd.x - lrStart.x;
        const f32 lfDy = lrEnd.y - lrStart.y;
        const f32 lfDz = lrEnd.z - lrStart.z;
        const f32 lfLengthSq = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;

        if (KF_SHORT_LINE_LENGTH_SQ > lfLengthSq)
        {
            // 0x82813318 / 0x82813320: the line's box, all four lanes -- vmaxfp128 / vminfp128 (v127 start, v123 end).
            CgsGeometric::AxisAlignedBox lBox;
            lBox.mMin.x = VmxMinFp(lrStart.x, lrEnd.x);
            lBox.mMin.y = VmxMinFp(lrStart.y, lrEnd.y);
            lBox.mMin.z = VmxMinFp(lrStart.z, lrEnd.z);
            lBox.mMin.w = VmxMinFp(lrStart.w, lrEnd.w);
            lBox.mMax.x = VmxMaxFp(lrStart.x, lrEnd.x);
            lBox.mMax.y = VmxMaxFp(lrStart.y, lrEnd.y);
            lBox.mMax.z = VmxMaxFp(lrStart.z, lrEnd.z);
            lBox.mMax.w = VmxMaxFp(lrStart.w, lrEnd.w);

            const s32 liNumLeaves = lpPolySoupListSpacialMap->RunQuery(lBox);   // 0x828132C8

            if (liNumLeaves > 0)
            {
                const u16*                             lpau16Leaves = lpPolySoupListSpacialMap->GetOutputQueryBuffer();
                const CgsGeometric::PolygonSoupLeafNode* lpaLeafNodes = lpPolySoupListSpacialMap->GetLeafNodes();

                CgsGeometric::PolySoupLineNearestResult lTemp;   // sp+0x120, 112 bytes

                for (s32 liLeaf = 0; liLeaf < liNumLeaves; ++liLeaf)
                {
                    const CgsGeometric::PolygonSoupLeafNode& lrLeaf = lpaLeafNodes[lpau16Leaves[liLeaf]];

                    if (!LeafOverlapsBoxXYZ(lrLeaf.mBox, lBox))
                    {
                        continue;
                    }

                    if (!CgsGeometric::IntersectLinePolygonSoupNearestSingleSided(*lrLeaf.mpPolygonSoup, &lTemp,
                                                                                  lrStart, lrEnd))
                    {
                        continue;   // `vcmpeqfp128. v0, v1, zero` -- no hit in this soup
                    }

                    // 0x828133E8..0x828133F4: `vcmpgtfp. best+0x50, tmp+0x50` -- STRICTLY nearer
                    // replaces (all four lanes carry the same splatted value).
                    if (lpBest->mLineParam.x > lTemp.mLineParam.x)
                    {
                        std::memcpy(lpBest, &lTemp, sizeof(CgsGeometric::PolySoupLineNearestResult));   // 14 qwords
                    }
                }
            }
        }
        else
        {
            // 0x82813440: the SEGMENT leaf gather, then per leaf the inlined slab test and the short
            // arm's nearest test + strictly-nearer copy (see the decode above). Until 2026-09-24 this
            // arm was a named trap. Besides the race car's 10 m above-ground ray, the queue it serves is
            // fed by the director's camera probes -- BrnDirector::SceneQueryInterface::LineTestNearest
            // 0x82233048 (VisibilityTest, GroundConstraint, FrustrumCollisionResolver, the collision
            // policies, BehaviourRoadRunner, BehaviourRenderMetrics) -- whose lines the caller sizes.
            const s32 liNumLeaves = lpPolySoupListSpacialMap->RunQuery(lrLine);   // 0x82813444

            if (liNumLeaves > 0)
            {
                const Vector4 lLineStart = { lrStart.x, lrStart.y, lrStart.z, lrStart.w };   // v127
                const Vector4 lLineEnd   = { lrEnd.x,   lrEnd.y,   lrEnd.z,   lrEnd.w };     // v123

                const u16*                               lpau16Leaves = lpPolySoupListSpacialMap->GetOutputQueryBuffer();
                const CgsGeometric::PolygonSoupLeafNode* lpaLeafNodes = lpPolySoupListSpacialMap->GetLeafNodes();

                CgsGeometric::PolySoupLineNearestResult lTemp;   // sp+0x120, 112 bytes (the short arm's slot)

                for (s32 liLeaf = 0; liLeaf < liNumLeaves; ++liLeaf)
                {
                    const CgsGeometric::PolygonSoupLeafNode& lrLeaf = lpaLeafNodes[lpau16Leaves[liLeaf]];

                    if (!CgsGeometric::TestLineStartEndAxisAlignedBox(lLineStart, lLineEnd, lrLeaf.mBox))
                    {
                        continue;   // 0x82813884 `bne` on an all-zero mask
                    }

                    if (!CgsGeometric::IntersectLinePolygonSoupNearestSingleSided(*lrLeaf.mpPolygonSoup, &lTemp,
                                                                                  lrStart, lrEnd))
                    {
                        continue;   // 0x828138B0 -- no hit in this soup
                    }

                    // 0x828138B4..0x828138D4: `vcmpgtfp. best+0x50, tmp+0x50` -- STRICTLY nearer replaces.
                    if (lpBest->mLineParam.x > lTemp.mLineParam.x)
                    {
                        std::memcpy(lpBest, &lTemp, sizeof(CgsGeometric::PolySoupLineNearestResult));   // 14 qwords
                    }
                }
            }
        }

        // 0x82813930..0x82813960: mu16NumResults = (1.0 >= best t). The one write of the count.
        lpList->mu16NumResults = (1.0f >= lpBest->mLineParam.x) ? 1 : 0;

        return static_cast<u16>(liResultListIndex);
    }

    // =============================================================================================
    // TestLineAgainstPolySoupListDoubleSided -- the YES/NO twin of the function above. Its
    // instruction stream is the Nearest driver's with the per-leaf body swapped:
    //   * the same "Invalid line\n" assert (its own line), PrepareNewPrimitiveTestResultsList
    //     (1, tagA, tagB), and the same {2,2,2,2} seed stored to the single record's +0x50 lane;
    //   * the same 20 m split (400.0 > |end - start|^2). Short arm: the min/max box of the two end
    //     points, RunQuery(box), the six-lane xyz leaf overlap. Long arm: RunQuery(line), then
    //     TestLineStartEndAxisAlignedBox inlined whole (the Nearest long arm's sequence instruction
    //     for instruction, register allocation aside);
    //   * per surviving leaf: CgsGeometric::TestLinePolygonSoupDoubleSided(*leaf.mpPolygonSoup,
    //     start, end) and `vcmpeqfp128.` of its mask against zero -- the first soup that answers
    //     yes stores 1 to the list's count and returns;
    //   * no hit anywhere: the count is stored as 0. No result record is written on either path.
    // =============================================================================================
    u16 BaseCollisionGenerator::TestLineAgainstPolySoupListDoubleSided(
        const CgsGeometric::Line&                 lrLine,
        CgsGeometric::PolygonSoupListSpatialMap*  lpPolySoupListSpacialMap,
        u32                                       lu32UserTagA,
        u16                                       lu16UserTagB)
    {
        CGS_ASSERT(lrLine.IsValid(), "Invalid line\n");

        const s32 liResultListIndex = PrepareNewPrimitiveTestResultsList(1, lu32UserTagA, lu16UserTagB);

        CollisionResultList* lpList = mapCollisionResultLists[static_cast<u16>(liResultListIndex)];
        CgsGeometric::PolySoupLineNearestResult* lpRecord =
            reinterpret_cast<CgsGeometric::PolySoupLineNearestResult*>(lpList->mpResults);
        lpRecord->mLineParam.x = KF_LINE_PARAM_NO_HIT;
        lpRecord->mLineParam.y = KF_LINE_PARAM_NO_HIT;
        lpRecord->mLineParam.z = KF_LINE_PARAM_NO_HIT;
        lpRecord->mLineParam.w = KF_LINE_PARAM_NO_HIT;

        const Vector3& lrStart = reinterpret_cast<const Vector3&>(lrLine.mStart);
        const Vector3& lrEnd   = reinterpret_cast<const Vector3&>(lrLine.mEnd);

        const f32 lfDx = lrEnd.x - lrStart.x;
        const f32 lfDy = lrEnd.y - lrStart.y;
        const f32 lfDz = lrEnd.z - lrStart.z;
        const f32 lfLengthSq = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;

        if (KF_SHORT_LINE_LENGTH_SQ > lfLengthSq)
        {
            CgsGeometric::AxisAlignedBox lBox;
            lBox.mMin.x = VmxMinFp(lrStart.x, lrEnd.x);
            lBox.mMin.y = VmxMinFp(lrStart.y, lrEnd.y);
            lBox.mMin.z = VmxMinFp(lrStart.z, lrEnd.z);
            lBox.mMin.w = VmxMinFp(lrStart.w, lrEnd.w);
            lBox.mMax.x = VmxMaxFp(lrStart.x, lrEnd.x);
            lBox.mMax.y = VmxMaxFp(lrStart.y, lrEnd.y);
            lBox.mMax.z = VmxMaxFp(lrStart.z, lrEnd.z);
            lBox.mMax.w = VmxMaxFp(lrStart.w, lrEnd.w);

            const s32 liNumLeaves = lpPolySoupListSpacialMap->RunQuery(lBox);

            for (s32 liLeaf = 0; liLeaf < liNumLeaves; ++liLeaf)
            {
                const CgsGeometric::PolygonSoupLeafNode& lrLeaf =
                    lpPolySoupListSpacialMap->GetLeafNodes()[lpPolySoupListSpacialMap->GetOutputQueryBuffer()[liLeaf]];

                if (!LeafOverlapsBoxXYZ(lrLeaf.mBox, lBox))
                {
                    continue;
                }

                if (CgsGeometric::TestLinePolygonSoupDoubleSided(*lrLeaf.mpPolygonSoup, lrStart, lrEnd).GetBool())
                {
                    lpList->SetNumResults(1);
                    return static_cast<u16>(liResultListIndex);
                }
            }
        }
        else
        {
            const s32 liNumLeaves = lpPolySoupListSpacialMap->RunQuery(lrLine);

            const Vector4 lLineStart = { lrStart.x, lrStart.y, lrStart.z, lrStart.w };
            const Vector4 lLineEnd   = { lrEnd.x,   lrEnd.y,   lrEnd.z,   lrEnd.w };

            for (s32 liLeaf = 0; liLeaf < liNumLeaves; ++liLeaf)
            {
                const CgsGeometric::PolygonSoupLeafNode& lrLeaf =
                    lpPolySoupListSpacialMap->GetLeafNodes()[lpPolySoupListSpacialMap->GetOutputQueryBuffer()[liLeaf]];

                if (!CgsGeometric::TestLineStartEndAxisAlignedBox(lLineStart, lLineEnd, lrLeaf.mBox))
                {
                    continue;
                }

                if (CgsGeometric::TestLinePolygonSoupDoubleSided(*lrLeaf.mpPolygonSoup, lrStart, lrEnd).GetBool())
                {
                    lpList->SetNumResults(1);
                    return static_cast<u16>(liResultListIndex);
                }
            }
        }

        lpList->SetNumResults(0);
        return static_cast<u16>(liResultListIndex);
    }

    // =============================================================================================
    // CollideLineAgainstPolySoupList @ 0x82812AE0 (439 insns) -- the ALL-HITS twin of the function
    // above (crash parity FX-SCENEMGR, 2026-09-24; no body before). Callers:
    // SceneManagerModule::ProcessLineTestFine (0x828CDFD8) and ProcessTriangleCollisionLineTests
    // (0x828C7088), both with lu16MaxNumResults == 0x20 and tags 0 / 0.
    //
    //   0x82812B14..0x82812B98  CGS_ASSERT(Line::IsValid(line), "Invalid line\n") -- :880 (0x370)
    //   0x82812BAC  bl PrepareNewPrimitiveTestResultsList(lu16MaxNumResults, tagA, tagB) -> idx
    //   0x82812BB4  v127 = line+0x00 (start) ; v123 = line+0x10 (end) ; v126 = end - start
    //   0x82812BD4  lfs flt_8200889C (0x43C80000 == 400.0) splat ; vmsum3fp128 |d|^2 ;
    //               vcmpgtfp. 400 > |d|^2 -- the same 20 m split as the Nearest twin
    //   0x82812C44  r28 = mapCollisionResultLists[idx]->mpResults (the 112-byte record array)
    //   -- short arm (under 20 m) --
    //   0x82812C20..0x82812C40  box = {vminfp128(start,end), vmaxfp128(start,end)}, all four lanes
    //   0x82812C4C  n = PolygonSoupListSpatialMap::RunQuery(map, &box)
    //   per leaf i < n (0x82812C64..0x82812CFC): leaf = mpLeafNodes (+0x48) + 48 *
    //               mpOutputQueryBuffer[i] (+0x58) ; the six-lane xyz overlap (vcmpgefp x2, vand,
    //               vpermwi128 0x4B/0x87, vspltw 0, vcmpeqfp128. vs 0) -> skip ; else
    //   0x82812CE8  found += CgsGeometric::IntersectLinePolygonSoupSingleSided(*leaf.mpPolygonSoup
    //               (+0x20), v1 = start, v2 = end, results + 0x70 * found, lu16MaxNumResults - found)
    //               -- DecFIGS 0xB6C1AC `(const PolygonSoup&, Vector3, Vector3,
    //               IntersectLinePolygonSoupResult*, int)`, the ALL-HITS soup kernel @0x8283C598
    //   -- long arm (20 m and over) --
    //   0x82812D04  r26 = &mapCollisionResultLists[idx] (kept for the tail) ; r15 = list->mpResults ;
    //               r16 = (u16)lu16MaxNumResults
    //   0x82812D1C  n = sub_82843E98(map, &line) == PolygonSoupListSpatialMap::RunQuery(const Line&)
    //               (DecFIGS 0xB64574, the line-slab leaf gather) ; n <= 0 -> the count store
    //   per leaf (0x82812D64, count-down r14, byte cursor r17 over mpOutputQueryBuffer):
    //     leaf = mpLeafNodes (+0x48) + 48 * mpOutputQueryBuffer[i] (+0x58), then
    //     CgsGeometric::TestLineStartEndAxisAlignedBox(start, end, leaf box) INLINED WHOLE -- its
    //     vrefp128 + three Newton-Raphson steps (0x82812D64..0x82812DE4; the earlier note here said
    //     two -- there are three, see CgsLineTests.cpp), its three "Line reciprocal X / Y / Z is 0\n"
    //     tripwires (0x82812DF0 / 0x82812E88 / 0x82812F20, CgsLineTests.cpp:441..443) and the slab /
    //     endpoint test 0x82812F90..0x82813150 ; no hit -> next leaf
    //     0x8281317C  found += IntersectLinePolygonSoupSingleSided(*leaf.mpPolygonSoup (+0x20),
    //                 v1 = start, v2 = end, results + 0x70 * found, max - found) -- as the short arm
    //   0x8281319C  lwz list ; sth found -> list+0x0C (mu16NumResults) ; return idx
    //
    // ⭐ BOTH CALLEES LANDED 2026-09-24 (crash parity FX-GEOMETRIC): IntersectLinePolygonSoupSingleSided
    // @0x8283C598 (CgsPolygonSoupTests_LineNearest.cpp) and PolygonSoupListSpatialMap::RunQuery(const
    // Line&) @0x82843E98 (CgsPolygonSoupListSpatialMap_Query.cpp). The two traps that stood at their
    // call sites are replaced by the calls and the long arm's per-leaf slab test, as their DELETE-WHEN
    // asked. Neither arm guards `found < max` before calling the kernel (the console passes max - found
    // as it is); the kernel writes a record before it tests its own limit -- CONSOLE-QUIRK, reproduced.
    // =============================================================================================
    u16 BaseCollisionGenerator::CollideLineAgainstPolySoupList(
        const CgsGeometric::Line&                 lrLine,
        CgsGeometric::PolygonSoupListSpatialMap*  lpPolySoupListSpacialMap,
        u16                                       lu16MaxNumResults,
        u32                                       lu32UserTagA,
        u16                                       lu16UserTagB)
    {
        CGS_ASSERT(lrLine.IsValid(), "Invalid line\n");   // :880

        const s32 liResultListIndex =
            PrepareNewPrimitiveTestResultsList(lu16MaxNumResults, lu32UserTagA, lu16UserTagB);
        CollisionResultList* lpList = mapCollisionResultLists[static_cast<u16>(liResultListIndex)];

        const Vector3& lrStart = reinterpret_cast<const Vector3&>(lrLine.mStart);
        const Vector3& lrEnd   = reinterpret_cast<const Vector3&>(lrLine.mEnd);

        // r28 / r15: the list's 112-byte record array (the kernel writes IntersectLinePolygonSoupResult
        // records; the tree names that record PolySoupLineNearestResult -- see CgsPolygonSoupTests.h).
        CgsGeometric::PolySoupLineNearestResult* lpaResults =
            reinterpret_cast<CgsGeometric::PolySoupLineNearestResult*>(lpList->mpResults);

        const f32 lfDx = lrEnd.x - lrStart.x;
        const f32 lfDy = lrEnd.y - lrStart.y;
        const f32 lfDz = lrEnd.z - lrStart.z;
        const f32 lfLengthSq = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;

        s32 liNumFound = 0;   // r25

        if (KF_SHORT_LINE_LENGTH_SQ > lfLengthSq)
        {
            // 0x82812C20..0x82812C40: the line's box, all four lanes -- vmaxfp128 / vminfp128 (v127 start, v123 end).
            CgsGeometric::AxisAlignedBox lBox;
            lBox.mMin.x = VmxMinFp(lrStart.x, lrEnd.x);
            lBox.mMin.y = VmxMinFp(lrStart.y, lrEnd.y);
            lBox.mMin.z = VmxMinFp(lrStart.z, lrEnd.z);
            lBox.mMin.w = VmxMinFp(lrStart.w, lrEnd.w);
            lBox.mMax.x = VmxMaxFp(lrStart.x, lrEnd.x);
            lBox.mMax.y = VmxMaxFp(lrStart.y, lrEnd.y);
            lBox.mMax.z = VmxMaxFp(lrStart.z, lrEnd.z);
            lBox.mMax.w = VmxMaxFp(lrStart.w, lrEnd.w);

            const s32 liNumLeaves = lpPolySoupListSpacialMap->RunQuery(lBox);   // 0x82812C4C

            if (liNumLeaves > 0)
            {
                const u16*                               lpau16Leaves = lpPolySoupListSpacialMap->GetOutputQueryBuffer();
                const CgsGeometric::PolygonSoupLeafNode* lpaLeafNodes = lpPolySoupListSpacialMap->GetLeafNodes();

                for (s32 liLeaf = 0; liLeaf < liNumLeaves; ++liLeaf)
                {
                    const CgsGeometric::PolygonSoupLeafNode& lrLeaf = lpaLeafNodes[lpau16Leaves[liLeaf]];

                    if (!LeafOverlapsBoxXYZ(lrLeaf.mBox, lBox))
                    {
                        continue;
                    }

                    // 0x82812CE8: v1 = start, v2 = end, r4 = results + 0x70 * found, r5 = max - found.
                    liNumFound += CgsGeometric::IntersectLinePolygonSoupSingleSided(
                        *lrLeaf.mpPolygonSoup, lrStart, lrEnd, lpaResults + liNumFound,
                        static_cast<s32>(lu16MaxNumResults) - liNumFound);
                }
            }
        }
        else
        {
            // 0x82812D1C: the SEGMENT leaf gather, then per leaf the inlined slab test and the same
            // soup kernel (see the decode above).
            const s32 liNumLeaves = lpPolySoupListSpacialMap->RunQuery(lrLine);

            if (liNumLeaves > 0)
            {
                const Vector4 lLineStart = { lrStart.x, lrStart.y, lrStart.z, lrStart.w };   // v127
                const Vector4 lLineEnd   = { lrEnd.x,   lrEnd.y,   lrEnd.z,   lrEnd.w };     // v123

                const u16*                               lpau16Leaves = lpPolySoupListSpacialMap->GetOutputQueryBuffer();
                const CgsGeometric::PolygonSoupLeafNode* lpaLeafNodes = lpPolySoupListSpacialMap->GetLeafNodes();

                for (s32 liLeaf = 0; liLeaf < liNumLeaves; ++liLeaf)
                {
                    const CgsGeometric::PolygonSoupLeafNode& lrLeaf = lpaLeafNodes[lpau16Leaves[liLeaf]];

                    if (!CgsGeometric::TestLineStartEndAxisAlignedBox(lLineStart, lLineEnd, lrLeaf.mBox))
                    {
                        continue;   // 0x82813160 `bne` on an all-zero mask
                    }

                    // 0x8281317C: the same call as the short arm.
                    liNumFound += CgsGeometric::IntersectLinePolygonSoupSingleSided(
                        *lrLeaf.mpPolygonSoup, lrStart, lrEnd, lpaResults + liNumFound,
                        static_cast<s32>(lu16MaxNumResults) - liNumFound);
                }
            }
        }

        // 0x8281319C..0x828131A4: the one write of the count.
        lpList->mu16NumResults = static_cast<u16>(liNumFound);

        NoteLineSoupListOverrun(liNumFound, lu16MaxNumResults);   // [DIAG] NOT IN THE X360 BINARY (see its banner)

        return static_cast<u16>(liResultListIndex);
    }

    // =============================================================================================
    // BaseCollisionGenerator::TestSphereAgainstPolySoupList @ 0x82812950 (~100) -- ADDED 2026-09-25
    // (crash parity FX-FOLLOWUPS item 2). An export HOLE, read with tools/re/ppcdis.py (and the
    // classic-VX / vpermwi128 words decoded by hand). DWARF CgsCollisionGenerator.cpp:814 names the
    // locals lu16ResultListIndex (:817), lSphere (:822), lSphereBoundingBox (:823), liSoup (:834),
    // liNumSoups (:837) and lpSpacialNode (:843), and the calls: vpu::operator+, Sphere::GetRadius,
    // vpu::operator-, CollisionResultList::SetNumResults, GetPolygonSoupNodeForResult, three
    // VecAnd, VecSplat_Word<0>, MaskScalar::GetBool (x2), SetNumResults.
    //
    //   r3 = this, r4 = lpSphere, r5 = lpSpatialData, r6 = tagA, r7 = tagB
    //   0x8281297C  idx = PrepareNewPrimitiveTestResultsList(1, tagA, tagB)   (r4 = 1, r5 <- r6, r6 <- r7)
    //   0x82812980  lSphere = *lpSphere (two ld/std to sp+0x60; the kernel gets the COPY)
    //   0x828129AC  v13 = vspltw(sphere, 3) = GetRadius()
    //   0x828129B0  min = sphere - v13 (vsubfp), 0x828129B4 max = sphere + v13 (vaddfp), ALL FOUR lanes
    //   0x828129C4  n = lpSpatialData->RunQuery(lSphereBoundingBox)
    //   per i < n   leaf = GetLeafNodes()[GetOutputQueryBuffer()[i]] (+0x48 / +0x58, re-read per pass):
    //     0x82812A18..0x82812A40  leaf.max >= box.min & box.max >= leaf.min (two vcmpgefp, a vand,
    //                 vpermwi128 0x4B / 0x87, two vand, vspltw 0) -- x/y/z, LeafOverlapsBoxXYZ above;
    //                 all-zero -> next leaf
    //     0x82812A60  TestSpherePolygonSoup(leaf.mpPolygonSoup (+0x20), &lSphere); `vcmpeqfp128. v1,
    //                 zero` all-equal -> next leaf, else 0x82812AB0: the list's count = 1 and RETURN
    //   0x82812A88  none: the list's count = 0 (`sth r29 (0), 0xC`)
    //   return      idx (`mr r3, r24`)
    // No result RECORD is ever written: the answer is the count alone, which is all its one caller
    // (ProcessVolumeTestDeepest, `lhz 0xC` of GetResultList's copy) reads.
    // =============================================================================================
    u16 BaseCollisionGenerator::TestSphereAgainstPolySoupList(const CgsGeometric::Sphere*              lpSphere,
                                                              CgsGeometric::PolygonSoupListSpatialMap* lpSpatialData,
                                                              u32                                      luUserTagA,
                                                              u16                                      lu16UserTagB)
    {
        const u16 lu16ResultListIndex = static_cast<u16>(
            PrepareNewPrimitiveTestResultsList(1, luUserTagA, lu16UserTagB));   // :817
        CollisionResultList* lpList = mapCollisionResultLists[lu16ResultListIndex];

        const CgsGeometric::Sphere lSphere = *lpSphere;                          // :822

        CgsGeometric::AxisAlignedBox lSphereBoundingBox;                         // :823
        lSphereBoundingBox.mMin = lSphere.GetPosition() - lSphere.GetRadius();
        lSphereBoundingBox.mMax = lSphere.GetPosition() + lSphere.GetRadius();

        const s32 liNumSoups = lpSpatialData->RunQuery(lSphereBoundingBox);      // :837

        for (s32 liSoup = 0; liSoup < liNumSoups; ++liSoup)                      // :834
        {
            const CgsGeometric::PolygonSoupLeafNode* lpSpacialNode =             // :843
                &lpSpatialData->GetLeafNodes()[lpSpatialData->GetOutputQueryBuffer()[liSoup]];

            if (!LeafOverlapsBoxXYZ(lpSpacialNode->mBox, lSphereBoundingBox))
            {
                continue;
            }

            if (CgsGeometric::TestSpherePolygonSoup(*lpSpacialNode->mpPolygonSoup, lSphere).GetBool())
            {
                lpList->SetNumResults(1);                                        // 0x82812AB0
                return lu16ResultListIndex;
            }
        }

        lpList->SetNumResults(0);                                                // 0x82812A88
        return lu16ResultListIndex;
    }
}
}
