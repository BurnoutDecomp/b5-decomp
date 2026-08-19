#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeManager.h"   // VolumeManager::GetVolumeIndexByID
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"     // KI_INVALID_VOLUME_INDEX
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // [DIAG wave Q5] CgsDev::Log::gpDebugPrint

// CgsSceneManager::EntityManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (DWARF primary file
// GameShared/GameClasses/SceneManager/CgsEntityManager.cpp):
//   EntityManager::Prepare                       @0x828C5FC8
//   EntityManager::AddEntity                     @0x828C6090
//   EntityManager::RemoveEntity                  @0x828CD6F8
//   EntityManager::GetFirstEntityVolumeInstance  @0x828C5DC0
//   EntityManager::GetVolumeInstanceIdByIndex    @0x828B9E10
//   EntityManager::SetVolumeForCollision         @0x828CD658
//   EntityManager::SetVolumeForCollisionByIndex  @0x828B9E80
//   EntityManager::SetVolumeInstanceTransform    @0x828CD580
//   EntityManager::SetVolumePadding              @0x828BA088
//
// ADDED 2026-08-18, wave Q5 cluster A2 (the scene-collision middle) -- the volume
// half, i.e. everything between "a prop/car posted AddVolumeInstance" and "the
// broad phase can see a VolumeInstance":
//   EntityManager::AddVolumeInstance             @0x828CD790  (cpp:189)
//   EntityManager::AddVolumeInstanceToEntity     (inlined @0x828CD930; cpp:274)
//   EntityManager::RemoveVolumeInstanceFromEntity@0x828BA180  (cpp:299)
//   EntityManager::GetVolumeInstanceIndexByID    @0x828CD4B8  (h:324)
//   EntityManager::GetVolumeInstance const       @0x828B9F28  (h:488/493)
//   EntityManager::GetVolumeInstance non-const   @0x828B9FD8  (h:518/523)
//
// ADDED 2026-08-19, wave Q5 cluster RMLEG -- the REMOVAL half (the console inlined every
// one of these, so none has an X360 address of its own; each cites the inline expansions
// it was recovered from in its own banner):
//   EntityManager::RemoveVolumeInstance          (DWARF h:120; inlined @0x828D21C4..0x828D21EC)
//   EntityManager::RemoveVolumeInstanceByIndex   (DWARF h:126; inlined @0x828CDA30..0x828CDA48)
//   EntityManager::GetFirstEntityIndex           (DWARF h:216; pool scan @0x828CDA94)
//   EntityManager::GetNextEntityIndex            (DWARF h:220; pool scan @0x828CDB64)
//
// ⚠️ TWO OF THESE RETIRE LIVE SILENT-WRONG STUBS, they are not "absences":
//   GameSource/World/WorldLinkStubs.cpp:1789 GetVolumeInstanceIndexByID **returned 0**
//     -- every caller resolved volume-instance index 0, a valid-looking answer, every
//     frame; and :1806 GetVolumeInstance **returned NULL**. Both must be deleted by the
//     conductor in the same integration step that mounts this file, or the link sees two
//     definitions.
//
// Asm walk: the two ObjectPool Clear instantiations are real X360 symbols
// (SceneManagerEntity<10000> at this+0, VolumeInstance<5048> at this+0x27600);
// the two IndexedHashTable Clears are inlined bucket loops (541 bins @+0xE7840
// over the shared element array @+0xCA380, then 509 bins @+0x123B20 over
// +0xE91A0), each ending in the one-byte constructed flag; return true.
//
// CONST/NON-CONST DISAMBIGUATION (used throughout below). The X360 export truncates
// the two ObjectPool::operator[] instantiations to distinct symbols, and which one a
// body calls tells us its own constness:
//   VolumeInstance pool: @0x828B71E8 = const operator[], @0x828B7348 = non-const
//   SceneManagerEntity pool: `CgsSceneManager::Scen` = const, `..::SceneManag` @0x828B6D70
//                            = non-const.
// So 0x828B9F28 (calls 0x828B71E8, and is itself called from the const
// GetFirstEntityVolumeInstance @0x828C5E58) is the CONST GetVolumeInstance overload and
// 0x828B9FD8 (calls 0x828B7348) is the non-const one -- which also matches the console's
// own definition-line order, CgsEntityManager.h:488 (DWARF h:175, const) before :518
// (DWARF h:180, non-const).

