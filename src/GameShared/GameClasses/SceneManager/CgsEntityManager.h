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

        // @ 0x828CD4B8 -- map a VolumeInstanceId to its pool index (-1 if absent).
        s32  GetVolumeInstanceIndexByID(VolumeInstanceId lVolumeInstanceId) const;

        VolumeInstance*       GetVolumeInstance(s32 liIndex);
        const VolumeInstance* GetVolumeInstance(s32 liIndex) const;

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
