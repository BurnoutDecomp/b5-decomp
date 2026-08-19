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
// gated on the X360 ARTIST ledger. Byte offsets are NOT preserved on the x64 PC
// compile (pointers widen) per the project's semantic-parity rule; the X360
// anchors are documented in the .cpp.
//
// X360 functions bodied by this TU (see the .cpp banner for the full list):
//   EntityManager::Prepare                       @ 0x828C5FC8
//   EntityManager::AddEntity                     @ 0x828C6090
//   EntityManager::RemoveEntity                  @ 0x828CD6F8
//   EntityManager::AddVolumeInstance             @ 0x828CD790   <-- wave Q5 / A2
//   EntityManager::AddVolumeInstanceToEntity     (inlined into the above)
//   EntityManager::RemoveVolumeInstanceFromEntity@ 0x828BA180   <-- wave Q5 / A2
//   EntityManager::GetVolumeInstanceIndexByID    @ 0x828CD4B8   <-- wave Q5 / A2
//   EntityManager::GetVolumeInstance (const)     @ 0x828B9F28   <-- wave Q5 / A2
//   EntityManager::GetVolumeInstance (non-const) @ 0x828B9FD8   <-- wave Q5 / A2
//   EntityManager::GetFirstEntityVolumeInstance  @ 0x828C5DC0
//   EntityManager::GetVolumeInstanceIdByIndex    @ 0x828B9E10
//   EntityManager::SetVolumeForCollision         @ 0x828CD658
//   EntityManager::SetVolumeForCollisionByIndex  @ 0x828B9E80
//   EntityManager::SetVolumeInstanceTransform    @ 0x828CD580
//   EntityManager::SetVolumePadding              @ 0x828BA088
// ===========================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                           // Vector3 / Matrix44Affine

// ⚠️ INCLUDE ORDER IS LOAD-BEARING, DO NOT ALPHABETISE.
// CgsIndexedHashTable.h's hash/ordering customisation point is a free function
// `CgsContainers::KeyBits(const TKey&)`: a generic template for keys with an integral
// conversion, plus per-key overloads declared next to their own type. VolumeInstanceId
// has no implicit u64 conversion, so its overload (bottom of CgsVolumeInstanceId.h) is
// the only viable candidate -- and because the call inside Insert/GetInternal is a
// DEPENDENT unqualified call into the SAME namespace as the template, that overload is
// only found if it is declared BEFORE the template's definition (ADL cannot reach
// CgsContainers from a CgsSceneManager key). So the key headers come first. Instantiating
// IndexedHashTable<VolumeInstanceId,u32,509>::Insert/Get with the other order is a hard
// C2440 on `static_cast<u64>(const VolumeInstanceId&)`.
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"          // EntityId
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"          // VolumeId
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // VolumeInstanceId (+ its KeyBits)
#include "GameShared/GameClasses/Containers/CgsIndexedHashTable.h"    // IndexedHashTable(Element)
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"          // ObjectPool<T,N,TIndex>
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstance.h"    // VolumeInstance (pooled by value)

namespace CgsSceneManager
{
    class VolumeManager;

    // Capacities the DWARF declares for the pools / index range.
    const s32 KI_MAX_NUM_ENTITIES         = 10000;
    const s32 KI_MAX_NUM_VOLUME_INSTANCES = 5048;

    // DWARF CgsEntityManager.h:48/49 (namespace scope). KI_INVALID_VOLUME_INSTANCE_INDEX
    // is dumped as 4294967295 == the int32_t -1 the X360 compares against everywhere
    // (`cmpwi r,-1`); KU16_INVALID_ENTITY_INDEX is the 0xFFFF AddEntity returns and
    // AddVolumeInstance's CgsEntityManager.cpp:201 tripwire tests.
    const s32 KI_INVALID_VOLUME_INSTANCE_INDEX = -1;
    const u16 KU16_INVALID_ENTITY_INDEX        = 65535;

    // DWARF CgsEntityManager.h:64 -- one registered entity: its public id plus the
    // head of its volume-instance chain.
    struct SceneManagerEntity
    {
        EntityId mUserID;                  // h:66 (X360 +0x00)
        s32      miFirstVolumeInstance;    // h:67 (X360 +0x04)
        u16      mu16NumVolumeInstances;   // h:68 (X360 +0x08)
    };

    class EntityManager
    {
    public:
        void Construct(VolumeManager* lpVolumeManager);

        // @ 0x828C5FC8 -- clear both pools and both id->index tables.
        bool Prepare();

        // @ 0x828C6090 -- register a new entity: pop a pool slot, stamp the public id
        // (and the empty volume-instance chain) into it, publish the slot in the
        // id->index table, and return the slot index. Returns KU16_INVALID_ENTITY_INDEX
        // when the pool is full. The returned index is what every downstream broad-phase
        // structure (SpatialPartition, CoarseQueryResultBuffer) keys on.
        u16 AddEntity(EntityId lEntityId);

