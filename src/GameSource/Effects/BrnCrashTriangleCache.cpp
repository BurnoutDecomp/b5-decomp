#include "types.hpp"
#include "GameSource/Effects/BrnCrashTriangleCache.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"   // CgsGeometric::Triangle4 (the incoming batches)
#include "SharedClasses/World/BrnCollisionTag.h"                        // KU_COLLISION_MASK_SURFACE_ID / KU8_COLLISION_INVISIBLE_SURFACE_ID

#include <cmath>     // std::sqrt (the vrsqrtefp + Newton lowering in CollideWithTriangleCache)
#include <cstring>   // std::memcpy (raw lane words)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnEffects::BrnCrashTriangleCache::Construct                  @ 0x8227B240
//   BrnEffects::BrnCrashTriangleCache::InsertTriangleIntoCache     @ 0x8227B2D0
//   BrnEffects::BrnCrashTriangleCache::CheckForDuplicateTriangles  @ 0x822847B0   (PS3 0xEECD0)
//   BrnEffects::BrnCrashTriangleCache::CollideWithTriangleCache    @ 0x822849B8   (PS3 0xE0ADC)
//   BrnEffects::BrnCrashTriangleCache::AddTriangles                @ 0x8228CDA8   (PS3 0xEF58C;
//       CalculateHashForPackedTriangle, DWARF .cpp:117, is inlined into it on both consoles)
//
// ============================================================================================
// CRASH PARITY FX-FX (2026-09-23): G09-D1..D5. What the previous body did, and why it mattered
// once the cache has a reader (CollideWithTriangleCache, the crash-debris collision):
//
//  D1  AddTriangles took a local 0xA0-byte `BrnEffects::Triangle4` fork (9 vertex lanes, then
//      "mSurfaceTags" at +0x90) and EffectsModule reinterpret_cast the real 0xE0-byte
//      CgsGeometric::Triangle4 array to it. Batch k was read from base + 0xA0*k (built exe
//      `add rbx, 0xa0` @0x1400F5A86): every batch after the first was assembled from the
//      previous batch's tag/edge-cosine words, and the last ~29% of the input was never read.
//      Console: r31 = a2 + 0x20 @0x8228CDC0 ... `addi r31, r31, 0xE0` @0x8228D1F4 (PS3 +224).
//  D2  The skip test read the fork's +0x90 -- the real mValidMasks -- through a NUMERIC
//      float->u32 cast (`cvttss2si` @0x1400F59F6). A valid lane is 0xFFFFFFFF, a NaN, which
//      converts to 0, and the invented `tag == 0` test then skipped EVERY real triangle.
//      Console, per lane: the tags (T+0xA0) are read as raw words (lvx128 @0x8228D09C, stvx128
//      to the stack, `lwz` @0x8228D0B4/D104/D14C/D194); surface id ((tag >> 16) & 0x3F0) >> 4
//      == 17 skips (srwi 16 ; extrwi 12,16 ; clrlwi 26 ; `cmplwi 0x11` @0x8228D0C4); otherwise
//      mValidMasks lane == 0.0f skips (vspltw ; vcmpeqfp. against vspltisw 0 @0x8228D0D8..E8).
//      There is no tag == 0 test.
//  D3  The duplicate hash was an invented nine-coordinate sum. Console (decoded from the raw
//      words; IDA prints classic vmaddfp in field order D,A,B,C => D = A*C + B):
//      (V2X + V2Y) * (V1Y * V1Z + (V2X + V2Y)) + V2Z -- vaddfp @0x8228CF2C, vmaddfp @0x8228CF70
//      (0x11AD532E), vmaddfp @0x8228D060 (0x100A4B6E); PS3 0xEF7E4..0xEF870 is the same
//      expression unfused. V0X/V0Y/V0Z/V1X take no part.
//  D4  The search covered packs [0, count (+1 if a pack was part-filled)) and, in the pack being
//      filled, only lanes < mnNextComponentToFill. Console: K = (count == 48) ? 48 : count + 1
//      (0x822847F0 / 0x8228481C / 0x8228483C / 0x82284854); packs K..0, K+1 iterations, all four
//      lanes (0x82284890..0x822848C0; PS3 ctr = K+1). Only the COUNTERS are reset between crashes
//      (EffectsModule 0x82296F30 / 0x82296FBC / 0x822972D0), so a new crash compares against the
//      previous crash's hashes, and after a wrap the still-live old lanes of the pack being
//      overwritten are compared. K == 48 (count >= 47) reads maPackedTriangles[48].mVertexHash,
//      i.e. the counter block at +0x1E00 -- reproduced below without the out-of-bounds read.
//  D5  Triangle i was compared after triangles 0..i-1 of the same batch had been inserted. The
//      console accumulates all four found-masks against the PRE-insert cache in the one loop
//      (vor/vor128 @0x822848B0..BC) and only then inserts (bl @0x82284904 / 0x82284938 /
//      0x8228496C / 0x822849A0), so two identical triangles in one batch are both kept.
//  D6  CollideWithTriangleCache (the cache's only reader) had no declaration and no body. It is
//      reconstructed below from 0x822849B8..0x82284EE4 (see its banner).
// Every behaviour above was confirmed by executing the ARTIST instruction words themselves (a
// PPC/VMX128 emulation of 0x8228CDA8 -> 0x822847B0 -> 0x8227B2D0, and of 0x822849B8);
// tests/FxFxCrashTriangleCache.cpp carries the resulting cases.
// ============================================================================================

