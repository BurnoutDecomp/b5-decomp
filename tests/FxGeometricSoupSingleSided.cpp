// FX-GEOMETRIC (crash parity 2026-09-24): CgsGeometric::IntersectLinePolygonSoupSingleSided @0x8283C598 -- the
// all-hits single-sided line-vs-soup kernel under BaseCollisionGenerator::CollideLineAgainstPolySoupList.
// run_fxgeometric_soup_single_sided.py pastes the production CgsPolygonSoupTests_LineNearest.cpp (the kernel,
// its 4-wide IntersectLinePolySoupTriangleSingleSided4 and the Nearest sibling) plus the production
// UnpackPolygonSoupVertices; CgsPolygonSoup.cpp (GetPolygon/GetVertex) is compiled alongside.
//
// The soup below is hand-built so ONE vertical segment meets every batch kind the asm has, in order:
//   Q0,Q1 = a quad PAIR      (lanes (A0,A1,A2) (A3,A2,A1) (B0,B1,B2) (B3,B2,B1); tags A,A,B,B)
//   Q2    = the ODD quad     (lanes 0/1 read; 2/3 are duplicates the asm never reads)
//   T0..3 = a triangle QUARTET (lane k = triangle k, its own tag)
//   T4,T5 = two ODD triangles (one per kernel call, lane 0)
// Expected records (ARTIST 0x8283C774.. per lane): V0/V1/V2 in the lane's order, t (the kernel's slot,
// splatted), P = S + (E-S)*t, n = normalize((V1-V0) x (V2-V1)), the tag splatted; written BEFORE the
// `found >= max` test, so max caps the count and max <= 0 still writes one record.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::vector<std::string> gaAsserts;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
}
}

namespace CgsGeometric {
#include "fxg_soup_unpack.inc"   // UnpackPolygonSoupVertices, verbatim from CgsPolygonSoupTests.cpp
}
#include "fxg_soup_kernel.inc"   // CgsPolygonSoupTests_LineNearest.cpp, verbatim

using CgsGeometric::PolySoupLineNearestResult;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Near(f32 a, f32 b, f32 lfTol = 1.0e-4f) { return std::fabs(a - b) <= lfTol; }

// ---- the soup -----------------------------------------------------------------------------------
// World = packed / 16 (origin 0, scale 1/16). All coordinates are multiples of 1/16.
struct Soup
{
    std::vector<u16> mau16Packed;                  // 3 per vertex, host order (the PC leaf's order)
    std::vector<CgsGeometric::PolygonSoupPoly> maPolys;
    std::vector<Vector3> maWorld;                  // the same vertices, for expectations
    CgsGeometric::PolygonSoup mSoup;

    u8 Vertex(f32 x, f32 y, f32 z)
    {
        maWorld.push_back(Vector3{ x, y, z, 0.0f });
        mau16Packed.push_back(static_cast<u16>(x * 16.0f));
        mau16Packed.push_back(static_cast<u16>(y * 16.0f));
        mau16Packed.push_back(static_cast<u16>(z * 16.0f));
        return static_cast<u8>(maWorld.size() - 1);
    }
    void Poly(u32 tag, u8 a, u8 b, u8 c, u8 d)
    {
        CgsGeometric::PolygonSoupPoly p;
        std::memset(&p, 0, sizeof(p));
        p.muSurfaceTag = tag;
        p.mau8VertexIndex[0] = a; p.mau8VertexIndex[1] = b; p.mau8VertexIndex[2] = c; p.mau8VertexIndex[3] = d;
        maPolys.push_back(p);
    }
    // A horizontal unit quad at height y whose strip order (0,1,3,2 around the perimeter) winds UP
    // for both split triangles (A0,A1,A2) and (A3,A2,A1).
    void QuadUp(u32 tag, f32 x0, f32 y, f32 z0)
    {
        const u8 a0 = Vertex(x0, y, z0), a1 = Vertex(x0, y, z0 + 1.0f),
                 a2 = Vertex(x0 + 1.0f, y, z0), a3 = Vertex(x0 + 1.0f, y, z0 + 1.0f);
        Poly(tag, a0, a1, a2, a3);
    }
    void TriUp(u32 tag, f32 x0, f32 y, f32 z0)
    {
        const u8 v0 = Vertex(x0, y, z0), v1 = Vertex(x0, y, z0 + 1.0f), v2 = Vertex(x0 + 1.0f, y, z0);
        Poly(tag, v0, v1, v2, 0xFF);
    }
    void TriDown(u32 tag, f32 x0, f32 y, f32 z0)
    {
        const u8 v0 = Vertex(x0, y, z0), v1 = Vertex(x0 + 1.0f, y, z0), v2 = Vertex(x0, y, z0 + 1.0f);
        Poly(tag, v0, v1, v2, 0xFF);
    }
    void Finish(u8 lu8NumQuads)
    {
        std::memset(&mSoup, 0, sizeof(mSoup));
        mSoup.miPosX = mSoup.miPosY = mSoup.miPosZ = 0;
        mSoup.mfScale = 1.0f / 16.0f;
        mSoup.mpPolygons = reinterpret_cast<u8*>(maPolys.data());
        mSoup.mpVertices = reinterpret_cast<u8*>(mau16Packed.data());
        mSoup.mu8NumPolygons = static_cast<u8>(maPolys.size());
        mSoup.mu8NumQuads = lu8NumQuads;
        mSoup.mu8NumVertices = static_cast<u8>(maWorld.size());
    }
};

