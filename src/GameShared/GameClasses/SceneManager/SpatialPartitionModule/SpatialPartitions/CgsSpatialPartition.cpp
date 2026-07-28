#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsSpatialPartition.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsSceneManager::SpatialPartition member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The partition owns a fixed pool of broad-phase entity
// links (X360 this + 0x80, 8-byte stride) and a parallel pool of bounding spheres
// (X360 this + 0x13900, 16-byte stride); both are named members here (see the header).

namespace CgsSceneManager
{

// 0x828BA3B0 -- AddEntity: fill the entity's pool record, then splice it into the
// broad-phase graph. The X360 passes this(r3) + lu16Id(r4) untouched into AllocEntity
// (sub_828B1078); on a successful allocation it dispatches the virtual
// AddEntityToGraph (vtable slot +0x38) with the same id. A null return trips the assert.
void SpatialPartition::AddEntity(u16 lu16Id, u32 lxTypeFlags, Vector3 lPosition, float32_t lfRadius)
{
    SpatialPartitionEntityLink* lpNewEntity =
        AllocEntity(lu16Id, lxTypeFlags, lPosition, lfRadius);

    if (lpNewEntity != 0)
    {
        AddEntityToGraph(lu16Id);
        return;
    }

    CGS_ASSERT(lpNewEntity != 0, "lpNewEntity != NULL");
}

// 0x828B1078 -- AllocEntity: stamp the entity's pool record. The record IS the pool
// slot at the caller-supplied index (the entity-manager slot the scene module just
// allocated -- the partition never picks an index of its own), so "allocate" here means
// "initialise": publish the entity-type mask (the word FrustumTestEntities masks the
// query flags against), park the intrusive chain link as unlinked, and write the
// bounding sphere into the parallel pool. Returns the record, or null when the index
// is out of range.
SpatialPartitionEntityLink*
SpatialPartition::AllocEntity(u16 lu16Id, u32 lxTypeFlags, Vector3 lPosition, float32_t lfRadius)
{
    CGS_ASSERT(lu16Id < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");
    if (lu16Id >= KI_MAX_NUM_ENTITIES)
    {
        return 0;
    }

    // The X360 body writes ONLY the type-flags word here (@0x828B1188 `stw r26, 0(r3)`);
    // the two list links are set by the node the entity is filed on. They are parked as
    // unlinked so a stale chain can never be walked before the insert runs.
    SpatialPartitionEntityLink& lrLink = maEntityLinks[lu16Id];
    lrLink.mx32TypeFlags  = lxTypeFlags;
    lrLink.mu16NextEntity = KU_INVALID_ENTITY_LINK;
    lrLink.mu16PrevEntity = KU_INVALID_ENTITY_LINK;

    CgsGeometric::Sphere& lrSphere = maEntityBoundingSpheres[lu16Id];
    lrSphere.mPositionRadius.x = lPosition.x;
    lrSphere.mPositionRadius.y = lPosition.y;
    lrSphere.mPositionRadius.z = lPosition.z;
    lrSphere.mPositionRadius.w = lfRadius;

    return &lrLink;
}

// 0x828AA038 -- CalcEntityIndex: recover a link's pool index from its address. The X360
// computes (nodeAddr - this - 0x80) >> 3, i.e. the byte distance from `this` to the link
// minus the 128-byte header, divided by the 8-byte stride == the index into the pool.
u16 SpatialPartition::CalcEntityIndex(const SpatialPartitionEntityLink& lrEntity) const
{
    const u32 luIndex = static_cast<u32>(&lrEntity - &maEntityLinks[0]);

    CGS_ASSERT(luIndex < static_cast<u32>(KI_MAX_NUM_ENTITIES),
               "luIndex < (uint32_t)KI_MAX_NUM_ENTITIES");

    return static_cast<u16>(luIndex);
}

// 0x828A9F68 -- GetEntityBoundingSphere: index into the bounding-sphere pool. The X360
// forms 16 * (index + 5008) + this: sizeof(Sphere)==16 and 5008*16 == 80128 == 0x13900 ==
// the byte base of the sphere pool. Bounds-asserts against KI_MAX_NUM_ENTITIES first.
CgsGeometric::Sphere& SpatialPartition::GetEntityBoundingSphere(u16 lu16Index)
{
    CGS_ASSERT(lu16Index < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");
    return maEntityBoundingSpheres[lu16Index];
}

// 0x828A9FD0 -- GetEntityBoundingSphereConst: const twin of GetEntityBoundingSphere.
const CgsGeometric::Sphere& SpatialPartition::GetEntityBoundingSphereConst(u16 lu16Index) const
{
    CGS_ASSERT(lu16Index < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");
    return maEntityBoundingSpheres[lu16Index];
}

} // namespace CgsSceneManager
