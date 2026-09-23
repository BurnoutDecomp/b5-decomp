// FX-FX (crash parity 2026-09-23): BrnEffects::BrnCrashTriangleCache regressions.
//
//   G09-D1  AddTriangles @0x8228CDA8 walks the real 0xE0-byte CgsGeometric::Triangle4
//           (`addi r31, r31, 0xE0` @0x8228D1F4), not a 0xA0-byte fork.
//   G09-D2  the per-lane skip test: raw tag bits (T+0xA0), surface id 17 skips, else a zero
//           valid-mask lane (T+0x90) skips; no tag == 0 test (0x8228D09C..0x8228D1E0).
//   G09-D3  the duplicate hash (V2X+V2Y)*(V1Y*V1Z+(V2X+V2Y))+V2Z (0x8228CF2C/CF70/D060).
//   G09-D4  CheckForDuplicateTriangles @0x822847B0 searches packs K..0, K = count==48 ? 48 :
//           count+1, all four lanes, including stale packs and (count >= 47) the counter block.
//   G09-D5  the four incoming triangles are all compared against the PRE-insert cache.
//   G09-D6  CollideWithTriangleCache @0x822849B8, the cache's reader (had no body): one-sided
//           segment test (det > 1e-8, slack 1e-5*det), unit normal cross(V0-V1, V0-V2), param
//           t/det, nearest hit below the line's w wins, packs 0..K-1 with K = count==48 ? 48 : count+1.
//
// Built by run_fxfx_crash_triangle_cache.py against the PRODUCTION BrnCrashTriangleCache.cpp
// (working tree, or `--rev <b5 rev>` for the RED side) plus SharedClasses/World/BrnCollisionTag.cpp
// (BrnWorld::KU8_COLLISION_INVISIBLE_SURFACE_ID). Every expected value below is what the ARTIST
// instruction words themselves produce: the cases were run through a PPC/VMX128 emulation of
// 0x8228CDA8 -> 0x822847B0 -> 0x8227B2D0 and of 0x822849B8 (raw words from the image, VMX128
// register fields per tools/re/vmx128.py, vmaddfp fused, VMX non-Java mode) before being written
// down here. The runner defines FXFX_HAS_COLLIDE=0 for a revision with no CollideWithTriangleCache
// body; its checks then count as failed.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"
#include "GameSource/Effects/BrnCrashTriangleCache.h"

#include <cstdio>
#include <cstring>

// CGS_ASSERT link stubs for BrnCollisionTag.cpp's GetTrafficInfo (never reached here).
namespace CgsDev
{
namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return 0; }
}
}

using namespace BrnEffects;

namespace
{
    // What EffectsModule::HandlePlayerTriangleCache does (EffectsModule.cpp:1960 / :1968): hand the
    // scene manager's CgsGeometric::Triangle4 array to AddTriangles as whatever type it declares.
    // Resolving that type from the declaration lets the same harness build against the pre-fix
    // source (a 0xA0-byte BrnEffects::Triangle4 fork) and the fix (the real type).
    template <typename T> struct AddTrianglesArgument;
    template <typename C, typename A> struct AddTrianglesArgument<void (C::*)(const A*, u32)> { typedef A Type; };
    typedef AddTrianglesArgument<decltype(&BrnCrashTriangleCache::AddTriangles)>::Type IncomingBatch;

    void Add(BrnCrashTriangleCache& lrCache, const CgsGeometric::Triangle4* lpBatches, u32 luNumBatches)
    {
        lrCache.AddTriangles(reinterpret_cast<const IncomingBatch*>(lpBatches), luNumBatches);
    }

    struct Tri { f32 mafV[3][3]; };

    // The emulation's triangle family: all coordinates are small dyadic rationals, so every hash
    // below is exact in f32 whether or not the multiply-adds are fused.
    Tri T(int i)
    {
        const f32 lf = static_cast<f32>(i);
        Tri lTri = { { { lf + 1.0f, 2.0f * lf + 0.5f, -lf - 3.0f },
                       { lf + 4.0f, 7.5f - lf, 2.0f + lf },
                       { 3.0f * lf - 1.0f, lf + 0.25f, 5.0f } } };
        return lTri;
    }

    Tri Make(f32 a, f32 b, f32 c, f32 d, f32 e, f32 f, f32 g, f32 h, f32 k)
    {
        Tri lTri = { { { a, b, c }, { d, e, f }, { g, h, k } } };
        return lTri;
    }

    const u32 KU_VALID = 0xFFFFFFFFu;
    u32 Tag(u32 luSurfaceId) { return (luSurfaceId << 20) | 0x1234u; }

