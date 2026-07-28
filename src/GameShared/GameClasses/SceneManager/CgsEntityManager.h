#pragma once

// ===========================================================================
// CgsSceneManager::EntityManager
//   Home: GameShared/GameClasses/SceneManager/CgsEntityManager.{h,cpp}
//
// The SceneManager's entity / volume-instance registry. It owns the pooled
// SceneManagerEntity + VolumeInstance records and the two indexed hash tables
// that map an EntityId / VolumeInstanceId to its pool index. Embedded BY VALUE
// in CgsSceneManager::SceneManagerModule (mEntityManager).
//
// Member set + method signatures from the DecFIGS DWARF (CgsEntityManager.h);
// gated on the X360 ARTIST ledger. The previously-opaque pooled storage is now
// the real DWARF member list (this TU landed Prepare, which pins the hash-table
// layout -- see CgsIndexedHashTable.h). Byte offsets are NOT preserved on the
// x64 PC compile (pointers widen) per the project's semantic-parity rule; the
// X360 anchors Prepare touches are documented in the .cpp.
//
// X360 functions:
//   EntityManager::Prepare                    @ 0x828C5FC8  (this TU)
//   EntityManager::GetVolumeInstanceIndexByID @ 0x828CD4B8  (its own TU)
// ===========================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                           // Vector3 / Matrix44Affine
#include "GameShared/GameClasses/Containers/CgsIndexedHashTable.h"    // IndexedHashTable(Element)
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"          // ObjectPool<T,N,TIndex>
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"          // EntityId
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstance.h"    // VolumeInstance (pooled by value)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // VolumeInstanceId

namespace CgsSceneManager
{
    class VolumeManager;

    // Capacities the DWARF declares for the pools / index range.
    const s32 KI_MAX_NUM_ENTITIES         = 10000;
    const s32 KI_MAX_NUM_VOLUME_INSTANCES = 5048;

    // Sentinel returned by GetVolumeInstanceIndexByID for an id with no live pool slot
    // (the X360 asserts liIndex != this value in SetVolumePadding, baked as -1).
    const s32 KI_INVALID_VOLUME_INSTANCE_INDEX = -1;

    // DWARF CgsEntityManager.h:64 -- one registered entity: its public id plus the
    // head of its volume-instance chain.
    struct SceneManagerEntity
    {
        EntityId mUserID;                  // h:66
        s32      miFirstVolumeInstance;    // h:67
        u16      mu16NumVolumeInstances;   // h:68
    };

    class EntityManager
    {
    public:
        void Construct(VolumeManager* lpVolumeManager);

        // @ 0x828C5FC8 (this TU) -- clear both pools and both id->index tables.
        bool Prepare();

        // @ 0x828C6090 -- register a new entity: pop a pool slot, stamp the public id
        // (and the empty volume-instance chain) into it, publish the slot in the
        // id->index table, and return the slot index. Returns 0xFFFF when the pool is
        // full. The returned index is what every downstream broad-phase structure
        // (SpatialPartition, CoarseQueryResultBuffer) keys on.
        u16 AddEntity(EntityId lEntityId);

        // @ 0x828CD6F8 -- retire a slot: drop its id->index entry and free the slot.
        void RemoveEntity(u16 lu16Index);

        // The index -> public-id lookup SceneManagerModule::ProcessFrustumTestJobResults
        // @0x828C7838 uses to turn a coarse-query result index back into an EntityId
        // (the X360 truncated accessor `CgsSceneManager::Scen(&mEntityManager, index)`
        // hands back &mEntityPool[index], whose first word is mUserID). Named here.
        EntityId GetEntityIdByIndex(u16 lu16Index) const;

        // Slot-liveness probe (the same mEntityPool.IsObjectAllocated the X360 asserts
        // on before touching a slot); named so callers do not reach into the pool.
        bool IsEntityIndexAllocated(u16 lu16Index) const;

        // The id -> index lookup through mEntityIdToIndex (the twin of
        // GetVolumeInstanceIndexByID @0x828CD4B8 on the other table); -1 when absent.
        s32 GetEntityIndexByID(EntityId lEntityId) const;

        // @ 0x828CD4B8 -- map a VolumeInstanceId to its pool index (-1 if absent).
        s32  GetVolumeInstanceIndexByID(VolumeInstanceId lVolumeInstanceId) const;

        VolumeInstance*       GetVolumeInstance(s32 liIndex);
        const VolumeInstance* GetVolumeInstance(s32 liIndex) const;

        // @ 0x828C5DC0 -- the first volume instance registered against an entity slot.
        // Optionally writes that instance's pool index through lpiFirstVolumeInstanceIndex
        // (nullable), then returns the instance itself.
        VolumeInstance* GetFirstEntityVolumeInstance(u16 lu16EntityIndex,
                                                     s32* lpiFirstVolumeInstanceIndex);

        // @ 0x828B9E10 -- the public id of the volume instance at a pool index.
        VolumeInstanceId GetVolumeInstanceIdByIndex(s32 liIndex) const;

        // @ 0x828CD658 -- toggle the "for collision" flag on the instance named by an id
        //                 (resolves the id to a pool index first).
        VolumeInstance* SetVolumeForCollision(const VolumeInstanceId& lrVolumeInstanceId,
                                              bool lbForCollision);

        // @ 0x828B9E80 -- toggle the "for collision" flag on the instance at a pool index.
        VolumeInstance* SetVolumeForCollisionByIndex(s32 liIndex, bool lbForCollision);

        // @ 0x828CD580 -- overwrite an instance's world transform (named by id) and return
        //                 the resulting position delta (new translation - old translation).
        Vector3 SetVolumeInstanceTransform(const VolumeInstanceId& lrVolumeInstanceId,
                                           const Matrix44Affine& lrTransform);

        // @ 0x828BA088 -- set the collision-padding vector on the instance at a pool index.
        VolumeInstance* SetVolumePadding(s32 liIndex, Vector3 lPadding);

    private:
        // DWARF h:239-248 (member order verbatim). The element arrays are shared by
        // every bucket of their table (see CgsIndexedHashTable.h).
        CgsContainers::ObjectPool<SceneManagerEntity, KI_MAX_NUM_ENTITIES, s32>     mEntityPool;         // h:239
        CgsContainers::ObjectPool<VolumeInstance, KI_MAX_NUM_VOLUME_INSTANCES, s32> mVolumeInstancePool; // h:240 (X360 +0x27600)
        CgsContainers::IndexedHashTableElement<EntityId, u16>          maEntityIdHashElements[KI_MAX_NUM_ENTITIES];       // h:242 (X360 +0xCA380)
        CgsContainers::IndexedHashTable<EntityId, u16, 541>            mEntityIdToIndex;     // h:243 (X360 +0xE7840)
        CgsContainers::IndexedHashTableElement<VolumeInstanceId, u32>  maVolumeInstanceIdHashElements[KI_MAX_NUM_ENTITIES]; // h:245 (X360 +0xE91A0)
        CgsContainers::IndexedHashTable<VolumeInstanceId, u32, 509>    mVolumeInstanceIdToIndex; // h:246 (X360 +0x123B20)
        VolumeManager*                                                 mpVolumeManager;      // h:248
    };
}
