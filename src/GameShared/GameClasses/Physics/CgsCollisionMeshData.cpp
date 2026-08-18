#include "GameShared/GameClasses/Physics/CgsCollisionMeshData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsPhysics::CollisionMeshData::FixDown @ 0x828A5F78
//   CgsPhysics::CollisionMeshData::FixUp   @ 0x828A5F90
//
// These (un)relocate the embedded collision-volume pointer around a resource load. The
// caller (CgsResource::ClusteredMeshResourceType::FixUp/FixDown) passes the load-base
// delta as the argument. FixDown subtracts the delta to turn an absolute pointer back
// into a serialised offset; FixUp adds it to turn a serialised offset back into a live
// pointer. Both are no-ops when the volume pointer is null (a clustered mesh with no
// collision payload). Member accessed by name (mpCollisionVolume); the delta arrives as
// a raw integer cast through void* at the call site.

namespace CgsPhysics
{
    // 2026-08-18 (wave Q5, VolumeManager mount): the DWARF-declared accessor (CgsCollisionMeshData.h:38)
    // was declared with no body anywhere; VolumeManager::GetRwVolume's STATIC-volume arm (the arm props
    // take, @0x828C5E68) reaches it through ResourcePtr<CollisionMeshData>::operator->. Header-inline
    // on the console (no out-of-line symbol): a plain member read.
    const VolRef::Volume* CollisionMeshData::GetVolume() const
    {
        return mpCollisionVolume;
    }

    void CollisionMeshData::FixDown(void* lpBaseAddr)
    {
        if (mpCollisionVolume)
        {
            const intptr_t liDelta = reinterpret_cast<intptr_t>(lpBaseAddr);
            mpCollisionVolume = reinterpret_cast<VolRef::Volume*>(
                reinterpret_cast<intptr_t>(mpCollisionVolume) - liDelta);
        }
    }

    void CollisionMeshData::FixUp(void* lpBaseAddr)
    {
        if (mpCollisionVolume)
        {
            const intptr_t liDelta = reinterpret_cast<intptr_t>(lpBaseAddr);
            mpCollisionVolume = reinterpret_cast<VolRef::Volume*>(
                reinterpret_cast<intptr_t>(mpCollisionVolume) + liDelta);
        }
    }
}
