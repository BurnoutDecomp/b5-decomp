#pragma once

// CgsGeometric::Frustum — a 6-plane view frustum stored as 8 swizzled plane lanes for the
// SoA culling tests (the 8th/extra lanes pad the SoA batch). Layout recovered from the DecFIGS
// DWARF (CgsFrustum.h:46/159): a single Vector4[8] maSwizzledPlanes member -> exactly 128 bytes
// (0x80), the size FrustumJobQueryInfo::operator= block-copies per maFrustum[] element.
//
// Only the data layout is provided here (the type is embedded by value in
// CgsSceneManager::FrustumJobQueryInfo); the Frustum member functions the X360 defines
// (GetPlane/SetPlane/... CgsFrustum.h:86+) are out-of-scope separate TUs.

#include "types.hpp"
#include "BrnCommonTypes.h"  // Vector4 (16-byte SIMD lane)
#include "vendor/renderware/collision/Plane.hpp"  // rw::collision::Plane (VectorToPlane return)
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"  // CgsGeometric::Sphere (IsSphereInFrustum arg)

namespace CgsGraphics
{
    struct CameraRwFrustum;   // GameShared/GameClasses/Graphics/CgsCamera.h
}

namespace CgsGeometric
{
    struct Frustum
    {
        // CgsFrustum.h:50 (DWARF) — plane index identifiers.
        enum PlaneId
        {
            PlaneLeft   = 0,
            PlaneTop    = 1,
            PlaneRight  = 2,
            PlaneBottom = 3,
            PlaneFar    = 4,
            PlaneNear   = 5,
        };

        // SetFromRwFrustum @ 0x82839FA8 -- build the 6-plane frustum from the
        // RW-side snapshot Camera::GetFrustum(CameraRwFrustum&) wrote.
        // ⚠ IT PERMUTES: RW (near, far, left, right, top, bottom) lands in stored
        // slots (5, 4, 0, 2, 1, 3) -- i.e. the stored slot IS the PlaneId below --
        // and slots 6/7 duplicate far/near. Proven from the @0x82839FA8 vperm
        // masks; do NOT "simplify" it back to identity. Body in CgsFrustum.cpp.
        void SetFromRwFrustum(const CgsGraphics::CameraRwFrustum& lrRw);

        // CalcVertices @ 0x82840DF8 -- solve the eight frustum corners as
        // plane-triple intersections and write them to lapVerts[0..7].
        // Body in CgsFrustum.cpp (reconstructed 2026-08-12, shadow-cascade wave).
        void CalcVertices(Vector4* lapVerts) const;

        // GetPlaneByIndex @ 0x8274EFE8 (CgsFrustum.h:242 assert) -- gather the
        // plane at luPlaneIndex out of the swizzled SoA lanes into an AoS vector
        // and return it (via VectorToPlane). Asserts luPlaneIndex < 8.
        rw::collision::Plane GetPlaneByIndex(u32 luPlaneIndex) const;

        // SetPlaneByIndex @ 0x827BAA48 (CgsFrustum.h:296 assert) -- pack lrPlane
        // (via PlaneToVector) and scatter its four components into the swizzled
        // SoA lane for luPlaneIndex. Asserts luPlaneIndex < 8.
        void SetPlaneByIndex(u32 luPlaneIndex, const rw::collision::Plane& lrPlane);

        // IsSphereInFrustum @ 0x828AF020 -- true iff lrSphere is on the inside
        // half-space of all 8 stored planes (SoA per-plane signed-distance test).
        bool IsSphereInFrustum(const Sphere& lrSphere) const;

        // CgsFrustum.h:159 (DWARF). 8 swizzled plane lanes = 128 bytes.
        Vector4 maSwizzledPlanes[8];

        // Never called. A MEMBER function so the body is a complete-class context
        // with private access (a bare static_assert in the class body is not).
        // Body + evidence in CgsFrustum.cpp.
        static void _AssertLayout();

    private:
        // CgsFrustum.h:145 (DWARF) -- pack a stored swizzled lane back into a
        // Plane. @ 0x82840DB0: negate all four lanes of the packed vector and
        // return it as a plane (the stored lane is the negation of the plane).
        // Body in CgsFrustum.cpp.
        rw::collision::Plane VectorToPlane(const Vector4& lrVector) const;

        // Inverse of VectorToPlane: pack a Plane into its stored (negated) lane
        // form. Declared here (sibling ledger func, its own TU); called by
        // SetPlaneByIndex @ 0x827BAA48.
        Vector4 PlaneToVector(const rw::collision::Plane& lrPlane) const;
    };
}
