// =============================================================================
// GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree_wSQ1.cpp
//
// LooseOctree's coarse LINE query entry (scene-query wave 1, 2026-09-02). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//
//   LooseOctree::LineTest          @ 0x828D01F0   (32 insns)   -- REAL
//   LooseOctree::LineTestOptimized @ 0x828CA5F8   (128 insns)  -- LOUD TRAP (see below)
//
// Split out of CgsLooseOctree.cpp (the mounted frustum/update body) so the new slot lands as
// its own mount and the shared TU is not rewritten.
//
// LineTest @0x828D01F0, instruction for instruction:
//   0x828D01F0..0x828D0210  save v126/v127 (the two Vector3 VALUE parameters ride in v1/v2 and
//                           must survive the StartMonitor call), r3 = this, r4 = flags, r5 = out
//   0x828D0224  lwz r31, dword_82F33F20   -> _miVPLineTestPerfMon ("Octree VP LineTest")
//   0x828D0228  bl  PerfMonCpu::StartMonitor(r31)        (UNCONDITIONAL -- no `> -1` guard here,
//                                                          unlike the SceneManagerModule passes)
//   0x828D0238  bl  LooseOctree::LineTestOptimized(this, flags, out, v1 = start, v2 = end)
//   0x828D0244  bl  PerfMonCpu::StopMonitor(r31)
//   return the LineTestOptimized result (r30 -> r3).
// The tree's PerfMonCpu::StartMonitor/StopMonitor no-op on an invalid handle, so the
// unregistered (-1) monitor is inert exactly as an unregistered console monitor would be.
// =============================================================================

#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"    // PerfMonCpu::Start/StopMonitor

namespace CgsSceneManager
{
    // DWARF CgsLooseOctree.h:120 / X360 dword_82F33F20 -- not yet registered by the tree's
    // Construct (see the header note); -1 == invalid handle.
    s32 LooseOctree::_miVPLineTestPerfMon = -1;

    // @ 0x828D01F0
    bool LooseOctree::LineTest(u32 lx32EntityTypeFlags, Vector3 lLineStart, Vector3 lLineEnd,
                               CoarseQueryResultBuffer<16384>* lpResultBufferOut)
    {
        const s32 liMonitor = _miVPLineTestPerfMon;                       // lwz r31, dword_82F33F20
        CgsDev::PerfMonCpu::StartMonitor(liMonitor);
        const bool lbResult = LineTestOptimized(lx32EntityTypeFlags, lLineStart, lLineEnd,
                                                lpResultBufferOut);
        CgsDev::PerfMonCpu::StopMonitor(liMonitor);
        return lbResult;
    }

    // @ 0x828CA5F8 (128 insns) + LineTestRecursive @0x828BCF50 (731 insns).
    //
    // ⛔ NOT RECONSTRUCTED -- a LOUD trap, never a quiet "no entities". The walk fills the
    // coarse result buffer with every octree entity whose bounding sphere the segment crosses;
    // a stub that BeginResultsBatch/EndResultsBatch'd an empty batch would make every octree
    // line query report "nothing in the way" -- the silent-drop class. The scene-query wave 1
    // consumer (SceneManagerModule::ProcessLineTestNearest @0x828D38C0) reaches this slot only
    // for queries whose entity-type flags are NOT exactly the WORLD bit (2); the race car's
    // above-ground rays are world-only and never come here.
    bool LooseOctree::LineTestOptimized(u32 /*lx32EntityTypeFlags*/, Vector3 /*lLineStart*/,
                                        Vector3 /*lLineEnd*/,
                                        CoarseQueryResultBuffer<16384>* /*lpResultBufferOut*/)
    {
        CGS_ASSERT(false, "LooseOctree::LineTestOptimized @0x828CA5F8 (+ LineTestRecursive @0x828BCF50) "
                          "is not reconstructed -- octree line queries have no answer yet");
        return false;
    }
}