    void SetLane(Vector4& lrV, u32 luLane, f32 lfValue) { (&lrV.x)[luLane] = lfValue; }
    void SetLaneBits(Vector4& lrV, u32 luLane, u32 luBits) { std::memcpy(&(&lrV.x)[luLane], &luBits, sizeof(u32)); }

    CgsGeometric::Triangle4 Batch(const Tri (&laTris)[4], const u32 (&lauMasks)[4], const u32 (&lauTags)[4])
    {
        CgsGeometric::Triangle4 lBatch;
        std::memset(&lBatch, 0, sizeof(lBatch));
        Vector4* lapRows[9] = { &lBatch.mVertex0X, &lBatch.mVertex0Y, &lBatch.mVertex0Z,
                                &lBatch.mVertex1X, &lBatch.mVertex1Y, &lBatch.mVertex1Z,
                                &lBatch.mVertex2X, &lBatch.mVertex2Y, &lBatch.mVertex2Z };
        for (u32 luLane = 0; luLane < 4; ++luLane)
        {
            for (u32 luRow = 0; luRow < 9; ++luRow)
            {
                SetLane(*lapRows[luRow], luLane, laTris[luLane].mafV[luRow / 3][luRow % 3]);
            }
            SetLaneBits(lBatch.mValidMasks, luLane, lauMasks[luLane]);
            SetLaneBits(lBatch.mSurfaceTags, luLane, lauTags[luLane]);
            SetLane(lBatch.mEdge0Cosigns, luLane, 0.5f);
            SetLane(lBatch.mEdge1Cosigns, luLane, 0.5f);
            SetLane(lBatch.mEdge2Cosigns, luLane, 0.5f);
        }
        return lBatch;
    }

    // Four T(i) triangles, all valid, surface 3.
    CgsGeometric::Triangle4 BatchT(int a, int b, int c, int d)
    {
        const Tri laTris[4] = { T(a), T(b), T(c), T(d) };
        const u32 lauMasks[4] = { KU_VALID, KU_VALID, KU_VALID, KU_VALID };
        const u32 lauTags[4] = { Tag(3), Tag(3), Tag(3), Tag(3) };
        return Batch(laTris, lauMasks, lauTags);
    }

    // One valid triangle in lane 0, three masked-off fillers.
    CgsGeometric::Triangle4 BatchOne(const Tri& lrTri)
    {
        const Tri laTris[4] = { lrTri, T(9001), T(9002), T(9003) };
        const u32 lauMasks[4] = { KU_VALID, 0u, 0u, 0u };
        const u32 lauTags[4] = { Tag(3), Tag(3), Tag(3), Tag(3) };
        return Batch(laTris, lauMasks, lauTags);
    }

    // ---- inspection --------------------------------------------------------------------------
    const Vector4Lane& Row(const BrnCrashTriangleCache& lrCache, u32 luPack, u32 luRow)
    {
        const BrnCrashTrianglePackedFormat& lrPack = lrCache.maPackedTriangles[luPack];
        const Vector4Lane* lapRows[10] = { &lrPack.mVertexHash,
                                           &lrPack.mVertex0X, &lrPack.mVertex0Y, &lrPack.mVertex0Z,
                                           &lrPack.mVertex1X, &lrPack.mVertex1Y, &lrPack.mVertex1Z,
                                           &lrPack.mVertex2X, &lrPack.mVertex2Y, &lrPack.mVertex2Z };
        return *lapRows[luRow];
    }

    bool LaneHolds(const BrnCrashTriangleCache& lrCache, u32 luPack, u32 luLane, const Tri& lrTri)
    {
        for (u32 luRow = 0; luRow < 9; ++luRow)
        {
            if (Row(lrCache, luPack, luRow + 1).GetComponent(luLane) != lrTri.mafV[luRow / 3][luRow % 3])
            {
                return false;
            }
        }
        return true;
    }

    bool LaneIsZero(const BrnCrashTriangleCache& lrCache, u32 luPack, u32 luLane)
    {
        for (u32 luRow = 0; luRow < 10; ++luRow)
        {
            if (Row(lrCache, luPack, luRow).GetComponent(luLane) != 0.0f)
            {
                return false;
            }
        }
        return true;
    }

