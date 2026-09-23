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
//
// Built by run_fxfx_crash_triangle_cache.py against the PRODUCTION BrnCrashTriangleCache.cpp
// (working tree, or `--rev <b5 rev>` for the RED side) plus SharedClasses/World/BrnCollisionTag.cpp
// (BrnWorld::KU8_COLLISION_INVISIBLE_SURFACE_ID). Every expected value below is what the ARTIST
// instruction words themselves produce: the cases were run through a PPC/VMX128 emulation of
// 0x8228CDA8 -> 0x822847B0 -> 0x8227B2D0 (raw words from the image, VMX128 register fields per
// tools/re/vmx128.py, vmaddfp fused, VMX non-Java mode) before being written down here.
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

    std::printf("%d/%d checks passed\n", giChecks - giFailures, giChecks);
    return giFailures == 0 ? 0 : 1;
}
