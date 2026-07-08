// ============================================================================
// GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.cpp
//
// CgsGeometric polygon-soup intersection free functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The X360 bodies of this TU are dense hand-vectorised
// VMX/AltiVec (SoA transposes, sphere/line-triangle SIMD kernels, packed-vertex
// unpacking via .rdata permute tables). This first batch bodies the two
// groundable SoA triangle-buffer management helpers, whose asm is scalar plumbing
// around a per-lane triangle accumulator:
//
//   CgsGeometric::PolySoupAddToTriangleBuffer    @ 0x82844B70
//   CgsGeometric::PolySoupFinishTriangleBuffer   @ 0x8283B3B8
//
// The remaining four ledger functions of this TU are left for later waves and
// are BLOCKED for now (see the file-tail note): they require un-recovered .rdata
// VMX permute/mask tables (UnpackPolygonSoupVertices) or depend on un-homed
// heavy-VMX collaborators (TestSphereTriangle4SOA,
// IntersectLinePolySoupTriangleSingleSided4/DoubleSided).
// ============================================================================

#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"  // CgsGeometric::Triangle4
#include "BrnCommonTypes.h"                                            // Vector3 / Vector4 / VecFloat

#include <cstring>   // std::memcpy (mask bit-pattern construction)

namespace CgsGeometric
{
    // ------------------------------------------------------------------------
    // IntermediateTriangles -- the translation-unit-private SoA accumulator that
    // ExtractTriangle4ListIntersectingSphere / the Add & Finish helpers fill one
    // lane (one triangle) at a time before flushing a full batch of four into a
    // Triangle4 via PolySoupCopyTriangleBufferIntoTriangle4.
    //
    // LAYOUT (the accessed portion is proven by the PolySoupAddToTriangleBuffer
    // asm @ 0x82844B70, which for lane index L in [0,4) stores six 16-byte
    // registers at byte 16*(L + 4*row) for row in 0..5, and the surface tag at
    // byte 4*(96 + L) == 0x180 + 4*L):
    //
    //   +0x000  Vector3 mVertex0[4]      row 0  (v1) -- per-lane triangle vertex 0
    //   +0x040  Vector3 mVertex1[4]      row 1  (v2) -- per-lane triangle vertex 1
    //   +0x080  Vector3 mVertex2[4]      row 2  (v3) -- per-lane triangle vertex 2
    //   +0x0C0  VecFloat mEdgeCosine0[4] row 3  (v4) -- per-lane edge-0 cosine
    //   +0x100  VecFloat mEdgeCosine1[4] row 4  (v5) -- per-lane edge-1 cosine
    //   +0x140  VecFloat mEdgeCosine2[4] row 5  (v6) -- per-lane edge-2 cosine
    //   +0x180  u32      mauSurfaceTags[4]       -- per-lane surface tag
    //
    // The full instance is 0x640 (1600) bytes on the X360 stack (var_17C0 in
    // ExtractTriangle4ListIntersectingSphere). The Add/Finish helpers touch only
    // the first 0x190 bytes above; the trailing region is consumed exclusively by
    // the un-homed PolySoupCopyTriangleBufferIntoTriangle4 and is not attested by
    // this batch, so it is sized (not interpreted) to keep sizeof exact.
    // FLAGGED opaque: interior layout of the +0x190.. tail not attested here.
    // ------------------------------------------------------------------------
    struct IntermediateTriangles
    {
        Vector3  mVertex0[4];       // +0x000
        Vector3  mVertex1[4];       // +0x040
        Vector3  mVertex2[4];       // +0x080
        VecFloat mEdgeCosine0[4];   // +0x0C0
        VecFloat mEdgeCosine1[4];   // +0x100
        VecFloat mEdgeCosine2[4];   // +0x140
        u32      mauSurfaceTags[4]; // +0x180
        u8       mauOpaqueTail[0x640 - 0x190]; // +0x190  used only by CopyTriangleBufferIntoTriangle4
    };

    // Flush of a full batch copies the accumulator into one Triangle4; it lives
    // in this same TU but is reconstructed in a later wave. Declared here so the
    // two helpers below compile against it (link-time trap stub until then).
    void PolySoupCopyTriangleBufferIntoTriangle4(Triangle4* lpOutTriangles,
                                                 IntermediateTriangles* lpBuffer,
                                                 s32 liWriteIndex);