namespace BrnEffects
{
    // (types + constants: BrnCrashTriangleCache.h)

    namespace
    {
        // Lane `luLane` of a Triangle4 member, as the float it is.
        inline f32 GetLane(const Vector4& lrVector, u32 luLane)
        {
            return (&lrVector.x)[luLane];
        }

        // Lane `luLane` of a Triangle4 member as a raw word. mSurfaceTags travels in a vector
        // register and is only ever stored and re-read as words (DWARF .cpp:235
        // `VectorIntrinsicUnion lSurfaceTagsIntrinsic`), never converted.
        inline u32 GetLaneBits(const Vector4& lrVector, u32 luLane)
        {
            u32 luBits;
            std::memcpy(&luBits, &(&lrVector.x)[luLane], sizeof(u32));
            return luBits;
        }

        // VecFloat(<member>.GetComponent(lane)): lane `luLane` of a Triangle4 member splatted
        // across all four lanes of a packed vector (lvsl 4*lane ; vspltw 0 ; vperm, e.g.
        // 0x8228CDF0 / 0x8228CE18 for lane 0 of mVertex0Z).
        inline void SplatLane(Vector4Lane& lrDestination, const Vector4& lrSource, u32 luLane)
        {
            const f32 lfValue = GetLane(lrSource, luLane);
            for (u32 luComponent = 0; luComponent < KU_TRIANGLES_PER_PACK; ++luComponent)
            {
                lrDestination.SetComponent(luComponent, lfValue);
            }
        }

        // vcmpeqfp as the X360 VMX unit executes it: non-Java mode, so a denormal operand reads
        // as (signed) zero. Only observable on the counter-block row (see CheckForDuplicateTriangles)
        // and on hashes within 2^-126 of zero.
        inline f32 FlushDenormalToZero(f32 lfValue)
        {
            u32 luBits;
            std::memcpy(&luBits, &lfValue, sizeof(u32));
            if ((luBits & 0x7F800000u) == 0u)
            {
                luBits &= 0x80000000u;
            }
            f32 lfFlushed;
            std::memcpy(&lfFlushed, &luBits, sizeof(u32));
            return lfFlushed;
        }

        inline bool CompEqualVmx(f32 lfA, f32 lfB)
        {
            return FlushDenormalToZero(lfA) == FlushDenormalToZero(lfB);
        }

        // The two tolerances of the segment-vs-triangle test in CollideWithTriangleCache; the PS3
        // build names them (TOC loads @0xE0C68 / @0xE0C7C). On X360 both are .bss splats written by
        // CRT thunks, so the image word 0 is NOT their value:
        //   rw::collision::RTINTSECEPSILON_VecFloat  0x82FACC00 <- CRT 0x82C4AF10 splats flt_8200D5F0
        //       == 0x322BCC77 == 1.0e-8f. A lane can only hit when its determinant exceeds it:
        //       the test is one-sided, back faces never hit.
        //   rw::collision::RTINTSECEDGEEPS_VecFloat  0x82FACBE0 <- CRT 0x82C4AF38 splats flt_82004884
        //       == 0x3727C5AC == 1.0e-5f. The barycentric / segment slack, scaled by the determinant.
        const f32 KF_RTINTSECEPSILON = 1.0e-8f;
        const f32 KF_RTINTSECEDGEEPS = 1.0e-5f;
    }

