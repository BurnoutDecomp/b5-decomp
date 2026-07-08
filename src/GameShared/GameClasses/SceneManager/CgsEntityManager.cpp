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