    namespace
    {
        // Build a Triangle4 per-lane validity mask (Mask4 == Vector4) with the
        // leading `liCount` lanes marked true. The X360 mask convention (see
        // rw::math::vpu::MaskScalar) is a full 0xFFFFFFFF word per true lane
        // (reads as NaN, != 0.0) and +0.0 for a false lane -- the flush asm builds
        // it with `vspltisw 0 / vnot` (all four lanes, the full-batch case) and
        // with progressive `vrlimi128 v,~0,{8,0xC,0xE},0` inserts (lanes x, x|y,
        // x|y|z) for the 1/2/3-lane partial-batch cases.
        inline Triangle4::Mask4 MakeLeadingLaneMask(s32 liCount)
        {
            Triangle4::Mask4 lMask;
            lMask.SetZero();
            const u32 luTrueLane = 0xFFFFFFFFu;
            if (liCount > 0) std::memcpy(&lMask.x, &luTrueLane, sizeof(u32));
            if (liCount > 1) std::memcpy(&lMask.y, &luTrueLane, sizeof(u32));
            if (liCount > 2) std::memcpy(&lMask.z, &luTrueLane, sizeof(u32));
            if (liCount > 3) std::memcpy(&lMask.w, &luTrueLane, sizeof(u32));
            return lMask;
        }
    }

    // ------------------------------------------------------------------------
    // PolySoupAddToTriangleBuffer @ 0x82844B70
    //
    // Append one triangle (its three vertices, three edge cosines and surface
    // tag) into lane `lriLaneCount` of the SoA accumulator. When the fourth lane
    // is filled, flush the batch of four into lpOutTriangles[lriWriteIndex]:
    // copy the accumulator, mark all four lanes valid, assert the batch, then
    // reset the lane counter and advance the write index. If the write index has
    // reached the caller-supplied maximum, the last slot is overwritten and the
    // "overflowed" flag (the accumulated return value) is set.
    // ------------------------------------------------------------------------
    s32 PolySoupAddToTriangleBuffer(Triangle4* lpOutTriangles,
                                    s32 liMaxTriangles,
                                    IntermediateTriangles* lpBuffer,
                                    Vector3 lVertex0,
                                    Vector3 lVertex1,
                                    Vector3 lVertex2,
                                    VecFloat lEdgeCosine0,
                                    VecFloat lEdgeCosine1,
                                    VecFloat lEdgeCosine2,
                                    u32 luSurfaceTag,
                                    s32& lriWriteIndex,
                                    s32& lriLaneCount)
    {
        s32 liOverflowed = 0;

        const s32 liLane = lriLaneCount;
        lpBuffer->mVertex0[liLane]     = lVertex0;
        lpBuffer->mVertex1[liLane]     = lVertex1;
        lpBuffer->mVertex2[liLane]     = lVertex2;
        lpBuffer->mEdgeCosine0[liLane] = lEdgeCosine0;
        lpBuffer->mEdgeCosine1[liLane] = lEdgeCosine1;
        lpBuffer->mEdgeCosine2[liLane] = lEdgeCosine2;
        lpBuffer->mauSurfaceTags[liLane] = luSurfaceTag;

        lriLaneCount = liLane + 1;
        if (lriLaneCount == 4)
        {
            if (lriWriteIndex >= liMaxTriangles)
            {
                liOverflowed = 1;
                --lriWriteIndex;
            }
            PolySoupCopyTriangleBufferIntoTriangle4(lpOutTriangles, lpBuffer, lriWriteIndex);
            lpOutTriangles[lriWriteIndex].mValidMasks = MakeLeadingLaneMask(4);
            lpOutTriangles[lriWriteIndex].AssertIsValid();
            lriLaneCount = 0;
            ++lriWriteIndex;
        }

        return liOverflowed;
    }

    // ------------------------------------------------------------------------
    // PolySoupFinishTriangleBuffer @ 0x8283B3B8
    //
    // Flush a partially-filled accumulator (1..3 lanes) at the end of a sweep.
    // Copies the accumulator into lpOutTriangles[lriWriteIndex], marks only the
    // filled lanes valid, resets the lane counter and advances the write index.
    // Nothing happens (and 0 is returned) if the accumulator is empty. As in Add,
    // a write index that has reached the maximum overwrites the last slot and
    // sets the overflow flag.
    // ------------------------------------------------------------------------
    s32 PolySoupFinishTriangleBuffer(Triangle4* lpOutTriangles,
                                     s32 liMaxTriangles,
                                     IntermediateTriangles* lpBuffer,
                                     s32& lriWriteIndex,
                                     s32& lriLaneCount)
    {
        s32 liOverflowed = 0;

        if (lriLaneCount != 0)
        {
            if (lriWriteIndex >= liMaxTriangles)
            {
                liOverflowed = 1;
                --lriWriteIndex;
            }
            PolySoupCopyTriangleBufferIntoTriangle4(lpOutTriangles, lpBuffer, lriWriteIndex);
            lpOutTriangles[lriWriteIndex].mValidMasks = MakeLeadingLaneMask(lriLaneCount);
            lriLaneCount = 0;
            ++lriWriteIndex;
        }

        return liOverflowed;
    }
}