    bool CacheHolds(const BrnCrashTriangleCache& lrCache, const Tri& lrTri)
    {
        for (u32 luPack = 0; luPack < KU_MAX_NUMBER_PACKED_TRIANGLES; ++luPack)
        {
            for (u32 luLane = 0; luLane < 4; ++luLane)
            {
                if (LaneHolds(lrCache, luPack, luLane, lrTri))
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool Counters(const BrnCrashTriangleCache& lrCache, u32 luCount, u32 luNextPack, u32 luNextComponent)
    {
        return lrCache.mnNumberOfPackedTriangles == luCount
            && lrCache.mnNextPackedTriangleToFill == luNextPack
            && lrCache.mnNextComponentToFill == luNextComponent;
    }

    bool V0XLanes(const BrnCrashTriangleCache& lrCache, u32 luPack, f32 a, f32 b, f32 c, f32 d)
    {
        const Vector4Lane& lrRow = Row(lrCache, luPack, 1);
        return lrRow.GetComponent(0) == a && lrRow.GetComponent(1) == b
            && lrRow.GetComponent(2) == c && lrRow.GetComponent(3) == d;
    }

    // ---- reporting ---------------------------------------------------------------------------
    int giChecks = 0;
    int giFailures = 0;

    void Check(bool lbPass, const char* lpcName)
    {
        ++giChecks;
        if (!lbPass)
        {
            ++giFailures;
        }
        std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcName);
    }

    BrnCrashTriangleCache& FreshCache()
    {
        static BrnCrashTriangleCache sCache;
        sCache.Construct();
        return sCache;
    }

    CgsGeometric::Triangle4 gaBatches[64];

    const int KI_D6_CHECKS = 15;

#if FXFX_HAS_COLLIDE
    typedef BrnCrashLineTriangleCacheFormat Line;

    const Tri KZERO = { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } } };

    // A horizontal triangle at height y. As wound it is front-facing (det > 0) for a DOWNWARD line;
    // flipped, for an upward one. cross(V0-V1, V0-V2) is (0, 16, 0) / (0, -16, 0).
    Tri H(f32 y, bool lbFlip = false)
    {
        return lbFlip ? Make(-2, y, -2, 2, y, -2, 0, y, 2) : Make(-2, y, -2, 0, y, 2, 2, y, -2);
    }

    // Write four triangles straight into pack `luPack` (the SoA rows; the hash row is not read).
    void PutPack(BrnCrashTriangleCache& lrCache, u32 luPack, const Tri& a, const Tri& b, const Tri& c, const Tri& d)
    {
        const Tri* lapTris[4] = { &a, &b, &c, &d };
        BrnCrashTrianglePackedFormat& lrPack = lrCache.maPackedTriangles[luPack];
        Vector4Lane* lapRows[9] = { &lrPack.mVertex0X, &lrPack.mVertex0Y, &lrPack.mVertex0Z,
                                    &lrPack.mVertex1X, &lrPack.mVertex1Y, &lrPack.mVertex1Z,
                                    &lrPack.mVertex2X, &lrPack.mVertex2Y, &lrPack.mVertex2Z };
        for (u32 luLane = 0; luLane < 4; ++luLane)
        {
            for (u32 luRow = 0; luRow < 9; ++luRow)
            {
                lapRows[luRow]->SetComponent(luLane, lapTris[luLane]->mafV[luRow / 3][luRow % 3]);
            }
        }
    }

    Line Seg(f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1, f32 lfW = 1.0f)
    {
        Line lLine;
        std::memset(&lLine, 0, sizeof(lLine));
        lLine.mLineStartPosition.x = x0; lLine.mLineStartPosition.y = y0; lLine.mLineStartPosition.z = z0;
        lLine.mLineEndPos.x = x1;        lLine.mLineEndPos.y = y1;        lLine.mLineEndPos.z = z1;
        lLine.mLineIntersectNormalPlusLineParms.w = lfW;   // UpdateBucket's seed is {0, 0, 0, 1.0}
        return lLine;
    }

    // The downward probe x = 0.5, z = 0 from y = 3 to y = -1 (param = (3 - y) / 4).
    Line Down(f32 lfW = 1.0f) { return Seg(0.5f, 3.0f, 0.0f, 0.5f, -1.0f, 0.0f, lfW); }

    void Collide(BrnCrashTriangleCache& lrCache, u32 luCount, Line* lpLines, u32 luNumLines)
    {
        lrCache.mnNumberOfPackedTriangles = luCount;
        lrCache.mnNextPackedTriangleToFill = 0;
        lrCache.mnNextComponentToFill = 0;
        lrCache.CollideWithTriangleCache(lpLines, luNumLines);
    }

    bool Is(const Line& lrLine, f32 x, f32 y, f32 z, f32 w)
    {
        const Vector3Plus& lrOut = lrLine.mLineIntersectNormalPlusLineParms;
        return lrOut.x == x && lrOut.y == y && lrOut.z == z && lrOut.w == w;
    }