static void BuildScene(Soup& s)
{
    // quads first (the soup contract): the pair Q0/Q1, then the odd quad Q2
    s.QuadUp(0xA0A00000u, 16.0f, 42.0f, 16.0f);          // Q0  y 42 -- line at local (0.25,0.25) -> lane 0
    s.QuadUp(0xB1B10001u, 16.0f, 40.0f, 16.0f);          // Q1  y 40 -- lane 2 (B0,B1,B2)
    s.QuadUp(0xC2C20002u, 15.5f, 38.0f, 15.5f);          // Q2  y 38, shifted -> local (0.75,0.75) -> lane 1
    // triangles: the quartet T0..T3, then the odd T4, T5
    s.TriUp  (0xD3D30003u, 16.0f, 37.0f, 16.0f);         // T0  hit (lane 0)
    s.TriDown(0xE4E40004u, 16.0f, 36.0f, 16.0f);         // T1  back face -> no hit
    s.TriUp  (0xF5F50005u, 16.0f, 35.0f, 16.0f);         // T2  hit (lane 2)
    s.TriUp  (0x06060006u, 21.0f, 34.0f, 16.0f);         // T3  up, but the line misses it in x
    s.TriUp  (0x17170007u, 16.0f, 33.0f, 16.0f);         // T4  odd triangle, hit
    s.TriUp  (0x28280008u, 16.0f, 31.0f, 16.0f);         // T5  odd triangle, hit
    s.Finish(3);
}

