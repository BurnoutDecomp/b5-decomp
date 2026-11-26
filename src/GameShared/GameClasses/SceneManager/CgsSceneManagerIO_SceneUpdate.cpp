#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                        // BaseEventQueue<T>::AddEvent / GetLength / GetMaxLength (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"      // InSceneUpdateInterface + the embedded queues/elements

// Scene update producers stage an event and append it to the corresponding queue.
// ARTIST checks capacity before the append, even when the diagnostic fires.
// The historical assertion paths and lines come from the FireAssert arguments.

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    namespace
    {
        const char KAC_SCENE_UPDATE_ABSOLUTE_ARTIST_FILE[] =
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\scenemanager\\CgsSceneManagerIO_SceneUpdate.h";
    }

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
        lEvent.mEntityId = lEntityId;
        lEvent.mPosition = lPosition;

        CGS_ASSERT_STREAM_AT(mUpdatePositionQueue.GetLength() < mUpdatePositionQueue.GetMaxLength(),
                   "SceneManager.mUpdatePositionQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 884);
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

    // ARTIST 0x822B1518 and DecFIGS CgsSceneManagerIO_SceneUpdate.h:367.
    void InSceneUpdateInterface::AddDynamicVolume(VolumeId lVolumeId, const rw::collision::Volume* lpVolume,
                                                 u8 lxVolumeTypeFlags)
    {
        InEventAddDynamicVolume lEvent;
        lEvent.mVolumeId = lVolumeId;
        lEvent.mxVolumeTypeID = lxVolumeTypeFlags;
        // DecFIGS volume.h:39 aliases the SDK Volume to VolRef::Volume.
        lEvent.mVolumeBuffer.SetVolume(reinterpret_cast<const VolRef::Volume*>(lpVolume));
        CGS_ASSERT_STREAM_AT(mAddDynamicVolumeQueue.GetLength() < mAddDynamicVolumeQueue.GetMaxLength(),
                   "SceneManager.mAddDynamicVolumeQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 933);
        mAddDynamicVolumeQueue.AddEvent(lEvent);
    }

    // ARTIST 0x822B15F8 and DecFIGS CgsSceneManagerIO_SceneUpdate.h:373.
    void InSceneUpdateInterface::ReplaceDynamicVolume(VolumeId lVolumeId, const rw::collision::Volume* lpVolume)
    {
        InEventReplaceDynamicVolume lEvent;
        lEvent.mVolumeId = lVolumeId;
        lEvent.mVolumeBuffer.SetVolume(reinterpret_cast<const VolRef::Volume*>(lpVolume));
        CGS_ASSERT_STREAM_AT(mReplaceDynamicVolumeQueue.GetLength() < mReplaceDynamicVolumeQueue.GetMaxLength(),
                   "SceneManager.mReplaceDynamicVolumeQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 954);
        mReplaceDynamicVolumeQueue.AddEvent(lEvent);
    }

    // ----- Remove an entity from the scene (X360 0x822B12D0) -----
    void InSceneUpdateInterface::RemoveEntity(EntityId lEntityId, bool lbRemoveVolumeInstances)
    {
        InEventRemoveEntity lEvent;
        lEvent.mEntityId = lEntityId;   // stw a2 @+0x00
        lEvent.mbRemoveVolumeInstances = lbRemoveVolumeInstances;

        CGS_ASSERT_STREAM_AT(mRemoveEntityQueue.GetLength() < mRemoveEntityQueue.GetMaxLength(),
                   "SceneManager.mRemoveEntityQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 863);
        mRemoveEntityQueue.AddEvent(lEvent);
    }

    // ----- Remove an entity's body from the collision set (X360 0x822B19D0) -----
    // Stages the 64-bit id and appends to mRemoveForCollisionQueue.
    void InSceneUpdateInterface::RemoveForCollision(VolumeInstanceId lVolumeInstanceId)
    {
        InEventRemoveForCollision lEvent;
        lEvent.mVolumeInstanceId = lVolumeInstanceId;

        CGS_ASSERT_STREAM_AT(mRemoveForCollisionQueue.GetLength() < mRemoveForCollisionQueue.GetMaxLength(),
                   "SceneManager.mRemoveForCollisionQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 1089);
        mRemoveForCollisionQueue.AddEvent(lEvent);
    }

    // ----- Remove a dynamic volume (X360 0x822B16D0) -----
    void InSceneUpdateInterface::RemoveVolume(VolumeId lVolumeId)
    {
        InEventRemoveVolume lEvent;
        lEvent.mVolumeId = lVolumeId;

        CGS_ASSERT_STREAM_AT(mRemoveVolumeQueue.GetLength() < mRemoveVolumeQueue.GetMaxLength(),
                   "SceneManager.mRemoveVolumeQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 973);
        mRemoveVolumeQueue.AddEvent(lEvent);
    }

    // ----- Remove a volume instance (X360 0x822B1798) -----
    void InSceneUpdateInterface::RemoveVolumeInstance(VolumeInstanceId lVolumeInstanceId)
    {
        InEventRemoveVolumeInstance lEvent;
        lEvent.mVolumeInstanceId = lVolumeInstanceId;

        CGS_ASSERT_STREAM_AT(mRemoveVolumeInstanceQueue.GetLength() < mRemoveVolumeInstanceQueue.GetMaxLength(),
                   "SceneManager.mRemoveVolumeInstanceQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 1019);
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
        if (lVolumeInstanceId.GetEntityIDOwner() > VolumeInstanceId::KU_MAX_ENTITY_TYPE)
        {
            // ARTIST 0x822CB6E4–0x822CB6FC prints the packed ID in hexadecimal,
            // then restores decimal mode on the shared assertion stream.
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            ((lMessage << "Burnout Specific: Bad Volume Instance ").SetPrintMode(CgsDev::E_PRINTMODE_HEX)
                << lVolumeInstanceId.muId).SetPrintMode(CgsDev::E_PRINTMODE_DECIMAL);
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                                       CGS_ASSERT_LOCATION(KAC_SCENE_UPDATE_ARTIST_FILE, 994));
            CgsDev::Assert::EndAssert();
        }

        InEventAddVolumeInstance lEvent;
        lEvent.mVolumeInstanceId = lVolumeInstanceId;   // std a2 @+0x00
        lEvent.mVolumeId         = lVolumeId;           // std a3 @+0x08
        lEvent.mTransform        = lrTransform;         // four stvx128 lanes @+0x10

        CGS_ASSERT_STREAM_AT(mAddVolumeInstanceQueue.GetLength() < mAddVolumeInstanceQueue.GetMaxLength(),
                   "SceneManager.mAddVolumeInstanceQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 1000);
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
    void InSceneUpdateInterface::AddForCollision(VolumeInstanceId lVolumeInstanceId,
                                                 InEventAddForCollision::CullingGroup lCullingGroup,
                                                 rw::physics::BodyState leBodyState,
                                                 Vector3 lPadding,
                                                 EAddForCollisionCacheOptions leCacheOptions)
    {
        CGS_ASSERT_AT(leCacheOptions < E_NUM_CACHE_OPTIONS,
                   "leCacheOptions < E_NUM_CACHE_OPTIONS", KAC_SCENE_UPDATE_ARTIST_FILE, 1062);

        InEventAddForCollision lEvent;
        lEvent.mVolumeInstanceId = lVolumeInstanceId;
        lEvent.mPadding = lPadding;
        lEvent.mCullGroup        = lCullingGroup;        // stw a3 @+0x18
        lEvent.SetBodyState(leBodyState);                // stb @+0x1C, own tripwire
        lEvent.SetCacheOptions(leCacheOptions);          // stb @+0x1D, own tripwire

        CGS_ASSERT_STREAM_AT(mAddForCollisionQueue.GetLength() < mAddForCollisionQueue.GetMaxLength(),
                   "SceneManager.mAddForCollisionQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 1070);
        mAddForCollisionQueue.AddEvent(lEvent);
    }

    // ----- Set an entity's bounding radius (X360 0x822B1450) -----
    // Stages { mEntityId = a2, mfRadius = a3 } and appends to mSetEntityRadiusQueue.
    void InSceneUpdateInterface::SetEntityRadius(CgsSceneManager::EntityId lEntityId, f32 lfBoundingRadius)
    {
        InEventSetEntityRadius lEvent;
        lEvent.mEntityId = lEntityId;         // stw a2 @+0x00
        lEvent.mfRadius  = lfBoundingRadius;   // stfs a3 @+0x04

        CGS_ASSERT_STREAM_AT(mSetEntityRadiusQueue.GetLength() < mSetEntityRadiusQueue.GetMaxLength(),
                   "SceneManager.mSetEntityRadiusQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 905);
        mSetEntityRadiusQueue.AddEvent(lEvent);
    }

    // ----- Clear the culling table (X360 0x827BAB78) -----
    // Stages the single bool flag and appends to mClearCullingTableQueue.
    void InSceneUpdateInterface::ClearCullingTable(bool lbCullAll)
    {
        InEventClearCullingTable lEvent;
        lEvent.mbCullAll = lbCullAll;   // stb a2 @+0x00

        CGS_ASSERT_STREAM_AT(mClearCullingTableQueue.GetLength() < mClearCullingTableQueue.GetMaxLength(),
                   "SceneManager.mClearCullingTableQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ABSOLUTE_ARTIST_FILE, 1150);
        mClearCullingTableQueue.AddEvent(lEvent);
    }

    // ----- Clear an entity's volume padding (X360 0x822B1C30) -----
    // Stages the entity handle and appends to mClearEntityPaddingQueue.
    void InSceneUpdateInterface::ClearEntityVolumesPadding(CgsSceneManager::EntityId lEntityId)
    {
        InEventClearEntityPadding lEvent;
        lEvent.mEntity = lEntityId;   // stw a2 @+0x00

        CGS_ASSERT_STREAM_AT(mClearEntityPaddingQueue.GetLength() < mClearEntityPaddingQueue.GetMaxLength(),
                   "SceneManager.mClearEntityPaddingQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 1193);
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

        CGS_ASSERT_STREAM_AT(mSetVolumeInstanceTransformQueue.GetLength() < mSetVolumeInstanceTransformQueue.GetMaxLength(),
                   "SceneManager.mSetVolumeInstanceTransformQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 1040);
        mSetVolumeInstanceTransformQueue.AddEvent(lEvent);
    }

    // PropZoneManager::RemoveAllPropsAndParts inlines this at 0x822DF018–0x822DF038.
    // The call at 0x822DF028 reserves the slot, then the caller stores the owner byte.
    void InSceneUpdateInterface::RemoveAllEntities(u8 lu8Owner)
    {
        mRemoveAllEntitiesQueue.AllocateEvent()->mu8Owner = lu8Owner;
    }

    // ----- Set a volume instance's culling group (X360 0x822B1A98) -----
    void InSceneUpdateInterface::SetVolumeInstanceCullingGroup(VolumeInstanceId lVolumeInstanceId,
                                                               InEventAddForCollision::CullingGroup lCullingGroup)
    {
        InEventSetVolumeInstanceCullingGroup lEvent;
        lEvent.mCullingGroup = lCullingGroup;
        lEvent.mVolumeInstanceId = lVolumeInstanceId;

        CGS_ASSERT_STREAM_AT(mSetVolumeInstanceCullingGroupQueue.GetLength() < mSetVolumeInstanceCullingGroupQueue.GetMaxLength(),
                   "SceneManager.mSetVolumeInstanceCullingGroupQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 1110);
        mSetVolumeInstanceCullingGroupQueue.AddEvent(lEvent);
    }

    // ----- Add a volume instance to the triangle cache (X360 0x8270DA10) -----
    // Two non-gating range tripwires guard the cache-options enum (asm `cmpwi 3; blt` and
    // `cmpwi 2; bne`), then stage { mVolumeInstanceId = a2 (std, 8B), meCacheOptions = a3
    // (stw, 4B) } and append to mAddVolumeInstanceForCachingQueue.
    void InSceneUpdateInterface::AddVolumeInstanceForCaching(VolumeInstanceId lVolumeInstanceId,
                                                              EAddForCollisionCacheOptions leCacheOptions)
    {
        CGS_ASSERT_AT(leCacheOptions < E_NUM_CACHE_OPTIONS,
                   "leCacheOptions < E_NUM_CACHE_OPTIONS", KAC_SCENE_UPDATE_ABSOLUTE_ARTIST_FILE, 1232);
        CGS_ASSERT_AT(leCacheOptions != E_DO_NOT_ADD_TO_CACHE_MANAGER,
                   "leCacheOptions != E_DO_NOT_ADD_TO_CACHE_MANAGER", KAC_SCENE_UPDATE_ABSOLUTE_ARTIST_FILE, 1233);

        InEventAddVolumeInstanceForCaching lEvent;
        lEvent.mVolumeInstanceId = lVolumeInstanceId;   // std a2 @+0x00
        lEvent.meCacheOptions    = leCacheOptions;      // stw a3 @+0x08

        CGS_ASSERT_STREAM_AT(mAddVolumeInstanceForCachingQueue.GetLength() < mAddVolumeInstanceForCachingQueue.GetMaxLength(),
                   "SceneManager.mAddVolumeInstanceForCachingQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ABSOLUTE_ARTIST_FILE, 1240);
        mAddVolumeInstanceForCachingQueue.AddEvent(lEvent);
    }

    // ----- Enable/disable a culling-group adjacency pair (X360 0x822B1B60) -----
    void InSceneUpdateInterface::SetCullingGroupPair(InEventAddForCollision::CullingGroup lGroupA,
                                                    InEventAddForCollision::CullingGroup lGroupB, bool lbCull)
    {
        InEventSetCullingGroupPair lEvent;
        lEvent.mbCull = lbCull;
        lEvent.mCullingGroupA = lGroupA;
        lEvent.mCullingGroupB = lGroupB;

        CGS_ASSERT_STREAM_AT(mSetCullingGroupPairQueue.GetLength() < mSetCullingGroupPairQueue.GetMaxLength(),
                   "SceneManager.mSetCullingGroupPairQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 1133);
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
        mClearEntityPaddingQueue.Construct();
        mSetVolumeInstanceCullingGroupQueue.Construct();
        mAddVolumeInstanceForCachingQueue.Construct();
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

        CGS_ASSERT_STREAM_AT(mAddEntityQueue.GetLength() < mAddEntityQueue.GetMaxLength(),
                   "SceneManager.mAddEntityQueue too small, increase value in SceneManagerConstants.h",
                                                KAC_SCENE_UPDATE_ARTIST_FILE, 825);
        mAddEntityQueue.AddEvent(lEvent);
    }

    // ----- Merge another interface's staged events onto this one (X360 0x827A9340) -----
    // The whole-interface merge the entity-module -> scene bridges run once per phase:
    // each of the 25 embedded queues appends the matching source queue
    // (BaseEventQueue<T>::Append -- a block copy onto the tail plus the length bump).
    // ARTIST 0x827A9340 determines the order of the queue appends.
    void InSceneUpdateInterface::Append(const InSceneUpdateInterface& lrSource)
    {
        mSetVolumeInstanceTransformQueue.Append(lrSource.mSetVolumeInstanceTransformQueue);
        mUpdatePositionQueue.Append(lrSource.mUpdatePositionQueue);
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
        mSetVolumeInstanceCullingGroupQueue.Append(lrSource.mSetVolumeInstanceCullingGroupQueue);
        mAddVolumeInstanceForCachingQueue.Append(lrSource.mAddVolumeInstanceForCachingQueue);
        mClearEntityPaddingQueue.Append(lrSource.mClearEntityPaddingQueue);
        mAddToCacheQueue.Append(lrSource.mAddToCacheQueue);
        mUpdateCachedPositionQueue.Append(lrSource.mUpdateCachedPositionQueue);
        mRemoveFromCacheQueue.Append(lrSource.mRemoveFromCacheQueue);
        // ARTIST 0x827A94F4 and 0x827A9514 append this queue twice.
        mUpdateCachedPositionQueue.Append(lrSource.mUpdateCachedPositionQueue);
        mAddPolySoupListQueue.Append(lrSource.mAddPolySoupListQueue);
        mClearPolySoupListsQueue.Append(lrSource.mClearPolySoupListsQueue);
        mRemoveAllEntitiesQueue.Append(lrSource.mRemoveAllEntitiesQueue);
    }

    // ----- Reset every embedded queue's live count (X360 0x822B10C8) -----
    // The X360 inlines a `stw 0, miLength(queue)` for each of the 25 embedded queues (the 25
    // zeroed offsets == Construct's 25 queue bases + 8). Each maps to the queue's own Clear()
    // (BaseEventQueue<T>::Clear -> miLength = 0), in the original store order.
    void InSceneUpdateInterface::Clear()
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
        mSetVolumeInstanceCullingGroupQueue.Clear();
        mAddVolumeInstanceForCachingQueue.Clear();
        mClearEntityPaddingQueue.Clear();
        mAddToCacheQueue.Clear();
        mUpdateCachedPositionQueue.Clear();
        mRemoveFromCacheQueue.Clear();
        mAddPolySoupListQueue.Clear();
        mClearPolySoupListsQueue.Clear();
        mRemoveAllEntitiesQueue.Clear();
    }

    // ----- Teardown: reset every embedded queue's live count (X360 0x828BA280) -----
    // The inline queues own their backing storage, and ARTIST resets the 25 live counts.
    void InSceneUpdateInterface::Destruct()
    {
        mUpdatePositionQueue.Destruct();
        mSetVolumeInstanceTransformQueue.Destruct();
        mAddEntityQueue.Destruct();
        mAddDynamicVolumeQueue.Destruct();
        mAddForCollisionQueue.Destruct();
        mAddVolumeInstanceQueue.Destruct();
        mForceNoPaddingQueue.Destruct();
        mRemoveEntityQueue.Destruct();
        mRemoveForCollisionQueue.Destruct();
        mRemoveVolumeQueue.Destruct();
        mRemoveVolumeInstanceQueue.Destruct();
        mReplaceDynamicVolumeQueue.Destruct();
        mSetCullingGroupPairQueue.Destruct();
        mClearCullingTableQueue.Destruct();
        mSetEntityRadiusQueue.Destruct();
        mSetPaddingQueue.Destruct();
        mSetVolumeInstanceCullingGroupQueue.Destruct();
        mAddVolumeInstanceForCachingQueue.Destruct();
        mClearEntityPaddingQueue.Destruct();
        mAddToCacheQueue.Destruct();
        mRemoveFromCacheQueue.Destruct();
        mUpdateCachedPositionQueue.Destruct();
        mAddPolySoupListQueue.Destruct();
        mClearPolySoupListsQueue.Destruct();
        mRemoveAllEntitiesQueue.Destruct();
    }

    // Size pins for the three volume-instance element records defined by this TU (their stride
    // is load-bearing: it fixes each queue's capacity against Construct's queue-base offsets).
    CGS_STATIC_ASSERT((sizeof(InEventSetVolumeInstanceTransform)     == 80), "InEventSetVolumeInstanceTransform X360 stride 0x50 (id + affine matrix)");
    CGS_STATIC_ASSERT((sizeof(InEventSetVolumeInstanceCullingGroup)  == 16), "InEventSetVolumeInstanceCullingGroup X360 stride 0x10 (id + culling-group word)");
    CGS_STATIC_ASSERT((sizeof(InEventAddVolumeInstanceForCaching)    == 16), "InEventAddVolumeInstanceForCaching X360 stride 0x10 (id + cache-options word)");
}
}