    bool Near(const Line& lrLine, u32 luX, u32 luY, u32 luZ, u32 luW)
    {
        const u32 lauBits[4] = { luX, luY, luZ, luW };
        const Vector3Plus& lrOut = lrLine.mLineIntersectNormalPlusLineParms;
        const f32 lafOut[4] = { lrOut.x, lrOut.y, lrOut.z, lrOut.w };
        for (u32 li = 0; li < 4; ++li)
        {
            f32 lfConsole;
            std::memcpy(&lfConsole, &lauBits[li], sizeof(f32));
            const f32 lfDelta = lafOut[li] - lfConsole;
            if (!(lfDelta <= 2.0e-6f && lfDelta >= -2.0e-6f))
            {
                return false;
            }
        }
        return true;
    }

    // One line, cache holding `lrTri` in pack 0 lane 0 only.
    Line OneTriangle(const Tri& lrTri, const Line& lrLine)
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        PutPack(lrCache, 0, lrTri, KZERO, KZERO, KZERO);
        Line lLine = lrLine;
        Collide(lrCache, 0, &lLine, 1);
        return lLine;
    }
#endif
}

int main()
{
    // ---- G09-D1 / D2 / D3: two batches, one of each skip reason -----------------------------------
    // b0 = T0 T1 T2 T3, all valid, surfaces 5 5 17 9     -> T2 is an invisible surface
    // b1 = T4 T5 T6 T7, valid V 0 V V, surfaces 3 3 3 and a raw 0 tag   -> T5 is masked off
    // Console result: counters (1, 1, 2); pack 0 = T0 T1 T3 T4; pack 1 = T6 T7 - -.
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        const Tri laTris0[4] = { T(0), T(1), T(2), T(3) };
        const u32 lauMasks0[4] = { KU_VALID, KU_VALID, KU_VALID, KU_VALID };
        const u32 lauTags0[4] = { Tag(5), Tag(5), Tag(0x11), Tag(9) };
        const Tri laTris1[4] = { T(4), T(5), T(6), T(7) };
        const u32 lauMasks1[4] = { KU_VALID, 0u, KU_VALID, KU_VALID };
        const u32 lauTags1[4] = { Tag(3), Tag(3), Tag(3), 0u };
        gaBatches[0] = Batch(laTris0, lauMasks0, lauTags0);
        gaBatches[1] = Batch(laTris1, lauMasks1, lauTags1);
        Add(lrCache, gaBatches, 2);

        Check(Counters(lrCache, 1, 1, 2), "[G09-D1] two batches leave counters (1, 1, 2)");
        Check(LaneHolds(lrCache, 0, 0, T(0)) && LaneHolds(lrCache, 0, 1, T(1))
                  && LaneHolds(lrCache, 0, 2, T(3)) && LaneHolds(lrCache, 0, 3, T(4)),
              "[G09-D1] pack 0 holds T0 T1 T3 T4, every vertex row exact");
        Check(LaneHolds(lrCache, 1, 0, T(6)) && LaneHolds(lrCache, 1, 1, T(7))
                  && LaneIsZero(lrCache, 1, 2) && LaneIsZero(lrCache, 1, 3),
              "[G09-D1] batch 1 is read at +0xE0: pack 1 holds T6 T7, lanes 2-3 untouched");
        Check(!CacheHolds(lrCache, T(2)), "[G09-D2] surface id 17 (invisible) is never cached");
        Check(!CacheHolds(lrCache, T(5)), "[G09-D2] a lane whose valid mask is 0 is never cached");
        Check(LaneHolds(lrCache, 1, 1, T(7)), "[G09-D2] a zero surface tag IS cached (no tag == 0 test)");

        const Vector4Lane& lrHash0 = Row(lrCache, 0, 0);
        const Vector4Lane& lrHash1 = Row(lrCache, 1, 0);
        Check(lrHash0.GetComponent(0) == -5.6875f && lrHash0.GetComponent(1) == 78.9375f
                  && lrHash0.GetComponent(2) == 384.6875f && lrHash0.GetComponent(3) == 557.8125f
                  && lrHash1.GetComponent(0) == 824.5625f && lrHash1.GetComponent(1) == 870.1875f,
              "[G09-D3] cached hashes are (V2X+V2Y)*(V1Y*V1Z+(V2X+V2Y))+V2Z: -5.6875 78.9375 384.6875 557.8125 | 824.5625 870.1875");
    }

    // ---- G09-D2: the tag is decoded from its raw bits ------------------------------------------------
    // tags 0xFFF01234 (a NaN pattern, surface 0x3F), 0x61107FFF (surface 17 + drivable/fatal flag
    // bits), 0x01200000 (surface 0x12), 0x7F7FFFFF (surface 0x37). Console: (0, 0, 3), V0X 11 13 14.
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        const Tri laTris[4] = { T(10), T(11), T(12), T(13) };
        const u32 lauMasks[4] = { KU_VALID, KU_VALID, KU_VALID, KU_VALID };
        const u32 lauTags[4] = { 0xFFF01234u, 0x61107FFFu, 0x01200000u, 0x7F7FFFFFu };
        gaBatches[0] = Batch(laTris, lauMasks, lauTags);
        Add(lrCache, gaBatches, 1);
        Check(Counters(lrCache, 0, 0, 3) && V0XLanes(lrCache, 0, 11.0f, 13.0f, 14.0f, 0.0f),
              "[G09-D2] raw tag bits: NaN-patterned tag kept, surface 17 with flag bits skipped");
    }

    // ---- G09-D3: what the hash ignores and what it separates ------------------------------------------
    // [A] then [A2, R]: A2 differs from A only in V0 and V1X (same hash 684) -> rejected; R is A with
    // its vertices rotated (hash 228) -> kept. Console: (0, 0, 2), V0X 1 4.
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        const Tri lA = Make(1, 2, 3, 4, 5, 6, 7, 8, 9);
        const Tri lA2 = Make(-11, 0.5f, 13, -2, 5, 6, 7, 8, 9);
        const Tri lR = Make(4, 5, 6, 7, 8, 9, 1, 2, 3);
        const u32 lauMasksA[4] = { KU_VALID, 0u, 0u, 0u };
        const u32 lauMasksB[4] = { KU_VALID, KU_VALID, 0u, 0u };
        const u32 lauTags[4] = { Tag(3), Tag(3), Tag(3), Tag(3) };
        const Tri laFirst[4] = { lA, T(9001), T(9002), T(9003) };
        const Tri laSecond[4] = { lA2, lR, T(9002), T(9003) };
        gaBatches[0] = Batch(laFirst, lauMasksA, lauTags);
        gaBatches[1] = Batch(laSecond, lauMasksB, lauTags);
        Add(lrCache, gaBatches, 2);
        Check(Counters(lrCache, 0, 0, 2) && V0XLanes(lrCache, 0, 1.0f, 4.0f, 0.0f, 0.0f),
              "[G09-D3] hash ignores V0/V1X (A2 deduped against A) and separates a vertex rotation (R kept)");
    }

    // ---- G09-D4: the search range --------------------------------------------------------------------
    // (a) a new crash re-adding the previous crash's batch: the stale pack 0 rejects all four.
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        gaBatches[0] = BatchT(0, 1, 2, 3);
        Add(lrCache, gaBatches, 1);
        const bool lbFirstCrashFilled = Counters(lrCache, 1, 1, 0);
        lrCache.ResetCounters();
        Add(lrCache, gaBatches, 1);
        Check(lbFirstCrashFilled && Counters(lrCache, 0, 0, 0),
              "[G09-D4] after ResetCounters the stale pack 0 still rejects its own triangles");
    }
    // (b) count 0 -> K = 1: stale pack 1 is searched (T5 rejected), stale pack 2 is not (T9 kept).
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        gaBatches[0] = BatchT(0, 1, 2, 3);
        gaBatches[1] = BatchT(4, 5, 6, 7);
        gaBatches[2] = BatchT(8, 9, 10, 11);
        Add(lrCache, gaBatches, 3);
        lrCache.ResetCounters();
        gaBatches[0] = BatchT(5, 9, 20, 21);
        Add(lrCache, gaBatches, 1);
        Check(Counters(lrCache, 0, 0, 3) && V0XLanes(lrCache, 0, 10.0f, 21.0f, 22.0f, 4.0f),
              "[G09-D4] count 0 searches packs 1..0 (stale pack 1 hit), not pack 2");
    }
    // (c) after a wrap, the old lanes >= mnNextComponentToFill of the pack being overwritten are
    //     still compared: T2 (pack 0 lane 2) is rejected.
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        for (int liPack = 0; liPack < 48; ++liPack)
        {
            gaBatches[liPack] = BatchT(4 * liPack, 4 * liPack + 1, 4 * liPack + 2, 4 * liPack + 3);
        }
        Add(lrCache, gaBatches, 48);
        gaBatches[0] = BatchOne(T(500));
        Add(lrCache, gaBatches, 1);
        const bool lbWrapped = Counters(lrCache, 48, 0, 1);
        gaBatches[0] = BatchOne(T(2));
        Add(lrCache, gaBatches, 1);
        Check(lbWrapped && Counters(lrCache, 48, 0, 1) && V0XLanes(lrCache, 0, 501.0f, 2.0f, 3.0f, 4.0f),
              "[G09-D4] after the wrap, pack 0 lanes 1-3 are still searched (T2 rejected)");
    }
    // (d) count 47 -> K = 48 reads the counter block as a 49th row; a +-0 hash matches it.
    //     V2 = (1, -1, 0) makes the hash exactly +0.
    {
        const Tri lZeroHash = Make(1, 2, 3, 4, 5, 6, 1, -1, 0);
        BrnCrashTriangleCache& lrCache = FreshCache();
        for (int liPack = 0; liPack < 48; ++liPack)
        {
            gaBatches[liPack] = BatchT(4 * liPack, 4 * liPack + 1, 4 * liPack + 2, 4 * liPack + 3);
        }
        Add(lrCache, gaBatches, 48);
        lrCache.ResetCounters();
        for (int liPack = 0; liPack < 47; ++liPack)
        {
            gaBatches[liPack] = BatchT(1000 + 4 * liPack, 1001 + 4 * liPack, 1002 + 4 * liPack, 1003 + 4 * liPack);
        }
        Add(lrCache, gaBatches, 47);
        const bool lbAt47 = Counters(lrCache, 47, 47, 0);
        gaBatches[0] = BatchOne(lZeroHash);
        Add(lrCache, gaBatches, 1);
        Check(lbAt47 && Counters(lrCache, 47, 47, 0),
              "[G09-D4] count 47: the counter-block row rejects a +0 hash");

        // control: count 46 -> K = 47, all 48 real packs searched, no counter-block row -> kept.
        BrnCrashTriangleCache& lrControl = FreshCache();
        for (int liPack = 0; liPack < 48; ++liPack)
        {
            gaBatches[liPack] = BatchT(4 * liPack, 4 * liPack + 1, 4 * liPack + 2, 4 * liPack + 3);
        }
        Add(lrControl, gaBatches, 48);
        lrControl.ResetCounters();
        for (int liPack = 0; liPack < 46; ++liPack)
        {
            gaBatches[liPack] = BatchT(1000 + 4 * liPack, 1001 + 4 * liPack, 1002 + 4 * liPack, 1003 + 4 * liPack);
        }
        Add(lrControl, gaBatches, 46);
        gaBatches[0] = BatchOne(lZeroHash);
        Add(lrControl, gaBatches, 1);
        Check(Counters(lrControl, 46, 46, 1), "[G09-D4] control: count 46 has no counter-block row (+0 hash kept)");
    }

    // ---- G09-D5: same-batch duplicates ---------------------------------------------------------------
    // T7 T7 T8 T7 in one batch: all four are compared against the pre-insert cache -> all inserted.
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        gaBatches[0] = BatchT(7, 7, 8, 7);
        Add(lrCache, gaBatches, 1);
        Check(Counters(lrCache, 1, 1, 0) && V0XLanes(lrCache, 0, 8.0f, 8.0f, 9.0f, 8.0f),
              "[G09-D5] identical triangles in one batch are all inserted");
    }

    // ---- G09-D6: CollideWithTriangleCache ------------------------------------------------------------