int main()
{
    Soup s;
    BuildScene(s);
    const Vector3 lStart = Vector3{ 16.25f, 52.0f, 16.25f, 0.0f };
    const Vector3 lEnd   = Vector3{ 16.25f, 22.0f, 16.25f, 0.0f };

    // ---- the full walk ------------------------------------------------------------------------------
    PolySoupLineNearestResult laResults[12];
    std::memset(laResults, 0xCD, sizeof(laResults));
    const s32 liFound = CgsGeometric::IntersectLinePolygonSoupSingleSided(s.mSoup, lStart, lEnd, laResults, 32);
    Check(liFound == 7, "W1 seven single-sided hits: Q0, Q1, Q2, T0, T2, T4, T5 (T1 is a back face, T3 is missed)");

    struct Expect { u32 tag; f32 y; u8 v0, v1, v2; };
    // Vertex indices: Q0 0..3, Q1 4..7, Q2 8..11, T0 12..14, T1 15..17, T2 18..20, T3 21..23, T4 24..26, T5 27..29.
    const Expect laExpect[7] =
    {
        { 0xA0A00000u, 42.0f, 0, 1, 2 },      // Q0 lane 0 (A0,A1,A2)
        { 0xB1B10001u, 40.0f, 4, 5, 6 },      // Q1 = pair lane 2 (B0,B1,B2)
        { 0xC2C20002u, 38.0f, 11, 10, 9 },    // Q2 odd quad lane 1 (A3,A2,A1)
        { 0xD3D30003u, 37.0f, 12, 13, 14 },   // T0 quartet lane 0
        { 0xF5F50005u, 35.0f, 18, 19, 20 },   // T2 quartet lane 2
        { 0x17170007u, 33.0f, 24, 25, 26 },   // T4 odd
        { 0x28280008u, 31.0f, 27, 28, 29 },   // T5 odd
    };
    bool lbOrder = true, lbVerts = true, lbT = true, lbPos = true, lbNormal = true, lbTag = true, lbW = true;
    for (int i = 0; i < 7; ++i)   // a slot the kernel did not write still holds the 0xCD fill -> fails
    {
        const PolySoupLineNearestResult& r = laResults[i];
        const Expect& e = laExpect[i];
        const f32 lfT = (52.0f - e.y) / 30.0f;
        lbOrder = lbOrder && (r.mau32Tag[0] == e.tag);
        lbVerts = lbVerts && r.mVertex0.x == s.maWorld[e.v0].x && r.mVertex0.y == s.maWorld[e.v0].y && r.mVertex0.z == s.maWorld[e.v0].z
                          && r.mVertex1.x == s.maWorld[e.v1].x && r.mVertex1.z == s.maWorld[e.v1].z
                          && r.mVertex2.x == s.maWorld[e.v2].x && r.mVertex2.z == s.maWorld[e.v2].z;
        lbT = lbT && Near(r.mLineParam.x, lfT) && r.mLineParam.y == r.mLineParam.x && r.mLineParam.z == r.mLineParam.x
                  && r.mLineParam.w == r.mLineParam.x;
        lbPos = lbPos && Near(r.mPosition.x, 16.25f) && Near(r.mPosition.y, e.y) && Near(r.mPosition.z, 16.25f)
                      && r.mPosition.w == 0.0f;
        lbNormal = lbNormal && Near(r.mNormal.x, 0.0f) && Near(r.mNormal.y, 1.0f) && Near(r.mNormal.z, 0.0f) && r.mNormal.w == 0.0f;
        lbTag = lbTag && r.mau32Tag[1] == e.tag && r.mau32Tag[2] == e.tag && r.mau32Tag[3] == e.tag;
        // The unpack's w lane (bits(scale) * scale) rides along on every vertex, as on the console.
        lbW = lbW && r.mVertex0.w != 0.0f && r.mVertex0.w == r.mVertex1.w && r.mVertex1.w == r.mVertex2.w;
    }
    Check(lbOrder, "W2 records come in the asm's batch order (pair lanes 0..3, odd quad, quartet lanes 0..3, odd tris) with each poly's own tag");
    Check(lbVerts, "W3 each record's V0/V1/V2 are the lane's split order (odd-quad lane 1 = (A3,A2,A1))");
    Check(lbT, "W4 +0x50 carries the lane's t splatted to all four lanes");
    Check(lbPos, "W5 +0x40 = S + (E-S)*t on all four lanes");
    Check(lbNormal, "W6 +0x30 = normalize((V1-V0) x (V2-V1)) -- up for these windings, w 0");
    Check(lbTag, "W7 +0x60 = the surface tag splatted over muSurfaceTag and the padding");
    Check(lbW, "W8 the vertices keep the unpack's w lane (not cleaned)");
    Check(gaAsserts.empty(), "W9 no tripwire on a well-formed soup");

    // ---- the limit is tested AFTER each record -------------------------------------------------------
    {
        PolySoupLineNearestResult laCapped[8];
        std::memset(laCapped, 0xCD, sizeof(laCapped));
        const s32 liN = CgsGeometric::IntersectLinePolygonSoupSingleSided(s.mSoup, lStart, lEnd, laCapped, 3);
        u8 lau8Sentinel[sizeof(PolySoupLineNearestResult)];
        std::memset(lau8Sentinel, 0xCD, sizeof(lau8Sentinel));
        Check(liN == 3 && laCapped[2].mau32Tag[0] == 0xC2C20002u
              && std::memcmp(&laCapped[3], lau8Sentinel, sizeof(lau8Sentinel)) == 0,
              "L1 max 3 -> three records (Q0, Q1, Q2), the fourth slot untouched, return 3");
    }
    {
        PolySoupLineNearestResult laCapped[4];
        std::memset(laCapped, 0xCD, sizeof(laCapped));
        const s32 liN = CgsGeometric::IntersectLinePolygonSoupSingleSided(s.mSoup, lStart, lEnd, laCapped, 0);
        u8 lau8Sentinel[sizeof(PolySoupLineNearestResult)];
        std::memset(lau8Sentinel, 0xCD, sizeof(lau8Sentinel));
        Check(liN == 1 && laCapped[0].mau32Tag[0] == 0xA0A00000u
              && std::memcmp(&laCapped[1], lau8Sentinel, sizeof(lau8Sentinel)) == 0,
              "L2 max 0 -> the console still writes ONE record before its test (returns 1)");
    }
    {
        PolySoupLineNearestResult laCapped[8];
        const s32 liN = CgsGeometric::IntersectLinePolygonSoupSingleSided(s.mSoup, lStart, lEnd, laCapped, 7);
        Check(liN == 7 && laCapped[6].mau32Tag[0] == 0x28280008u, "L3 max == the hit count -> all seven, return 7");
    }

    // ---- misses -----------------------------------------------------------------------------------------
    {
        PolySoupLineNearestResult laNone[4];
        std::memset(laNone, 0xCD, sizeof(laNone));
        const Vector3 lHighEnd = Vector3{ 16.25f, 45.0f, 16.25f, 0.0f };   // stops above every floor (t > 1)
        Check(CgsGeometric::IntersectLinePolygonSoupSingleSided(s.mSoup, lStart, lHighEnd, laNone, 32) == 0,
              "M1 a segment that ends above every floor finds nothing");
        const Vector3 lUpStart = Vector3{ 16.25f, 22.0f, 16.25f, 0.0f };   // the same line walked UPWARD
        Check(CgsGeometric::IntersectLinePolygonSoupSingleSided(s.mSoup, lUpStart, lStart, laNone, 32) == 1,
              "M2 walked upward only the one DOWN-wound triangle (T1) faces the line: single-sided");
        Check(laNone[0].mau32Tag[0] == 0xE4E40004u && Near(laNone[0].mNormal.y, -1.0f),
              "M3 ... and its record carries T1's tag and its own (down) normal");
    }

    std::printf("FxGeometricSoupSingleSided: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
