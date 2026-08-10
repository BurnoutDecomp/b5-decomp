#pragma once

// CgsSceneManager::CgsCollision::TriangleList — a flat list of collision triangles:
// a base pointer to a contiguous array of 224-byte Triangle4 records plus the count.
// Used by the physics world-contact paths (vehicle/deformable/prop) to validate and
// align the triangle buffer before running collision queries.
//
// Layout from the X360 asm (CgsTriangleList.h source path attested by the assert in
// CheckAlignment @ 0x825B3858):
//   +0x00  Triangle4* base pointer (the contiguous triangle array)
//   +0x04  s32        triangle count
//
// ⭐⭐ ODR FORK #2 RETIRED 2026-08-10 (fill-worker wave 2).
//
// This header used to declare `namespace CgsGeometric { namespace Triangle4 {
// int AssertIsValid(void*); } }` -- a NAMESPACE of the same qualified name as the
// struct in CgsTriangle4.h, whose line 96 declares the real `void
// CgsGeometric::Triangle4::AssertIsValid() const`. Two different mangled symbols
// spelled identically: the member wired to nothing, and the free function wired to a
// `{ return 0; }` in CgsTriangleList_embed_check.cpp. So this list's validation pass
// has been a NO-OP since the day it landed, and CgsPolygonSoupTests.cpp (which calls
// the member form) could not be mounted at all.
//
// The reason the fork existed was the belief that the member had no body. IT DOES:
// X360 0x825BD808 (46), with GetAOSTriangle @0x825B2808 and AOSTriangle::IsValid
// @0x825BD208 already reconstructed in CgsTriangle4.cpp. Both AssertIsValid bodies
// are now in that TU and the TU is mounted, so this home calls the REAL member and
// the element type is typed rather than walked as a byte cursor.
#include "types.hpp"
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"  // CgsGeometric::Triangle4 (the ONE definition)

namespace CgsSceneManager
{
namespace CgsCollision
{
    // Stride of one Triangle4 record in the collision triangle array (0xE0 bytes),
    // taken from the `v3 += 224` cursor advance in ValidateTriangles. ⭐ Now gated
    // against the real type rather than trusted as a literal (the console's 224 IS
    // sizeof(Triangle4): it is a pointer-free, runtime-carved SoA float record, so it
    // does not widen on x64 -- confirmed at runtime last wave, `sizeofTriangle4=224`).
    const s32 KI_TRIANGLE4_STRIDE = 224;
    static_assert(sizeof(CgsGeometric::Triangle4) == KI_TRIANGLE4_STRIDE,
                  "Triangle4 stride must stay 224 (pointer-free SoA record; pinnable)");

    struct TriangleList
    {
        CgsGeometric::Triangle4* mpTriangles;  // +0x00  base of the contiguous Triangle4 array
        s32                      miNumTriangles;  // +0x04  number of Triangle4 records

        // CheckAlignment @ 0x825B3858 — assert the triangle base pointer is 16-byte
        // aligned (fires "Triangles not aligned to 16 bytes: <ptr>").
        void CheckAlignment() const;

        // ValidateTriangles @ 0x825C0C28 — run Triangle4::AssertIsValid over every
        // record in the list (debug validation; no-op when the list is empty).
        void ValidateTriangles() const;
    };
}
}
