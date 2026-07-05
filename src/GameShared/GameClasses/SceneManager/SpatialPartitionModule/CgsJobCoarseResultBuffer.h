#pragma once

// ============================================================================
// GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsJobCoarseResultBuffer.h
//
// CgsSceneManager::JobCoarseResultBuffer -- per-frustum-test-job coarse-result staging
// buffer: up to KU_JOB_BUFFER_MAX_NUM_QUERIES (16) query runs packed into one shared u16
// pool. Reconstructed from BURNOUT_X360_ARTIST.XEX + DecFIGS DWARF
// (CgsJobCoarseResultBuffer.h:47/82-86 + :28/29).
//
// LAYOUT (DWARF-authoritative; size == 140 (0x8C), stride 0x100 inside
// LooseOctree::maJobResultBuffers -- proven by the WaitForFrustumTestJobResults @0x828B2558
// anchors: r27 = &maQueryOffsets[0] (buffer+0x08); lwz 0x80(r27) == mpu16Buffer (+0x88);
// lwz 0x40(r30) == maQueryNumResults[q] (0x48 base)):
//   +0x00  u32  muNumQueries
//   +0x04  u32  muCurrentWriteOffset
//   +0x08  u32  maQueryOffsets[16]
//   +0x48  u32  maQueryNumResults[16]
//   +0x88  u16* mpu16Buffer
// ============================================================================

#include "types.hpp"

namespace rw { struct IResourceAllocator; }

namespace CgsSceneManager
{
    // CgsJobCoarseResultBuffer.h:28/29 (DWARF).
    static const u32 KU_JOB_RESULT_BUFFER_SIZE     = 8192;
    static const u32 KU_JOB_BUFFER_MAX_NUM_QUERIES = 16;

    // CgsJobCoarseResultBuffer.h:47 (DWARF).
    struct JobCoarseResultBuffer
    {
        u32  muNumQueries;                                     // +0x00
        u32  muCurrentWriteOffset;                             // +0x04
        u32  maQueryOffsets[KU_JOB_BUFFER_MAX_NUM_QUERIES];     // +0x08
        u32  maQueryNumResults[KU_JOB_BUFFER_MAX_NUM_QUERIES];  // +0x48
        u16* mpu16Buffer;                                      // +0x88

        void Construct(rw::IResourceAllocator* lpAllocator);   // :52 (declare-only)
        void Clear();                                          // :56 (declare-only)
    };
}
