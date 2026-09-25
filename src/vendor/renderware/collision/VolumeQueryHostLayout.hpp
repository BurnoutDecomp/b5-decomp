#pragma once

// ============================================================================
// vendor/renderware/collision/VolumeQueryHostLayout.hpp  (2026-09-25, crash parity FX-FOLLOWUPS)
//
// NOT X360: host GPInstance / VolRef widths. The HOST sizes of the backing stores the two in-place
// rw::collision queries carve out of their caller's buffer. The console derives every one of them
// from its own object and record sizes; on x64 four of those sizes differ, so every carve site and
// every descriptor total derives from the constants below instead of the console literals:
//
//   * VolumeVolumeQuery header   console 0x50 -- the 0x48-byte object rounded to 16
//                                (Construct @0x82BB38F0: 0x82BB390C `addi r11, r31, 0x50`)
//                                -> host sizeof(VolumeVolumeQuery) rounded to 16.
//   * VolumeBBoxQuery header     console 0x100 -- the 0xF1-byte object rounded to 16
//                                (Initialize @0x82BBBD90: 0x82BBBD9C `addi r10, r11, 0x100`)
//                                -> host sizeof(VolumeBBoxQuery) rounded to 16.
//   * the 1xN staging region     console 8 bytes per result: GetPrimitiveBBoxOverlaps @0x82BB3AB0
//                                meters a budget of `slwi r27, r11, 3` == 8R bytes at 12 per group
//                                header and 4 per staged pair, and Construct carves exactly 8R for it
//                                (m_intersectionBuffer = staging + 8R). The host VolRef1xN is 16 bytes
//                                of header and 8 per pair (pointer-widened), so the SAME metering
//                                decisions write up to 2 * 8R - 8 host bytes -> host 16 per result.
//   * the instancing scratch     console 192 * (R + 1): the 2072R + 192 of GetResourceDescriptor
//                                @0x82BB3A20 minus the 8R staging and 1872R results. That is slot 0 plus
//                                at most R N-side instances (the vRefsN of a group come from the
//                                R-entry primitive buffer of the bbox sub-query), at the console
//                                GPInstance stride 0xC0 (PrimitiveBatchIntersect @0x82BABC78
//                                `addi r27, r27, 0xC0`) -> host (R + 1) * sizeof(GPInstance).
// The staging METERING stays console-exact (the same staging decisions -- VolumeQuery.cpp); only the
// carve sizes move. The 0x750 result stride and the 0x80 VolRef / 0x60 Volume strides are native
// (static_asserted below / in their headers).
//
// Included by the rw::collision TUs only: it pulls VolumeBBoxQuery.hpp and GPInstance.hpp, whose vpu
// vocabulary must not reach the CgsSceneManager consumers (see VolumeBBoxQuery.hpp). Those consumers
// size their buffers from VolumeQuery.hpp's KU_VOLUME_VOLUME_QUERY_HOST_SIZE_R100, which
// VolumeQuery.cpp static_asserts against VolumeVolumeQueryResourceSize(100) here.
// ============================================================================

#include "types.hpp"
#include "vendor/renderware/collision/VolumeQuery.hpp"       // VolumeVolumeQuery
#include "vendor/renderware/collision/VolumeBBoxQuery.hpp"   // VolumeBBoxQuery
#include "vendor/renderware/collision/GPInstance.hpp"        // GPInstance / VolRef1xN / PrimitivePairIntersectResult
#include "vendor/renderware/collision/CollisionVolume.hpp"   // Volume (VolumeLineQuery's instanced-volume pool)
#include "vendor/renderware/collision/LineSegIntersect.hpp"  // VolumeLineSegIntersectResult (its result buffer)

#include <cstddef>   // size_t

namespace rw
{
namespace collision
{
    // The query objects' own sizes, rounded to their 16-byte alignment (the console carves at 0x50 / 0x100).
    constexpr u32 KU_VOLUME_VOLUME_QUERY_HEADER_SIZE =
        static_cast<u32>((sizeof(VolumeVolumeQuery) + 15u) & ~static_cast<size_t>(15u));
    constexpr u32 KU_VOLUME_BBOX_QUERY_HEADER_SIZE =
        static_cast<u32>((sizeof(VolumeBBoxQuery) + 15u) & ~static_cast<size_t>(15u));

    // The 1xN staging region per result: console 8 (the metered budget), host twice that.
    constexpr u32 KU_VOLUME_VOLUME_QUERY_STAGING_BYTES_PER_RESULT = 16u;