    void BrnCrashTrianglePackedFormat::Clear()
    {
        mVertexHash.Clear();
        mVertex0X.Clear();
        mVertex0Y.Clear();
        mVertex0Z.Clear();
        mVertex1X.Clear();
        mVertex1Y.Clear();
        mVertex1Z.Clear();
        mVertex2X.Clear();
        mVertex2Y.Clear();
        mVertex2Z.Clear();
    }

    void BrnCrashTrianglePackedFormat::SetScalarTriangle(
        const BrnCrashTrianglePackedFormat& lTriangle,
        u32 luDestinationComponent)
    {
        mVertexHash.SetComponent(luDestinationComponent, lTriangle.mVertexHash.GetComponent(0));
        mVertex0X.SetComponent(luDestinationComponent, lTriangle.mVertex0X.GetComponent(0));
        mVertex0Y.SetComponent(luDestinationComponent, lTriangle.mVertex0Y.GetComponent(0));
        mVertex0Z.SetComponent(luDestinationComponent, lTriangle.mVertex0Z.GetComponent(0));
        mVertex1X.SetComponent(luDestinationComponent, lTriangle.mVertex1X.GetComponent(0));
        mVertex1Y.SetComponent(luDestinationComponent, lTriangle.mVertex1Y.GetComponent(0));
        mVertex1Z.SetComponent(luDestinationComponent, lTriangle.mVertex1Z.GetComponent(0));
        mVertex2X.SetComponent(luDestinationComponent, lTriangle.mVertex2X.GetComponent(0));
        mVertex2Y.SetComponent(luDestinationComponent, lTriangle.mVertex2Y.GetComponent(0));
        mVertex2Z.SetComponent(luDestinationComponent, lTriangle.mVertex2Z.GetComponent(0));
    }

    void BrnCrashTriangleCache::Construct()
    {
        for (u32 lnCacheLoop = 0; lnCacheLoop < KU_MAX_NUMBER_PACKED_TRIANGLES; ++lnCacheLoop)
        {
            maPackedTriangles[lnCacheLoop].Clear();
        }

        mnNumberOfPackedTriangles = 0;
        mnNextPackedTriangleToFill = 0;
        mnNextComponentToFill = 0;
    }

