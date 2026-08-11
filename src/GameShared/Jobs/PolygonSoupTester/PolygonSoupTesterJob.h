#pragma once

// ============================================================================
// GameShared/Jobs/PolygonSoupTester/PolygonSoupTesterJob.h
//
// PolygonSoupTesterJob -- the collision job context that runs polygon-soup tests
// off a job thread. Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The console's own file path is baked into every assert in the family:
//   D:\P4\B5_MAIN\Burnout\MAIN\Code\GameShared\Jobs\PolygonSoupTester\PolygonSoupTesterJob.cpp
// (and PolygonSoupTester.cpp for the entry point), which is why this home sits under
// GameShared/Jobs/PolygonSoupTester/ rather than with the collision generator.
//
// X360 homes reconstructed in this pass (fill-worker wave 2, 2026-08-10):
//   PolygonSoupTesterJob::Execute                        @0x82915930  (107)
//   PolygonSoupTesterJob::ExecuteFillTriangleCacheStream  @0x82915D88 (145)
//   PolygonSoupTesterJob::FillTriangleCache               @0x82915FD0 (219)
//   PolygonSoupTesterJob::AllocateMemory                  @0x82916B98  (99)
//   PolygonSoupTesterJob::RunBoxQuery                     @0x82916D28  (46)
//   PolygonSoupTesterJob::LoadPrimitive                   @0x82916AB8   (8)
// Declared-only (the other two Execute arms; named boot gates in the .cpp):
//   PolygonSoupTesterJob::ExecuteFillTriangleCache        @0x82915AE0 (170)
//   PolygonSoupTesterJob::ExecuteLineTest                 @0x82916340 (159)
//
// ─── LAYOUT ──────────────────────────────────────────────────────────────────
// Every offset below is read out of the bodies, never guessed:
//   +0x00      (16 bytes)  never touched by any body in this family     [UNATTESTED]
//   +0x10      mpJobDescription        `a1[4] = a2` in Execute; `*(a1+16)` in the
//                                      stream arm
//   +0x14      (12 bytes)  never touched                                [UNATTESTED]
//   +0x20      maArena[102400]         the job's scratch arena; AllocateMemory
//                                      returns `a1 + 32 + cursor`
//   +0x19020   miAllocCursor           `*(a1 + 102432)`; the bump cursor, and the
//                                      value FillTriangleCache/RunBoxQuery save and
//                                      restore around their scoped allocations
//   +0x19024   maiFetchTags[7]         `a1[25609..25615] = -1` in Execute
//   sizeof     0x19040 == 102464       == PolygonSoupTesterEntry's `spuId * 0x19040`
//
// ⚠️ mpJobDescription WIDENS 4->8 on x64. This object is a runtime-carved static, not a
// serialized record, so the standing rule says widen -- and nothing reads it through a
// console byte offset (AllocateMemory reaches the arena and the cursor by NAME here,
// where the console reaches them as `this + 32` and `this + 102432`).
// ============================================================================

#include "types.hpp"

#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"      // Sphere (by value, the query)
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"   // Triangle4 (the fill destination)
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h" // AxisAlignedBox (the derived query box)

namespace CgsGeometric { struct PolygonSoupListSpatialMap; }
namespace CgsSceneManager { namespace CgsCollision { struct CollisionJobDescription; } }

struct PolygonSoupTesterJob
{
    // The scratch arena AllocateMemory bump-allocates out of. The console's overflow
    // tripwire compares against this exact number (`>= 102400`), so it is the arena's
    // SIZE, not a count.
    static const s32 KI_ARENA_BYTES = 102400;

    // ------------------------------------------------------------------------
    // Execute @0x82915930 -- the job body. Resets the fetch tags and the bump
    // cursor, latches the descriptor, then switches on the descriptor's type byte.
    // lpvJobData is EA::Jobs::Job::SetData's payload, i.e. the CollisionBatch's
    // 256-byte descriptor slot.
    // ------------------------------------------------------------------------
    void Execute(void* lpvJobData);

    // ------------------------------------------------------------------------
    // ExecuteFillTriangleCacheStream @0x82915D88 -- drain the command stream: one
    // FillTriangleCache per posted command, one result posted back per command.
    // ------------------------------------------------------------------------
    void ExecuteFillTriangleCacheStream();

