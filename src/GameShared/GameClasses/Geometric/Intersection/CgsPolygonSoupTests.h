#pragma once

// ============================================================================
// GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h
//
// The CgsGeometric polygon-soup intersection free functions that have callers
// outside their own TU. Declarations only; the bodies live in
// CgsPolygonSoupTests.cpp and are cited there by X360 address.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"

namespace CgsGeometric
{
    struct PolygonSoup;
    struct Sphere;
    struct Triangle4;

    // ExtractTriangle4ListIntersectingSphere @0x82844C80 (602)
    //
    // Walk every polygon of one soup, keep the triangles whose surface actually
    // intersects lSphere, and pack them four-at-a-time into lpTriangle4Buffer.
    // Returns the number of Triangle4 BATCHES written (0..liBufferSize) and, if
    // lpiOutOverun is non-null, stores there the number of times the buffer ran
    // out of room and the last slot had to be overwritten.
    //
    // Signature and parameter names from the PS3 DWARF (twin @0xB66A5C,
    //  _ZN12CgsGeometric38ExtractTriangle4ListIntersectingSphereERKNS_11PolygonSoupE
    //  RKNS_6SphereEPNS_9Triangle4EiPi).
    // ⚠️ The DWARF types the return `void`; the X360 caller FillTriangleCache
    // @0x829162C0 does `lwz r11,var_60 / add r11,r11,r3 / stw r11,var_60`, so the
    // return IS the batch count. The asm wins.
    s32 ExtractTriangle4ListIntersectingSphere(const PolygonSoup& lPolygonSoup,
                                               const Sphere&      lSphere,
                                               Triangle4*         lpTriangle4Buffer,
                                               s32                liBufferSize,
                                               s32*               lpiOutOverun);

    // UnpackPolygonSoupVertices @0x8283B480 (40)
    //
    // Expand the soup's packed 6-byte vertices into 16-byte Vector3s:
    //   world.xyz = ((s32)soup.miPos.xyz + (u32)u16be(vertex.xyz)) * soup.mfScale
    // lpOutVertexPositions must have room for soup.GetNumVertices() entries.
    void UnpackPolygonSoupVertices(Vector3*           lpOutVertexPositions,
                                   const PolygonSoup& lPolygonSoup);

    // ------------------------------------------------------------------------------------------
    // ADDED 2026-09-02 (scene-query wave 1b): the NEAREST single-sided line-vs-soup family, the
    // synchronous kernel under BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest
    // @0x828131C0 (every race car's above-ground ray). Bodies in CgsPolygonSoupTests_LineNearest.cpp.
    // ------------------------------------------------------------------------------------------

    // The 112-byte record IntersectLinePolygonSoupNearestSingleSided writes through its r4 (seven
    // `stvx128` at +0x00/+0x10/+0x20/+0x30/+0x40/+0x50 and one `stvlx128` at +0x60, read off
    // 0x8283C4E0..0x8283C598). It is what CollideLineAgainstPolySoupListNearest copies (14 qwords)
    // into the CollisionResultList's single CollisionResult, and what the SceneManagerModule's
    // type-2 poster reads back at +0x40 / +0x30 / +0x50 / +0x60. HOST NAME: the PS3 DWARF name
    // for this struct was not recovered this wave; the layout is the asm's.
    struct alignas(16) PolySoupLineNearestResult
    {
        Vector3 mVertex0;        // +0x00  winning triangle, in the kernel's (V0,V1,V2) order
        Vector3 mVertex1;        // +0x10
        Vector3 mVertex2;        // +0x20
        Vector3 mNormal;         // +0x30  normalize((V1-V0) x (V2-V1)) -- faces the line's start
        Vector3 mPosition;       // +0x40  start + (end - start) * t
        Vector4 mLineParam;      // +0x50  t, splatted to all four lanes (2.0 == no hit)
        u32     mau32Tag[4];     // +0x60  the poly's surface tag, splatted (material hi16 / group lo16)
    };
    static_assert(sizeof(PolySoupLineNearestResult) == 112,
                  "PolySoupLineNearestResult is the 112-byte nearest-hit record (14 qwords copied)");

    // IntersectLinePolySoupTriangleSingleSided4 @0x8283B520 (152)
    //
    // Four triangles at once (lane k = triangle k of the three arrays), one segment. Plane-crossing
    // parameter t = ((V0-S).n) / ((E-S).n) with n = (V1-V0) x (V2-V1), then three edge functions
    // (P-Vi).(ei x n) that must share a sign, single-sided ((E-S).n <= 0), t in [0,1]. Writes lane
    // k's t to lafOutT[k] (the console writes it splatted to the k'th out slot) and returns the hit
    // mask, bit k == lane k (the console returns it as a 4-lane all-ones/zeros vector in v1).
    u32 IntersectLinePolySoupTriangleSingleSided4(const Vector3  laV0[4],
                                                  const Vector3  laV1[4],
                                                  const Vector3  laV2[4],
                                                  const Vector3& lStart,
                                                  const Vector3& lEnd,
                                                  f32            lafOutT[4]);

    // IntersectLinePolygonSoupNearestSingleSided @0x8283BC98 (575)
    //
    // Nearest single-sided hit of one segment against one soup: unpack the vertices, run the 4-wide
    // kernel over quad pairs / the odd quad / triangle quartets / the odd triangles, keep the lowest
    // t (seeded 2.0; ties go to the LATER triangle -- `vcmpgefp best, t`), then fill lpOutResult.
    // Returns true iff 1.0 >= best t (the console returns that compare splatted in v1). The line
    // arrives in v1 (start) / v2 (end) on the console.
    bool IntersectLinePolygonSoupNearestSingleSided(const PolygonSoup&          lPolygonSoup,
                                                    PolySoupLineNearestResult*  lpOutResult,
                                                    const Vector3&              lStart,
                                                    const Vector3&              lEnd);
}