    // @ 0x8228CDA8 (DWARF .cpp:163). One pass per Triangle4 batch: unpack the four triangles
    // into splatted packed triangles and hash them, decide per lane whether the triangle may
    // enter the cache at all, then hand the four to CheckForDuplicateTriangles.
    void BrnCrashTriangleCache::AddTriangles(const CgsGeometric::Triangle4* lpInTriangles, u32 lnNum4Triangles)
    {
        const CgsGeometric::Triangle4* lpIncomingTriangles = lpInTriangles;
        BrnCrashTrianglePackedFormat laPackedTriangles[KU_TRIANGLES_PER_PACK];
        bool labMaskValues[KU_TRIANGLES_PER_PACK];

        for (u32 lnTriangleLoop = 0; lnTriangleLoop < lnNum4Triangles; ++lnTriangleLoop)
        {
            // The nine vertex members at T+0x00..T+0x80 (r31 = T+0x20: loads at r31-0x20 .. r31+0x60).
            for (u32 luComponent = 0; luComponent < KU_TRIANGLES_PER_PACK; ++luComponent)
            {
                BrnCrashTrianglePackedFormat& lrPackedTriangle = laPackedTriangles[luComponent];
                SplatLane(lrPackedTriangle.mVertex0X, lpIncomingTriangles->mVertex0X, luComponent);
                SplatLane(lrPackedTriangle.mVertex0Y, lpIncomingTriangles->mVertex0Y, luComponent);
                SplatLane(lrPackedTriangle.mVertex0Z, lpIncomingTriangles->mVertex0Z, luComponent);
                SplatLane(lrPackedTriangle.mVertex1X, lpIncomingTriangles->mVertex1X, luComponent);
                SplatLane(lrPackedTriangle.mVertex1Y, lpIncomingTriangles->mVertex1Y, luComponent);
                SplatLane(lrPackedTriangle.mVertex1Z, lpIncomingTriangles->mVertex1Z, luComponent);
                SplatLane(lrPackedTriangle.mVertex2X, lpIncomingTriangles->mVertex2X, luComponent);
                SplatLane(lrPackedTriangle.mVertex2Y, lpIncomingTriangles->mVertex2Y, luComponent);
                SplatLane(lrPackedTriangle.mVertex2Z, lpIncomingTriangles->mVertex2Z, luComponent);
                CalculateHashForPackedTriangle(&lrPackedTriangle);
            }

            // Per lane: an invisible-surface triangle (surface id 17) is never cached; otherwise a
            // lane whose valid mask is zero is not a triangle. (DWARF .cpp:235-240 locals.)
            for (u32 luComponent = 0; luComponent < KU_TRIANGLES_PER_PACK; ++luComponent)
            {
                const u32 luSurfaceTag = GetLaneBits(lpIncomingTriangles->mSurfaceTags, luComponent);    // T+0xA0
                const u16 luMaterialId = static_cast<u16>(luSurfaceTag >> 16);                             // srwi 16
                const u8  lu8SurfaceId =
                    static_cast<u8>((luMaterialId & BrnWorld::KU_COLLISION_MASK_SURFACE_ID) >> 4);       // extrwi 12,16 ; clrlwi 26

                bool lbSkipTriangle = (lu8SurfaceId == BrnWorld::KU8_COLLISION_INVISIBLE_SURFACE_ID);     // cmplwi 0x11
                if (!lbSkipTriangle)
                {
                    lbSkipTriangle = CompEqualVmx(GetLane(lpIncomingTriangles->mValidMasks, luComponent), 0.0f);   // T+0x90
                }
                labMaskValues[luComponent] = lbSkipTriangle;
            }

            CheckForDuplicateTriangles(laPackedTriangles, labMaskValues);
            ++lpIncomingTriangles;                                                                 // +0xE0
        }
    }

