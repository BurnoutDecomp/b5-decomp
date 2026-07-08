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

        // --- PENDING DECLARATIONS (cross-TU; bodies live in their own not-
        //     yet-landed TUs; call shapes attested at the BrnWorld::ShadowMap::
        //     ComputeTSMMatrix @ 0x827BFF58 call sites) ----------------------

        // @call 0x827C0058: build the 6-plane frustum from the RW-side
        // snapshot Camera::GetFrustum(CameraRwFrustum&) wrote.
        void SetFromRwFrustum(const CgsGraphics::CameraRwFrustum& lrRw);
        // @call 0x827C0064: write the 8 corner vertices.
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