#if FXFX_HAS_COLLIDE
    const int liChecksBeforeD6 = giChecks;
    {
        Check(Is(OneTriangle(H(1.0f), Down()), 0.0f, 1.0f, 0.0f, 0.5f),
              "[G09-D6] a front-facing hit writes the unit normal (0,1,0) and param t/det 0.5");
        Check(Is(OneTriangle(H(1.0f), Seg(0.5f, -1.0f, 0.0f, 0.5f, 3.0f, 0.0f)), 0.0f, 0.0f, 0.0f, 1.0f)
                  && Is(OneTriangle(H(1.0f, true), Seg(0.5f, -1.0f, 0.0f, 0.5f, 3.0f, 0.0f)), 0.0f, -1.0f, 0.0f, 0.5f),
              "[G09-D6] one-sided: a back face never hits; the flipped triangle hit from below gives (0,-1,0) 0.5");
        Check(Is(OneTriangle(H(1.0f), Seg(5.0f, 3.0f, 0.0f, 5.0f, -1.0f, 0.0f)), 0.0f, 0.0f, 0.0f, 1.0f),
              "[G09-D6] a line outside the triangle misses");
        Check(Is(OneTriangle(H(1.0f), Seg(0.5f, 3.0f, 0.0f, 0.5f, 1.5f, 0.0f)), 0.0f, 0.0f, 0.0f, 1.0f),
              "[G09-D6] a segment ending short of the triangle misses (t > det)");
    }
    {
        // lanes: y = -0.5 (0.875), 2.0 (0.25), 0.5 (0.625), and a nearer BACK face at 2.5.
        BrnCrashTriangleCache& lrCache = FreshCache();
        PutPack(lrCache, 0, H(-0.5f), H(2.0f), H(0.5f), H(2.5f, true));
        Line lLine = Down();
        Collide(lrCache, 0, &lLine, 1);
        BrnCrashTriangleCache& lrReversed = FreshCache();
        PutPack(lrReversed, 0, H(2.5f, true), H(0.5f), H(2.0f), H(-0.5f));
        Line lReversed = Down();
        const bool lbFirst = Is(lLine, 0.0f, 1.0f, 0.0f, 0.25f);
        Collide(lrReversed, 0, &lReversed, 1);
        Check(lbFirst && Is(lReversed, 0.0f, 1.0f, 0.0f, 0.25f),
              "[G09-D6] the nearest front-facing lane wins (0.25) in either lane order; the nearer back face is ignored");
    }
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        PutPack(lrCache, 0, H(0.0f), KZERO, KZERO, KZERO);
        PutPack(lrCache, 1, KZERO, KZERO, H(2.0f), KZERO);
        Line lLine = Down();
        Collide(lrCache, 1, &lLine, 1);
        const bool lbNearInPack1 = Is(lLine, 0.0f, 1.0f, 0.0f, 0.25f);
        PutPack(lrCache, 0, H(2.0f), KZERO, KZERO, KZERO);
        PutPack(lrCache, 1, KZERO, KZERO, H(0.0f), KZERO);
        lLine = Down();
        Collide(lrCache, 1, &lLine, 1);
        Check(lbNearInPack1 && Is(lLine, 0.0f, 1.0f, 0.0f, 0.25f),
              "[G09-D6] the nearest hit across packs wins, whichever pack holds it");
    }
    {
        const Line lLine = OneTriangle(H(1.0f), Down(0.3f));
        Check(Is(lLine, 0.0f, 0.0f, 0.0f, 0.3f), "[G09-D6] a hit beyond the line's current w (0.5 >= 0.3) is not taken");
    }
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        PutPack(lrCache, 1, H(1.0f), KZERO, KZERO, KZERO);
        Line lLine = Down();
        Collide(lrCache, 0, &lLine, 1);
        const bool lbCount0 = Is(lLine, 0.0f, 0.0f, 0.0f, 1.0f);
        lLine = Down();
        Collide(lrCache, 1, &lLine, 1);
        Check(lbCount0 && Is(lLine, 0.0f, 1.0f, 0.0f, 0.5f),
              "[G09-D6] K = count + 1: count 0 does not search pack 1, count 1 does");
    }
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        PutPack(lrCache, 47, KZERO, KZERO, KZERO, H(1.0f));
        Line la[3] = { Down(), Down(), Down() };
        Collide(lrCache, 48, &la[0], 1);
        Collide(lrCache, 46, &la[1], 1);
        Collide(lrCache, 47, &la[2], 1);
        Check(Is(la[0], 0.0f, 1.0f, 0.0f, 0.5f) && Is(la[1], 0.0f, 0.0f, 0.0f, 1.0f) && Is(la[2], 0.0f, 1.0f, 0.0f, 0.5f),
              "[G09-D6] count 48 and count 47 search pack 47, count 46 does not");
    }
    {
        BrnCrashTriangleCache& lrCache = FreshCache();
        PutPack(lrCache, 0, H(1.0f), KZERO, KZERO, KZERO);
        Line la[3] = { Seg(5.0f, 3.0f, 0.0f, 5.0f, -1.0f, 0.0f), Seg(0.25f, 3.0f, -0.5f, 0.25f, -1.0f, -0.5f),
                       Seg(-7.0f, 3.0f, 0.0f, -7.0f, -1.0f, 0.0f) };
        Collide(lrCache, 0, la, 3);
        Check(Is(la[0], 0.0f, 0.0f, 0.0f, 1.0f) && Is(la[1], 0.0f, 1.0f, 0.0f, 0.5f) && Is(la[2], 0.0f, 0.0f, 0.0f, 1.0f),
              "[G09-D6] lines are independent (0x30 stride): only the middle of three is hit");
    }
    {
        // V0 (0,0,0) V1 (0,0,2) V2 (2,0,0); det = 8, so the slack is 8e-5 in v = 4x.
        const Tri lEdge = Make(0, 0, 0, 0, 0, 2, 2, 0, 0);
        const Line lInside = OneTriangle(lEdge, Seg(-1.5e-5f, 1.0f, 0.5f, -1.5e-5f, -1.0f, 0.5f));
        const Line lOutside = OneTriangle(lEdge, Seg(-3.0e-5f, 1.0f, 0.5f, -3.0e-5f, -1.0f, 0.5f));
        Check(Is(lInside, 0.0f, 1.0f, 0.0f, 0.5f) && Is(lOutside, 0.0f, 0.0f, 0.0f, 1.0f),
              "[G09-D6] RTINTSECEDGEEPS 1e-5: 1.5e-5 outside the edge still hits, 3e-5 misses");
    }
    {
        // A 1e-4 triangle crossed through its interior: det = |D.y| * 1e-8.
        const Tri lTiny = Make(0, 0, 0, 0, 0, 1.0e-4f, 1.0e-4f, 0, 0);
        const Line lAbove = OneTriangle(lTiny, Seg(2.0e-5f, 0.75f, 2.0e-5f, 2.0e-5f, -0.75f, 2.0e-5f));
        const Line lBelow = OneTriangle(lTiny, Seg(2.0e-5f, 0.25f, 2.0e-5f, 2.0e-5f, -0.25f, 2.0e-5f));
        Check(Is(lAbove, 0.0f, 1.0f, 0.0f, 0.5f) && Is(lBelow, 0.0f, 0.0f, 0.0f, 1.0f),
              "[G09-D6] RTINTSECEPSILON 1e-8: det 1.5e-8 hits, det 5e-9 does not");
    }
    {
        // Tilted triangles, two packs, count 1, three lines -- console results (emulated, raw bits).
        BrnCrashTriangleCache& lrCache = FreshCache();
        const Tri lA = Make(-1.5f, 0.25f, -2.0f, 0.75f, 1.5f, 2.25f, 2.5f, -0.5f, -1.25f);
        const Tri lB = Make(-2.0f, 2.0f, -1.0f, 0.5f, 2.75f, 1.5f, 1.75f, 1.25f, -2.5f);
        const Tri lC = Make(3.0f, -1.0f, 3.0f, -3.0f, -1.25f, 2.0f, 0.0f, -0.75f, -3.0f);
        const Tri lD = Make(-0.5f, 3.5f, -0.5f, 0.5f, 3.25f, 0.75f, 0.75f, 3.75f, -0.75f);
        PutPack(lrCache, 0, lA, KZERO, lB, KZERO);
        PutPack(lrCache, 1, lC, lD, KZERO, KZERO);
        Line la[3] = { Seg(0.1f, 4.0f, -0.2f, 0.3f, -3.0f, 0.1f), Seg(-0.8f, 3.0f, 0.4f, 0.9f, -2.0f, -0.6f),
                       Seg(1.2f, -3.0f, 0.9f, 0.2f, 4.0f, 0.3f) };
        Collide(lrCache, 1, la, 3);
        Check(Near(la[0], 0xBE05BD38u, 0x3F7266F6u, 0x3E9674DFu, 0x3D96B917u),
              "[G09-D6] golden line 0: (-0.1306046, 0.9468836, 0.2938604) param 0.0735952 (+-2e-6)");
        Check(Near(la[1], 0x3D5C1A5Cu, 0x3F70BCD4u, 0xBEABF497u, 0x3E001498u),
              "[G09-D6] golden line 1: (0.0537361, 0.9403813, -0.3358505) param 0.1250786 (+-2e-6)");
        Check(Near(la[2], 0x3D586804u, 0xBF7F0CE0u, 0xBD8B1E4Cu, 0x3E958489u),
              "[G09-D6] golden line 2 (upward): (0.0528336, -0.9962902, -0.0679289) param 0.2920268 (+-2e-6)");
    }
    if (giChecks - liChecksBeforeD6 != KI_D6_CHECKS)
    {
        Check(false, "[G09-D6] harness bookkeeping: KI_D6_CHECKS is out of date");
    }
#else
    for (int li = 0; li < KI_D6_CHECKS; ++li)
    {
        Check(false, "[G09-D6] CollideWithTriangleCache has no body in this revision");
    }
#endif

    std::printf("%d/%d checks passed\n", giChecks - giFailures, giChecks);
    return giFailures == 0 ? 0 : 1;
}
