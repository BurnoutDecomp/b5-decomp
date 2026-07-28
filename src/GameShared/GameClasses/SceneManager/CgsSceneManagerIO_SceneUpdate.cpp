#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                        // BaseEventQueue<T>::AddEvent / GetLength / GetMaxLength (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"      // InSceneUpdateInterface + the embedded queues/elements

#include <cstring>   // std::memcpy (opaque collision-volume image staging, models the Xbox block-copy)

// =============================================================================
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface -- the grounded producer methods
// (X360 0x822B10C8..0x822CB8E4). Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity,
// not byte match).
//
// SHARED SHAPE (every producer below): stage one event record on the stack, then
//   if ( queue.miLength >= queue.miMaxLength )   // asm: lwzx miLength; lwzx miMaxLength; blt skip
//       <fire "SceneManager.<member> too small, increase value in SceneManagerConstants.h">
//   queue.AddEvent(event);                        // BaseEventQueue<T>::AddEvent -- appends unconditionally
// The capacity assert is a NON-GATING tripwire (CGS_ASSERT collapses Begin/Fire/EndAssert and
// the blt always skips it when there is room); AddEvent always runs. The methods return void
// (the X360 tail-calls AddEvent and forwards its result, which the callers discard).
//
// The five formerly-blocked ledger functions of this class are now emitted here, grounded by
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct @ 0x822E6550 (which names
// every embedded queue's element type + capacity, so the whole embedded set is recovered):
//   * Clear / Destruct              -- zero the live count (miLength) of all 25 embedded queues
//                                      (X360 inlines 25 `stw 0, miLength(queue)`); each maps to a
//                                      named queue's BaseEventQueue<T>::Clear(). Both return `this`.
//   * SetVolumeInstanceTransform    -- targets mSetVolumeInstanceTransformQueue (X360 +0x8010);
//   * AddVolumeInstanceForCaching   -- targets mAddVolumeInstanceForCachingQueue (X360 +0xBF4C0);
//   * SetVolumeInstanceCullingGroup -- targets mSetVolumeInstanceCullingGroupQueue (X360 +0xBF920).
//   The three volume-instance element records have no prior committed home, so their field layout
//   is read off each producer's stores (@0x822CB7E8 / 0x8270DA10 / 0x822B1A98) and their capacity
//   off Construct's offsets -- defined in CgsSceneManagerIO_SceneUpdate.h and size-pinned below.
// =============================================================================

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // ----- Update an entity's scene position (X360 0x822B1398) -----
    // Stages { mPosition = transform translation lane (v1), mEntityId = a2 } and appends to
    // mUpdatePositionQueue. The X360 receives the position pre-extracted in the v1 vmx arg; the
    // committed (u32, const Matrix44Affine&) signature the consumers call extracts it here via Pos().
    void InSceneUpdateInterface::SetEntityPosition(u32 luEntityId, const Matrix44Affine& lTransform)
    {
        InEventSetEntityPosition lEvent;
        lEvent.mPosition = lTransform.Pos();   // wAxis translation lane (stvx128 v1)
        lEvent.mEntityId = luEntityId;         // stw a2

        CGS_ASSERT(mUpdatePositionQueue.GetLength() < mUpdatePositionQueue.GetMaxLength(),
                   "SceneManager.mUpdatePositionQueue too small, increase value in SceneManagerConstants.h");
        mUpdatePositionQueue.AddEvent(lEvent);
    }

    // ----- Add an entity to the scene (X360 0x822B11F8) -----
    // Stages { mTransformLane = v1 (caller-supplied, not exposed by this committed 3-arg signature),
    // mEntityId = a2, miField14 = a3, mfField18 = a4 } and appends to mAddEntityQueue.
    void InSceneUpdateInterface::AddEntity(CgsSceneManager::EntityId lEntityId, u32 luEntityTypeFlag, f32 lfBoundingRadius)
    {
        InEventAddEntity lEvent;
        lEvent.mEntityId = lEntityId;                            // stw a2 @+0x10
        lEvent.miField14 = static_cast<s32>(luEntityTypeFlag);   // stw a3 @+0x14
        lEvent.mfField18 = lfBoundingRadius;                     // stfs a4 @+0x18
        // mTransformLane (@+0x00, X360 `stvx128 v1`) is the caller-supplied vmx arg; the committed
        // 3-arg signature does not expose it, so it is left as the record's own storage.

        CGS_ASSERT(mAddEntityQueue.GetLength() < mAddEntityQueue.GetMaxLength(),
                   "SceneManager.mAddEntityQueue too small, increase value in SceneManagerConstants.h");
        mAddEntityQueue.AddEvent(lEvent);
    }

    // ----- Add a dynamic collision volume (X360 0x822B1518) -----
    // Stages { muId = a2, mu8Flags = a4, maVolumeData = memcpy(a3, 128) } and appends to
    // mAddDynamicVolumeQueue.
    void InSceneUpdateInterface::AddDynamicVolume(CgsSceneManager::EntityId lEntityId, const void* lpVolumeImage, u8 lu8VolumeTypeFlag)
    {
        InEventAddDynamicVolume lEvent;
        lEvent.muId     = static_cast<u64>(static_cast<u32>(lEntityId));  // std a2 @+0x00
        lEvent.mu8Flags = lu8VolumeTypeFlag;                             // stb a4 @+0x08
        std::memcpy(lEvent.maVolumeData, lpVolumeImage, 128);           // memcpy(@+0x10, a3, 0x80)

        CGS_ASSERT(mAddDynamicVolumeQueue.GetLength() < mAddDynamicVolumeQueue.GetMaxLength(),
                   "SceneManager.mAddDynamicVolumeQueue too small, increase value in SceneManagerConstants.h");
        mAddDynamicVolumeQueue.AddEvent(lEvent);
    }

    // ----- Replace an existing dynamic volume's collision image (X360 0x822B15F8) -----
    // Stages { muId = a2, maVolumeData = memcpy(a3, 128) } and appends to mReplaceDynamicVolumeQueue
    // (no flags byte, unlike AddDynamicVolume).
    void InSceneUpdateInterface::ReplaceDynamicVolume(CgsSceneManager::EntityId lEntityId, const void* lpVolumeImage)
    {
        InEventReplaceDynamicVolume lEvent;
        lEvent.muId = static_cast<u64>(static_cast<u32>(lEntityId));   // std a2 @+0x00
        std::memcpy(lEvent.maVolumeData, lpVolumeImage, 128);        // memcpy(@+0x10, a3, 0x80)

        CGS_ASSERT(mReplaceDynamicVolumeQueue.GetLength() < mReplaceDynamicVolumeQueue.GetMaxLength(),
                   "SceneManager.mReplaceDynamicVolumeQueue too small, increase value in SceneManagerConstants.h");
        mReplaceDynamicVolumeQueue.AddEvent(lEvent);
    }

    // ----- Remove an entity from the scene (X360 0x822B12D0) -----
    // Stages { mEntityId = a2, muOptions = a3 } and appends to mRemoveEntityQueue.
    void InSceneUpdateInterface::RemoveEntity(CgsSceneManager::EntityId lEntityId, u32 luFlags)
    {
        InEventRemoveEntity lEvent;
        lEvent.mEntityId = lEntityId;   // stw a2 @+0x00
        lEvent.muOptions = luFlags;     // stb a3 @+0x04 (option word; X360 writes its low byte)

        CGS_ASSERT(mRemoveEntityQueue.GetLength() < mRemoveEntityQueue.GetMaxLength(),
                   "SceneManager.mRemoveEntityQueue too small, increase value in SceneManagerConstants.h");
        mRemoveEntityQueue.AddEvent(lEvent);
    }

    // ----- Remove an entity's body from the collision set (X360 0x822B19D0) -----
    // Stages the 64-bit id and appends to mRemoveForCollisionQueue.
    void InSceneUpdateInterface::RemoveForCollision(CgsSceneManager::EntityId lEntityId)
    {
        InEventRemoveForCollision lEvent;
        lEvent.muCollisionId = static_cast<u64>(static_cast<u32>(lEntityId));  // std a2 @+0x00

        CGS_ASSERT(mRemoveForCollisionQueue.GetLength() < mRemoveForCollisionQueue.GetMaxLength(),
                   "SceneManager.mRemoveForCollisionQueue too small, increase value in SceneManagerConstants.h");
        mRemoveForCollisionQueue.AddEvent(lEvent);
    }

    // ----- Remove a dynamic volume (X360 0x822B16D0) -----
    void InSceneUpdateInterface::RemoveVolume(CgsSceneManager::EntityId lEntityId)
    {
        InEventRemoveVolume lEvent;
        lEvent.mVolumeId = VolumeId(static_cast<u64>(static_cast<u32>(lEntityId)));  // std a2 @+0x00

        CGS_ASSERT(mRemoveVolumeQueue.GetLength() < mRemoveVolumeQueue.GetMaxLength(),
                   "SceneManager.mRemoveVolumeQueue too small, increase value in SceneManagerConstants.h");
        mRemoveVolumeQueue.AddEvent(lEvent);
    }

    // ----- Remove a volume instance (X360 0x822B1798) -----
    void InSceneUpdateInterface::RemoveVolumeInstance(CgsSceneManager::EntityId lEntityId)
    {
        InEventRemoveVolumeInstance lEvent;
        lEvent.mVolumeInstanceId.muId = static_cast<u64>(static_cast<u32>(lEntityId));  // std a2 @+0x00

        CGS_ASSERT(mRemoveVolumeInstanceQueue.GetLength() < mRemoveVolumeInstanceQueue.GetMaxLength(),
                   "SceneManager.mRemoveVolumeInstanceQueue too small, increase value in SceneManagerConstants.h");
        mRemoveVolumeInstanceQueue.AddEvent(lEvent);
    }

    // ----- Set an entity's bounding radius (X360 0x822B1450) -----
    // Stages { mEntityId = a2, mfRadius = a3 } and appends to mSetEntityRadiusQueue.
    void InSceneUpdateInterface::SetEntityRadius(CgsSceneManager::EntityId lEntityId, f32 lfBoundingRadius)
    {
        InEventSetEntityRadius lEvent;
        lEvent.mEntityId = lEntityId;         // stw a2 @+0x00
        lEvent.mfRadius  = lfBoundingRadius;   // stfs a3 @+0x04

        CGS_ASSERT(mSetEntityRadiusQueue.GetLength() < mSetEntityRadiusQueue.GetMaxLength(),
                   "SceneManager.mSetEntityRadiusQueue too small, increase value in SceneManagerConstants.h");
        mSetEntityRadiusQueue.AddEvent(lEvent);
    }

    // ----- Clear the culling table (X360 0x827BAB78) -----
    // Stages the single bool flag and appends to mClearCullingTableQueue.
    void InSceneUpdateInterface::ClearCullingTable(bool lbCullAll)
    {
        InEventClearCullingTable lEvent;
        lEvent.mbCullAll = lbCullAll;   // stb a2 @+0x00

        CGS_ASSERT(mClearCullingTableQueue.GetLength() < mClearCullingTableQueue.GetMaxLength(),
                   "SceneManager.mClearCullingTableQueue too small, increase value in SceneManagerConstants.h");
        mClearCullingTableQueue.AddEvent(lEvent);
    }

    // ----- Clear an entity's volume padding (X360 0x822B1C30) -----
    // Stages the entity handle and appends to mClearEntityPaddingQueue.
    void InSceneUpdateInterface::ClearEntityVolumesPadding(CgsSceneManager::EntityId lEntityId)
    {
        InEventClearEntityPadding lEvent;
        lEvent.mEntity = lEntityId;   // stw a2 @+0x00

        CGS_ASSERT(mClearEntityPaddingQueue.GetLength() < mClearEntityPaddingQueue.GetMaxLength(),
                   "SceneManager.mClearEntityPaddingQueue too small, increase value in SceneManagerConstants.h");
        mClearEntityPaddingQueue.AddEvent(lEvent);
    }

    // ----- Set a volume instance's transform (X360 0x822CB7E8) -----
    // Stages { mVolumeInstanceId = a2 (std, 8B), mTransform = *a3 (four vmx lanes, 64B) } and
    // appends to mSetVolumeInstanceTransformQueue.
    void InSceneUpdateInterface::SetVolumeInstanceTransform(VolumeInstanceId lVolumeInstanceId, const Matrix44Affine& lTransform)
    {
        InEventSetVolumeInstanceTransform lEvent;
        lEvent.mVolumeInstanceId = lVolumeInstanceId;   // std a2 @+0x00
        lEvent.mTransform        = lTransform;           // four stvx128 lanes @+0x10

        CGS_ASSERT(mSetVolumeInstanceTransformQueue.GetLength() < mSetVolumeInstanceTransformQueue.GetMaxLength(),
                   "SceneManager.mSetVolumeInstanceTransformQueue too small, increase value in SceneManagerConstants.h");
        mSetVolumeInstanceTransformQueue.AddEvent(lEvent);
    }

    // ----- Set a volume instance's culling group (X360 0x822B1A98) -----
    // Stages { mVolumeInstanceId = a2 (std, 8B), miCullingGroupId = a3 (stw, 4B) } and appends
    // to mSetVolumeInstanceCullingGroupQueue.
    void InSceneUpdateInterface::SetVolumeInstanceCullingGroup(VolumeInstanceId lVolumeInstanceId, s32 liCullingGroupId)
    {
        InEventSetVolumeInstanceCullingGroup lEvent;
        lEvent.mVolumeInstanceId = lVolumeInstanceId;   // std a2 @+0x00
        lEvent.miCullingGroupId  = liCullingGroupId;    // stw a3 @+0x08

        CGS_ASSERT(mSetVolumeInstanceCullingGroupQueue.GetLength() < mSetVolumeInstanceCullingGroupQueue.GetMaxLength(),
                   "SceneManager.mSetVolumeInstanceCullingGroupQueue too small, increase value in SceneManagerConstants.h");
        mSetVolumeInstanceCullingGroupQueue.AddEvent(lEvent);
    }

    // ----- Add a volume instance to the triangle cache (X360 0x8270DA10) -----
    // Two non-gating range tripwires guard the cache-options enum (asm `cmpwi 3; blt` and
    // `cmpwi 2; bne`), then stage { mVolumeInstanceId = a2 (std, 8B), meCacheOptions = a3
    // (stw, 4B) } and append to mAddVolumeInstanceForCachingQueue.
    void InSceneUpdateInterface::AddVolumeInstanceForCaching(VolumeInstanceId lVolumeInstanceId, s32 leCacheOptions)
    {
        CGS_ASSERT(leCacheOptions < 3,  "leCacheOptions < E_NUM_CACHE_OPTIONS");           // enum bound E_NUM_CACHE_OPTIONS == 3
        CGS_ASSERT(leCacheOptions != 2, "leCacheOptions != E_DO_NOT_ADD_TO_CACHE_MANAGER"); // enum value E_DO_NOT_ADD_TO_CACHE_MANAGER == 2

        InEventAddVolumeInstanceForCaching lEvent;
        lEvent.mVolumeInstanceId = lVolumeInstanceId;   // std a2 @+0x00
        lEvent.meCacheOptions    = leCacheOptions;      // stw a3 @+0x08

        CGS_ASSERT(mAddVolumeInstanceForCachingQueue.GetLength() < mAddVolumeInstanceForCachingQueue.GetMaxLength(),
                   "SceneManager.mAddVolumeInstanceForCachingQueue too small, increase value in SceneManagerConstants.h");
        mAddVolumeInstanceForCachingQueue.AddEvent(lEvent);
    }

    // ----- Enable/disable a culling-group adjacency pair (X360 0x822B1B60) -----
    // Stages { muGroupA, muGroupB, muEnabled } (three 32-bit words) and appends to
    // mSetCullingGroupPairQueue after the standard "queue too small" tripwire
    // (X360-baked CgsSceneManagerIO_SceneUpdate.h:1133).
    void InSceneUpdateInterface::SetCullingGroupPair(u8 lu8GroupA, u8 lu8GroupB, u8 lu8Flags)
    {
        InEventSetCullingGroupPair lEvent;
        lEvent.muGroupA  = lu8GroupA;
        lEvent.muGroupB  = lu8GroupB;
        lEvent.muEnabled = lu8Flags;

        CGS_ASSERT(mSetCullingGroupPairQueue.GetLength() < mSetCullingGroupPairQueue.GetMaxLength(),
                   "SceneManager.mSetCullingGroupPairQueue too small, increase value in SceneManagerConstants.h");
        mSetCullingGroupPairQueue.AddEvent(lEvent);
    }

    // (ClearCullingTable @0x827BAB78 already lives earlier in this TU.)

    // ----- One-time bring-up: point every embedded queue at its inline storage -----
    // (X360 0x822E6550: 25 EventQueue<T,N>::Construct calls -- each stores the inline
    // maEvents base, the capacity and a zero live count; the 25 stored queue bases are
    // exactly Clear's queue set. The per-instantiation out-of-line Construct bodies are
    // this tree's EventQueue_*_Construct explicit-instantiation TUs.)
    void InSceneUpdateInterface::Construct()
    {
        mUpdatePositionQueue.Construct();
        mSetVolumeInstanceTransformQueue.Construct();
        mAddEntityQueue.Construct();
        mAddDynamicVolumeQueue.Construct();
        mAddForCollisionQueue.Construct();
        mAddVolumeInstanceQueue.Construct();
        mForceNoPaddingQueue.Construct();
        mRemoveEntityQueue.Construct();
        mRemoveForCollisionQueue.Construct();
        mRemoveVolumeQueue.Construct();
        mRemoveVolumeInstanceQueue.Construct();
        mReplaceDynamicVolumeQueue.Construct();
        mSetCullingGroupPairQueue.Construct();
        mClearCullingTableQueue.Construct();
        mSetEntityRadiusQueue.Construct();
        mSetPaddingQueue.Construct();
        mAddVolumeInstanceForCachingQueue.Construct();
        mClearEntityPaddingQueue.Construct();
        mSetVolumeInstanceCullingGroupQueue.Construct();
        mAddToCacheQueue.Construct();
        mUpdateCachedPositionQueue.Construct();
        mRemoveFromCacheQueue.Construct();
        mAddPolySoupListQueue.Construct();
        mClearPolySoupListsQueue.Construct();
        mRemoveAllEntitiesQueue.Construct();
    }

    // ----- Add an entity to the scene, FULL producer signature (X360 0x822B11F8) -----
    // The 3-arg slice above predates the vmx lane being decoded; this is the real
    // producer the world/prop/traffic entity modules call. The bounding-sphere CENTRE is
    // the caller's v1 vmx argument (`stvx128 v1` at event +0x00), then the id / entity-type
    // flag / radius scalars at +0x10 / +0x14 / +0x18.
    void InSceneUpdateInterface::AddEntity(CgsSceneManager::EntityId lEntityId, u32 luEntityTypeFlag,
                                           Vector3 lCentre, f32 lfBoundingRadius)
    {
        InEventAddEntity lEvent;
        lEvent.mTransformLane = lCentre;                         // stvx128 v1 @+0x00
        lEvent.mEntityId      = lEntityId;                       // stw a2 @+0x10
        lEvent.miField14      = static_cast<s32>(luEntityTypeFlag); // stw a3 @+0x14
        lEvent.mfField18      = lfBoundingRadius;                // stfs a4 @+0x18

        CGS_ASSERT(mAddEntityQueue.GetLength() < mAddEntityQueue.GetMaxLength(),
                   "SceneManager.mAddEntityQueue too small, increase value in SceneManagerConstants.h");
        mAddEntityQueue.AddEvent(lEvent);
    }

    // ----- Merge another interface's staged events onto this one (X360 0x827A9340) -----
    // The whole-interface merge the entity-module -> scene bridges run once per phase:
    // each of the 25 embedded queues appends the matching source queue
    // (BaseEventQueue<T>::Append -- a block copy onto the tail plus the length bump).
    // Order is irrelevant (the queues are independent); the list mirrors Clear's.
    void InSceneUpdateInterface::Append(const InSceneUpdateInterface& lrSource)
    {
        mUpdatePositionQueue.Append(lrSource.mUpdatePositionQueue);
        mSetVolumeInstanceTransformQueue.Append(lrSource.mSetVolumeInstanceTransformQueue);
        mAddEntityQueue.Append(lrSource.mAddEntityQueue);
        mAddDynamicVolumeQueue.Append(lrSource.mAddDynamicVolumeQueue);
        mAddForCollisionQueue.Append(lrSource.mAddForCollisionQueue);
        mAddVolumeInstanceQueue.Append(lrSource.mAddVolumeInstanceQueue);
        mForceNoPaddingQueue.Append(lrSource.mForceNoPaddingQueue);
        mRemoveEntityQueue.Append(lrSource.mRemoveEntityQueue);
        mRemoveForCollisionQueue.Append(lrSource.mRemoveForCollisionQueue);
        mRemoveVolumeQueue.Append(lrSource.mRemoveVolumeQueue);
        mRemoveVolumeInstanceQueue.Append(lrSource.mRemoveVolumeInstanceQueue);
        mReplaceDynamicVolumeQueue.Append(lrSource.mReplaceDynamicVolumeQueue);
        mSetCullingGroupPairQueue.Append(lrSource.mSetCullingGroupPairQueue);
        mClearCullingTableQueue.Append(lrSource.mClearCullingTableQueue);
        mSetEntityRadiusQueue.Append(lrSource.mSetEntityRadiusQueue);
        mSetPaddingQueue.Append(lrSource.mSetPaddingQueue);
        mAddVolumeInstanceForCachingQueue.Append(lrSource.mAddVolumeInstanceForCachingQueue);
        mClearEntityPaddingQueue.Append(lrSource.mClearEntityPaddingQueue);
        mSetVolumeInstanceCullingGroupQueue.Append(lrSource.mSetVolumeInstanceCullingGroupQueue);
        mAddToCacheQueue.Append(lrSource.mAddToCacheQueue);
        mUpdateCachedPositionQueue.Append(lrSource.mUpdateCachedPositionQueue);
        mRemoveFromCacheQueue.Append(lrSource.mRemoveFromCacheQueue);
        mAddPolySoupListQueue.Append(lrSource.mAddPolySoupListQueue);
        mClearPolySoupListsQueue.Append(lrSource.mClearPolySoupListsQueue);
        mRemoveAllEntitiesQueue.Append(lrSource.mRemoveAllEntitiesQueue);
    }

    // ----- Reset every embedded queue's live count (X360 0x822B10C8) -----
    // The X360 inlines a `stw 0, miLength(queue)` for each of the 25 embedded queues (the 25
    // zeroed offsets == Construct's 25 queue bases + 8). Each maps to the queue's own Clear()
    // (BaseEventQueue<T>::Clear -> miLength = 0); order is irrelevant (independent counters).
    InSceneUpdateInterface* InSceneUpdateInterface::Clear()
    {
        mUpdatePositionQueue.Clear();
        mSetVolumeInstanceTransformQueue.Clear();
        mAddEntityQueue.Clear();
        mAddDynamicVolumeQueue.Clear();
        mAddForCollisionQueue.Clear();
        mAddVolumeInstanceQueue.Clear();
        mForceNoPaddingQueue.Clear();
        mRemoveEntityQueue.Clear();
        mRemoveForCollisionQueue.Clear();
        mRemoveVolumeQueue.Clear();
        mRemoveVolumeInstanceQueue.Clear();
        mReplaceDynamicVolumeQueue.Clear();
        mSetCullingGroupPairQueue.Clear();
        mClearCullingTableQueue.Clear();
        mSetEntityRadiusQueue.Clear();
        mSetPaddingQueue.Clear();
        mAddVolumeInstanceForCachingQueue.Clear();
        mClearEntityPaddingQueue.Clear();
        mSetVolumeInstanceCullingGroupQueue.Clear();
        mAddToCacheQueue.Clear();
        mUpdateCachedPositionQueue.Clear();
        mRemoveFromCacheQueue.Clear();
        mAddPolySoupListQueue.Clear();
        mClearPolySoupListsQueue.Clear();
        mRemoveAllEntitiesQueue.Clear();
        return this;
    }

    // ----- Teardown: reset every embedded queue's live count (X360 0x828BA280) -----
    // Byte-identical to Clear (the inline stores the same 25 miLength=0 zeroes and returns this);
    // the inline queues own no heap, so teardown is purely the count reset.
    InSceneUpdateInterface* InSceneUpdateInterface::Destruct()
    {
        mUpdatePositionQueue.Clear();
        mSetVolumeInstanceTransformQueue.Clear();
        mAddEntityQueue.Clear();
        mAddDynamicVolumeQueue.Clear();
        mAddForCollisionQueue.Clear();
        mAddVolumeInstanceQueue.Clear();
        mForceNoPaddingQueue.Clear();
        mRemoveEntityQueue.Clear();
        mRemoveForCollisionQueue.Clear();
        mRemoveVolumeQueue.Clear();
        mRemoveVolumeInstanceQueue.Clear();
        mReplaceDynamicVolumeQueue.Clear();
        mSetCullingGroupPairQueue.Clear();
        mClearCullingTableQueue.Clear();
        mSetEntityRadiusQueue.Clear();
        mSetPaddingQueue.Clear();
        mAddVolumeInstanceForCachingQueue.Clear();
        mClearEntityPaddingQueue.Clear();
        mSetVolumeInstanceCullingGroupQueue.Clear();
        mAddToCacheQueue.Clear();
        mUpdateCachedPositionQueue.Clear();
        mRemoveFromCacheQueue.Clear();
        mAddPolySoupListQueue.Clear();
        mClearPolySoupListsQueue.Clear();
        mRemoveAllEntitiesQueue.Clear();
        return this;
    }

    // Size pins for the three volume-instance element records defined by this TU (their stride
    // is load-bearing: it fixes each queue's capacity against Construct's queue-base offsets).
    static_assert(sizeof(InEventSetVolumeInstanceTransform)     == 80, "InEventSetVolumeInstanceTransform X360 stride 0x50 (id + affine matrix)");
    static_assert(sizeof(InEventSetVolumeInstanceCullingGroup)  == 16, "InEventSetVolumeInstanceCullingGroup X360 stride 0x10 (id + culling-group word)");
    static_assert(sizeof(InEventAddVolumeInstanceForCaching)    == 16, "InEventAddVolumeInstanceForCaching X360 stride 0x10 (id + cache-options word)");
}
}
