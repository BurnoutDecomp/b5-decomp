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
}
