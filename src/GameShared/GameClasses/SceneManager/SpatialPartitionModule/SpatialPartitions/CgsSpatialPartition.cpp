#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsSpatialPartition.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsSceneManager::SpatialPartition member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The partition owns a fixed pool of broad-phase entity
// nodes (this + 0x80, 8-byte stride) and a parallel pool of bounding spheres
// (this + 0x13900, 16-byte stride).

namespace CgsSceneManager
{

// 0x828BA3B0 -- AddEntity: allocate a new entity node in the fixed pool, then splice
// it into the broad-phase graph. The X360 passes this(r3) + lu16Id(r4) untouched into
// AllocEntity (sub_828B1078); on a successful allocation it dispatches the virtual
// AddEntityToGraph (vtable slot +0x38) with the same id. A null return trips the assert.
void SpatialPartition::AddEntity(u16 lu16Id, u32 lxTypeFlags, Vector3 lPosition, float32_t lfRadius)
{
    SpatialPartitionEntityNode* lpNewEntity =
        AllocEntity(lu16Id, lxTypeFlags, lPosition, lfRadius);

    if (lpNewEntity != nullptr)
    {
        AddEntityToGraph(lu16Id);
        return;
    }

    CGS_ASSERT(lpNewEntity != nullptr, "lpNewEntity != NULL");
}

// 0x828AA038 -- CalcEntityIndex: recover a node's pool index from its address. The X360
// computes (nodeAddr - this - 0x80) >> 3, i.e. the byte distance from `this` to the node
// minus the 128-byte header, divided by the 8-byte node stride == the index into
// maEntities (whose byte base is GetBaseNode() == this + 0x80). Result is masked to u16.
u16 SpatialPartition::CalcEntityIndex(const SpatialPartitionEntityNode& lrEntity) const
{
    const u8* lpBase = reinterpret_cast<const u8*>(this) + KU_ENTITY_ARRAY_BYTE_OFFSET;
    const u8* lpNode = reinterpret_cast<const u8*>(&lrEntity);
    const u32 luIndex =
        static_cast<u32>(lpNode - lpBase) / sizeof(SpatialPartitionEntityNode);

    CGS_ASSERT(luIndex < static_cast<u32>(KI_MAX_NUM_ENTITIES),
               "luIndex < (uint32_t)KI_MAX_NUM_ENTITIES");

    return static_cast<u16>(luIndex);
}

// 0x828A9F68 -- GetEntityBoundingSphere: index into the bounding-sphere pool. The X360
// forms 16 * (index + 5008) + this: sizeof(Sphere)==16 and 5008*16 == 80128 == 0x13900 ==
// the byte base of maEntityBoundingSpheres (immediately after the 0x80 header + the
// 8-byte x 10000 maEntities array). Bounds-asserts against KI_MAX_NUM_ENTITIES first.
CgsGeometric::Sphere& SpatialPartition::GetEntityBoundingSphere(u16 lu16Index)
{
    CGS_ASSERT(lu16Index < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");
    return maEntityBoundingSpheres[lu16Index];
}

// 0x828A9FD0 -- GetEntityBoundingSphereConst: const twin of GetEntityBoundingSphere.
// Same address math (16 * (index + 5008) + this) and the same bounds assert; the only
// difference in the X360 build is the const-qualified `this` and the assert line number.
const CgsGeometric::Sphere& SpatialPartition::GetEntityBoundingSphereConst(u16 lu16Index) const
{
    CGS_ASSERT(lu16Index < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");
    return maEntityBoundingSpheres[lu16Index];
}

} // namespace CgsSceneManager