    // @ 0x822847B0 (DWARF .cpp:343). Seed each incoming triangle's found-mask from its skip byte
    // (true -> all-ones: cntlzw/rlwinm/xori selecting row 1 of the {0 | ~0} table at 0x8327F240,
    // filled by CRT thunk 0x82C74368), OR in every lane of every searched pack that carries the
    // same hash -- all four triangles against the cache as it stands BEFORE this batch inserts
    // anything -- then insert, in order, each triangle whose mask has no lane set (CompAnyTrue
    // through the gather-MSB permute 0x8327F110 = {0x0004080C} x4, CRT thunk 0x82C740C0).
    // The per-lane masks are collapsed to one bool per triangle: their only reader is CompAnyTrue.
    void BrnCrashTriangleCache::CheckForDuplicateTriangles(
        BrnCrashTrianglePackedFormat* lpaPackedTriangles,
        bool* lpbAddTriangleBool)
    {
        bool labResultsMask[KU_TRIANGLES_PER_PACK];
        for (u32 luTriangle = 0; luTriangle < KU_TRIANGLES_PER_PACK; ++luTriangle)
        {
            labResultsMask[luTriangle] = lpbAddTriangleBool[luTriangle];
        }

        const s32 lnNumberPackedTriangles =
            (mnNumberOfPackedTriangles == KU_MAX_NUMBER_PACKED_TRIANGLES)
                ? static_cast<s32>(KU_MAX_NUMBER_PACKED_TRIANGLES)
                : static_cast<s32>(mnNumberOfPackedTriangles) + 1;

        for (s32 lnCacheLoop = lnNumberPackedTriangles; lnCacheLoop >= 0; --lnCacheLoop)
        {
            Vector4Lane lCachedHashes;
            if (lnCacheLoop < static_cast<s32>(KU_MAX_NUMBER_PACKED_TRIANGLES))
            {
                lCachedHashes = maPackedTriangles[lnCacheLoop].mVertexHash;
            }
            else
            {
                // lnCacheLoop == 48, reached whenever mnNumberOfPackedTriangles >= 47. The console
                // loads maPackedTriangles[48].mVertexHash, which is this+0x1E00: the counter block
                // {mnNumberOfPackedTriangles, mnNextPackedTriangleToFill, mnNextComponentToFill,
                // tail pad} compared as floats. The counters are 0..48, i.e. zero or denormal, so
                // under non-Java mode the row matches exactly a +-0 hash. The console's tail pad
                // (+0x1E0C of its 0x1E10-byte struct) is never written; it is taken as 0 here --
                // the one assumption of this row. Rebuilt from the members rather than read past
                // the end of maPackedTriangles.
                const u32 lauCounterBlock[4] =
                {
                    mnNumberOfPackedTriangles, mnNextPackedTriangleToFill, mnNextComponentToFill, 0u
                };
                std::memcpy(lCachedHashes.mafValues, lauCounterBlock, sizeof(lauCounterBlock));
            }

            for (u32 luTriangle = 0; luTriangle < KU_TRIANGLES_PER_PACK; ++luTriangle)
            {
                const Vector4Lane& lrIncomingHash = lpaPackedTriangles[luTriangle].mVertexHash;
                for (u32 luLane = 0; luLane < KU_TRIANGLES_PER_PACK; ++luLane)
                {
                    if (CompEqualVmx(lCachedHashes.GetComponent(luLane), lrIncomingHash.GetComponent(luLane)))
                    {
                        labResultsMask[luTriangle] = true;
                    }
                }
            }
        }

        for (u32 luTriangle = 0; luTriangle < KU_TRIANGLES_PER_PACK; ++luTriangle)
        {
            if (!labResultsMask[luTriangle])
            {
                InsertTriangleIntoCache(&lpaPackedTriangles[luTriangle]);
            }
        }
    }

