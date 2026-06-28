#ifndef CGS_SWEPT_SPHERE_H
#define CGS_SWEPT_SPHERE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, VecFloat (rw::math::vpu aliases)

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h
//
// CgsGeometric::SweptSphere -- a sphere swept along a direction (a capsule-like
// volume): a start position + radius packed in one Vector3Plus, and a sweep
// direction + length packed in a second. Used by the deformation system's
// per-frame continuous collision (maSweptSpheres in DeformableObject).
//
// Homed at its mirrored DWARF path
// (references/DecFIGS/dwarfdump/.../Geometric/Primitives/CgsSweptSphere.h:28).
// Embedded BY VALUE in DeformableObject (maSweptSpheres[24]) so its full layout
// is needed -- this is the real DWARF layout, not a placeholder.
//
// LAYOUT (DWARF :71-72): two 16-byte Vector3Plus lanes (sizeof == 32):
//   +0x00  Vector3Plus mPositionAndRadius    xyz = start centre, w = radius
//   +0x10  Vector3Plus mDirectionAndLength   xyz = sweep direction, w = length
//
// All methods are DECLARED-ONLY: their bodies live in the SweptSphere TU.
// ============================================================================

namespace CgsGeometric
{
    struct SweptSphere
    {
        // DWARF :36/:41 -- set from (pos, radius, dir, length) or from the two packed lanes.
        void Set(Vector3 lPosition, VecFloat lvfRadius, Vector3 lDirection, VecFloat lvfLength);
        void Set(Vector3Plus lPositionAndRadius, Vector3Plus lDirectionAndLength);

        // DWARF :44-53 -- unpack the centre / radius / direction / length.
        const rw::math::vpu::Vector3 GetPosition() const;   // :44
        VecFloat                     GetRadius()   const;   // :47
        const rw::math::vpu::Vector3 GetDirection() const;  // :50
        VecFloat                     GetLength()   const;   // :53

        // DWARF :56-59 -- the two packed lanes verbatim.
        Vector3Plus GetPositionAndRadius()  const;   // :56
        Vector3Plus GetDirectionAndLength() const;   // :59

        // DWARF :63-67 -- mutate centre / radius in place.
        void SetPosition(Vector3 lPosition);   // :63
        void SetRadius(VecFloat lvfRadius);    // :67

    private:
        Vector3Plus mPositionAndRadius;    // +0x00 (DWARF :71) xyz = centre, w = radius
        Vector3Plus mDirectionAndLength;   // +0x10 (DWARF :72) xyz = direction, w = length
    };
}

#endif // CGS_SWEPT_SPHERE_H
