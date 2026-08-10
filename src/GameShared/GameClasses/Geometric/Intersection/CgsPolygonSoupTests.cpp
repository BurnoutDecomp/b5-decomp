// ============================================================================
// GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.cpp
//
// CgsGeometric polygon-soup intersection free functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
//   CgsGeometric::PolySoupAddToTriangleBuffer            @ 0x82844B70   (68)
//   CgsGeometric::PolySoupFinishTriangleBuffer           @ 0x8283B3B8   (49)
//   CgsGeometric::PolySoupCopyTriangleBufferIntoTriangle4@ 0x82839690   (97)
//   CgsGeometric::UnpackPolygonSoupVertices              @ 0x8283B480   (40)
//   CgsGeometric::TestSphereTriangle4SOA                 @ 0x8283FD50  (144)
//   CgsGeometric::ExtractTriangle4ListIntersectingSphere @ 0x82844C80  (602)
//
// ⭐⭐ 2026-08-11 (the extractor wave). The file-tail note this banner used to
// carry -- "BLOCKED: un-recovered .rdata VMX permute/mask tables ... un-homed
// heavy-VMX collaborators" -- is RETIRED, and both halves of it were wrong in
// an instructive way:
//   * the permute/mask tables are not `.rdata` literals at all. They are zero in
//     the image because they are built at runtime by C++ dynamic initialisers
//     (0x82C6DBD0 / 0x82C6DC10 for the unpack pair, five more for
//     LoadEdgeCosines). A full-text scan of the 30,084 export JSONs for the
//     addresses found every writer. Not one constant here is guessed.
//   * ExtractTriangle4ListIntersectingSphere is not "dense VMX". Its 602
//     instructions are a loop driver plus four AoS->SoA `vmrghw/vmrglw`
//     transposes. The only real kernel in the family is TestSphereTriangle4SOA
//     (144), and that turns out to be a PUBLISHED algorithm (see its banner).
//
// PC LOWERING, stated once for the whole file: the tree's rw::math::vpu vectors
// are plain 16-byte, 16-aligned {x,y,z,w} structs with no SIMD operations, and
// the established precedent for this family (CgsTriangle4.cpp) is portable
// scalar float math. Everything below follows it. ⭐ That is also the safest
// possible answer to the "which lane?" hazard: once the console's SoA registers
// are written as `f32 v[4]`, a lane is an ARRAY INDEX and there is no swizzle
// left to get wrong. The lane assignments themselves were settled from the
// caller's transposes, cross-checked against the caller's own per-lane
// PolySoupAddToTriangleBuffer arguments, and confirmed a third time by the PS3
// DWARF parameter names (lVertex0X..lVertex2Z @0xB55A24).
// ============================================================================

#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"

#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"  // CgsGeometric::Triangle4
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"     // CgsGeometric::Sphere
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include "BrnCommonTypes.h"                                            // Vector3 / Vector4 / VecFloat