    // ============================================================================================
    // CollideWithTriangleCache  @ 0x822849B8  (332 insns; PS3 0xE0ADC; DWARF .cpp:467)
    //
    // Caller: BrnParticle::Native::BrnDebrisArrayLite::UpdateBucket @0x82C08410 (an export hole,
    // read with ppcdis): `bl 0x822849B8` @0x82C08884 with r3 = the dispatch-side cache copy, r4 =
    // sp+0xD0 (the bucket's debris lines), r5 = the line count; gated on a non-zero line count, on
    // BrnDebrisArrayLite::mbCollisionEnabled and on a non-zero cache count (0x82C08850..0x82C0886C).
    //
    // For each searched pack -- the same K as CheckForDuplicateTriangles, (count == 48) ? 48 :
    // count + 1, but walked 0..K-1 (`ble` exit @0x822849D4, `addi r10, r10, 0xA0` @0x82284ED8), so
    // the part-filled pack and nothing past the array -- and for each line (stride 0x30), the four
    // lanes run the segment-vs-triangle test the DWARF names rw::collision::TriangleLineSegIntersect
    // (inlined; outputs lOutDeterminant / lOutBarycentricParams1 / lOutBarycentricParams2 /
    // lOutLineParams, lIntersectMask), register-for-register:
    //     D = end - start                       0x82284A60
    //     e1 = V1 - V0, e2 = V2 - V0            0x82284A74..0x82284A9C
    //     P = D x e2                            0x82284AC8..0x82284AF0
    //     T = start - V0                        0x82284AF8..0x82284B00
    //     det = e1.P, u = T.P                   0x82284B1C / B24 / B28 / B40
    //     Q = T x e1                            0x82284B08..0x82284B3C
    //     v = D.Q, t = e2.Q                     0x82284B54 / B58 / B68 / B80
    //     lo = -det * RTINTSECEDGEEPS           vxor sign mask (vslw of all-ones) 0x82284B38, 0x82284B44
    //     hi = det - lo                         0x82284B50
    //     hit = det > RTINTSECEPSILON && u >= lo && !(u > hi) && v >= lo && !(u + v > hi)
    //           && t >= lo && !(t > hi)         0x82284B64..0x82284BB4
    // (the epsilon splat reaches its compare through a vperm/vperm/vsldoi reshuffle, 0x82284B5C /
    // B6C / B74, with the image permute controls 0x82CDA400 / 0x82CDA3C0 -- all lanes stay 1e-8).
    // No lane hit -> next line (CompAnyTrue through 0x8327F110, `bne` @0x82284BE4). Otherwise every
    // lane's unit normal cross(V0 - V1, V0 - V2) (vmrghw/vmrglw transpose, then the yzx-permute cross
    // 0x82284C64..0x82284D2C, vrsqrtefp + two Newton steps) and parameter t / det (vrefp + two
    // Newton steps, vmulfp @0x82284D64) are formed, and lanes 0 -> 3 in order replace the line's
    // {normal, w} when the lane hit and !(param >= w) (vcmpgefp ; vnot ; vand ; vsel ; vrlimi128
    // cascade 0x82284D80..0x82284EBC), so the nearest hit wins and a tie keeps the earlier one.
    // Upper bounds are `!(x > hi)`, as shipped (vcmpgtfp + vnot): a NaN passes them and fails the
    // lower ones. PC lowering: 1/sqrt and / are exact where the console refines estimates -- the
    // project's standing choice (BrnVehicleManager_PlayerStuck.cpp, ContactGeneratorJob.cpp).
    // ============================================================================================
    void BrnCrashTriangleCache::CollideWithTriangleCache(
        BrnCrashLineTriangleCacheFormat* lpLinesToTest,
        u32 luNumberLines) const
    {
        const s32 lnNumPackedTris =
            (mnNumberOfPackedTriangles == KU_MAX_NUMBER_PACKED_TRIANGLES)
                ? static_cast<s32>(KU_MAX_NUMBER_PACKED_TRIANGLES)
                : static_cast<s32>(mnNumberOfPackedTriangles) + 1;

        for (s32 liTriBatch = 0; liTriBatch < lnNumPackedTris; ++liTriBatch)
        {
            const BrnCrashTrianglePackedFormat* lpTriBatch = &maPackedTriangles[liTriBatch];
            BrnCrashLineTriangleCacheFormat* lpCurrentLine = lpLinesToTest;

            for (u32 luLineTest = 0; luLineTest < luNumberLines; ++luLineTest, ++lpCurrentLine)
            {
                const Vector3& lrStart = lpCurrentLine->mLineStartPosition;
                const f32 lfDeltaX = lpCurrentLine->mLineEndPos.x - lrStart.x;
                const f32 lfDeltaY = lpCurrentLine->mLineEndPos.y - lrStart.y;
                const f32 lfDeltaZ = lpCurrentLine->mLineEndPos.z - lrStart.z;

                f32  lafOutDeterminant[KU_TRIANGLES_PER_PACK];
                f32  lafOutLineParams[KU_TRIANGLES_PER_PACK];
                bool labIntersectMask[KU_TRIANGLES_PER_PACK];
                bool lbAnyIntersection = false;

                for (u32 luLane = 0; luLane < KU_TRIANGLES_PER_PACK; ++luLane)
                {
                    const f32 lfVertex0X = lpTriBatch->mVertex0X.GetComponent(luLane);
                    const f32 lfVertex0Y = lpTriBatch->mVertex0Y.GetComponent(luLane);
                    const f32 lfVertex0Z = lpTriBatch->mVertex0Z.GetComponent(luLane);

                    const f32 lfEdge1X = lpTriBatch->mVertex1X.GetComponent(luLane) - lfVertex0X;
                    const f32 lfEdge1Y = lpTriBatch->mVertex1Y.GetComponent(luLane) - lfVertex0Y;
                    const f32 lfEdge1Z = lpTriBatch->mVertex1Z.GetComponent(luLane) - lfVertex0Z;
                    const f32 lfEdge2X = lpTriBatch->mVertex2X.GetComponent(luLane) - lfVertex0X;
                    const f32 lfEdge2Y = lpTriBatch->mVertex2Y.GetComponent(luLane) - lfVertex0Y;
                    const f32 lfEdge2Z = lpTriBatch->mVertex2Z.GetComponent(luLane) - lfVertex0Z;

                    // P = D x e2
                    const f32 lfPX = (lfEdge2Z * lfDeltaY) - (lfEdge2Y * lfDeltaZ);
                    const f32 lfPY = (lfEdge2X * lfDeltaZ) - (lfEdge2Z * lfDeltaX);
                    const f32 lfPZ = (lfEdge2Y * lfDeltaX) - (lfEdge2X * lfDeltaY);

                    // T = start - V0 ; Q = T x e1
                    const f32 lfTX = lrStart.x - lfVertex0X;
                    const f32 lfTY = lrStart.y - lfVertex0Y;
                    const f32 lfTZ = lrStart.z - lfVertex0Z;
                    const f32 lfQX = (lfTY * lfEdge1Z) - (lfTZ * lfEdge1Y);
                    const f32 lfQY = (lfTZ * lfEdge1X) - (lfTX * lfEdge1Z);
                    const f32 lfQZ = (lfTX * lfEdge1Y) - (lfTY * lfEdge1X);

                    const f32 lfDeterminant        = (lfEdge1X * lfPX) + (lfEdge1Y * lfPY) + (lfEdge1Z * lfPZ);
                    const f32 lfBarycentricParams1 = (lfTX * lfPX) + (lfTY * lfPY) + (lfTZ * lfPZ);
                    const f32 lfBarycentricParams2 = (lfQX * lfDeltaX) + (lfQY * lfDeltaY) + (lfQZ * lfDeltaZ);
                    const f32 lfLineParams         = (lfEdge2X * lfQX) + (lfEdge2Y * lfQY) + (lfEdge2Z * lfQZ);

                    const f32 lfLowerBound = (-lfDeterminant) * KF_RTINTSECEDGEEPS;
                    const f32 lfUpperBound = lfDeterminant - lfLowerBound;

                    labIntersectMask[luLane] =
                        (lfDeterminant > KF_RTINTSECEPSILON)
                        && (lfBarycentricParams1 >= lfLowerBound) && !(lfBarycentricParams1 > lfUpperBound)
                        && (lfBarycentricParams2 >= lfLowerBound)
                        && !((lfBarycentricParams1 + lfBarycentricParams2) > lfUpperBound)
                        && (lfLineParams >= lfLowerBound) && !(lfLineParams > lfUpperBound);

                    lafOutDeterminant[luLane] = lfDeterminant;
                    lafOutLineParams[luLane]  = lfLineParams;
                    lbAnyIntersection = lbAnyIntersection || labIntersectMask[luLane];
                }

                if (!lbAnyIntersection)
                {
                    continue;
                }

                Vector3Plus& lrNormalPlusParam = lpCurrentLine->mLineIntersectNormalPlusLineParms;
                for (u32 luLane = 0; luLane < KU_TRIANGLES_PER_PACK; ++luLane)
                {
                    // lIsTriNNearest = hit && !(param >= w). The console forms every lane's param
                    // (and normal) before the cascade; only a hit lane can write, so a missed lane's
                    // division is skipped here.
                    if (!labIntersectMask[luLane])
                    {
                        continue;
                    }
                    const f32 lfLineParam = lafOutLineParams[luLane] / lafOutDeterminant[luLane];
                    if (lfLineParam >= lrNormalPlusParam.w)
                    {
                        continue;
                    }

                    // lTriNNormal: cross(V0 - V1, V0 - V2), normalised.
                    const f32 lfVertex0X = lpTriBatch->mVertex0X.GetComponent(luLane);
                    const f32 lfVertex0Y = lpTriBatch->mVertex0Y.GetComponent(luLane);
                    const f32 lfVertex0Z = lpTriBatch->mVertex0Z.GetComponent(luLane);
                    const f32 lfAX = lfVertex0X - lpTriBatch->mVertex1X.GetComponent(luLane);
                    const f32 lfAY = lfVertex0Y - lpTriBatch->mVertex1Y.GetComponent(luLane);
                    const f32 lfAZ = lfVertex0Z - lpTriBatch->mVertex1Z.GetComponent(luLane);
                    const f32 lfBX = lfVertex0X - lpTriBatch->mVertex2X.GetComponent(luLane);
                    const f32 lfBY = lfVertex0Y - lpTriBatch->mVertex2Y.GetComponent(luLane);
                    const f32 lfBZ = lfVertex0Z - lpTriBatch->mVertex2Z.GetComponent(luLane);

                    const f32 lfNormalX = (lfAY * lfBZ) - (lfAZ * lfBY);
                    const f32 lfNormalY = (lfAZ * lfBX) - (lfAX * lfBZ);
                    const f32 lfNormalZ = (lfAX * lfBY) - (lfAY * lfBX);
                    const f32 lfInvLength =
                        1.0f / std::sqrt((lfNormalX * lfNormalX) + (lfNormalY * lfNormalY) + (lfNormalZ * lfNormalZ));

                    lrNormalPlusParam.x = lfNormalX * lfInvLength;     // SetVector3
                    lrNormalPlusParam.y = lfNormalY * lfInvLength;
                    lrNormalPlusParam.z = lfNormalZ * lfInvLength;
                    lrNormalPlusParam.w = lfLineParam;                 // SetPlus
                }
            }
        }
    }

