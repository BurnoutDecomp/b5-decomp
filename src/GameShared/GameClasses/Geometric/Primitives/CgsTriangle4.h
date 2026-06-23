#ifndef CGS_TRIANGLE4_H
#define CGS_TRIANGLE4_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 / Vector4 / VecFloat (rw::math::vpu aliases)

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h
//
// CgsGeometric::Triangle4 -- a SoA (structure-of-arrays) batch of FOUR triangles
// packed for SIMD. Each geometric component is a 16-byte Vector4 holding the
// same component of the four triangles across its x/y/z/w lanes (lane i belongs
// to triangle i). This is the OWNING home for the X360 ARTIST build's Triangle4
// TU; it declares the class shape and the two functions defined in CgsTriangle4.cpp:
//
//     CgsGeometric::Triangle4::GetAOSTriangle      @ 0x825B2808
//     CgsGeometric::Triangle4::AOSTriangle::IsValid @ 0x825BD208
//
// LAYOUT (from the DecFIGS DWARF for this source path; offsets confirmed against
// the GetAOSTriangle asm, which lvx128-loads each component as a 16-byte vector
// at this+0x00, +0x10, ... +0xD0):
//
//   +0x00  Vector4 mVertex0X      vertex 0, x of the four triangles
//   +0x10  Vector4 mVertex0Y      vertex 0, y
//   +0x20  Vector4 mVertex0Z      vertex 0, z
//   +0x30  Vector4 mVertex1X      vertex 1, x
//   +0x40  Vector4 mVertex1Y      vertex 1, y
//   +0x50  Vector4 mVertex1Z      vertex 1, z
//   +0x60  Vector4 mVertex2X      vertex 2, x
//   +0x70  Vector4 mVertex2Y      vertex 2, y
//   +0x80  Vector4 mVertex2Z      vertex 2, z
//   +0x90  Mask4   mValidMasks    per-lane "this lane holds a real triangle" mask
//   +0xA0  Vector4 mSurfaceTags   per-lane surface tag
//   +0xB0  Vector4 mEdge0Cosigns  per-lane edge-0 cosine
//   +0xC0  Vector4 mEdge1Cosigns  per-lane edge-1 cosine
//   +0xD0  Vector4 mEdge2Cosigns  per-lane edge-2 cosine
//   sizeof == 0xE0 (224)
//
// The DWARF spells the mask type `Mask4`; on this PC reconstruction a lane mask
// is just another 16-byte register, so Mask4 is aliased to Vector4 (same layout
// and alignment) to keep member offsets exact.
// ============================================================================

namespace CgsGeometric
{
    // A single triangle extracted out of one SoA lane, in array-of-structs form
    // (the shape GetAOSTriangle writes). Member offsets are proven by the asm's
    // stvx128 / stfs store targets: mVertex0 @+0x00, mVertex1 @+0x10,
    // mVertex2 @+0x20, mNormal @+0x30, then three trailing floats @+0x40/+0x44/+0x48.
    struct Triangle4
    {
        typedef Vector4 Mask4;   // a per-lane boolean mask register (Vector4 layout)

        struct AOSTriangle
        {
            Vector3 mVertex0;        // +0x00  triangle vertex 0 (xyz)
            Vector3 mVertex1;        // +0x10  triangle vertex 1 (xyz)
            Vector3 mVertex2;        // +0x20  triangle vertex 2 (xyz)
            Vector3 mNormal;         // +0x30  unit face normal (xyz)
            f32     mfEdgeCosine0;   // +0x40  edge-0 cosine
            f32     mfEdgeCosine1;   // +0x44  edge-1 cosine
            f32     mfEdgeCosine2;   // +0x48  edge-2 cosine

            // True iff the triangle is well formed: all four xyz rows finite, the
            // normal is unit length within tolerance, and the edge cosines lie in
            // their recovered ranges. (Body @ 0x825BD208.)
            bool IsValid() const;

            // Debug-only assert wrapper around IsValid (declared to match the
            // DWARF class shape; bodied in its own TU).
            void AssertIsValid() const;
        };

        Vector4 mVertex0X;     // +0x00
        Vector4 mVertex0Y;     // +0x10
        Vector4 mVertex0Z;     // +0x20
        Vector4 mVertex1X;     // +0x30
        Vector4 mVertex1Y;     // +0x40
        Vector4 mVertex1Z;     // +0x50
        Vector4 mVertex2X;     // +0x60
        Vector4 mVertex2Y;     // +0x70
        Vector4 mVertex2Z;     // +0x80
        Mask4   mValidMasks;   // +0x90
        Vector4 mSurfaceTags;  // +0xA0
        Vector4 mEdge0Cosigns; // +0xB0
        Vector4 mEdge1Cosigns; // +0xC0
        Vector4 mEdge2Cosigns; // +0xD0

        // Extract triangle `liLane` (0..3) out of the SoA batch into roOut,
        // computing the unit face normal from the two edges and copying the
        // per-lane edge cosines. (Body @ 0x825B2808.)
        void GetAOSTriangle(s32 liLane, AOSTriangle& roOut) const;

        // Debug-only batch validation (declared to match the DWARF class shape;
        // bodied in its own TU).
        void AssertIsValid() const;
    };

    // Tolerance the face-normal unit-length check compares against
    // (K_TRIANGLE_NORMAL_TOLERANCE). The .rdata value is not present in the
    // export; a small epsilon is used and flagged.
    extern const VecFloat K_TRIANGLE_NORMAL_TOLERANCE;
}

#endif // CGS_TRIANGLE4_H