#include <cstring>   // std::memcpy (mask bit-pattern construction)
#include <cmath>     // std::sqrt (the sphere/triangle normal)

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
    // ⭐⭐ 2026-08-11: THE OPAQUE TAIL IS RETIRED, AND ITS JUSTIFICATION WAS FALSE.
    // This struct used to carry `u8 mauOpaqueTail[0x640 - 0x190]` with the note
    // "consumed exclusively by the un-homed PolySoupCopyTriangleBufferIntoTriangle4".
    // That function is now decoded in full (all 97 instructions, below) and the
    // HIGHEST byte it reads from this buffer is +0x180. Nothing anywhere touches
    // the tail. The 0x640 was never a struct size: it is the distance on the X360
    // stack from var_17C0 to the next local (the unpacked-vertex buffer at
    // var_1180), i.e. frame padding. This is a runtime stack local, never
    // serialised and pointer-free, so shrinking it to its attested extent is
    // free -- and it takes one more opaque `u8[N]` off a live path.
    //   sizeof == 0x190 (400).
    // The class NAME is not inferred: the PS3 DWARF mangle for
    // PolySoupAddToTriangleBuffer @0xB65074 spells the third parameter
    // `PNS_21IntermediateTrianglesE`.
    // ------------------------------------------------------------------------
    struct alignas(16) IntermediateTriangles
    {
        Vector3  mVertex0[4];       // +0x000
        Vector3  mVertex1[4];       // +0x040
        Vector3  mVertex2[4];       // +0x080
        VecFloat mEdgeCosine0[4];   // +0x0C0
        VecFloat mEdgeCosine1[4];   // +0x100
        VecFloat mEdgeCosine2[4];   // +0x140
        u32      mauSurfaceTags[4]; // +0x180
    };

    // Flush of a full batch copies the accumulator into one Triangle4.
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

    // ------------------------------------------------------------------------
    // PolySoupCopyTriangleBufferIntoTriangle4 @0x82839690 (97)
    //
    // Transpose the four accumulated lanes into one SoA Triangle4.
    //   dest = lpOutTriangles + liWriteIndex        (`mulli r11, r5, 0xE0`)
    // Six 4x4 `vmrghw/vmrglw` transposes -- one per accumulator row -- plus a
    // straight 16-byte copy of the four surface tags:
    //   +0x00/+0x10/+0x20 <- mVertex0[0..3].x / .y / .z
    //   +0x30/+0x40/+0x50 <- mVertex1[0..3].x / .y / .z
    //   +0x60/+0x70/+0x80 <- mVertex2[0..3].x / .y / .z
    //   +0xA0             <- mauSurfaceTags[0..3]      (verbatim, `lvx128`/`stvx128`)
    //   +0xB0/+0xC0/+0xD0 <- mEdgeCosineN[0..3].x
    // ⭐ The three cosine gathers take only lane .x of each source because
    // LoadEdgeCosines stores splats -- the console's `vmrghw` chain there reads
    // element 0 of every operand.
    // ⚠️ +0x90 (mValidMasks) is NOT written here; the two callers stamp it
    // immediately afterwards. Reproduced as such.
    //
    // The store targets are a third independent witness for the Triangle4 layout
    // in CgsTriangle4.h (which came from the DecFIGS DWARF) and they agree slot
    // for slot.
    // ------------------------------------------------------------------------
    void PolySoupCopyTriangleBufferIntoTriangle4(Triangle4* lpOutTriangles,
                                                 IntermediateTriangles* lpBuffer,
                                                 s32 liWriteIndex)
    {
        Triangle4& lrDest = lpOutTriangles[liWriteIndex];

        f32* const lapV0[3] = { &lrDest.mVertex0X.x, &lrDest.mVertex0Y.x, &lrDest.mVertex0Z.x };
        f32* const lapV1[3] = { &lrDest.mVertex1X.x, &lrDest.mVertex1Y.x, &lrDest.mVertex1Z.x };
        f32* const lapV2[3] = { &lrDest.mVertex2X.x, &lrDest.mVertex2Y.x, &lrDest.mVertex2Z.x };

        for (s32 liLane = 0; liLane < 4; ++liLane)
        {
            const Vector3& lrA = lpBuffer->mVertex0[liLane];
            const Vector3& lrB = lpBuffer->mVertex1[liLane];
            const Vector3& lrC = lpBuffer->mVertex2[liLane];

            lapV0[0][liLane] = lrA.x;  lapV0[1][liLane] = lrA.y;  lapV0[2][liLane] = lrA.z;
            lapV1[0][liLane] = lrB.x;  lapV1[1][liLane] = lrB.y;  lapV1[2][liLane] = lrB.z;
            lapV2[0][liLane] = lrC.x;  lapV2[1][liLane] = lrC.y;  lapV2[2][liLane] = lrC.z;

            (&lrDest.mEdge0Cosigns.x)[liLane] = lpBuffer->mEdgeCosine0[liLane].x;
            (&lrDest.mEdge1Cosigns.x)[liLane] = lpBuffer->mEdgeCosine1[liLane].x;
            (&lrDest.mEdge2Cosigns.x)[liLane] = lpBuffer->mEdgeCosine2[liLane].x;
        }

        // The tag row is moved as one 16-byte register; the destination is typed
        // Vector4 but holds four u32 bit patterns, so this is a bit copy and not
        // a float conversion.
        std::memcpy(&lrDest.mSurfaceTags, lpBuffer->mauSurfaceTags, 4 * sizeof(u32));
    }

    // ------------------------------------------------------------------------
    // UnpackPolygonSoupVertices @0x8283B480 (40)
    //
    //   lvlx v12, soup            -> the soup's first 16 bytes, i.e. the raw
    //                                words {miPosX, miPosY, miPosZ, bits(mfScale)}
    //   vspltw v11, v12, 3        -> splat(mfScale)
    //   per vertex (6-byte stride, GetVertex(0) supplies the base):
    //     lvlx/lvrx/vor           -> the 16 bytes at &vertex[i] (unaligned)
    //     vperm  by unk_830393D0  -> bytes {00 00 00 01 | 00 00 02 03 |
    //                                       00 00 04 05 | 00 00 00 00}
    //     vand   by unk_830391F0  -> {0x0000FFFF, 0x0000FFFF, 0x0000FFFF, 0}
    //     vadduwm v12             -> integer add of the soup origin
    //     vcfsx  (SIGNED, UIMM 0) -> float
    //     vmulfp v11              -> * mfScale
    //     stvx128 -> out[i]       (16-byte stride)
    //
    // ⭐ THE TWO CONSTANTS DECIDE THE ONE OPEN QUESTION IN THIS FORMULA. Both are
    // zero in the image and are written at runtime (0x82C6DC10 builds the perm as
    // CreateIntegerVector(1, 0x203, 0x405, 0); 0x82C6DBD0 builds the mask as
    // CreateIntegerVector(0xFFFF, 0xFFFF, 0xFFFF, 0)). The perm brings the two
    // bytes of component k into the low half of word k and the mask clears the
    // junk above them, so the packed components are **big-endian UNSIGNED 16-bit**
    // and are ZERO-extended, not sign-extended. The signed part of the sum is the
    // soup's s32 origin. CgsPolygonSoup.h:20 records the formula; this settles its
    // signedness from the image.
    //
    // ⚠️⚠️ AND THE w LANE IS JUNK, ON PURPOSE-BY-ACCIDENT, AND IS REPRODUCED.
    // The addend's lane 3 is the RAW BIT PATTERN of mfScale (it came in with the
    // `lvlx` of the soup header) and the mask's lane 3 is zero, so every unpacked
    // vertex gets `w = (f32)(s32)bits(mfScale) * mfScale` -- a large meaningless
    // float. It never reaches TestSphereTriangle4SOA (the extractor's transposes
    // take only the z lanes out of the merges that carry it), but it DOES travel
    // into Triangle4 through PolySoupAddToTriangleBuffer. Not "cleaned" to zero:
    // that would be a silent divergence from the console for no stated reason.
    //
    // ⚠️ PC LOWERING, FLAGGED: the console reads 16 bytes at each 6-byte vertex
    // with lvlx/lvrx, which cannot fault past the record. A 16-byte unaligned host
    // load could, at the tail of a resource, so the three components are read
    // directly. Same bytes, same result, no over-read.
    //
    // ⭐⭐ FLAG PC-platform leaf -- THE ONE PLACE THIS FUNCTION IS NOT THE CONSOLE.
    // The console's vperm control assembles each component BIG-endian, because the
    // X360 bundle is big-endian. WORLDCOL.BIN in this build is a PORTED, platform-4
    // (little-endian) bundle: tools/assets/bundles/world_support_transcode.py emits
    // every packed vertex with `struct.pack_into(e + '3h', ...)`, e == '<'. So the
    // components must be read in HOST order, exactly as CgsPolygonSoup.h:66 already
    // records for mu16SoupSize ("a two-byte array would have forced that consumer to
    // hand-assemble the halfword, which on this LITTLE-endian host is the opposite
    // byte order from the console `lhz` it was copying").
    // MEASURED, not reasoned: read straight out of build/game/WORLDCOL.BIN, soup 86
    // poly 3 of resource BF2191AA -- the junkyard floor quad whose four world-space
    // vertices CgsPolygonSoup.h:27 records from an independent measurement:
    //   raw 3e04f7009810  host-order -> (2986.305, -3.525, -2005.995)   MATCHES
    //                     byte-swapped-> (3208.155, 941.250, -1485.795)  garbage
    // and the same for all four vertices, all twelve components, to the millimetre.
    // ------------------------------------------------------------------------
    void UnpackPolygonSoupVertices(Vector3* lpOutVertexPositions,
                                   const PolygonSoup& lPolygonSoup)
    {
        const u32 lu32NumVertices = lPolygonSoup.mu8NumVertices;

        // `lvlx v12, r0, r31` + `vspltw v11, v12, 3`: the header quad, and the
        // scale splat taken out of its lane 3.
        const f32 lfScale = lPolygonSoup.mfScale;

        // The w lane of the integer addend is the scale's own bit pattern.
        s32 liScaleBits;
        std::memcpy(&liScaleBits, &lPolygonSoup.mfScale, sizeof(s32));

        const s32 laiOrigin[4] =
        {
            lPolygonSoup.miPosX, lPolygonSoup.miPosY, lPolygonSoup.miPosZ, liScaleBits
        };

        const u8* lpVertex = lPolygonSoup.GetVertex(0);

        for (u32 luIndex = 0; luIndex < lu32NumVertices; ++luIndex)
        {
            // vperm + vand: three u16 ZERO-extended (the mask is 0x0000FFFF, not a
            // sign extension); lane 3 masked to 0. Host byte order -- see the
            // PC-platform leaf note above.
            u16 lau16Packed[3];
            std::memcpy(lau16Packed, lpVertex, 3 * sizeof(u16));

            const u32 lau32Packed[4] =
            {
                static_cast<u32>(lau16Packed[0]),
                static_cast<u32>(lau16Packed[1]),
                static_cast<u32>(lau16Packed[2]),
                0u
            };

            Vector3& lrOut = lpOutVertexPositions[luIndex];

            // vadduwm (wrapping 32-bit add), vcfsx (SIGNED convert), vmulfp.
            lrOut.x = static_cast<f32>(static_cast<s32>(
                          static_cast<u32>(laiOrigin[0]) + lau32Packed[0])) * lfScale;
            lrOut.y = static_cast<f32>(static_cast<s32>(
                          static_cast<u32>(laiOrigin[1]) + lau32Packed[1])) * lfScale;
            lrOut.z = static_cast<f32>(static_cast<s32>(
                          static_cast<u32>(laiOrigin[2]) + lau32Packed[2])) * lfScale;
            lrOut.w = static_cast<f32>(static_cast<s32>(
                          static_cast<u32>(laiOrigin[3]) + lau32Packed[3])) * lfScale;

            lpVertex += KI_VERTEX_STRIDE;   // `addi r3, r3, 6`
        }
    }

    // ------------------------------------------------------------------------
    // TestSphereTriangle4SOA @0x8283FD50 (144)
    //
    // A LEAF function: no prologue, no calls (`xrefs_from` is empty), one memory
    // access (`lvx128 v0, r0, r3` = the sphere). Nine 16-byte vector parameters
    // hold the four triangles in SoA form and it returns a per-lane mask in v1.
    //
    // ⭐⭐ THIS IS A PUBLISHED ALGORITHM: Ericson, *Real-Time Collision Detection*
    // 5.2.7 `TestSphereTriangle` -- the separating-axis form -- widened to four
    // triangles. It was DERIVED here, not pattern-matched, and the derivation is
    // what settles the arithmetic:
    //
    //   Ericson's edge-AB test is
    //       Q1 = (bb-ab)A + (aa-ab)B,   e1 = |A-B|^2,   QC = C*e1 - Q1
    //       sep = dot(Q1,Q1) > rr*e1*e1  &&  dot(Q1,QC) > 0
    //   Expanding |Q1|^2 by hand collapses to (aa*bb - ab*ab) * e1 == G*e1, so
    //       dot(Q1,Q1) > rr*e1*e1   <=>   G > rr*e1
    //       dot(Q1,QC) = e1*(dot(Q1,C) - G) > 0  <=>  (bb-ab)ac + (aa-ab)bc > G
    //   and those two ARE the asm, verbatim, at 0x8283FEB4..0x8283FEF8 (edge B-D),
    //   0x8283FEB8..0x8283FF04 (edge D-A) and 0x8283FF38..0x8283FF64 (edge A-B).
    //
    // The rest, instruction by instruction:
    //   N       = cross(P1-P0, P2-P1)         0x8283FD7C..FDA0 (three vnmsubfp)
    //   nUnit   = N * rsqrt(|N|^2)            vrsqrtefp + ONE Newton-Raphson step
    //                                         (0x8283FDB4..FDD0; the constants are
    //                                          vcfsx(vspltisw 1,1)=0.5f and
    //                                          vcfsx(vspltisw 1,0)=1.0f, immediate-
    //                                          derived -- no .rdata is involved)
    //   A,B,D   = P0-C, P1-C, P2-C            0x8283FDE0..FE04
    //   plane   = dot(A,nUnit)^2 > r^2        0x8283FF48 / 0x8283FF68
    //   inside  = r^2 > |A|^2 || > |B|^2 || > |D|^2       0x8283FE54..FE84
    //   vertexN = (dot(N,M) > |N|^2) for both other vertices
    //   result  = ~( plane | (!inside && (vertexSep | edgeSep)) )   0x8283FF78..FF88
    //
    // ⚠️ PC LOWERING, FLAGGED: `vrsqrtefp` + 1 NR is a ~23-bit reciprocal square
    // root; a true `1.0f/sqrt` is used. It feeds only a `> r^2` comparison, and an
    // x86 `rsqrtps` is a DIFFERENT estimate, so imitating the shape would not be
    // more faithful. The DEGENERATE case still matches: |N|^2 == 0 gives inf on
    // both, then 0*inf = NaN, and `NaN > r^2` is false on both, so a zero-area
    // triangle is not plane-separated on either.
    //
    // ⛔⛔ AND A REAL CONSOLE BUG, REPRODUCED VERBATIM AND FLAGGED, NOT CORRECTED.
    // At most one vertex axis can separate (Cauchy-Schwarz), and it must be the
    // vertex with the smallest |P-C|^2, so the console guards the three vertex
    // tests with a 3-way minimum cascade. Three arms are right; the fourth ANDs
    // the "D is closest" region with the vertex-B test:
    //     0x8283FF50  vand v13, v13, v10   ; (b2<=a2) && (d2<=b2)   -> D is closest
    //     0x8283FF5C  vand v13, v13, v1    ; ... && Bsep            <- WRONG VERTEX
    // (raw words checked against the image: 11AD5404 / 11AD0C04 -- IDA is not
    // misprinting.) Net effect: `vertexSep = Asep | Bsep | (Dsep && b2 > a2)`, so
    // when |D|^2 <= |B|^2 <= |A|^2 the vertex-D axis is never tested and the
    // function answers "intersecting" for a sphere that in fact misses near D.
    // A conservative false positive -- a few extra triangles in the cache -- which
    // is why it survived shipping. Correcting it would silently diverge from the
    // console's triangle set.
    // ------------------------------------------------------------------------
    Triangle4::Mask4 TestSphereTriangle4SOA(const Sphere& lSphere,
                                            Vector4 lVertex0X, Vector4 lVertex0Y, Vector4 lVertex0Z,
                                            Vector4 lVertex1X, Vector4 lVertex1Y, Vector4 lVertex1Z,
                                            Vector4 lVertex2X, Vector4 lVertex2Y, Vector4 lVertex2Z)
    {
        const f32* const lapP0[3] = { &lVertex0X.x, &lVertex0Y.x, &lVertex0Z.x };
        const f32* const lapP1[3] = { &lVertex1X.x, &lVertex1Y.x, &lVertex1Z.x };
        const f32* const lapP2[3] = { &lVertex2X.x, &lVertex2Y.x, &lVertex2Z.x };

        // `lvx128 v0, r0, r3` then three vspltw + one for the radius.
        const f32 lfCx = lSphere.mPositionRadius.x;
        const f32 lfCy = lSphere.mPositionRadius.y;
        const f32 lfCz = lSphere.mPositionRadius.z;
        const f32 lfR2 = lSphere.mPositionRadius.w * lSphere.mPositionRadius.w;

        Triangle4::Mask4 lResult;
        lResult.SetZero();
        f32* const lpaResultLane = &lResult.x;

        const u32 luTrueLane = 0xFFFFFFFFu;

        for (s32 liLane = 0; liLane < 4; ++liLane)
        {
            const f32 lfP0x = lapP0[0][liLane], lfP0y = lapP0[1][liLane], lfP0z = lapP0[2][liLane];
            const f32 lfP1x = lapP1[0][liLane], lfP1y = lapP1[1][liLane], lfP1z = lapP1[2][liLane];
            const f32 lfP2x = lapP2[0][liLane], lfP2y = lapP2[1][liLane], lfP2z = lapP2[2][liLane];

            // N = cross(P1 - P0, P2 - P1)
            const f32 lfEax = lfP1x - lfP0x, lfEay = lfP1y - lfP0y, lfEaz = lfP1z - lfP0z;
            const f32 lfEbx = lfP2x - lfP1x, lfEby = lfP2y - lfP1y, lfEbz = lfP2z - lfP1z;

            const f32 lfNx = lfEay * lfEbz - lfEaz * lfEby;
            const f32 lfNy = lfEaz * lfEbx - lfEax * lfEbz;
            const f32 lfNz = lfEax * lfEby - lfEay * lfEbx;

            const f32 lfLenSq  = lfNx * lfNx + lfNy * lfNy + lfNz * lfNz;
            const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);   // vrsqrtefp + 1 NR

            const f32 lfUnitNx = lfNx * lfInvLen;
            const f32 lfUnitNy = lfNy * lfInvLen;
            const f32 lfUnitNz = lfNz * lfInvLen;

            // A, B, D -- the three vertices relative to the sphere centre.
            const f32 lfAx = lfP0x - lfCx, lfAy = lfP0y - lfCy, lfAz = lfP0z - lfCz;
            const f32 lfBx = lfP1x - lfCx, lfBy = lfP1y - lfCy, lfBz = lfP1z - lfCz;
            const f32 lfDx = lfP2x - lfCx, lfDy = lfP2y - lfCy, lfDz = lfP2z - lfCz;

            const f32 lfA2 = lfAx * lfAx + lfAy * lfAy + lfAz * lfAz;
            const f32 lfB2 = lfBx * lfBx + lfBy * lfBy + lfBz * lfBz;
            const f32 lfD2 = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;

            const f32 lfAB = lfAx * lfBx + lfAy * lfBy + lfAz * lfBz;
            const f32 lfBD = lfBx * lfDx + lfBy * lfDy + lfBz * lfDz;
            const f32 lfDA = lfDx * lfAx + lfDy * lfAy + lfDz * lfAz;

            // --- the plane axis (0x8283FE28 / 0x8283FF48 / 0x8283FF68) --------
            const f32 lfPlaneDistance = lfAx * lfUnitNx + lfAy * lfUnitNy + lfAz * lfUnitNz;
            const bool lbPlaneSeparates = (lfPlaneDistance * lfPlaneDistance) > lfR2;

            // --- "is any vertex inside the sphere?" (0x8283FE54..FE84) --------
            const bool lbAnyVertexInside = (lfR2 > lfA2) || (lfR2 > lfB2) || (lfR2 > lfD2);

            // --- the three vertex axes ---------------------------------------
            const bool lbSepVertexA = (lfAB > lfA2) && (lfDA > lfA2);   // 0x8283FF30
            const bool lbSepVertexB = (lfAB > lfB2) && (lfBD > lfB2);   // 0x8283FE88
            const bool lbSepVertexD = (lfDA > lfD2) && (lfBD > lfD2);   // 0x8283FF34

            // The console's minimum cascade, with its fourth arm exactly as
            // shipped (see the banner: it names Bsep where D is the minimum).
            const bool lbBGreaterThanA = (lfB2 > lfA2);   // v7
            const bool lbAGreaterThanD = (lfA2 > lfD2);   // v6
            const bool lbDGreaterThanB = (lfD2 > lfB2);   // v5

            const bool lbVertexSeparates =
                  ( lbBGreaterThanA && !lbAGreaterThanD && lbSepVertexA)    // 0x8283FF44
               || ( lbBGreaterThanA &&  lbAGreaterThanD && lbSepVertexD)    // 0x8283FF4C
               || (!lbBGreaterThanA &&  lbDGreaterThanB && lbSepVertexB)    // 0x8283FF54
               || (!lbBGreaterThanA && !lbDGreaterThanB && lbSepVertexB);   // 0x8283FF5C ⛔ bug

            // --- the three edge axes -----------------------------------------
            // For edge (X,Y) with third vertex Z:  G = x2*y2 - xy*xy
            //   sep = ( xz*(y2-xy) + yz*(x2-xy) > G ) && ( G > r^2 * |X-Y|^2 )
            const f32 lfGbd = lfB2 * lfD2 - lfBD * lfBD;                 // 0x8283FEB4
            const f32 lfGda = lfD2 * lfA2 - lfDA * lfDA;                 // 0x8283FEB8
            const f32 lfGab = lfA2 * lfB2 - lfAB * lfAB;                 // 0x8283FECC

            const f32 lfLenSqBD = (lfB2 - lfBD) + (lfD2 - lfBD);         // 0x8283FEC8
            const f32 lfLenSqDA = (lfD2 - lfDA) + (lfA2 - lfDA);         // 0x8283FED4
            const f32 lfLenSqAB = (lfA2 - lfAB) + (lfB2 - lfAB);         // 0x8283FEFC

            const bool lbSepEdgeBD = ((lfAB * (lfD2 - lfBD) + lfDA * (lfB2 - lfBD)) > lfGbd)
                                  && (lfGbd > lfR2 * lfLenSqBD);         // 0x8283FEF8
            const bool lbSepEdgeDA = ((lfBD * (lfA2 - lfDA) + lfAB * (lfD2 - lfDA)) > lfGda)
                                  && (lfGda > lfR2 * lfLenSqDA);         // 0x8283FF04
            const bool lbSepEdgeAB = ((lfDA * (lfB2 - lfAB) + lfBD * (lfA2 - lfAB)) > lfGab)
                                  && (lfGab > lfR2 * lfLenSqAB);         // 0x8283FF64

            const bool lbEdgeSeparates = lbSepEdgeAB || lbSepEdgeBD || lbSepEdgeDA;

            // --- the fold (0x8283FF78..0x8283FF88) ----------------------------
            const bool lbSeparated =
                lbPlaneSeparates
             || (!lbAnyVertexInside && (lbVertexSeparates || lbEdgeSeparates));

            if (!lbSeparated)
            {
                // `vnot v1, v0`: an intersecting lane is all-ones, which is what
                // the caller's `vcmpeqfp` against zero distinguishes.
                std::memcpy(&lpaResultLane[liLane], &luTrueLane, sizeof(u32));
            }
        }

        return lResult;
    }

    // ------------------------------------------------------------------------
    // ExtractTriangle4ListIntersectingSphere @0x82844C80 (602)
    //
    // The whole body, in four blocks that differ only in trip count and lane
    // population. Nothing here is math; the 602 instructions are the header
    // decode, four AoS->SoA transposes and the per-lane dispatch.
    //
    //   lbz r11, 0x1B(soup)                     mu8NumQuads
    //   lbz r10, 0x1A(soup)                     mu8NumPolygons
    //   srwi  r9,  r11, 1      -> r28  = numQuads >> 1     quad PAIRS   (4 tris)
    //   subf  r10, r11, r10    -> numTris = numPolys - numQuads
    //   extrwi r11, r10, 14,16 -> r20  = numTris  >> 2     tri QUADS    (4 tris)
    //   mullw r9,  r28, 0xFFFE -> r22  = numQuads & 1      the odd quad (2 tris)
    //   mullw r11, r20, 0xFFFC -> r19  = numTris  & 3      the odd tris (1 each)
    // (0xFFFE and 0xFFFC are -2 and -4 in 16 bits; these are remainders, not
    //  magic numbers.)
    //
    // Quads come first in the polygon array and each is split into TWO triangles
    // along its strip diagonal. CgsPolygonSoup.h:25 records, measured over
    // WORLDCOL.BIN, that the four indices run 0-1-3-2 around the perimeter; the
    // asm splits them as (V0,V1,V2) and (V3,V2,V1), which is the same pair.
    // ⭐ THE DIAGONAL IS NOT A REAL EDGE, and the console says so with a
    // sentinel: both split triangles get `vcfsx(vspltisw 2, 0)` == 2.0f as their
    // MIDDLE edge cosine (0x82844E64 / 0x82844EC4 / 0x82844F24 / 0x82844F84),
    // which is outside any legal cosine. Independent corroboration that this is
    // deliberate: CgsTriangle4.cpp's AOSTriangle::IsValid checks cosine 0 and 2
    // against (-1.1, 1.1) but cosine 1 against (-1.1, 2.0999999) -- a window
    // widened for exactly this value, and that literal was read out of a
    // different function in a different TU months ago.
    //
    // Both triangles of a quad carry the quad's own surface tag (`lwz r29, 0(poly)`
    // is loaded once per quad and passed for both lanes).
    // ------------------------------------------------------------------------
    namespace
    {
        // The unpacked-vertex scratch. A vertex index is a u8 and 0xFF is the
        // "no vertex" sentinel, so 256 entries covers the domain exactly; the PS3
        // twin's frame confirms the same 4096 bytes (`_BYTE v461[4096]`).
        const s32 KI_MAX_POLYGON_SOUP_VERTICES = 256;

        // `vcfsx v5, vspltisw(2), 0` -- the quad-diagonal edge-cosine sentinel.
        const f32 KF_QUAD_DIAGONAL_EDGE_COSINE = 2.0f;

        inline VecFloat SplatVecFloat(f32 lfValue)
        {
            VecFloat lValue;
            lValue.x = lfValue; lValue.y = lfValue;
            lValue.z = lfValue; lValue.w = lfValue;
            return lValue;
        }

        // One SoA batch under construction: four triangles' worth of AoS vertices
        // plus the transposed component arrays TestSphereTriangle4SOA takes.
        struct ExtractBatch
        {
            Vector3  maV0[4];
            Vector3  maV1[4];
            Vector3  maV2[4];
            VecFloat maEdge0[4];
            VecFloat maEdge1[4];
            VecFloat maEdge2[4];
            u32      mauTag[4];
            s32      miCount;

            void Reset() { miCount = 0; }

            void Push(const Vector3& lrV0, const Vector3& lrV1, const Vector3& lrV2,
                      const VecFloat& lrE0, const VecFloat& lrE1, const VecFloat& lrE2,
                      u32 luTag)
            {
                const s32 liLane = miCount;
                maV0[liLane] = lrV0; maV1[liLane] = lrV1; maV2[liLane] = lrV2;
                maEdge0[liLane] = lrE0; maEdge1[liLane] = lrE1; maEdge2[liLane] = lrE2;
                mauTag[liLane] = luTag;
                ++miCount;
            }
        };

        // The `vmrghw/vmrglw` cascade, written as what it is: a transpose. The
        // console fills all four lanes even when only one or two carry a distinct
        // triangle (it duplicates the last one through self-merges), then tests
        // only the populated lanes -- which is exactly what looping to
        // lrBatch.miCount does.
        inline void RunBatch(ExtractBatch& lrBatch,
                             const Sphere& lSphere,
                             Triangle4* lpTriangle4Buffer,
                             s32 liBufferSize,
                             IntermediateTriangles* lpAccumulator,
                             s32& lriWriteIndex,
                             s32& lriLaneCount,
                             s32& lriOverrun)
        {
            if (lrBatch.miCount == 0)
            {
                return;
            }

            Vector4 lP0X, lP0Y, lP0Z, lP1X, lP1Y, lP1Z, lP2X, lP2Y, lP2Z;
            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                // Lanes beyond miCount replay the last triangle, matching the
                // console's self-merge duplication; their result is discarded.
                const s32 liSource = (liLane < lrBatch.miCount) ? liLane : (lrBatch.miCount - 1);
                (&lP0X.x)[liLane] = lrBatch.maV0[liSource].x;
                (&lP0Y.x)[liLane] = lrBatch.maV0[liSource].y;
                (&lP0Z.x)[liLane] = lrBatch.maV0[liSource].z;
                (&lP1X.x)[liLane] = lrBatch.maV1[liSource].x;
                (&lP1Y.x)[liLane] = lrBatch.maV1[liSource].y;
                (&lP1Z.x)[liLane] = lrBatch.maV1[liSource].z;
                (&lP2X.x)[liLane] = lrBatch.maV2[liSource].x;
                (&lP2Y.x)[liLane] = lrBatch.maV2[liSource].y;
                (&lP2Z.x)[liLane] = lrBatch.maV2[liSource].z;
            }

            const Triangle4::Mask4 lHitMask =
                TestSphereTriangle4SOA(lSphere, lP0X, lP0Y, lP0Z,
                                                lP1X, lP1Y, lP1Z,
                                                lP2X, lP2Y, lP2Z);

            for (s32 liLane = 0; liLane < lrBatch.miCount; ++liLane)
            {
                // `vspltw v0, mask, lane ; vcmpeqfp. v0, v0, 0 ; mfocrf ; bne`
                // -- a lane that compares equal to zero is a miss and is skipped.
                if ((&lHitMask.x)[liLane] == 0.0f)
                {
                    continue;
                }

                lriOverrun += PolySoupAddToTriangleBuffer(lpTriangle4Buffer, liBufferSize,
                                                          lpAccumulator,
                                                          lrBatch.maV0[liLane],
                                                          lrBatch.maV1[liLane],
                                                          lrBatch.maV2[liLane],
                                                          lrBatch.maEdge0[liLane],
                                                          lrBatch.maEdge1[liLane],
                                                          lrBatch.maEdge2[liLane],
                                                          lrBatch.mauTag[liLane],
                                                          lriWriteIndex, lriLaneCount);
            }

            lrBatch.Reset();
        }
    }

    s32 ExtractTriangle4ListIntersectingSphere(const PolygonSoup& lPolygonSoup,
                                               const Sphere&      lSphere,
                                               Triangle4*         lpTriangle4Buffer,
                                               s32                liBufferSize,
                                               s32*               lpiOutOverun)
    {
        // --- the header decode --------------------------------------------------
        const s32 liNumPolygons = lPolygonSoup.mu8NumPolygons;   // +0x1A
        const s32 liNumQuads    = lPolygonSoup.mu8NumQuads;      // +0x1B
        const s32 liNumTriangles = liNumPolygons - liNumQuads;

        // --- the two stack scratch buffers -------------------------------------
        // ⚠️ alignas is explicit here on purpose. The standing hazard from the
        // previous wave is that a widened pointer silently moved a 16-aligned
        // buffer and the first aligned load faulted, with no gate able to see it.
        // Vector3 already carries alignas(16), but the intent is written down.
        alignas(16) Vector3 laVertices[KI_MAX_POLYGON_SOUP_VERTICES];
        alignas(16) IntermediateTriangles lAccumulator;   // var_17C0

        UnpackPolygonSoupVertices(laVertices, lPolygonSoup);

        // `GetPolygon(0)` then `addi r31, r31, 0xC` per polygon.
        const PolygonSoupPoly* lpPoly =
            reinterpret_cast<const PolygonSoupPoly*>(lPolygonSoup.GetPolygon(0));

        s32 liWriteIndex = 0;   // var_18D0 -- also the return value
        s32 liLaneCount  = 0;   // var_18CC
        s32 liOverrun    = 0;   // r30

        ExtractBatch lBatch;
        lBatch.Reset();

        // --- BLOCK 1 + 2: the quads, in pairs, then the odd one ----------------
        // Loop A (0x82844D34..0x82844FD0) takes two quads per iteration and fills
        // all four lanes; block 2 (0x82844FE0..0x82845150) handles numQuads & 1
        // and fills only lanes 0 and 1.
        for (s32 liQuad = 0; liQuad < liNumQuads; ++liQuad, ++lpPoly)
        {
            VecFloat lEdge0, lEdge1, lEdge2, lEdge3;
            lpPoly->LoadEdgeCosines(lEdge0, lEdge1, lEdge2, lEdge3);

            const Vector3& lrV0 = laVertices[lpPoly->mau8VertexIndex[0]];
            const Vector3& lrV1 = laVertices[lpPoly->mau8VertexIndex[1]];
            const Vector3& lrV2 = laVertices[lpPoly->mau8VertexIndex[2]];
            const Vector3& lrV3 = laVertices[lpPoly->mau8VertexIndex[3]];
            const u32 luTag = lpPoly->muSurfaceTag;

            const VecFloat lDiagonal = SplatVecFloat(KF_QUAD_DIAGONAL_EDGE_COSINE);

            // triangle 0 = (V0, V1, V2): edges V0->V1 (out0), V1->V2 (DIAGONAL),
            //                                  V2->V0 (out2)
            lBatch.Push(lrV0, lrV1, lrV2, lEdge0, lDiagonal, lEdge2, luTag);
            // triangle 1 = (V3, V2, V1): edges V3->V2 (out3), V2->V1 (DIAGONAL),
            //                                  V1->V3 (out1)
            lBatch.Push(lrV3, lrV2, lrV1, lEdge3, lDiagonal, lEdge1, luTag);

            if (lBatch.miCount == 4)
            {
                RunBatch(lBatch, lSphere, lpTriangle4Buffer, liBufferSize,
                         &lAccumulator, liWriteIndex, liLaneCount, liOverrun);
            }
        }
        RunBatch(lBatch, lSphere, lpTriangle4Buffer, liBufferSize,
                 &lAccumulator, liWriteIndex, liLaneCount, liOverrun);

        // --- BLOCK 3 + 4: the triangles, four at a time, then the remainder ----
        // Loop C (0x82845164..0x82845490) and block 4 (0x828454A8..0x828455A8).
        // A triangle record carries KU8_POLYGON_NO_VERTEX (0xFF) in vertex slot 3
        // and the console never reads it here; nor does this.
        for (s32 liTriangle = 0; liTriangle < liNumTriangles; ++liTriangle, ++lpPoly)
        {
            VecFloat lEdge0, lEdge1, lEdge2, lEdge3;
            lpPoly->LoadEdgeCosines(lEdge0, lEdge1, lEdge2, lEdge3);
            (void)lEdge3;   // the 4th output is written and discarded (r7/var_17F0)

            lBatch.Push(laVertices[lpPoly->mau8VertexIndex[0]],
                        laVertices[lpPoly->mau8VertexIndex[1]],
                        laVertices[lpPoly->mau8VertexIndex[2]],
                        lEdge0, lEdge1, lEdge2, lpPoly->muSurfaceTag);

            if (lBatch.miCount == 4)
            {
                RunBatch(lBatch, lSphere, lpTriangle4Buffer, liBufferSize,
                         &lAccumulator, liWriteIndex, liLaneCount, liOverrun);
            }
        }
        RunBatch(lBatch, lSphere, lpTriangle4Buffer, liBufferSize,
                 &lAccumulator, liWriteIndex, liLaneCount, liOverrun);

        // --- the tail (0x828455AC..0x828455D4) ---------------------------------
        liOverrun += PolySoupFinishTriangleBuffer(lpTriangle4Buffer, liBufferSize,
                                                  &lAccumulator, liWriteIndex, liLaneCount);

        if (lpiOutOverun)
        {
            *lpiOutOverun = liOverrun;
        }

        return liWriteIndex;
    }
}
