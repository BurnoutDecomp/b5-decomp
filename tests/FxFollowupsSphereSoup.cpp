// FX-FOLLOWUPS item 2 (crash parity 2026-09-25): the WORLD arm of the director's camera sphere --
//   CgsGeometric::TestSpherePolygonSoup @0x828455E8                         (an export hole)
//   BaseCollisionGenerator::TestSphereAgainstPolySoupList @0x82812950        (an export hole)
// run_fxfollowups_sphere_soup.py pastes the PRODUCTION text:
//   fxfu_soup.inc   UnpackPolygonSoupVertices, TestSphereTriangle4SOA and TestSpherePolygonSoup with
//                   its file-local helpers (CgsPolygonSoupTests.cpp)
//   fxfu_list.inc   LeafOverlapsBoxXYZ + BaseCollisionGenerator::TestSphereAgainstPolySoupList
//                   (CgsCollisionGenerator.cpp)
// and compiles CgsPolygonSoup.cpp (GetPolygon / GetVertex) and CgsSphere.cpp (GetPosition /
// GetRadius) alongside. The generator, its result lists and the spatial map are small fixtures;
// the soup kernel under the driver is the production one behind a counting shim.
//
// Every expectation comes from the ARTIST decode (see the two banners in production):
//   * the soup walk: quad PAIRS -> lanes (A0,A1,A2) (A3,A2,A1) (B0,B1,B2) (B3,B2,B1); the odd quad ->
//     (Q0,Q1,Q2) (Q3,Q2,Q1); triangle QUARTETS -> lane k = triangle k; odd triangles one per call;
//     numTris = (u8)(numPolys - numQuads);
//   * the answer: v126 (every lane 0xFFFFFFFF) at the first hit lane, v127 (every lane 0) otherwise;
//   * THE VERTEX ORDER IS PINNED THROUGH THE CONSOLE'S OWN TestSphereTriangle4SOA BUG: the fourth arm
//     of its minimum cascade tests vertex B where D (= P2) is the closest, so a sphere that misses a
//     triangle near its P2 corner, with |P2-C| <= |P1-C| <= |P0-C| and only the vertex-P2 axis
//     separating, reads as a HIT. Each "bug" scene below is a true geometric MISS (checked with an
//     independent closest-point routine) that the console reports as a HIT, and that the reversed
//     vertex order reports as a miss -- so only the console's lane order passes;
//   * the driver: PrepareNewPrimitiveTestResultsList(1, tagA, tagB); the box = centre -/+ splat(r) in
//     ALL four lanes; leaves in GetOutputQueryBuffer() order; leaf.max >= box.min && box.max >=
//     leaf.min on x/y/z (touching counts); the kernel gets the local COPY of the sphere; count 1 and
//     return at the first soup hit, count 0 otherwise; the return is Prepare's index.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/math/vpu/types.h"
#include "rw/math/vpu/vector4_operation.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace CgsGeometric
{
#include "fxfu_soup.inc"
}

// ---- the driver's fixtures ----------------------------------------------------------------------
namespace CgsGeometric
{
    // The spatial map: RunQuery records the box it was handed and answers with the scripted leaf list.
    struct PolygonSoupListSpatialMap
    {
        std::vector<PolygonSoupLeafNode> maLeaves;
        std::vector<u16>                 mau16Output;
        AxisAlignedBox                   mLastQueryBox;
        s32                              miNumQueries = 0;

        s32 RunQuery(const AxisAlignedBox& lrBox)
        {
            mLastQueryBox = lrBox;
            ++miNumQueries;
            return static_cast<s32>(mau16Output.size());
        }
        const PolygonSoupLeafNode* GetLeafNodes() const { return maLeaves.data(); }
        const u16* GetOutputQueryBuffer() const { return mau16Output.data(); }
    };

    // The counting shim the driver's call to TestSpherePolygonSoup is routed through (macro below):
    // it records which soup was asked, with which sphere ADDRESS, then asks the production kernel.
    static std::vector<const PolygonSoup*> gapSoupsAsked;
    static const Sphere*                   gpLastKernelSphere = nullptr;
    rw::math::vpu::MaskScalar CountedTestSpherePolygonSoup(const PolygonSoup& lrSoup, const Sphere& lrSphere)
    {
        gapSoupsAsked.push_back(&lrSoup);
        gpLastKernelSphere = &lrSphere;
        return TestSpherePolygonSoup(lrSoup, lrSphere);
    }
}