        // @ 0x828CD6F8 -- retire a slot: drop its id->index entry and free the slot.
        void RemoveEntity(u16 lu16Index);

        // @ 0x828CD790 (DWARF h:115, body CgsEntityManager.cpp:189) -- register one
        // collision-volume placement against the entity named inside lVolumeInstanceId,
        // and return its volume-instance pool index (or KI_INVALID_VOLUME_INSTANCE_INDEX
        // when the pool is exhausted). This is the scene side of
        // InSceneUpdateInterface::AddVolumeInstance: it is what turns a queued
        // AddVolumeInstance event into a live VolumeInstance the broad phase can sweep.
        s32 AddVolumeInstance(VolumeInstanceId lVolumeInstanceId,
                              VolumeId lVolumeId,
                              const Matrix44Affine& lrTransform);

        // ---- wave Q5 / cluster RMLEG (2026-08-19): the removal mirror of AddVolumeInstance.
        // DWARF h:120 / h:126. NEITHER HAS AN OUT-OF-LINE X360 BODY -- both are absent from
        // progress/identity.json because the console INLINED them at every use, so the shape
        // below is recovered from the three verbatim inline expansions, which agree
        // instruction for instruction:
        //   * BridgeInputSceneUpdateInterfaceToSubModules leg 3 @0x828D21C4..0x828D21EC
        //       bl GetVolumeInstanceIndexByID ; bl RemoveVolumeInstanceFromEntity ;
        //       bl IndexedHashTable<VolumeInstanceId,u32,509>::Remove ;
        //       bl ObjectPool<VolumeInstance,5048,int>::FreeObject
        //     -- i.e. RemoveVolumeInstance(id) == RemoveVolumeInstanceByIndex(id, GetVolumeInstanceIndexByID(id)).
        //   * SceneManagerModule::RemoveAllEntityVolumeInstances tail @0x828CDA24..0x828CDA48
        //   * (reached through it) RemoveAllOwnerVolumeInstances @0x828CDA70
        //     -- both of the latter already HOLD the pool index and re-load the id off the
        //     record (`ld r28, 0x50(r31)`), which is exactly the ByIndex form's two arguments.
        // The trio's last two calls touch mVolumeInstanceIdToIndex / mVolumeInstancePool, which
        // are private -- that privacy is the ONLY reason drain leg 3 and the two RemoveAll*
        // walkers were parked in round 3 (scratchpad/waveQ5/e1a.owner.md section 3, P1/P2).
        void RemoveVolumeInstance(VolumeInstanceId lVolumeInstanceId);
        void RemoveVolumeInstanceByIndex(VolumeInstanceId lVolumeInstanceId, s32 liIndex);

        // The index -> public-id lookup SceneManagerModule::ProcessFrustumTestJobResults
        // @0x828C7838 uses to turn a coarse-query result index back into an EntityId
        // (the X360 truncated accessor `CgsSceneManager::Scen(&mEntityManager, index)`
        // hands back &mEntityPool[index], whose first word is mUserID). DWARF h:144.
        EntityId GetEntityIdByIndex(u16 lu16Index) const;

        // Slot-liveness probe (the same mEntityPool.IsObjectAllocated the X360 asserts
        // on before touching a slot); named so callers do not reach into the pool.
        bool IsEntityIndexAllocated(u16 lu16Index) const;

        // The id -> index lookup through mEntityIdToIndex (the twin of
        // GetVolumeInstanceIndexByID @0x828CD4B8 on the other table); -1 when absent.
        //
        // ⚠️ DWARF SHAPE DIVERGENCE, DELIBERATE (wave Q5 / A2): DWARF h:139 declares this
        // `uint16_t GetEntityIndexByID(EntityId) const`, i.e. absent == 0xFFFF ==
        // KU16_INVALID_ENTITY_INDEX. The tree's mounted callers
        // (CgsSceneManagerModule.cpp:719/776/792) all test `liIndex < 0` / `>= 0` against
        // an s32, so re-typing this to u16 would turn "absent" into the valid-looking
        // index 65535 in three live per-frame paths -- a silent wrong answer of exactly
        // the class this campaign keeps shipping. The s32/-1 form is kept and every
        // in-file consumer converts explicitly (see AddVolumeInstance). Re-type it only
        // together with those call sites.
        s32 GetEntityIndexByID(EntityId lEntityId) const;

        // @ 0x828CD4B8 (console emits it from CgsEntityManager.h:324) -- map a
        // VolumeInstanceId to its pool index, or KI_INVALID_VOLUME_INSTANCE_INDEX.
        s32 GetVolumeInstanceIndexByID(VolumeInstanceId lVolumeInstanceId) const;

        // DWARF h:175 / h:180 -- the pool-index -> record accessors. NULL for the
        // invalid index and for an unallocated slot (X360 @0x828B9F28 const /
        // @0x828B9FD8 non-const; the console emits the two definitions at
        // CgsEntityManager.h:488 and :518 respectively).
        const VolumeInstance* GetVolumeInstance(s32 liViIndex) const;
        VolumeInstance*       GetVolumeInstance(s32 liViIndex);