    // @ 0x8227B2D0. Writes the incoming scalar triangle into the current
    // component lane of the current packed slot, then advances the fill cursors.
    // The asm copies each Vector4 field of *lpPackedTriangle into component
    // mnNextComponentToFill of maPackedTriangles[mnNextPackedTriangleToFill] using
    // a per-component vsel mask (the unk_8327F240 lookup) -- the lane-set semantic
    // of SetScalarTriangle. When the 4th component fills (wraps to 0), the slot
    // cursor advances modulo 48 and the populated-slot count saturates at 48.
    void BrnCrashTriangleCache::InsertTriangleIntoCache(
        BrnCrashTrianglePackedFormat* lpPackedTriangle)
    {
        maPackedTriangles[mnNextPackedTriangleToFill].SetScalarTriangle(
            *lpPackedTriangle, mnNextComponentToFill);

        mnNextComponentToFill = (mnNextComponentToFill + 1) & (KU_TRIANGLES_PER_PACK - 1);

        if (mnNextComponentToFill == 0)
        {
            const u32 luNumberOfPackedTriangles = mnNumberOfPackedTriangles;
            mnNextPackedTriangleToFill =
                (mnNextPackedTriangleToFill + 1) % KU_MAX_NUMBER_PACKED_TRIANGLES;

            mnNumberOfPackedTriangles =
                (luNumberOfPackedTriangles == KU_MAX_NUMBER_PACKED_TRIANGLES)
                    ? KU_MAX_NUMBER_PACKED_TRIANGLES
                    : luNumberOfPackedTriangles + 1;
        }
    }