    // The result stride is native: console `mulli 0x750` (PrimitiveBatchIntersect / Construct's 1872 * a3).
    constexpr u32 KU_PRIMITIVE_PAIR_INTERSECT_RESULT_STRIDE = 1872u;
    static_assert(sizeof(PrimitivePairIntersectResult) == KU_PRIMITIVE_PAIR_INTERSECT_RESULT_STRIDE,
                  "the 0x750 PrimitivePairIntersectResult stride is the console's on the host too");

    // VolumeBBoxQuery::GetResourceDescriptor's total: header + (stackMax + results) VolRefs (0x80) + results
    // instanced Volumes (0x60) + the 0x27E0 spatial-map query workspace.
    constexpr u32 VolumeBBoxQueryResourceSize(u32 luStackMax, u32 luResBufferSize)
    {
        return KU_VOLUME_BBOX_QUERY_HEADER_SIZE
             + ((luStackMax + luResBufferSize) << 7)
             + 96u * luResBufferSize
             + 10208u;
    }

    constexpr u32 VolumeVolumeQueryStagingSize(u32 luResults)
    {
        return KU_VOLUME_VOLUME_QUERY_STAGING_BYTES_PER_RESULT * luResults;
    }

    constexpr u32 VolumeVolumeQueryInstancingSize(u32 luResults)
    {
        return (luResults + 1u) * static_cast<u32>(sizeof(GPInstance));
    }

    // VolumeVolumeQuery::GetResourceDescriptor's total (both sub-queries sized (results, results), as
    // Construct and GetResourceDescriptor call them).
    constexpr u32 VolumeVolumeQueryResourceSize(u32 luResults)
    {
        return KU_VOLUME_VOLUME_QUERY_HEADER_SIZE
             + 2u * VolumeBBoxQueryResourceSize(luResults, luResults)
             + VolumeVolumeQueryStagingSize(luResults)
             + KU_PRIMITIVE_PAIR_INTERSECT_RESULT_STRIDE * luResults
             + VolumeVolumeQueryInstancingSize(luResults);
    }

    // ---- VolumeLineQuery (2026-09-25) ----------------------------------------------------------------------
    // GetResourceDescriptor @0x82BB3838: `mulli r11, r5, 0x1B0 ; slwi r10, r4, 7 ; add ; addi 0x110 ; addi 0x2880`
    // == 0x1B0 * results + 0x80 * volumes + 0x110 + 0x2880, and Initialize @0x82BB3888 carves, from base + 0x110:
    // the stack VolRefs (0x80 * volumes), the primitive VolRefs (0x80 * results), the instanced Volumes
    // (0x60 * results), the VolumeLineSegIntersectResults (0xD0 * results), then the spatial-map query memory.
    // On the host the three record strides are native (VolRef 0x80, Volume 0x60 and VolumeLineSegIntersectResult
    // 0xD0 are static_asserted in their headers); only the header differs.
    // NOT X360: host VolumeLineQuery width -- console header 0x110.
    constexpr u32 KU_VOLUME_LINE_QUERY_HEADER_SIZE =
        static_cast<u32>((sizeof(VolumeLineQuery) + 15u) & ~static_cast<size_t>(15u));
    // The spatial-map query memory (the console's 0x2880). FLAG: the CONSOLE size, kept because nothing on
    // the host writes it -- the line walk that places the spatial-map line queries there (GetIntersections) is
    // not reconstructed. Re-derive it at host widths when that lands (the VolumeBBoxQuery 0x27E0 precedent).
    constexpr u32 KU_VOLUME_LINE_QUERY_SPATIAL_MAP_QUERY_MEM = 0x2880u;

    constexpr u32 VolumeLineQueryResourceSize(u32 luVolumes, u32 luResults)
    {
        return KU_VOLUME_LINE_QUERY_HEADER_SIZE
             + static_cast<u32>(sizeof(VolRef)) * luVolumes
             + static_cast<u32>(sizeof(VolRef) + sizeof(Volume) + sizeof(VolumeLineSegIntersectResult)) * luResults
             + KU_VOLUME_LINE_QUERY_SPATIAL_MAP_QUERY_MEM;
    }
    static_assert(sizeof(VolRef) + sizeof(Volume) + sizeof(VolumeLineSegIntersectResult) == 0x1B0u,
                  "VolumeLineQuery's per-result records are the console's 0x1B0 on the host too");
}
}