namespace CgsSceneManager { namespace CgsCollision {
    struct CollisionResultList
    {
        u16 mu16NumResults = 0xBEEF;
        CollisionResultList* SetNumResults(s32 liNumResults) { mu16NumResults = static_cast<u16>(liNumResults); return this; }
    };

    struct BaseCollisionGenerator
    {
        CollisionResultList* mapCollisionResultLists[200] = {};
        CollisionResultList  maLists[200];
        s32 miNextIndex = 7;
        s32 miPrepares = 0;
        u16 mu16LastMax = 0; u32 mu32LastTagA = 0; u16 mu16LastTagB = 0;

        BaseCollisionGenerator() { for (int i = 0; i < 200; ++i) mapCollisionResultLists[i] = &maLists[i]; }

        s32 PrepareNewPrimitiveTestResultsList(u16 lu16Max, u32 luTagA, u16 lu16TagB)
        {
            ++miPrepares; mu16LastMax = lu16Max; mu32LastTagA = luTagA; mu16LastTagB = lu16TagB;
            return miNextIndex;
        }

        u16 TestSphereAgainstPolySoupList(const CgsGeometric::Sphere*              lpSphere,
                                          CgsGeometric::PolygonSoupListSpatialMap* lpSpatialData,
                                          u32                                      luUserTagA,
                                          u16                                      lu16UserTagB);
    };

#define TestSpherePolygonSoup CountedTestSpherePolygonSoup
#include "fxfu_list.inc"
#undef TestSpherePolygonSoup
} }

using CgsGeometric::PolygonSoup;
using CgsGeometric::PolygonSoupPoly;
using CgsGeometric::Sphere;

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

// ---- a soup builder -------------------------------------------------------------------------------
// World = (packed - 1024) / 16: origin -1024 quanta, scale 1/16, so integer world coordinates in
// [-64, 4031] pack into the zero-extended u16 the console unpacks.
struct SoupBuilder
{
    std::vector<u16>             mau16Packed;
    std::vector<PolygonSoupPoly> maPolys;
    std::vector<Vector3>         maWorld;
    PolygonSoup                  mSoup;

    u8 Vertex(f32 x, f32 y, f32 z)
    {
        maWorld.push_back(Vector3{ x, y, z, 0.0f });
        mau16Packed.push_back(static_cast<u16>(x * 16.0f + 1024.0f));
        mau16Packed.push_back(static_cast<u16>(y * 16.0f + 1024.0f));
        mau16Packed.push_back(static_cast<u16>(z * 16.0f + 1024.0f));
        return static_cast<u8>(maWorld.size() - 1);
    }
    void Poly(u8 a, u8 b, u8 c, u8 d)
    {
        PolygonSoupPoly lPoly;
        std::memset(&lPoly, 0, sizeof(lPoly));
        lPoly.muSurfaceTag = 0x1234u;
        lPoly.mau8VertexIndex[0] = a; lPoly.mau8VertexIndex[1] = b;
        lPoly.mau8VertexIndex[2] = c; lPoly.mau8VertexIndex[3] = d;
        maPolys.push_back(lPoly);
    }
    // A horizontal unit quad in the y = 0 plane at (x0, z0): V0 (x0,z0), V1 (x0,z0+1), V2 (x0+1,z0),
    // V3 (x0+1,z0+1) -- the 0-1-3-2 perimeter order CgsPolygonSoup.h records.
    void Quad(f32 x0, f32 z0)
    {
        const u8 v0 = Vertex(x0, 0.0f, z0), v1 = Vertex(x0, 0.0f, z0 + 1.0f);
        const u8 v2 = Vertex(x0 + 1.0f, 0.0f, z0), v3 = Vertex(x0 + 1.0f, 0.0f, z0 + 1.0f);
        Poly(v0, v1, v2, v3);
    }
    // A triangle from three explicit world vertices, in that index order.
    void Tri(const Vector3& a, const Vector3& b, const Vector3& c)
    {
        const u8 i0 = Vertex(a.x, a.y, a.z), i1 = Vertex(b.x, b.y, b.z), i2 = Vertex(c.x, c.y, c.z);
        Poly(i0, i1, i2, 0xFF);
    }
    // The plain unit triangle at (x0, z0): (x0,z0) (x0,z0+1) (x0+1,z0).
    void UnitTri(f32 x0, f32 z0)
    {
        Tri(Vector3{ x0, 0.0f, z0, 0.0f }, Vector3{ x0, 0.0f, z0 + 1.0f, 0.0f }, Vector3{ x0 + 1.0f, 0.0f, z0, 0.0f });
    }
    PolygonSoup& Finish(u8 lu8NumQuads)
    {
        std::memset(&mSoup, 0, sizeof(mSoup));
        mSoup.miPosX = mSoup.miPosY = mSoup.miPosZ = -1024;
        mSoup.mfScale = 1.0f / 16.0f;
        mSoup.mpPolygons = reinterpret_cast<u8*>(maPolys.data());
        mSoup.mpVertices = reinterpret_cast<u8*>(mau16Packed.data());
        mSoup.mu8NumPolygons = static_cast<u8>(maPolys.size());
        mSoup.mu8NumQuads = lu8NumQuads;
        mSoup.mu8NumVertices = static_cast<u8>(maWorld.size());
        return mSoup;
    }
};