        // @ 0x828C5DC0 (DWARF h:189) -- the first volume instance registered against an
        // entity slot. Optionally writes that instance's pool index through
        // lpiFirstVolumeInstanceIndex (nullable), then returns the instance itself.
        const VolumeInstance* GetFirstEntityVolumeInstance(u16 lu16EntityIndex,
                                                           s32* lpiFirstVolumeInstanceIndex) const;

        // @ 0x828B9E10 (DWARF h:154) -- the public id of the volume instance at a pool index.
        VolumeInstanceId GetVolumeInstanceIdByIndex(s32 liIndex) const;

        // @ 0x828CD658 (DWARF h:165) -- toggle VolumeInstance's COLLIDABLE flag on the
        // instance named by an id (resolves the id to a pool index first).
        void SetVolumeForCollision(VolumeInstanceId lVolumeInstanceId, bool lbForCollision);

        // @ 0x828B9E80 (DWARF h:170) -- the same toggle, addressed by pool index.
        void SetVolumeForCollisionByIndex(s32 liIndex, bool lbForCollision);

        // @ 0x828CD580 (DWARF h:160) -- overwrite an instance's world transform (named by
        // id) and return the resulting position delta (new translation - old translation).
        Vector3 SetVolumeInstanceTransform(VolumeInstanceId lVolumeInstanceId,
                                           const Matrix44Affine& lrTransform);

        // @ 0x828BA088 (DWARF h:132) -- set the collision-padding vector on the instance
        // at a pool index.
        void SetVolumePadding(s32 liIndex, Vector3 lPadding);

        // DWARF h:216 / h:220 -- iterate the ALLOCATED entity slots in ascending index
        // order; -1 ends the walk. Added 2026-08-19 (wave Q5 / cluster RMLEG) because
        // SceneManagerModule::RemoveAllOwnerVolumeInstances @0x828CDA70 is a sweep over
        // every live entity and the X360 reaches mEntityPool's own
        // GetFirstObjectIndex/GetNextObjectIndex directly (0x828CDA94 / 0x828CDB64) --
        // the pool is private here, and these two DWARF-declared members are the console's
        // own names for exactly that pair. Header-declared only; no new X360 symbol (the
        // console inlined the pool scan at every use).
        s32 GetFirstEntityIndex() const;
        s32 GetNextEntityIndex(s32 liCurrentIndex) const;

    protected:
        // DWARF h:227 (body CgsEntityManager.cpp:274) -- push a volume-instance slot onto
        // the head of an entity's instance chain and bump its count. Fully INLINED into
        // AddVolumeInstance on the X360 (the 0x828CD930..0x828CD964 tail); kept as its own
        // member because DWARF declares it and RemoveVolumeInstanceFromEntity is its
        // out-of-line mirror.
        void AddVolumeInstanceToEntity(u16 lu16EntityIndex, s32 liVolumeInstanceIndex);

        // @ 0x828BA180 (DWARF h:231, body CgsEntityManager.cpp:299) -- unlink a
        // volume-instance slot from its owning entity's chain and drop the entity's count.
        void RemoveVolumeInstanceFromEntity(s32 liVolumeInstanceIndex);

    private:
        // DWARF h:239-248 (member order verbatim). The element arrays are shared by
        // every bucket of their table (see CgsIndexedHashTable.h).
        CgsContainers::ObjectPool<SceneManagerEntity, KI_MAX_NUM_ENTITIES, s32>     mEntityPool;         // h:239
        CgsContainers::ObjectPool<VolumeInstance, KI_MAX_NUM_VOLUME_INSTANCES, s32> mVolumeInstancePool; // h:240 (X360 +0x27600)
        CgsContainers::IndexedHashTableElement<EntityId, u16>          maEntityIdHashElements[KI_MAX_NUM_ENTITIES];       // h:242 (X360 +0xCA380)
        CgsContainers::IndexedHashTable<EntityId, u16, 541>            mEntityIdToIndex;     // h:243 (X360 +0xE7840)
        // ⚠️ DWARF h:245 sizes this array KI_MAX_NUM_ENTITIES (10000), NOT
        // KI_MAX_NUM_VOLUME_INSTANCES -- and the X360 agrees: element stride 24
        // (`slwi r10,idx,1 ; add r11,idx,r10 ; slwi r11,r11,3` @0x828CD8E0..0x828CD904)
        // over the span +0xE91A0..+0x123B20 == 240000 == 24 * 10000.
        CgsContainers::IndexedHashTableElement<VolumeInstanceId, u32>  maVolumeInstanceIdHashElements[KI_MAX_NUM_ENTITIES]; // h:245 (X360 +0xE91A0)
        CgsContainers::IndexedHashTable<VolumeInstanceId, u32, 509>    mVolumeInstanceIdToIndex; // h:246 (X360 +0x123B20)
        VolumeManager*                                                 mpVolumeManager;      // h:248 (X360 +0x125300)
    };
}