    // ------------------------------------------------------------------------
    // FillTriangleCache @0x82915FD0 -- sphere -> AABB -> level sweep -> per-leaf
    // extract. Returns the number of Triangle4 BATCHES written; the total triangle
    // overflow count is reported through lpiOutOverflow when non-NULL.
    // ------------------------------------------------------------------------
    u16 FillTriangleCache(const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap,
                          const CgsGeometric::Sphere*                    lpCacheSphere,
                          CgsGeometric::Triangle4*                       lpaDestinationTriangles,
                          u16                                            lu16MaxNumTriangleBatches,
                          s32*                                           lpiOutOverflow);

    // AllocateMemory @0x82916B98 -- align the cursor, hand back the arena slice,
    // advance the cursor by the aligned size.
    void* AllocateMemory(s32 liNumBytes, s32 liAlignment);

    // RunBoxQuery @0x82916D28 -- carve the two 2048-entry ping/pong buffers and the
    // node cache out of the arena and run the map's job-side box query. The cursor is
    // restored on the way out, so the buffers are scoped to the call.
    s32 RunBoxQuery(const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap,
                    const CgsGeometric::AxisAlignedBox*            lpQueryBox,
                    u16**                                          lppaOutResults,
                    s32*                                           lpiOutNumResults);

    // LoadPrimitive @0x82916AB8 -- `*a3 = a2`, eight instructions. On SPU this is the
    // DMA fetch of the primitive; on a shared-memory host it is the pointer itself.
    void LoadPrimitive(const void* lpvSource, const void** lppvOut);

    // The other two Execute arms. NOT reconstructed this wave (the fill-cache arm is
    // the synchronous twin of the stream arm, the line arm is the traction-line leg);
    // both are named boot gates in the .cpp, not silent returns.
    void ExecuteFillTriangleCache();
    void ExecuteLineTest();

    // --- members ------------------------------------------------------------
    u8   mauUnattested00[0x10];                       // X360 +0x00      [UNATTESTED]
    CgsSceneManager::CgsCollision::CollisionJobDescription* mpJobDescription; // X360 +0x10
    u8   mauUnattested14[0x0C];                       // X360 +0x14      [UNATTESTED]

    // ⚠️⚠️ THE `alignas` IS LOad-BEARING, AND ITS ABSENCE WAS AN ACCESS VIOLATION.
    // Everything AllocateMemory hands out is carved from here, and the first thing the
    // fill stream does with a slice is `movaps xmm0, [rdi]` -- the 16-byte copy of the
    // command record's Sphere (Sphere/AxisAlignedBox/Vector4/Triangle4 are all
    // alignas(16)). On the X360 that came free: the arena sits at `this + 0x20`, a
    // 16-aligned offset. On x64 mpJobDescription WIDENS 4 -> 8, which pushes the arena
    // to +0x24 and takes it OFF 16 -- and a plain global array of this struct has no
    // reason to be aligned either. The result was an AV inside FillTriangleCache at
    // +0xA1, resolved from `Get-WinEvent 'Application Error'` (fault offset 0x103001)
    // through Burnout_PC.map before any log was read.
    //
    // 128 rather than 16 because 128 is the alignment EVERY AllocateMemory call site
    // asks for (`AllocateMemory(a1, n, 128)` at all five of them); pinning the base to
    // that makes the cursor's alignment arithmetic mean on the host what it means on
    // the console. This is a runtime-carved static, never a serialized record, so
    // growing sizeof is free.
    alignas(128) u8 maArena[KI_ARENA_BYTES];          // X360 +0x20
    s32  miAllocCursor;                               // X360 +0x19020
    s32  maiFetchTags[7];                             // X360 +0x19024
};

// The six per-job-thread contexts the console's entry point indexes
// (`unk_83123940 + spuId * 0x19040`, spuId asserted < 6).
const s32 KI_NUM_POLYGON_SOUP_TESTER_JOBS = 6;
extern PolygonSoupTesterJob gaPolygonSoupTesterJobs[KI_NUM_POLYGON_SOUP_TESTER_JOBS];