static Sphere MakeSphere(f32 x, f32 y, f32 z, f32 r)
{
    Sphere lSphere;
    lSphere.mPositionRadius = Vector4{ x, y, z, r };
    return lSphere;
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, sizeof(lu)); return lu; }

// 1 = every lane 0xFFFFFFFF, 0 = every lane zero, -1 = anything else.
static int AnswerKind(const rw::math::vpu::MaskScalar& lrMask)
{
    const u32 x = Bits(lrMask.x), y = Bits(lrMask.y), z = Bits(lrMask.z), w = Bits(lrMask.w);
    if (x == 0xFFFFFFFFu && y == x && z == x && w == x) return 1;
    if (x == 0u && y == 0u && z == 0u && w == 0u) return 0;
    return -1;
}

// Independent geometry (Ericson 5.1.5 ClosestPtPointTriangle) -- only used to prove a "bug" scene
// is a TRUE miss.
static f32 Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vector3 Sub(const Vector3& a, const Vector3& b) { return Vector3{ a.x - b.x, a.y - b.y, a.z - b.z, 0.0f }; }
static Vector3 Mad(const Vector3& a, const Vector3& b, f32 t) { return Vector3{ a.x + b.x * t, a.y + b.y * t, a.z + b.z * t, 0.0f }; }
static f32 DistanceToTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c)
{
    const Vector3 ab = Sub(b, a), ac = Sub(c, a), ap = Sub(p, a);
    const f32 d1 = Dot(ab, ap), d2 = Dot(ac, ap);
    Vector3 q;
    if (d1 <= 0.0f && d2 <= 0.0f) q = a;
    else
    {
        const Vector3 bp = Sub(p, b);
        const f32 d3 = Dot(ab, bp), d4 = Dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) q = b;
        else
        {
            const f32 vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) q = Mad(a, ab, d1 / (d1 - d3));
            else
            {
                const Vector3 cp = Sub(p, c);
                const f32 d5 = Dot(ab, cp), d6 = Dot(ac, cp);
                if (d6 >= 0.0f && d5 <= d6) q = c;
                else
                {
                    const f32 vb = d5 * d2 - d1 * d6;
                    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) q = Mad(a, ac, d2 / (d2 - d6));
                    else
                    {
                        const f32 va = d3 * d6 - d5 * d4;
                        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
                            q = Mad(b, Sub(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6)));
                        else
                        {
                            const f32 denom = 1.0f / (va + vb + vc);
                            q = Mad(Mad(a, ab, vb * denom), ac, vc * denom);
                        }
                    }
                }
            }
        }
    }
    const Vector3 d = Sub(p, q);
    return std::sqrt(Dot(d, d));
}

// The production 4-wide kernel on ONE triangle in ONE vertex order (lane 0 read).
static bool ConsoleTriangleHit(const Sphere& lrSphere, const Vector3& p0, const Vector3& p1, const Vector3& p2)
{
    const Vector4 x0{ p0.x, p0.x, p0.x, p0.x }, y0{ p0.y, p0.y, p0.y, p0.y }, z0{ p0.z, p0.z, p0.z, p0.z };
    const Vector4 x1{ p1.x, p1.x, p1.x, p1.x }, y1{ p1.y, p1.y, p1.y, p1.y }, z1{ p1.z, p1.z, p1.z, p1.z };
    const Vector4 x2{ p2.x, p2.x, p2.x, p2.x }, y2{ p2.y, p2.y, p2.y, p2.y }, z2{ p2.z, p2.z, p2.z, p2.z };
    const CgsGeometric::Triangle4::Mask4 lMask =
        CgsGeometric::TestSphereTriangle4SOA(lrSphere, x0, y0, z0, x1, y1, z1, x2, y2, z2);
    return lMask.x != 0.0f;
}