    // DWARF .cpp:117 (inlined into AddTriangles on X360 and PS3). The live dataflow of the
    // console hash, lane-wise over the splatted packed triangle:
    //     (V2X + V2Y) * (V1Y * V1Z + (V2X + V2Y)) + V2Z
    // X360 fuses the two multiply-adds (vmaddfp @0x8228CF70 / @0x8228D060), PS3 does not
    // (vmaddfp with a zero addend, then vaddfp); written unfused as the source's Mult/Add. A hash
    // is only ever compared with hashes made by this same function.
    void BrnCrashTriangleCache::CalculateHashForPackedTriangle(BrnCrashTrianglePackedFormat* lpPackedTriangle)
    {
        for (u32 luComponent = 0; luComponent < KU_TRIANGLES_PER_PACK; ++luComponent)
        {
            const f32 lfVertex2XPlusY =
                lpPackedTriangle->mVertex2X.GetComponent(luComponent) + lpPackedTriangle->mVertex2Y.GetComponent(luComponent);
            const f32 lfVertex1YTimesZPlus =
                (lpPackedTriangle->mVertex1Y.GetComponent(luComponent) * lpPackedTriangle->mVertex1Z.GetComponent(luComponent))
                + lfVertex2XPlusY;
            const f32 lfHash =
                (lfVertex2XPlusY * lfVertex1YTimesZPlus) + lpPackedTriangle->mVertex2Z.GetComponent(luComponent);

            lpPackedTriangle->mVertexHash.SetComponent(luComponent, lfHash);
        }
    }
}
