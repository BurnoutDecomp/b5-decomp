#pragma once

// ============================================================================
// b5-decomp/src/SDKs/EATech/rwcollision/volume_debug_access.h
//
// Minimal typed view over an rw::collision::Volume for the debug renderers (the prop
// debug overlay's Draw). rwcollision is NOT present in the PC libs (it must be decompiled
// from the console build), and the Volume payload is an external EATech *serialised*
// collision record (a fixed 96-byte blob -- see SDKs/EATech/rwcollision/volume.cpp), so
// its fields are addressed by their fixed byte offsets in the blob, which is the
// documented exception to the no-offset rule for serialised / SPU data.
//
// Offsets are X360-console facts pinned by PropEntityDebugComponent::Draw @ 0x822A9770:
//   maType    @ +0x40 (64)  -- pointer to the volume's type descriptor; *maType is the
//                              rw::collision VOLUMETYPE the Draw switch keys on.
//   mExtent   @ +0x44 (68)  -- box half-extents (x@+0x44, y@+0x48, z@+0x4C); also reused
//                              as the half-height (x lane) for the capsule/cylinder cases.
//   mfRadius  @ +0x50 (80)  -- sphere / capsule / cylinder radius.
// (The relative transform the debug overlay multiplies in is reached via GetRelativeTransform,
// bodied in the rwcollision Volume TU; declared-only here.)
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Matrix44Affine

namespace rw
{
    namespace collision
    {
        // The rwcollision volume primitive kinds the prop debug renderer can draw. Values
        // are the rwcollision VOLUMETYPE ids the Draw switch dispatches on (1..5); the
        // default branch asserts "Collision volume very confused about what type it is".
        enum EVolumeType
        {
            E_VOLUMETYPE_SPHERE   = 1,
            E_VOLUMETYPE_CAPSULE  = 2,
            E_VOLUMETYPE_BBOX     = 4,
            E_VOLUMETYPE_CYLINDER = 5
        };

        // Typed accessor over the serialised 96-byte volume blob. The named getters read
        // the fixed-offset fields (documented serialised-data exception); the relative
        // transform is reached out-of-line (its own rwcollision TU).
        class Volume
        {
        public:
            // *(*(this + 0x40)) -- the type descriptor's leading word is the VOLUMETYPE.
            u32 GetType() const
            {
                const u32* const* lppTypeDescriptor = reinterpret_cast<const u32* const*>(maPayload + 0x40);
                return **lppTypeDescriptor;
            }

            // Box half-extents (x/y/z); x lane doubles as the capsule/cylinder half-height.
            const Vector3& GetExtent() const
            {
                return *reinterpret_cast<const Vector3*>(maPayload + 0x44);
            }

            // Sphere / capsule / cylinder radius.
            f32 GetRadius() const
            {
                return *reinterpret_cast<const f32*>(maPayload + 0x50);
            }

            // +0x30 -- the volume-local transform composed with the prop world transform
            // before drawing (bodied in the rwcollision Volume TU).
            const Matrix44Affine& GetRelativeTransform() const;

        private:
            u8 maPayload[96];   // serialised rwcollision volume record (see volume.cpp)
        };
    }
}
