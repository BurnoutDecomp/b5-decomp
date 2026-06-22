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
// The element type Triangle4 is a 224-byte (0xE0) record reconstructed in its own TU
// (CgsGeometric::Triangle4 — CgsTriangle4.cpp); it is referenced here only by byte
// stride + the external AssertIsValid validator, so it is NOT redefined in this home.
#include "types.hpp"

namespace CgsGeometric
{
    namespace Triangle4
    {
        // Debug validator for one Triangle4 record, homed in CgsTriangle4's TU
        // (forwards to AOSTriangle::AssertIsValid). The X360 takes the record address
        // as a 32-bit int; modelled host-portably as a pointer-to-record.
        // DEP-FLAG: defined in another group's TU (class:CgsGeometric::Triangle4),
        // declared extern here so this home stays self-contained.
        int AssertIsValid(void* lpTriangle);
    }
}

namespace CgsSceneManager
{
namespace CgsCollision
{
    // Stride of one Triangle4 record in the collision triangle array (0xE0 bytes),
    // taken from the `v3 += 224` cursor advance in ValidateTriangles.
    const s32 KI_TRIANGLE4_STRIDE = 224;

    struct TriangleList
    {
        u8* mpTriangles;     // +0x00  base of the contiguous Triangle4 array (byte cursor)
        s32 miNumTriangles;  // +0x04  number of Triangle4 records

        // CheckAlignment @ 0x825B3858 — assert the triangle base pointer is 16-byte
        // aligned (fires "Triangles not aligned to 16 bytes: <ptr>").
        void CheckAlignment() const;

        // ValidateTriangles @ 0x825C0C28 — run Triangle4::AssertIsValid over every
        // record in the list (debug validation; no-op when the list is empty).
        void ValidateTriangles() const;
    };
}
}