// The "bug" corner, relative to a triangle (T0, T1, T2) = (P + (1,0,0), P + (1,0,-1), P): the centre
// C = P + (-2, 0, -0.55), r = 1.9 sits in P's vertex region with |T2-C| 2.074 <= |T1-C| 3.034 <=
// |T0-C| 3.050, both edge lines through P within r (0.55 and 1.803) and P itself out of reach.
static const f32 KF_BUG_RADIUS = 1.9f;
static Vector3 BugCentre(const Vector3& lrP) { return Vector3{ lrP.x - 2.0f, lrP.y, lrP.z - 0.55f, 0.0f }; }

int main()
{
    // ================================================================================================
    // K: TestSpherePolygonSoup
    // ================================================================================================

    // ---- K1: one soup with every block kind: a quad pair, the odd quad, a triangle quartet and three
    // odd triangles, spread along x so a small sphere touches exactly one triangle.
    SoupBuilder lAll;
    lAll.Quad(0.0f, 0.0f);      // A  (pair)
    lAll.Quad(3.0f, 0.0f);      // B  (pair)
    lAll.Quad(6.0f, 0.0f);      // Q  (odd quad)
    lAll.UnitTri(9.0f, 0.0f);   // T0 (quartet)
    lAll.UnitTri(12.0f, 0.0f);  // T1
    lAll.UnitTri(15.0f, 0.0f);  // T2
    lAll.UnitTri(18.0f, 0.0f);  // T3
    lAll.UnitTri(21.0f, 0.0f);  // T4 (odd)
    lAll.UnitTri(24.0f, 0.0f);  // T5 (odd)
    lAll.UnitTri(27.0f, 0.0f);  // T6 (odd)
    const PolygonSoup& lrAll = lAll.Finish(3);

    struct Probe { const char* mpcLabel; f32 x, z; };
    // (0.2,0.2) touches only the (V0,V1,V2) half of a quad; (0.8,0.8) only the (V3,V2,V1) half.
    const Probe laHits[] =
    {
        { "K1 pair lane 0 (A0,A1,A2) hit -> all-ones",   0.2f,  0.2f },
        { "K1 pair lane 1 (A3,A2,A1) hit -> all-ones",   0.8f,  0.8f },
        { "K1 pair lane 2 (B0,B1,B2) hit -> all-ones",   3.2f,  0.2f },
        { "K1 pair lane 3 (B3,B2,B1) hit -> all-ones",   3.8f,  0.8f },
        { "K1 odd quad lane 0 (Q0,Q1,Q2) hit -> all-ones", 6.2f, 0.2f },
        { "K1 odd quad lane 1 (Q3,Q2,Q1) hit -> all-ones", 6.8f, 0.8f },
        { "K1 quartet lane 0 hit -> all-ones",           9.2f,  0.2f },
        { "K1 quartet lane 1 hit -> all-ones",          12.2f,  0.2f },
        { "K1 quartet lane 2 hit -> all-ones",          15.2f,  0.2f },
        { "K1 quartet lane 3 hit -> all-ones",          18.2f,  0.2f },
        { "K1 odd triangle 1 of 3 hit -> all-ones",     21.2f,  0.2f },
        { "K1 odd triangle 2 of 3 hit -> all-ones",     24.2f,  0.2f },
        { "K1 odd triangle 3 of 3 hit -> all-ones",     27.2f,  0.2f },
    };
    for (const Probe& lrProbe : laHits)
    {
        const Sphere lSphere = MakeSphere(lrProbe.x, 0.0f, lrProbe.z, 0.1f);
        Check(AnswerKind(CgsGeometric::TestSpherePolygonSoup(lrAll, lSphere)) == 1, lrProbe.mpcLabel);
    }

    // ---- K2: misses read every lane zero (v127).
    Check(AnswerKind(CgsGeometric::TestSpherePolygonSoup(lrAll, MakeSphere(40.0f, 0.0f, 0.2f, 0.5f))) == 0,
          "K2 a sphere beyond every polygon -> every lane zero");
    Check(AnswerKind(CgsGeometric::TestSpherePolygonSoup(lrAll, MakeSphere(0.5f, 0.6f, 0.5f, 0.5f))) == 0,
          "K2 a sphere above the pair, clear of the plane -> every lane zero");
    Check(AnswerKind(CgsGeometric::TestSpherePolygonSoup(lrAll, MakeSphere(1.5f, 0.0f, 0.5f, 0.4f))) == 0,
          "K2 a sphere in the gap between A and B -> every lane zero");

    // (No empty-soup case: the console calls GetPolygon(0) unconditionally at 0x82845678, whose bounds
    // tripwire fires for a soup with no polygons -- on the console too -- and no real soup is empty.)

    // ---- K4: THE VERTEX ORDER, through the console's minimum-cascade bug. First prove the scene: for
    // the unit triangle (P+(1,0,0), P+(1,0,-1), P) and the bug centre, the geometry is a MISS, the
    // console order reads a HIT and the reversed order a miss.
    {
        const Vector3 lP{ 100.0f, 0.0f, 100.0f, 0.0f };
        const Vector3 lT0{ lP.x + 1.0f, 0.0f, lP.z, 0.0f }, lT1{ lP.x + 1.0f, 0.0f, lP.z - 1.0f, 0.0f };
        const Vector3 lC = BugCentre(lP);
        const Sphere lSphere = MakeSphere(lC.x, lC.y, lC.z, KF_BUG_RADIUS);
        Check(DistanceToTriangle(lC, lT0, lT1, lP) > KF_BUG_RADIUS,
              "K4 the bug scene is a TRUE miss (closest point |P-C| 2.074 > r 1.9)");
        Check(ConsoleTriangleHit(lSphere, lT0, lT1, lP),
              "K4 TestSphereTriangle4SOA in the order (T0,T1,P) reads it as a HIT (the fourth-arm bug)");
        Check(!ConsoleTriangleHit(lSphere, lP, lT1, lT0),
              "K4 the reversed order (P,T1,T0) reads it as a miss (so the order is observable)");
    }

    // K5: the same corner inside each block -- only the console's lane order reads the hit.
    //   pair lane 1 = (A3,A2,A1): A1 is quad A's (x0, z0+1) corner, with A3 / A2 at +(1,0,0) / +(1,0,-1)
    //   pair lane 3 = (B3,B2,B1): the same on quad B
    //   odd quad lane 1 = (Q3,Q2,Q1)
    // Each quad is placed alone at the far end of its own soup so the other half-quad is out of reach
    // (checked: the (V0,V1,V2) half is 2.0 > r from the centre).
    {
        // A pair whose SECOND quad carries the corner; the first quad is far away.
        SoupBuilder lPair;
        lPair.Quad(500.0f, 500.0f);
        lPair.Quad(200.0f, 200.0f);          // B: B1 = (200, 0, 201)
        const PolygonSoup& lrPair = lPair.Finish(2);
        const Vector3 lB1{ 200.0f, 0.0f, 201.0f, 0.0f };
        const Vector3 lC = BugCentre(lB1);
        const Sphere lSphere = MakeSphere(lC.x, lC.y, lC.z, KF_BUG_RADIUS);
        Check(DistanceToTriangle(lC, Vector3{ 200.0f, 0.0f, 200.0f, 0.0f }, lB1, Vector3{ 201.0f, 0.0f, 200.0f, 0.0f }) > KF_BUG_RADIUS &&
              DistanceToTriangle(lC, Vector3{ 201.0f, 0.0f, 201.0f, 0.0f }, Vector3{ 201.0f, 0.0f, 200.0f, 0.0f }, lB1) > KF_BUG_RADIUS,
              "K5 pair lane 3 scene: both halves of quad B are TRUE misses");
        Check(AnswerKind(CgsGeometric::TestSpherePolygonSoup(lrPair, lSphere)) == 1,
              "K5 pair lane 3 (B3,B2,B1): the console order reads the bug corner as a HIT");

        SoupBuilder lPairA;
        lPairA.Quad(200.0f, 200.0f);         // A: A1 = (200, 0, 201)
        lPairA.Quad(500.0f, 500.0f);
        const PolygonSoup& lrPairA = lPairA.Finish(2);
        Check(AnswerKind(CgsGeometric::TestSpherePolygonSoup(lrPairA, lSphere)) == 1,
              "K5 pair lane 1 (A3,A2,A1): the console order reads the bug corner as a HIT");

        SoupBuilder lOdd;
        lOdd.Quad(200.0f, 200.0f);           // the odd quad alone
        const PolygonSoup& lrOdd = lOdd.Finish(1);
        Check(AnswerKind(CgsGeometric::TestSpherePolygonSoup(lrOdd, lSphere)) == 1,
              "K5 odd quad lane 1 (Q3,Q2,Q1): the console order reads the bug corner as a HIT");

        // Triangles: the poly's own (0,1,2) order with P last -- in the quartet (slot 2 of 4) and odd.
        const Vector3 lP{ 300.0f, 0.0f, 300.0f, 0.0f };
        const Vector3 lT0{ 301.0f, 0.0f, 300.0f, 0.0f }, lT1{ 301.0f, 0.0f, 299.0f, 0.0f };
        const Vector3 lCt = BugCentre(lP);
        const Sphere lTriSphere = MakeSphere(lCt.x, lCt.y, lCt.z, KF_BUG_RADIUS);

        SoupBuilder lQuartet;
        lQuartet.UnitTri(600.0f, 600.0f);
        lQuartet.UnitTri(620.0f, 600.0f);
        lQuartet.Tri(lT0, lT1, lP);          // lane 2
        lQuartet.UnitTri(640.0f, 600.0f);
        const PolygonSoup& lrQuartet = lQuartet.Finish(0);
        Check(AnswerKind(CgsGeometric::TestSpherePolygonSoup(lrQuartet, lTriSphere)) == 1,
              "K5 quartet lane 2 (T.0,T.1,T.2): the console order reads the bug corner as a HIT");

        SoupBuilder lOddTri;
        lOddTri.UnitTri(600.0f, 600.0f);
        lOddTri.Tri(lT0, lT1, lP);           // the 2nd of 2 odd triangles
        const PolygonSoup& lrOddTri = lOddTri.Finish(0);
        Check(AnswerKind(CgsGeometric::TestSpherePolygonSoup(lrOddTri, lTriSphere)) == 1,
              "K5 odd triangle (T.0,T.1,T.2): the console order reads the bug corner as a HIT");

        // And the same triangle stored REVERSED is (correctly) a miss -- the kernel keeps the poly's order.
        SoupBuilder lReversed;
        lReversed.Tri(lP, lT1, lT0);
        const PolygonSoup& lrReversed = lReversed.Finish(0);
        Check(AnswerKind(CgsGeometric::TestSpherePolygonSoup(lrReversed, lTriSphere)) == 0,
              "K5 the same triangle stored (P,T1,T0) is a miss: the kernel presents the poly's own order");
    }

    // ================================================================================================
    // L: BaseCollisionGenerator::TestSphereAgainstPolySoupList
    // ================================================================================================
    // Three soups: a far one (no hit), the K1 soup (hit at (0.2,0,0.2)), another far one.
    SoupBuilder lFar1; lFar1.Quad(50.0f, 50.0f); const PolygonSoup& lrFar1 = lFar1.Finish(1);
    SoupBuilder lFar2; lFar2.Quad(70.0f, 70.0f); const PolygonSoup& lrFar2 = lFar2.Finish(1);

    auto Leaf = [](const PolygonSoup* lpSoup, f32 lx0, f32 lx1)
    {
        CgsGeometric::PolygonSoupLeafNode lLeaf;
        std::memset(&lLeaf, 0, sizeof(lLeaf));
        lLeaf.mBox.mMin = Vector4{ lx0, -1.0f, -1.0f, 0.0f };
        lLeaf.mBox.mMax = Vector4{ lx1,  1.0f,  2.0f, 0.0f };
        lLeaf.mpPolygonSoup = lpSoup;
        return lLeaf;
    };

    {
        CgsSceneManager::CgsCollision::BaseCollisionGenerator lGen;
        CgsGeometric::PolygonSoupListSpatialMap lMap;
        // leaf 0: Far1, box x [0.3+, ...] -- its min.x equals the query box's max.x (touching counts)
        // leaf 1: the K1 soup, box x [-1, 30]
        // leaf 2: Far2, box x [0.31, 40] -- separated from the query box on x
        const Sphere lSphere = MakeSphere(0.2f, 0.0f, 0.2f, 0.1f);
        const f32 lfBoxMaxX = 0.2f + 0.1f;
        lMap.maLeaves.push_back(Leaf(&lrFar1, lfBoxMaxX, 60.0f));
        lMap.maLeaves.push_back(Leaf(&lrAll, -1.0f, 30.0f));
        lMap.maLeaves.push_back(Leaf(&lrFar2, lfBoxMaxX + 0.01f, 80.0f));
        lMap.mau16Output = { 2, 0, 1 };      // the output order the kernel must follow

        CgsGeometric::gapSoupsAsked.clear();
        const u16 lu16Index = lGen.TestSphereAgainstPolySoupList(&lSphere, &lMap, 0xA5A5A5A5u, 0x5A5Au);

        Check(lGen.miPrepares == 1 && lGen.mu16LastMax == 1 && lGen.mu32LastTagA == 0xA5A5A5A5u && lGen.mu16LastTagB == 0x5A5Au,
              "L1 PrepareNewPrimitiveTestResultsList(1, tagA, tagB) once (0x8281297C: r4 = 1, r5 <- r6, r6 <- r7)");
        Check(lu16Index == 7, "L1 returns Prepare's list index (`mr r3, r24`)");
        const CgsGeometric::AxisAlignedBox& lrBox = lMap.mLastQueryBox;
        Check(lMap.miNumQueries == 1 &&
              lrBox.mMin.x == 0.2f - 0.1f && lrBox.mMin.y == 0.0f - 0.1f && lrBox.mMin.z == 0.2f - 0.1f && lrBox.mMin.w == 0.1f - 0.1f &&
              lrBox.mMax.x == 0.2f + 0.1f && lrBox.mMax.y == 0.0f + 0.1f && lrBox.mMax.z == 0.2f + 0.1f && lrBox.mMax.w == 0.1f + 0.1f,
              "L2 RunQuery gets centre -/+ splat(r) in ALL four lanes (vsubfp 0x828129B0 / vaddfp 0x828129B4)");
        Check(CgsGeometric::gapSoupsAsked.size() == 2 && CgsGeometric::gapSoupsAsked[0] == &lrFar1 &&
              CgsGeometric::gapSoupsAsked[1] == &lrAll,
              "L3 leaves walked in output order; the separated leaf is skipped, the TOUCHING one is tested (>=), the hit ends the walk");
        Check(lGen.maLists[7].mu16NumResults == 1, "L4 a soup hit sets the list's count to 1 (0x82812AB0)");
        Check(CgsGeometric::gpLastKernelSphere != &lSphere,
              "L5 the kernel is handed the local COPY of the sphere (sp+0x60), not the caller's");
    }

    {
        CgsSceneManager::CgsCollision::BaseCollisionGenerator lGen;
        lGen.miNextIndex = 13;
        CgsGeometric::PolygonSoupListSpatialMap lMap;
        const Sphere lSphere = MakeSphere(0.5f, 0.6f, 0.5f, 0.5f);   // K2's clear-of-the-plane sphere
        lMap.maLeaves.push_back(Leaf(&lrAll, -1.0f, 30.0f));
        lMap.maLeaves.push_back(Leaf(&lrFar1, -1.0f, 60.0f));
        lMap.mau16Output = { 0, 1 };
        CgsGeometric::gapSoupsAsked.clear();
        const u16 lu16Index = lGen.TestSphereAgainstPolySoupList(&lSphere, &lMap, 0u, 0u);
        Check(lu16Index == 13 && lGen.maLists[13].mu16NumResults == 0 && CgsGeometric::gapSoupsAsked.size() == 2,
              "L6 no soup hit: every overlapping leaf is asked and the count is 0 (0x82812A88)");
    }

    {
        CgsSceneManager::CgsCollision::BaseCollisionGenerator lGen;
        CgsGeometric::PolygonSoupListSpatialMap lMap;
        const Sphere lSphere = MakeSphere(0.2f, 0.0f, 0.2f, 0.1f);
        CgsGeometric::gapSoupsAsked.clear();
        lGen.TestSphereAgainstPolySoupList(&lSphere, &lMap, 0u, 0u);
        Check(lGen.maLists[7].mu16NumResults == 0 && CgsGeometric::gapSoupsAsked.empty(),
              "L7 an empty query: count 0, no kernel call");
    }

    Check(guAsserts == 0, "no assert fired");

    std::printf("FxFollowupsSphereSoup: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
