#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"

// CgsSceneManager::EntityManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (DWARF primary file
// GameShared/GameClasses/SceneManager/CgsEntityManager.cpp):
//   EntityManager::Prepare                    @0x828C5FC8
//   EntityManager::GetFirstEntityVolumeInstance @0x828C5DC0
//   EntityManager::GetVolumeInstanceIdByIndex @0x828B9E10
//   EntityManager::SetVolumeForCollision      @0x828CD658
//   EntityManager::SetVolumeForCollisionByIndex @0x828B9E80
//   EntityManager::SetVolumeInstanceTransform @0x828CD580
//   EntityManager::SetVolumePadding           @0x828BA088
//
// Asm walk: the two ObjectPool Clear instantiations are real X360 symbols
// (SceneManagerEntity<10000> at this+0, VolumeInstance<5048> at this+0x27600);
// the two IndexedHashTable Clears are inlined bucket loops (541 bins @+0xE7840
// over the shared element array @+0xCA380, then 509 bins @+0x123B20 over
// +0xE91A0), each ending in the one-byte constructed flag; return true.

namespace CgsSceneManager
{

// @ 0x828C5FC8
bool EntityManager::Prepare()
{
    mEntityPool.Clear();
    mVolumeInstancePool.Clear();

    mEntityIdToIndex.Clear(maEntityIdHashElements);
    mVolumeInstanceIdToIndex.Clear(maVolumeInstanceIdHashElements);

    return true;
}

// The X360 stores the two volume flags inline; the asm here pins bit 0 (0x01) of
// VolumeInstance::mx8Flags as the "for collision" bit (both SetVolumeForCollision*
// `ori 1` / `clrrwi 1` the byte at instance+0x6A). The other flag-bit assignments
// stay unpinned (see CgsVolumeInstance.h).
static const u8 KU8_VOLUME_FLAG_FOR_COLLISION = 0x01;

// @ 0x828C5DC0
VolumeInstance* EntityManager::GetFirstEntityVolumeInstance(u16 lu16EntityIndex,
                                                            s32* lpiFirstVolumeInstanceIndex)
{
    CGS_ASSERT(static_cast<s32>(lu16EntityIndex) < KI_MAX_NUM_ENTITIES,
               "(int32_t)lu16EntityIndex < KI_MAX_NUM_ENTITIES");
    CGS_ASSERT(mEntityPool.IsObjectAllocated(lu16EntityIndex),
               "mEntityPool.IsObjectAllocated( lu16EntityIndex )");

    const s32 liFirstVolumeInstance = mEntityPool[lu16EntityIndex].miFirstVolumeInstance;
    if (lpiFirstVolumeInstanceIndex != NULL)
    {
        *lpiFirstVolumeInstanceIndex = liFirstVolumeInstance;
    }

    return GetVolumeInstance(liFirstVolumeInstance);
}

// @ 0x828B9E10
VolumeInstanceId EntityManager::GetVolumeInstanceIdByIndex(s32 liIndex) const
{
    CGS_ASSERT(liIndex < KI_MAX_NUM_VOLUME_INSTANCES && liIndex >= 0,
               "liIndex < KI_MAX_NUM_VOLUME_INSTANCES && liIndex >= 0");

    return mVolumeInstancePool[liIndex].mUserID;
}

// @ 0x828CD658
VolumeInstance* EntityManager::SetVolumeForCollision(const VolumeInstanceId& lrVolumeInstanceId,
                                                     bool lbForCollision)
{
    const s32 liIndex = GetVolumeInstanceIndexByID(lrVolumeInstanceId);
    if (liIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        CGS_ASSERT(false, "Volume instance not found");
        return NULL;
    }

    VolumeInstance& lrInstance = mVolumeInstancePool[liIndex];
    if (lbForCollision)
    {
        lrInstance.mx8Flags |= KU8_VOLUME_FLAG_FOR_COLLISION;
    }
    else
    {
        lrInstance.mx8Flags &= static_cast<u8>(~KU8_VOLUME_FLAG_FOR_COLLISION);
    }
    return &lrInstance;
}

// @ 0x828B9E80
VolumeInstance* EntityManager::SetVolumeForCollisionByIndex(s32 liIndex, bool lbForCollision)
{
    if (liIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        CGS_ASSERT(false, "Volume instance not found");
        return NULL;
    }

    VolumeInstance& lrInstance = mVolumeInstancePool[liIndex];
    if (lbForCollision)
    {
        lrInstance.mx8Flags |= KU8_VOLUME_FLAG_FOR_COLLISION;
    }
    else
    {
        lrInstance.mx8Flags &= static_cast<u8>(~KU8_VOLUME_FLAG_FOR_COLLISION);
    }
    return &lrInstance;
}

// @ 0x828CD580
Vector3 EntityManager::SetVolumeInstanceTransform(const VolumeInstanceId& lrVolumeInstanceId,
                                                  const Matrix44Affine& lrTransform)
{
    Vector3 lPositionDelta;

    const s32 liIndex = GetVolumeInstanceIndexByID(lrVolumeInstanceId);
    if (liIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        lPositionDelta.SetZero();
        CGS_ASSERT(false, "Volume instance not found");
    }
    else
    {
        VolumeInstance& lrInstance = mVolumeInstancePool[liIndex];

        // Movement delta (new translation - old translation) computed BEFORE the
        // world transform is overwritten -- the X360 `vsubfp128` of the two +0x30 rows.
        const Vector3 lOldTranslation = lrInstance.mWorldSpaceTransform.Pos();
        const Vector3& lrNewTranslation = lrTransform.Pos();
        lPositionDelta.x = lrNewTranslation.x - lOldTranslation.x;
        lPositionDelta.y = lrNewTranslation.y - lOldTranslation.y;
        lPositionDelta.z = lrNewTranslation.z - lOldTranslation.z;
        lPositionDelta.w = lrNewTranslation.w - lOldTranslation.w;

        lrInstance.mWorldSpaceTransform = lrTransform;
    }

    return lPositionDelta;
}

// ===========================================================================
// EntityManager::AddEntity @ 0x828C6090
//
// Pop a slot out of mEntityPool, stamp the entity record ({ mUserID = the public
// id, mu16NumVolumeInstances = 0, miFirstVolumeInstance = -1 } -- the X360 store
// order is id / count / first), then publish the slot in the id->index table by
// filling the shared hash element at the SAME index (element stride 12 on the X360:
// `12 * (index + 69024) + this`, i.e. &maEntityIdHashElements[index], key then value)
// and inserting it. Returns the slot index, or 0xFFFF when the pool is exhausted.
// ===========================================================================
u16 EntityManager::AddEntity(EntityId lEntityId)
{
    const s32 liIndex = mEntityPool.AllocateObject();

    CGS_ASSERT(liIndex < KI_MAX_NUM_ENTITIES,
               "Out of bounds index coming back from EntityPool.AllocateObject: ");

    if (liIndex == -1)
    {
        CGS_ASSERT(false, "Failed to allocate entity ");
        return 0xFFFF;
    }

    SceneManagerEntity& lrEntity = mEntityPool[liIndex];
    lrEntity.mUserID                = lEntityId;
    lrEntity.mu16NumVolumeInstances = 0;
    lrEntity.miFirstVolumeInstance  = -1;

    maEntityIdHashElements[liIndex].Set(lEntityId, static_cast<u16>(liIndex));
    mEntityIdToIndex.Insert(&maEntityIdHashElements[liIndex]);

    return static_cast<u16>(liIndex);
}

// ===========================================================================
// EntityManager::RemoveEntity @ 0x828CD6F8
//
// Drop the slot's id->index entry (keyed on the record's own mUserID) and hand the
// slot back to the pool.
// ===========================================================================
void EntityManager::RemoveEntity(u16 lu16Index)
{
    if (!mEntityPool.IsObjectAllocated(lu16Index))
    {
        CGS_ASSERT(false, "mEntityPool.IsObjectAllocated( (int32_t)lu16Index )");
        return;
    }

    mEntityIdToIndex.Remove(mEntityPool[lu16Index].mUserID);
    mEntityPool.FreeObject(lu16Index);
}

// The index -> id lookup (see the header): the X360 reads the first word of
// mEntityPool[index] through a truncated-name accessor.
EntityId EntityManager::GetEntityIdByIndex(u16 lu16Index) const
{
    CGS_ASSERT(static_cast<s32>(lu16Index) < KI_MAX_NUM_ENTITIES,
               "lu16Index < KI_MAX_NUM_ENTITIES");
    return mEntityPool[lu16Index].mUserID;
}

bool EntityManager::IsEntityIndexAllocated(u16 lu16Index) const
{
    return static_cast<s32>(lu16Index) < KI_MAX_NUM_ENTITIES
        && mEntityPool.IsObjectAllocated(lu16Index);
}

// The id -> index lookup (the twin of GetVolumeInstanceIndexByID @0x828CD4B8 on the
// other table): hash the id to its bucket and walk the ascending chain.
s32 EntityManager::GetEntityIndexByID(EntityId lEntityId) const
{
    const CgsContainers::IndexedHashTable<EntityId, u16, 541>::Element* lpElement =
        mEntityIdToIndex.Get(lEntityId);
    if (lpElement == 0)
    {
        return -1;
    }
    return static_cast<s32>(lpElement->GetValue());
}

// @ 0x828BA088
VolumeInstance* EntityManager::SetVolumePadding(s32 liIndex, Vector3 lPadding)
{
    CGS_ASSERT(liIndex < KI_MAX_NUM_VOLUME_INSTANCES,
               "liIndex < KI_MAX_NUM_VOLUME_INSTANCES");
    CGS_ASSERT(liIndex != KI_INVALID_VOLUME_INSTANCE_INDEX,
               "liIndex != KI_INVALID_VOLUME_INSTANCE_INDEX");
    CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
    CGS_ASSERT(mVolumeInstancePool.IsObjectAllocated(liIndex),
               "mVolumeInstancePool.IsObjectAllocated( liIndex )");

    VolumeInstance& lrInstance = mVolumeInstancePool[liIndex];
    lrInstance.mPadding = lPadding;
    return &lrInstance;
}

}