namespace CgsSceneManager
{

// ===========================================================================
// EntityManager::Construct   (DWARF body CgsEntityManager.cpp:51)
//
// NO X360 SYMBOL: the console folds this into SceneManagerModule::Construct, so it
// is DWARF-derived, not asm-derived. The DWARF call list for cpp:51 is exactly the
// two pooled members' lifecycle-Construct (`ObjectPool<SceneManagerEntity,10000,
// int32_t>::Construct` + `ObjectPool<VolumeInstance,5048,int32_t>::Construct`), and
// the sole parameter is named lpVolumeManager against the sole pointer member
// mpVolumeManager (h:248) -- that binding is the only reason the parameter exists.
//
// ⚠️ NOT CALLED ANYWHERE IN THE TREE TODAY. CgsSceneManagerModule.cpp's Construct
// cascade never runs `mEntityManager.Construct(&mVolumeManager)`, so mpVolumeManager
// is indeterminate the first time AddVolumeInstance dereferences it. That wiring is
// one line in a file this cluster does not own -- reported to the conductor, not
// papered over here with a defensive null-check the console does not have.
// ===========================================================================
void EntityManager::Construct(VolumeManager* lpVolumeManager)
{
    mpVolumeManager = lpVolumeManager;

    mEntityPool.Construct();
    mVolumeInstancePool.Construct();
}

// @ 0x828C5FC8
bool EntityManager::Prepare()
{
    mEntityPool.Clear();
    mVolumeInstancePool.Clear();

    mEntityIdToIndex.Clear(maEntityIdHashElements);
    mVolumeInstanceIdToIndex.Clear(maVolumeInstanceIdHashElements);

    return true;
}

// @ 0x828C5DC0 -- const per DWARF h:189 AND per the asm: it reaches the entity pool
// through the CONST operator[] (`CgsSceneManager::Scen`) and forwards to the CONST
// GetVolumeInstance @0x828B9F28.
const VolumeInstance* EntityManager::GetFirstEntityVolumeInstance(
    u16 lu16EntityIndex, s32* lpiFirstVolumeInstanceIndex) const
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

// ===========================================================================
// EntityManager::GetVolumeInstanceIndexByID @ 0x828CD4B8
//
// The console emits this from CgsEntityManager.h:324 (the assert's baked file/line),
// i.e. it is an out-of-class inline in the header; bodied in the .cpp here so exactly
// one definition emits across the ~15 includers.
//
// Asm: `IndexedHashTable<VolumeInstanceId,u32,509>::Get(&mVolumeInstanceIdToIndex, id)`
// (the truncated symbol `i` @0x828CBF18, which itself asserts mbInitialised and then
// returns node+8 == &element.mValue), then `lwz r3,0(r3)` -- the stored pool index.
// The miss arm streams "Volume instance not found: " << id into the assert buffer and
// returns -1 (`li r3,-1` @0x828CD568).
//
// ⚠️ RETIRES A LIVE CORRUPTION: WorldLinkStubs.cpp:1789 returned 0 here, i.e. it named
// volume-instance slot 0 for every id in the game, on every frame.
// ===========================================================================
s32 EntityManager::GetVolumeInstanceIndexByID(VolumeInstanceId lVolumeInstanceId) const
{
    const CgsContainers::IndexedHashTable<VolumeInstanceId, u32, 509>::Element* lpElement =
        mVolumeInstanceIdToIndex.Get(lVolumeInstanceId);
    if (lpElement == NULL)
    {
        CGS_ASSERT(false, "Volume instance not found: ");
        return KI_INVALID_VOLUME_INSTANCE_INDEX;
    }

    return static_cast<s32>(lpElement->GetValue());
}

// ===========================================================================
// EntityManager::GetVolumeInstance -- const @ 0x828B9F28 (h:488/493),
//                                     non-const @ 0x828B9FD8 (h:518/523)
//
// Both bodies are the same four steps, in this order (the branch order is
// load-bearing: the -1 early-out is BETWEEN the two asserts, so an invalid index
// returns NULL quietly while a merely-negative one trips "liViIndex >= 0"):
//     assert( liViIndex < KI_MAX_NUM_VOLUME_INSTANCES );   // blt @0x828B9F48
//     if ( liViIndex == -1 ) return NULL;                  // beq @0x828B9F6C
//     assert( liViIndex >= 0 );                            // bge @0x828B9F80
//     if ( !mVolumeInstancePool.IsObjectAllocated(liViIndex) ) return NULL;
//     return &mVolumeInstancePool[liViIndex];
//
// ⚠️ RETIRES WorldLinkStubs.cpp:1806, which returned NULL unconditionally.
// ===========================================================================
const VolumeInstance* EntityManager::GetVolumeInstance(s32 liViIndex) const
{
    CGS_ASSERT(liViIndex < KI_MAX_NUM_VOLUME_INSTANCES,
               "liViIndex < KI_MAX_NUM_VOLUME_INSTANCES");

    if (liViIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        return NULL;
    }

    CGS_ASSERT(liViIndex >= 0, "liViIndex >= 0");

    if (!mVolumeInstancePool.IsObjectAllocated(liViIndex))
    {
        return NULL;
    }

    return &mVolumeInstancePool[liViIndex];
}

VolumeInstance* EntityManager::GetVolumeInstance(s32 liViIndex)
{
    CGS_ASSERT(liViIndex < KI_MAX_NUM_VOLUME_INSTANCES,
               "liViIndex < KI_MAX_NUM_VOLUME_INSTANCES");

    if (liViIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        return NULL;
    }

    CGS_ASSERT(liViIndex >= 0, "liViIndex >= 0");

    if (!mVolumeInstancePool.IsObjectAllocated(liViIndex))
    {
        return NULL;
    }

    return &mVolumeInstancePool[liViIndex];
}

// @ 0x828CD658 -- DWARF h:165 returns void; the Hex-Rays `_DWORD *result` is just the
// pool pointer left in r3 by the tail (the asm never builds a return value, and the
// miss arm falls straight out of CgsDev::Assert::EndAssert).
void EntityManager::SetVolumeForCollision(VolumeInstanceId lVolumeInstanceId, bool lbForCollision)
{
    const s32 liIndex = GetVolumeInstanceIndexByID(lVolumeInstanceId);
    if (liIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        CGS_ASSERT(false, "Volume instance not found");
        return;
    }

    mVolumeInstancePool[liIndex].SetCollidable(lbForCollision);
}

// @ 0x828B9E80 -- the by-index twin (no id lookup; -1 is the only rejected input).
void EntityManager::SetVolumeForCollisionByIndex(s32 liIndex, bool lbForCollision)
{
    if (liIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        CGS_ASSERT(false, "Volume instance not found");
        return;
    }

    mVolumeInstancePool[liIndex].SetCollidable(lbForCollision);
}

// @ 0x828CD580
Vector3 EntityManager::SetVolumeInstanceTransform(VolumeInstanceId lVolumeInstanceId,
                                                  const Matrix44Affine& lrTransform)
{
    Vector3 lPositionDelta;

    const s32 liIndex = GetVolumeInstanceIndexByID(lVolumeInstanceId);
    if (liIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        lPositionDelta.SetZero();
        CGS_ASSERT(false, "Volume instance not found");
    }
    else
    {
        VolumeInstance& lrInstance = mVolumeInstancePool[liIndex];

        // Movement delta (new translation - old translation) computed BEFORE the
        // world transform is overwritten -- the X360 `vsubfp128 v127, v13, v0` of the
        // two +0x30 rows @0x828CD5E8.
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
// and inserting it. Returns the slot index, or KU16_INVALID_ENTITY_INDEX when the
// pool is exhausted.
// ===========================================================================
u16 EntityManager::AddEntity(EntityId lEntityId)
{
    const s32 liIndex = mEntityPool.AllocateObject();

    CGS_ASSERT(liIndex < KI_MAX_NUM_ENTITIES,
               "Out of bounds index coming back from EntityPool.AllocateObject: ");

    if (liIndex == -1)
    {
        CGS_ASSERT(false, "Failed to allocate entity ");
        return KU16_INVALID_ENTITY_INDEX;
    }

    SceneManagerEntity& lrEntity = mEntityPool[liIndex];
    lrEntity.mUserID                = lEntityId;
    lrEntity.mu16NumVolumeInstances = 0;
    lrEntity.miFirstVolumeInstance  = KI_INVALID_VOLUME_INSTANCE_INDEX;

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
// other table): hash the id to its bucket and walk the ascending chain. See the
// header for why this keeps the s32/-1 shape instead of the DWARF's u16/0xFFFF.
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

// @ 0x828BA088 -- DWARF h:132 returns void (the X360 leaves the pool pointer in r3
// after the padding store; nothing consumes it).
void EntityManager::SetVolumePadding(s32 liIndex, Vector3 lPadding)
{
    CGS_ASSERT(liIndex < KI_MAX_NUM_VOLUME_INSTANCES,
               "liIndex < KI_MAX_NUM_VOLUME_INSTANCES");
    CGS_ASSERT(liIndex != KI_INVALID_VOLUME_INSTANCE_INDEX,
               "liIndex != KI_INVALID_VOLUME_INSTANCE_INDEX");
    CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
    CGS_ASSERT(mVolumeInstancePool.IsObjectAllocated(liIndex),
               "mVolumeInstancePool.IsObjectAllocated( liIndex )");

    mVolumeInstancePool[liIndex].mPadding = lPadding;
}

// ===========================================================================
// EntityManager::AddVolumeInstance @ 0x828CD790   (DWARF body CgsEntityManager.cpp:189)
//
// THE SCENE-SIDE SEAT OF InSceneUpdateInterface::AddVolumeInstance. Until this
// landed, an AddVolumeInstance event drained into nothing: no VolumeInstance record
// ever existed, so the sweeper had no objects and the culler had no pairs -- which is
// why a car could not hit a prop.
//
// Asm spine (r3 = this, r4 = the 64-bit VolumeInstanceId, r5 = the 64-bit VolumeId,
// r6 = &transform; the id and the volume id each occupy ONE 64-bit GPR -- they are
// passed BY VALUE, matching DWARF h:115):
//
//   0x828CD7B8  VolumeInstance<5048,int>::AllocateObject(&mVolumeInstancePool)
//   0x828CD7C4  if (== -1) -> 0x828CD970: assert "liVolumeInstanceIndex != -1"
//                             (CgsEntityManager.cpp:218) and return -1
//   0x828CD7D0  lpVolumeInstance = &mVolumeInstancePool[liVolumeInstanceIndex]   (cpp:194)
//   0x828CD7D8  srdi r11,r27,32   -- the id's embedded 32-bit entity word         (cpp:196)
//   0x828CD7E8  IndexedHashTable<EntityId,u16,541>::Get(&mEntityIdToIndex, thatWord)
//                 -> lu16EntityIndex, or the `li r26,-1` default == 0xFFFF        (cpp:197)
//   0x828CD808  lwzx  mpVolumeManager (this+0x125300)
//   0x828CD814  IndexedHashTable<VolumeId,u32,227>::Get(&mpVolumeManager->mVolumeIdToIndex,
//                 lVolumeId) -> liVolumeIndex, or -1                              (cpp:198)
//                 == VolumeManager::GetVolumeIndexByID, inlined here.
//   0x828CD83C  assert "liVolumeIndex != KI_INVALID_VOLUME_INDEX"                 (cpp:200)
//   0x828CD864  assert "lu16EntityIndex != KU16_INVALID_ENTITY_INDEX"             (cpp:201)
//   0x828CD884  std  id,0x50 / sth entityIdx,0x64 / stw -1,0x60 / 4x stvx transform
//               / stw volIdx,0x5C / stb flags&0xFC,0x6A / stvx zero,0x40
//   0x828CD91C  maVolumeInstanceIdHashElements[idx].Set(id, idx)  (std key,0(e); stw val,8(e))
//   0x828CD924  mVolumeInstanceIdToIndex.Insert(&that element)
//   0x828CD930..0x828CD964  AddVolumeInstanceToEntity(lu16EntityIndex, idx), inlined
//   returns liVolumeInstanceIndex.
//
// The `stb r7 & 0xFC` at 0x828CD8F0 clears mx8Flags bits 0 and 1 in one store; DWARF
// lists the two calls it folds, VolumeInstance::SetCollidable then ::SetCached, so
// they are spelled out here. The padding lane is built from flt_82001CC0 x3 plus a
// zero word (0x828CD8E4/0x828CD8EC/0x828CD8F4/0x828CD8F8) -- Hex-Rays resolves that
// constant to 0.0f, i.e. the zero vector, which is SetZero() here.
// ===========================================================================
s32 EntityManager::AddVolumeInstance(VolumeInstanceId lVolumeInstanceId,
                                     VolumeId lVolumeId,
                                     const Matrix44Affine& lrTransform)
{
    const s32 liVolumeInstanceIndex = mVolumeInstancePool.AllocateObject();   // cpp:191
    if (liVolumeInstanceIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        CGS_ASSERT(false, "liVolumeInstanceIndex != -1");                    // cpp:218
        return liVolumeInstanceIndex;
    }

    VolumeInstance& lrVolumeInstance = mVolumeInstancePool[liVolumeInstanceIndex];  // cpp:194

    // cpp:196 -- the entity that owns this placement is named INSIDE the id: its
    // embedded 32-bit EntityId word sits at bit KU_ENTITY_ID_START_INDEX (32).
    // DWARF calls VolumeInstanceId::GetEntityId() here; that accessor does not exist
    // yet in CgsVolumeInstanceId.h (owned by another cluster), so the same shift is
    // spelled through that type's own named constant -- NOT a raw offset poke.
    const EntityId lEntityId(
        static_cast<u32>(lVolumeInstanceId.muId >> VolumeInstanceId::KU_ENTITY_ID_START_INDEX));

    // cpp:197 -- absent maps to KU16_INVALID_ENTITY_INDEX. The console gets that for
    // free (`li r26,-1` then `sth` truncates to 0xFFFF); GetEntityIndexByID keeps the
    // s32/-1 shape here (see the header), so the conversion is explicit.
    const s32 liEntityIndex   = GetEntityIndexByID(lEntityId);
    const u16 lu16EntityIndex = (liEntityIndex < 0)
                                    ? KU16_INVALID_ENTITY_INDEX
                                    : static_cast<u16>(liEntityIndex);

    // cpp:198 -- the VolumeManager pool index for this volume id.
    const s32 liVolumeIndex = mpVolumeManager->GetVolumeIndexByID(lVolumeId);

    CGS_ASSERT(liVolumeIndex != KI_INVALID_VOLUME_INDEX,
               "liVolumeIndex != KI_INVALID_VOLUME_INDEX");                  // cpp:200
    CGS_ASSERT(lu16EntityIndex != KU16_INVALID_ENTITY_INDEX,
               "lu16EntityIndex != KU16_INVALID_ENTITY_INDEX");              // cpp:201

    lrVolumeInstance.mUserID                     = lVolumeInstanceId;
    lrVolumeInstance.mu16EntityIndex             = lu16EntityIndex;
    lrVolumeInstance.miVolumeIndex               = liVolumeIndex;
    lrVolumeInstance.miNextEntityVolumeInstance  = KI_INVALID_VOLUME_INSTANCE_INDEX;
    lrVolumeInstance.mPadding.SetZero();
    lrVolumeInstance.mWorldSpaceTransform        = lrTransform;
    lrVolumeInstance.SetCollidable(false);
    lrVolumeInstance.SetCached(false);

    // Publish the slot in the id->index table through the shared element array (the
    // element index IS the pool index, exactly as AddEntity does on the other table).
    maVolumeInstanceIdHashElements[liVolumeInstanceIndex].Set(
        lVolumeInstanceId, static_cast<u32>(liVolumeInstanceIndex));
    mVolumeInstanceIdToIndex.Insert(&maVolumeInstanceIdHashElements[liVolumeInstanceIndex]);

    AddVolumeInstanceToEntity(lu16EntityIndex, liVolumeInstanceIndex);

    // [DIAG wave Q5] ADDITIVE, not console behaviour. The wave's success signal is
    // "the scene reports a non-zero volume-instance population" -- until this body
    // existed the count was structurally zero, so a first-hit line plus a running
    // total is the cheapest way to tell a landed pipeline from a silent one. Same
    // shape and guard as the existing [culling-diag] tally in CgsSceneManagerModule.cpp.
    if (CgsDev::Log::gpDebugPrint != 0)
    {
        static s32 siNumVolumeInstancesAdded = 0;
        ++siNumVolumeInstancesAdded;
        if (siNumVolumeInstancesAdded == 1)
        {
            *CgsDev::Log::gpDebugPrint
                << "[Q5-scene] FIRST VolumeInstance registered: entity index "
                << static_cast<s32>(lu16EntityIndex) << " volume index " << liVolumeIndex
                << " -> slot " << liVolumeInstanceIndex << "\n";
        }
        else if ((siNumVolumeInstancesAdded % 256) == 0)
        {
            *CgsDev::Log::gpDebugPrint << "[Q5-scene] volume-instance population "
                                       << siNumVolumeInstancesAdded << "\n";
        }
    }

    return liVolumeInstanceIndex;
}

// ===========================================================================
// EntityManager::AddVolumeInstanceToEntity   (DWARF h:227, body CgsEntityManager.cpp:274)
//
// Push the new slot onto the HEAD of the entity's singly-linked instance chain:
//   0x828CD944  liOldFirstVI = lpEntity->miFirstVolumeInstance          (cpp:280)
//   0x828CD94C  lpEntity->miFirstVolumeInstance = liVolumeInstanceIndex
//   0x828CD954  lpVolumeInstance->mu16EntityIndex = lu16EntityIndex
//   0x828CD958  lpVolumeInstance->miNextEntityVolumeInstance = liOldFirstVI
//   0x828CD95C  ++lpEntity->mu16NumVolumeInstances
// Fully inlined into AddVolumeInstance on the X360 (no standalone symbol); DWARF
// declares it and names the two BitArray::IsBitSet probes that are the two pools'
// operator[] slot-allocated tripwires.
// ===========================================================================
void EntityManager::AddVolumeInstanceToEntity(u16 lu16EntityIndex, s32 liVolumeInstanceIndex)
{
    SceneManagerEntity& lrEntity = mEntityPool[lu16EntityIndex];                     // cpp:276
    VolumeInstance& lrVolumeInstance = mVolumeInstancePool[liVolumeInstanceIndex];   // cpp:277

    const s32 liOldFirstVI = lrEntity.miFirstVolumeInstance;                         // cpp:280
    lrEntity.miFirstVolumeInstance = liVolumeInstanceIndex;

    lrVolumeInstance.mu16EntityIndex            = lu16EntityIndex;
    lrVolumeInstance.miNextEntityVolumeInstance = liOldFirstVI;

    ++lrEntity.mu16NumVolumeInstances;
}

// ===========================================================================
// EntityManager::RemoveVolumeInstanceFromEntity @ 0x828BA180
//                                              (DWARF h:231, body CgsEntityManager.cpp:299)
//
// Unlink one volume-instance slot from its owning entity's chain. The owner is read
// off the record itself (mu16EntityIndex), so the caller only supplies the slot index.
//
//   0x828BA1A0  lpVolumeInstance = &mVolumeInstancePool[liVolumeInstanceIndex]  (cpp:301)
//   0x828BA1B0  lpEntity = &mEntityPool[lpVolumeInstance->mu16EntityIndex]      (cpp:302)
//   0x828BA1C8  assert "lpEntity->mu16NumVolumeInstances > 0"                   (cpp:304)
//   0x828BA1E8  liCurrentVI = lpEntity->miFirstVolumeInstance                   (cpp:307)
//   0x828BA1F0  HEAD CASE: chain head is the slot -> relink the head, --count, return
//   0x828BA21C  WALK: lpCurrentVolumeInstance = &mVolumeInstancePool[liCurrentVI] (cpp:319)
//               if its ->miNextEntityVolumeInstance is the slot: splice it out,
//               --count, return; if that next is -1: fall out to the miss assert.
//   0x828BA238  assert "Entity does not contain the specified volume instance"  (cpp:329)
//
// (The `addis r11,r11,1 ; addi r11,r11,-1` pairs at 0x828BA1FC / 0x828BA268 are the
// compiler's 0x10000-1 form of a 16-bit decrement -- only the low halfword is stored
// back, so they are plain `--count`.)
// ===========================================================================
void EntityManager::RemoveVolumeInstanceFromEntity(s32 liVolumeInstanceIndex)
{
    VolumeInstance& lrVolumeInstance = mVolumeInstancePool[liVolumeInstanceIndex];   // cpp:301
    SceneManagerEntity& lrEntity = mEntityPool[lrVolumeInstance.mu16EntityIndex];    // cpp:302

    CGS_ASSERT(lrEntity.mu16NumVolumeInstances > 0,
               "lpEntity->mu16NumVolumeInstances > 0");                              // cpp:304

    s32 liCurrentVI = lrEntity.miFirstVolumeInstance;                                // cpp:307
    if (liCurrentVI == liVolumeInstanceIndex)
    {
        lrEntity.miFirstVolumeInstance = lrVolumeInstance.miNextEntityVolumeInstance;
        --lrEntity.mu16NumVolumeInstances;
        return;
    }

    while (liCurrentVI != KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        VolumeInstance& lrCurrentVolumeInstance = mVolumeInstancePool[liCurrentVI];  // cpp:319
        if (lrCurrentVolumeInstance.miNextEntityVolumeInstance == liVolumeInstanceIndex)
        {
            lrCurrentVolumeInstance.miNextEntityVolumeInstance =
                lrVolumeInstance.miNextEntityVolumeInstance;
            --lrEntity.mu16NumVolumeInstances;
            return;
        }
        liCurrentVI = lrCurrentVolumeInstance.miNextEntityVolumeInstance;
    }

    CGS_ASSERT(false, "Entity does not contain the specified volume instance");      // cpp:329
}

// ===========================================================================
// EntityManager::RemoveVolumeInstance        (DWARF h:120)
// EntityManager::RemoveVolumeInstanceByIndex (DWARF h:126)
//
// ⭐ WAVE Q5 / CLUSTER RMLEG (2026-08-19). The removal mirror of AddVolumeInstance, and
// the missing half of the scene's volume registry: until this landed, NOTHING in the
// tree ever took a VolumeInstanceId back out of mVolumeInstanceIdToIndex, so a re-created
// owner (the player car on a colour change / HIDE_ONLINE remove+create, prop-cell churn)
// re-inserted the SAME key and IndexedHashTable<VolumeInstanceId,u32,509>::Insert fired
// "2 elements with the same key inserted" on the next AddVolumeInstance.
//
// NO OUT-OF-LINE X360 SYMBOL: neither method has a row in progress/identity.json, because
// the console inlined both at every use. The shape below is the common factor of the three
// verbatim expansions, which agree instruction for instruction:
//
//   drain leg 3, BridgeInputSceneUpdateInterfaceToSubModules:
//     0x828D21C4  bl EntityManager::GetVolumeInstanceIndexByID   (r4 = the queued id)
//     0x828D21D4  bl EntityManager::RemoveVolumeInstanceFromEntity(r4 = that index)
//     0x828D21E0  bl IndexedHashTable<VolumeInstanceId,u32,509>::Remove
//                    (r3 = this + 0x123B20 == &mVolumeInstanceIdToIndex, r4 = the id)
//     0x828D21EC  bl ObjectPool<VolumeInstance,5048,int>::FreeObject
//                    (r3 = this + 0x27600 == &mVolumeInstancePool,     r4 = that index)
//   SceneManagerModule::RemoveAllEntityVolumeInstances tail:
//     0x828CDA30 / 0x828CDA3C / 0x828CDA48 -- the same last three, with the index already
//     in hand and the id re-loaded off the record (`ld r28, 0x50(r31)` @0x828CDA28).
//   SceneManagerModule::RemoveAllOwnerVolumeInstances reaches the same tail through it.
//
// So the id form is exactly `ByIndex(id, GetVolumeInstanceIndexByID(id))`, and the ByIndex
// form is the three-call trio. The two X360 offsets above are DOCUMENTATION: this build
// reaches both members by name.
//
// ⚠️ ORDER IS LOAD-BEARING. RemoveVolumeInstanceFromEntity reads mu16EntityIndex off the
// record it is unlinking, so it must run BEFORE the slot is freed; and the hash Remove
// takes the ID, which the pool slot still holds. Reordering silently corrupts the owner's
// chain.
// ===========================================================================
void EntityManager::RemoveVolumeInstance(VolumeInstanceId lVolumeInstanceId)
{
    const s32 liIndex = GetVolumeInstanceIndexByID(lVolumeInstanceId);

    // FLAG PC (not console): the X360 does NOT test the lookup result -- 0x828D21C8 moves
    // r3 straight into r4 and calls on. On the console an absent id therefore reaches
    // mVolumeInstancePool[-1]; here that is an out-of-bounds write into a 5048-element
    // by-value pool inside SceneManagerModule, i.e. live memory corruption of whatever
    // member precedes it. The tripwire below is the console's own class of dev assert and
    // is LOUD (it is not a silent swallow), and the early return only skips work that had
    // no target. Producers that can post an id twice do exist -- PropCellManager's
    // deactivate path and ActiveRaceCar::RemoveFromScene both post RemoveVolumeInstance
    // without first proving the id is still registered.
    if (liIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        // NOT a console rodata string -- this guard has no console counterpart, so it must
        // not borrow one (the image's "Volume instance not found for volume instance id "
        // @0x820F61A8 belongs to SceneManagerModule, not here).
        CGS_ASSERT(false, "PC guard: RemoveVolumeInstance called with an unregistered VolumeInstanceId");
        return;
    }

    RemoveVolumeInstanceByIndex(lVolumeInstanceId, liIndex);
}

void EntityManager::RemoveVolumeInstanceByIndex(VolumeInstanceId lVolumeInstanceId, s32 liIndex)
{
    RemoveVolumeInstanceFromEntity(liIndex);          // 0x828D21D4 / 0x828CDA30
    mVolumeInstanceIdToIndex.Remove(lVolumeInstanceId); // 0x828D21E0 / 0x828CDA3C
    mVolumeInstancePool.FreeObject(liIndex);          // 0x828D21EC / 0x828CDA48
}

// ===========================================================================
// EntityManager::GetFirstEntityIndex / GetNextEntityIndex   (DWARF h:216 / h:220)
//
// The allocated-slot walk over mEntityPool. The X360 calls the pool's own
// ObjectPool<SceneManagerEntity,10000,int>::GetFirstObjectIndex / GetNextObjectIndex
// directly on &mEntityManager (the pool is the manager's first member, so `r3 = &em`
// IS `&mEntityPool`) -- see SceneManagerModule::RemoveAllOwnerVolumeInstances
// @0x828CDA94 and @0x828CDB64. Both return -1 when no allocated slot remains.
//
// Clearing the CURRENT slot's bit mid-walk is safe and is what the console does:
// GetNextObjectIndex scans strictly ABOVE liCurrentIndex, so RemoveEntity(index)
// followed by GetNextEntityIndex(index) still enumerates every remaining slot exactly once.
// ===========================================================================
s32 EntityManager::GetFirstEntityIndex() const
{
    return mEntityPool.GetFirstObjectIndex();
}

s32 EntityManager::GetNextEntityIndex(s32 liCurrentIndex) const
{
    return mEntityPool.GetNextObjectIndex(liCurrentIndex);
}

}
