#ifndef CGS_COLLISION_MESH_DATA_H
#define CGS_COLLISION_MESH_DATA_H

// CgsPhysics::CollisionMeshData — the per-resource collision payload: a pointer to the
// serialised RenderWare collision volume plus a bounding sphere (centre + radius packed
// in a Vector3Plus). Reconstructed from BURNOUT_X360_ARTIST.XEX; member layout from the
// DecFIGS DWARF (CgsCollisionMeshData.h:47/105/106).
//
// Only the relocation helpers FixDown/FixUp are bodied in this slice (they (un)bias the
// embedded volume pointer by a load-base delta). The remaining accessors
// (Construct/Destruct/GetSpherePosition/GetSphereRadius/GetVolume/HasCollision) are
// declared-only — their bodies live elsewhere / are inlined away on X360.
#include "types.hpp"
#include "rw/math/vpu/types.h"

namespace VolRef { struct Volume; }

namespace CgsPhysics
{
    typedef rw::math::vpu::Vector3 Vector3;
    typedef rw::math::vpu::Vector3Plus Vector3Plus;

    // CgsCollisionMeshData.h:47
    struct CollisionMeshData
    {
        void Construct();
        void Destruct();

        // FixDown @ 0x828A5F78 / FixUp @ 0x828A5F90 — relocate the embedded volume
        // pointer by the resource load-base delta. lpBaseAddr is that delta (passed as
        // a raw value by CgsResource::ClusteredMeshResourceType::FixUp). No-op when the
        // volume pointer is null.
        void FixDown(void* lpBaseAddr);
        void FixUp(void* lpBaseAddr);

        Vector3        GetSpherePosition() const;
        f32            GetSphereRadius() const;
        const VolRef::Volume* GetVolume() const;
        bool           HasCollision();

    private:
        VolRef::Volume* mpCollisionVolume;  // CgsCollisionMeshData.h:105
        Vector3Plus     mBoundingSphere;    // CgsCollisionMeshData.h:106
    };
}

#endif
