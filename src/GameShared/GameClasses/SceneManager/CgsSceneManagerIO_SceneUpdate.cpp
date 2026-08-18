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

    // The DWARF-spelled overload (see the header note): the position arrives pre-extracted,
    // exactly as the X360's v1 argument @0x822B1398 -- same event, same queue, same assert.
    void InSceneUpdateInterface::SetEntityPosition(CgsSceneManager::EntityId lEntityId, Vector3 lPosition)
    {
        InEventSetEntityPosition lEvent;
        lEvent.mPosition = lPosition;          // stvx128 v1
        lEvent.mEntityId = lEntityId;          // stw a2

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

    // ----- Register a collision volume instance with the scene (X360 0x822CB650) -----
    // ADDED 2026-08-18 (wave Q4, collision seam). The DWARF's ONE spelling of this producer
    // (CgsSceneManagerIO_SceneUpdate.h:388) -- 64-bit VolumeInstanceId + 64-bit VolumeId +
    // the world transform. Decoded instruction by instruction from the raw `assembly` array
    // of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822CB650.json (Hex-Rays pseudocode not used):
    //
    //   0x822CB664  srdi  r11, r4, 32       ; the embedded entity word (bits [32..63])
    //   0x822CB66C  srwi  r11, r11, 24      ; its owner byte
    //   0x822CB674  cmplwi cr6, r11, 0xC    ; K_MAX_ENTITY_TYPE
    //   0x822CB69C  ble   -> skip           ; assert owner <= 0xC, message built by streaming
    //                                       ; the raw 64-bit id after the literal
    //                                       ; "Burnout Specific: Bad Volume Instance "
    //                                       ; (baked line 0x3E2 == 994)
    //   0x822CB730  std   r4,  event+0x00   ; mVolumeInstanceId
    //   0x822CB738  std   r5,  event+0x08   ; mVolumeId
    //   0x822CB714/28/48/5C/60/6C  four lvx128/stvx128 lanes r6[0x00..0x3F] -> event+0x10
    //   0x822CB744  lwzx  r11, this, 0x7D058 ; miLength  \  the standard "queue too small"
    //   0x822CB750  lwzx  r10, this, 0x7D054 ; miMaxLength >  tripwire, baked line 0x3E8 == 1000
    //   0x822CB770  blt   -> skip                        /
    //   0x822CB7CC  addis r3, this, 8 ; addi r3, r3, -0x2FB0   ; == this + 0x7D050
    //   0x822CB7D8  bl    BaseEventQueue<InEventAddVolumeInstance>::AddEvent
    //
    // this+0x7D050 is mAddVolumeInstanceQueue (the header's own X360 offset comment), and
    // +4/+8 off a queue base are miMaxLength/miLength -- consistent with every sibling
    // producer in this TU. The three console offsets are provenance only: this body reaches
    // the queue BY NAME (gotcha 1 -- 0x7D050 is a console byte offset, not a host one).
    //
    // MEASURED, NOT ASSUMED: the owner tripwire compares against 0xC (VolumeInstanceId::
    // KU_MAX_ENTITY_TYPE), NOT against E_ENTITYTYPE_PROP -- this producer is entity-type
    // agnostic; it is the PROP handles' own operator VolumeInstanceId() that pins owner == 3,
    // and that conversion fires at the CALL SITE, not here.
    void InSceneUpdateInterface::AddVolumeInstance(VolumeInstanceId lVolumeInstanceId,
                                                   VolumeId lVolumeId,
                                                   const Matrix44Affine& lrTransform)
    {
        CGS_ASSERT(lVolumeInstanceId.GetEntityIDOwner() <= VolumeInstanceId::KU_MAX_ENTITY_TYPE,
                   "Burnout Specific: Bad Volume Instance ");

        InEventAddVolumeInstance lEvent;
        lEvent.mVolumeInstanceId = lVolumeInstanceId;   // std a2 @+0x00
        lEvent.mVolumeId         = lVolumeId;           // std a3 @+0x08
        lEvent.mTransform        = lrTransform;         // four stvx128 lanes @+0x10

        CGS_ASSERT(mAddVolumeInstanceQueue.GetLength() < mAddVolumeInstanceQueue.GetMaxLength(),
                   "SceneManager.mAddVolumeInstanceQueue too small, increase value in SceneManagerConstants.h");
        mAddVolumeInstanceQueue.AddEvent(lEvent);
    }

    // ----- Put a registered volume instance into the broad phase (X360 0x822B1860) -----
    // ADDED 2026-08-18 (wave Q4, collision seam). DWARF CgsSceneManagerIO_SceneUpdate.h:418.
    // Decoded from the raw `assembly` array of 0x822B1860.json:
    //
    //   arg regs: r3 this, r4 VolumeInstanceId (64-bit), r5 CullingGroup, r6 BodyState,
    //             v1 the Vector3 padding (a vmx argument), r7 CacheOptions.
    //             -- the Vector3 is DWARF parameter 4 yet lands in v1 while r7 carries
    //             parameter 5, i.e. the vector argument does NOT consume a GPR slot here.
    //             Proved by the rodata the asm pairs with each register: r7 is the operand of
    //             the "leCacheOptions < E_NUM_CACHE_OPTIONS" compare (0x822B1890) and r6 of
    //             "(int)leBodyState <= 0xff" (0x822B18C0).
    //
    //   0x822B1890  cmpwi cr6, r7, 3 ; blt skip   ; assert leCacheOptions < E_NUM_CACHE_OPTIONS
    //                                             ; (baked line 0x426 == 1062)
    //   0x822B18BC  std   r4, event+0x10          ; mVolumeInstanceId
    //   0x822B18C4  stvx128 v1, event+0x00        ; mPadding (the Vector3 lane)
    //   0x822B18C8  stw   r5, event+0x18          ; mCullGroup
    //   0x822B18C0..EC  SetBodyState(r6)          ; tripwire line 0xBC == 188, then stb +0x1C
    //   0x822B18F0..1918 SetCacheOptions(r7)      ; tripwire line 0xC9 == 201, then stb +0x1D
    //   0x822B1928  lwzx  r11, this, 0x71048      ; miLength     \  "queue too small"
    //   0x822B192C  lwzx  r10, this, 0x71044      ; miMaxLength   > tripwire, line 0x42E == 1070
    //   0x822B1934  blt   -> skip                                /
    //   0x822B19AC  addis r3, this, 7 ; addi r3, r3, 0x1040      ; == this + 0x71040
    //   0x822B19B8  bl    BaseEventQueue<InEventAddForCollision>::AddEvent
    //
    // this+0x71040 is mAddForCollisionQueue. Console offsets are provenance only; the body
    // reaches the queue by name.
    //
    // FIELD-ORDER NOTE: the asm writes mVolumeInstanceId (+0x10) BEFORE mPadding (+0x00)
    // because the vmx store had to wait for v127's copy of v1; the two stores are independent
    // slots of the same stack record, so declaration order is used here.
    void InSceneUpdateInterface::AddForCollision(VolumeInstanceId lVolumeInstanceId,
                                                 InEventAddForCollision::CullingGroup lCullingGroup,
                                                 rw::physics::BodyState leBodyState,
                                                 Vector3 lPadding,
                                                 EAddForCollisionCacheOptions leCacheOptions)
    {
        CGS_ASSERT(leCacheOptions < E_NUM_CACHE_OPTIONS, "leCacheOptions < E_NUM_CACHE_OPTIONS");

        InEventAddForCollision lEvent;
        lEvent.mPadding          = lPadding;             // stvx128 v1 @+0x00
        lEvent.mVolumeInstanceId = lVolumeInstanceId;    // std a2 @+0x10
        lEvent.mCullGroup        = lCullingGroup;        // stw a3 @+0x18
        lEvent.SetBodyState(leBodyState);                // stb @+0x1C, own tripwire
        lEvent.SetCacheOptions(leCacheOptions);          // stb @+0x1D, own tripwire

        CGS_ASSERT(mAddForCollisionQueue.GetLength() < mAddForCollisionQueue.GetMaxLength(),
                   "SceneManager.mAddForCollisionQueue too small, increase value in SceneManagerConstants.h");
        mAddForCollisionQueue.AddEvent(lEvent);
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

    // ----- Drop every entity belonging to one entity-type owner -----
    // BODIED 2026-08-12 (prop-spawn link-closure pass).
    //
    // The X360 never emits this out of line -- it is a two-line header inline, and the one
    // producer in the build shows it whole. PropZoneManager::RemoveAllPropsAndParts
    // @0x822DEF50 inlines it at 0x822DF018-38 as:
    //     bl  sub_822B9738                 ; OutputBuffer_PreScene::GetSceneInputInterface
    //     addis r3,r3,0xC / addi r3,r3,0x7E3C   ; + 0xC7E3C == mRemoveAllEntitiesQueue
    //     bl  0x822C6AE8                   ; BaseEventQueue<T>::AddEvent()  -- the NO-ARG
    //                                      ; "reserve the tail slot" overload: asserts
    //                                      ; mpEvents != NULL and GetLength() < GetMaxLength(),
    //                                      ; returns mpEvents + miLength (UNSHIFTED, i.e.
    //                                      ; element stride 1), post-increments miLength
    //     li  r10, 3 / stb r10, 0(r11)     ; slot.mu8Owner = 3 == E_ENTITYTYPE_PROP
    //
    // So the whole body is "reserve a slot and write the owner byte" -- note there is NO
    // separate "queue too small" tripwire here as in the siblings above, because AddEvent()
    // itself carries the two asserts (the sibling producers use the by-value AddEvent(const
    // T&) overload, which does not).
    void InSceneUpdateInterface::RemoveAllEntities(u8 lu8Owner)
    {
        mRemoveAllEntitiesQueue.AddEvent().mu8Owner = lu8Owner;
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
