#pragma once

// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface -- the scene-input write
// interface embedded BY VALUE in the world entity-module IO output buffers
// (RaceCarEntityModuleIO / TriggerEntityModuleIO / PropEntityModuleIO OutputBuffer_*,
// and CgsSceneManager::SceneManagerIO::InputBuffer_Update). It is a large aggregate of
// fixed-capacity EventQueue<> members (mUpdatePositionQueue ... ) plus the producer
// methods that stage one event and append it to the matching queue.
//
// ARTIST Construct (0x822E6550) establishes the order, element types and capacities
// of all 25 queues. DecFIGS repeats their declarations at lines 585–609.
// Clear and Destruct reset those same queue counts (ARTIST 0x822B10C8 / 0x828BA280).
// The offsets beside the members are the native Xbox 360 layout.

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // global Matrix44Affine typedef
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"         // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // CgsSceneManager::VolumeInstanceId
#include "GameShared/GameClasses/Module/CgsEventQueue.h"             // CgsModule::EventQueue<T, N>

// The grounded per-queue element homes (each brings its complete element type by name).
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetEntityPosition.h"   // InEventSetEntityPosition
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddEntity.h"           // InEventAddEntity
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddDynamicVolume.h"    // InEventAddDynamicVolume
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveEntity.h"        // InEventRemoveEntity
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveForCollision.h"  // InEventRemoveForCollision
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveVolume.h"        // InEventRemoveVolume
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveVolumeInstance.h"// InEventRemoveVolumeInstance
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventReplaceDynamicVolume.h"// InEventReplaceDynamicVolume
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventClearCullingTable.h"   // InEventClearCullingTable
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetEntityRadius.h"     // InEventSetEntityRadius
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventClearEntityPadding.h"  // InEventClearEntityPadding
// The additive sibling queues also embedded in this aggregate (grounded by Construct @0x822E6550):
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddForCollision.h"     // InEventAddForCollision
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddVolumeInstance.h"   // InEventAddVolumeInstance
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventForceNoPadding.h"      // InEventForceNoPadding
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetCullingGroupPair.h" // InEventSetCullingGroupPair
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetPadding.h"          // InEventSetPadding
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveAllEntities.h"   // InEventRemoveAllEntities
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h"     // TriangleCacheManagerIO::InEvent{AddToCache,UpdateCachedPosition,RemoveFromCache}
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManagerIO_Events.h" // TriangleCollisionManagerIO::InEvent{AddPolySoupList,ClearPolySoupLists}

namespace rw { namespace collision { class Volume; } }

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // ---- Additive element records for the three volume-instance queues -------------------
    // No committed element home exists for these three (their explicit-instantiation TUs are
    // still todo), so they are defined here from their producers' asm-attested stores. Each
    // derives from a distinctly-named empty event base (CgsModule event-queue convention) to
    // avoid ODR clashes with the other per-element SceneManagerIO::Event bases.

    // mSetVolumeInstanceTransformQueue element (producer @0x822CB7E8: `std r4` id @+0x00, then
    // four `lvx128/stvx128` matrix lanes @+0x10). 16-byte aligned (carries the affine matrix),
    // sizeof 0x50 (80). Capacity 1024 pinned by Construct (offset delta 0x8010 -> 0x1C020).
    struct EventBaseSetVolumeInstanceTransform {};
    class CGS_ALIGN(16) InEventSetVolumeInstanceTransform : public EventBaseSetVolumeInstanceTransform
    {
    public:
        VolumeInstanceId mVolumeInstanceId; // +0x00 (std r4, 8B)
        // +0x08..0x0F pad to the 16-byte alignment forced by the affine matrix lane.
        Matrix44Affine   mTransform;        // +0x10 (four 16B lvx/stvx lanes, 64B)
    };

    // mSetVolumeInstanceCullingGroupQueue element (producer @0x822B1A98: `std r4` id @+0x00,
    // `stw r5` culling-group id @+0x08). 8-byte aligned, sizeof 0x10 (16). Capacity 1280.
    struct EventBaseSetVolumeInstanceCullingGroup {};
    class InEventSetVolumeInstanceCullingGroup : public EventBaseSetVolumeInstanceCullingGroup
    {
    public:
        VolumeInstanceId mVolumeInstanceId; // +0x00 (std r4, 8B)
        InEventAddForCollision::CullingGroup mCullingGroup; // +0x08
    };

    // mAddVolumeInstanceForCachingQueue element (producer @0x8270DA10: `std r30` id @+0x00,
    // `stw r31` cache-options enum word @+0x08). 8-byte aligned, sizeof 0x10 (16). Capacity 64.
    struct EventBaseAddVolumeInstanceForCaching {};
    class InEventAddVolumeInstanceForCaching : public EventBaseAddVolumeInstanceForCaching
    {
    public:
        VolumeInstanceId mVolumeInstanceId; // +0x00 (std r30, 8B)
        s32              meCacheOptions;     // +0x08 (stw r31, 4B; E_*_CACHE_OPTIONS enum word)
    };

    struct CGS_ALIGN(16) InSceneUpdateInterface
    {
        // ADDITIVE GROW (FLAG -- body owned by InSceneUpdateInterface's own TU): the
        // X360 trigger/prop output-buffer Construct bodies (e.g.
        // BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene::Construct @0x822EED90)
        // tail-call InSceneUpdateInterface::Construct on this aggregate. Declared-only here
        // so consuming TUs compile; the owning TU emits the body. No layout change.
        void Construct();

        // Whole-interface count resets (X360 0x822B10C8 / 0x828BA280). Both zero the live count
        // (miLength) of every embedded queue and return `this` (the X360 forwards the pointer;
        // InputBuffer_Update::Construct tail-returns Clear's result). Bodies emitted by this TU.
        void Clear();
        void Destruct();

        // ARTIST 0x822B1518 preserves all 64 bits of the volume ID.
        void AddDynamicVolume(VolumeId lVolumeId, const rw::collision::Volume* lpVolume, u8 lxVolumeTypeFlags);

        void AddEntity(CgsSceneManager::EntityId lEntityId, u32 luEntityTypeFlag, f32 lfBoundingRadius);
        // Full producer signature (X360 @ 0x822B11F8 stages the bounding-sphere CENTRE
        // vmx lane at event +0x00 before the id/flags/radius scalars; the world-entity
        // consumers pass it explicitly). The 3-arg slice above predates this overload.
        void AddEntity(CgsSceneManager::EntityId lEntityId, u32 luEntityTypeFlag,
                       Vector3 lCentre, f32 lfBoundingRadius);

        // ---- the DWARF's own 64-bit ADD half of the collision pair -------------------------
        // ADDED 2026-08-18 (wave Q4, collision seam). Both are the DecFIGS DWARF's ONLY
        // spelling of these two producers -- there is no EntityId form in the DWARF at all:
        //     CgsSceneManagerIO_SceneUpdate.h:388
        //         void AddVolumeInstance(VolumeInstanceId, VolumeId, const Matrix44Affine &);
        //     CgsSceneManagerIO_SceneUpdate.h:418
        //         void AddForCollision(VolumeInstanceId, SetRaceCarCullingGroupEvent::CullingGroup,
        //                              rw::physics::BodyState, Vector3,
        //                              EAddForCollisionCacheOptions);
        // (the same two lines appear five more times in the dumpfile, once per compilation
        //  unit that saw the header; :949/:1312/:1675/:2051 spell CullingGroup through the
        //  InEventAddForCollision typedef instead -- the same u32.)
        //
        // Both have a REAL out-of-line X360 body, emitted in this struct's own TU, so both are
        // bodied in CgsSceneManagerIO_SceneUpdate.cpp rather than inlined here:
        //     AddVolumeInstance @0x822CB650   (58 insns)
        //     AddForCollision   @0x822B1860   (91 insns)
        //
        // This 3-arg form is the console's only AddVolumeInstance producer; there is no
        // EntityId overload.
        void AddVolumeInstance(VolumeInstanceId lVolumeInstanceId, VolumeId lVolumeId,
                               const Matrix44Affine& lrTransform);
        void AddForCollision(VolumeInstanceId lVolumeInstanceId,
                             InEventAddForCollision::CullingGroup lCullingGroup,
                             rw::physics::BodyState leBodyState,
                             Vector3 lPadding,
                             EAddForCollisionCacheOptions leCacheOptions);

        void RemoveEntity(EntityId lEntityId, bool lbRemoveVolumeInstances);
        void RemoveForCollision(VolumeInstanceId lVolumeInstanceId);
        void RemoveVolumeInstance(VolumeInstanceId lVolumeInstanceId);
        void RemoveVolume(VolumeId lVolumeId);
        void SetVolumeInstanceTransform(CgsSceneManager::EntityId lEntityId, const Matrix44Affine& lrTransform);

        // ARTIST 0x822B15F8 passes the complete volume ID and SDK volume.
        void ReplaceDynamicVolume(VolumeId lVolumeId, const rw::collision::Volume* lpVolume);

        // ADDITIVE GROW: bodies emitted by this TU (X360 producers) -- these were not
        // previously declared. Signatures taken from the producers' asm-attested args.
        void SetEntityRadius(CgsSceneManager::EntityId lEntityId, f32 lfBoundingRadius);
        void ClearCullingTable(bool lbCullAll);
        // ADDITIVE (WorldModule::Prepare @0x827D53B0 stage 3 stages the ten
        // world culling-group pairs). Declaration-only; body with this TU's home
        // (pushes an InEventSetCullingGroupPair onto mSetCullingGroupPairQueue).
        void SetCullingGroupPair(InEventAddForCollision::CullingGroup lGroupA,
                                 InEventAddForCollision::CullingGroup lGroupB, bool lbCull);
        void ClearEntityVolumesPadding(CgsSceneManager::EntityId lEntityId);

        // Merged prop-entity write-side updaters. SetVolumeInstanceTransform(VolumeInstanceId)
        // is the @0x822CB7E8 producer (bodied by this TU -- it targets mSetVolumeInstanceTransformQueue);
        // pointer-/value-used by committed consumers, signatures unchanged.
        void SetEntityPosition(u32 luEntityId, const Matrix44Affine& lTransform);
        // The DWARF's ONE declared spelling (DecFIGS CgsSceneManagerIO_SceneUpdate.h:446, source :346):
        // the X360 producer @0x822B1398 takes the position pre-extracted in v1 and stages
        // InEventSetEntityPosition{ mPosition = v1, mEntityId = r4 } -- no matrix is read. The
        // (u32, const Matrix44Affine&) form above is the tree's MODEL for matrix-holding callers;
        // the replay-prop updaters (PropEntityModule::ReplayUpdateProps/PartsInScene @0x822DB370/
        // @0x822DB900) hand it a bare recorded position, which is this overload. Additive
        // (EntityId has operator u32, existing (u32, matrix) calls keep binding above).
        void SetEntityPosition(CgsSceneManager::EntityId lEntityId, Vector3 lPosition);
        void SetVolumeInstanceTransform(VolumeInstanceId lVolumeInstanceId, const Matrix44Affine& lTransform);

        // ⚠️ SIGNATURE CORRECTED 2026-08-12 (prop-spawn link-closure pass): this was
        // committed as a no-arg call. Both higher rungs say otherwise -- the DecFIGS DWARF
        // declares `void RemoveAllEntities(uint8_t)` (CgsSceneManagerIO_SceneUpdate.h:452)
        // and the X360 stores a payload byte of 3 (== E_ENTITYTYPE_PROP) into the queued
        // event at its only call site, PropZoneManager::RemoveAllPropsAndParts @0x822DEF50
        // (0x822DF038). The parameter is the entity-type OWNER whose entities to drop; the
        // matching payload member is InEventRemoveAllEntities::mu8Owner.
        void RemoveAllEntities(u8 lu8Owner);

        // ADDITIVE GROW: the two remaining volume-instance producers (bodied by this TU). Args
        // are asm-attested (r4 8-byte VolumeInstanceId + a 32-bit second word each).
        void SetVolumeInstanceCullingGroup(VolumeInstanceId lVolumeInstanceId,
                                           InEventAddForCollision::CullingGroup lCullingGroup);
        void AddVolumeInstanceForCaching(VolumeInstanceId lVolumeInstanceId, s32 leCacheOptions);     // @0x8270DA10

        // ---- the TRIANGLE-CACHE reposition producer ------------------------------------------
        // ADDED 2026-08-18 (wave Q round 2, shared-header owner). DWARF
        // CgsSceneManagerIO_SceneUpdate.h:478 -- `void UpdateCachedObjectPosition(int32_t,
        // Vector3, float32_t);` (source line, not the dumpfile line; the dumpfile prints it at
        // its line 506). It sits between AddCachedObject (:472) and RemoveCachedObject (:488),
        // neither of which is declared in this tree yet.
        //
        // A HEADER INLINE, deliberately: there is NO out-of-line body in the X360 export set (I
        // scanned the `name` field of every 0x*.json under .ida-exports/BURNOUT_X360_ARTIST.XEX
        // for "UpdateCachedObjectPosition"/"UpdateCachedPosition" -- the only hits are
        // TriangleCacheManagerIO::InEventUpdateCachedPosition::AddEvent, PhysicsModule::
        // UpdateCachedPositions and TriangleCacheManager::ProcessUpdateCachedPositionEvents,
        // none of them this), and every call site has it fully inlined.
        //
        // BODY, transcribed from the one recovered inlined copy -- BrnPhysics::Props::
        // PropManager::UpdateTriangleCache @0x826119A0, instructions 0x82611B00..0x82611B48:
        //     stw  <slot>, event+0x00                       (0x82611B28)
        //     <16B scratch>.xyz = the caller's Vector3      (0x82611B30 stvx128 v127)
        //     <16B scratch>.w   = the caller's f32 radius   (0x82611B38 stfs f0)
        //     copy that 16B scratch to event+0x10           (0x82611B40/0x82611B44)
        //     bl BaseEventQueue<InEventUpdateCachedPosition>::AddEvent(queue, &event)
        //                                                   (0x82611B48; queue == interface +
        //                                                    0xC5290 == mUpdateCachedPositionQueue)
        // Two stores in that window are DEAD (the w-lane zero at 0x82611B1C and the whole-vector
        // zero at 0x82611B20, both overwritten before any address is published) -- they are the
        // register-level way the compiler assembled a Vector3Plus, not behaviour, so they are not
        // transcribed. Called out rather than smoothed.
        //
        // MEASURED, NOT ASSUMED: this producer emits NO "queue too small" tripwire, unlike every
        // sibling producer in this struct. There is no `bl` and no length compare anywhere
        // between the interface fetch at 0x82611AFC and the AddEvent at 0x82611B48.
        //
        // INFERENCE, FLAGGED: both recovered call sites add the 0.1f cache padding
        // (flt_82004014, which I read out of the image: 3D CC CC CD == 0.1f big-endian) to the
        // radius BEFORE the call, so the asm alone cannot prove whether the source put the
        // padding in the caller or in here. The padding is left in the CALLERS because that is
        // what the two call sites of the (still-gated) EmitUpdateTriangleCacheEvent hook do --
        // BrnDetachedWheelManager.cpp:260 and BrnDetachedPartManager.cpp:298-299 both compute
        // `radius + KF_TRIANGLE_CACHE_PADDING + lfSweptDistance` before the call, and the
        // hook's own signature takes an already-padded radius.
        // ⚠️ CORRECTED 2026-08-18 (round 3): this used to call those two "this tree's two
        // already-committed PRODUCERS of the same event". NEITHER IS A PRODUCER. Both call
        // BrnPhysics::Deformation::EmitUpdateTriangleCacheEvent, which is a LOG-ONCE CONDUCTOR
        // GATE with an empty body (BrnDetachedWheelManager.cpp:315-329, declared
        // BrnDetachedWheelManager.h:105, second definition/caller pair in
        // BrnDetachedPartManager.cpp:77/:301); its own banner says "the real bodies are the
        // InEventUpdateCachedPosition / RemoveRigidBody queue AddEvents". There are ZERO
        // committed producers of InEventUpdateCachedPosition in the tree apart from this
        // inline and PropManager_wQ_03.cpp's open-coded block.
        // ⚠️ GATE NOW UNBLOCKED, NOT RETIRED (gotcha 7, reported not fixed -- foreign file):
        // that gate's whole body collapses to one forward to this producer. Conductor to body
        // + retire it; it is not this header's owner's file. If a later wave finds the padding
        // belongs in here instead, the fix is one line here and three at the call sites.
        //
        // NOT LANDED: the DWARF also declares a `VecFloat`-tailed overload at :484
        // (`void UpdateCachedObjectPosition(int32_t, Vector3, VecFloat)`). No X360 call site
        // attests it -- rung 1 decides what exists, and adding an unattested overload against
        // `VecFloat` (== rw::math::vpu::Vector4 in BrnCommonTypes.h) risks making the existing
        // f32 call sites ambiguous for nothing.
        //
        // NAMING NOTE: the DWARF calls the target queue mUpdateCachedObjectPositionQueue
        // (:606); this tree committed it as mUpdateCachedPositionQueue (below, X360 +0xC5290).
        // Not renamed here -- the committed spelling is load-bearing for existing consumers and
        // a rename is not this owner's call.
        void UpdateCachedObjectPosition(s32 liCacheSlot, Vector3 lPosition, f32 lfSphereRadius)
        {
            CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition lEvent;
            lEvent.miCacheSlot = liCacheSlot;                          // stw, event +0x00
            lEvent.mNewPositionAndRadius.SetVector3(lPosition);        // xyz lanes
            lEvent.mNewPositionAndRadius.SetPlus(lfSphereRadius);      // w lane

            mUpdateCachedPositionQueue.AddEvent(lEvent);               // X360 +0xC5290
        }

        // @ 0x827A9340 -- the whole-interface merge every entity-module -> scene bridge
        // drives (BridgeEntityModulesToSceneModule_PreScene @0x827AB490,
        // BridgePhysicsSceneUpdateToScene @0x827ABA40, ...): append all 25 embedded
        // queues from lrOther onto this one. The X360 tail-returns the pointer; the
        // committed void signature is load-bearing (existing callers) and the returned
        // register is an artifact of the tail branch, so this stays void.
        void Append(const InSceneUpdateInterface& lrOther);

        // Read accessors for the four ENTITY queues the scene manager's
        // BridgeInputSceneUpdateInterfaceToSubModules @0x828D1F88 drains (the X360 walks
        // each queue's base + length inline; named here so the consumer never reaches a
        // queue by offset).
        const CgsModule::EventQueue<InEventAddEntity, 5120>&        GetAddEntityQueue() const       { return mAddEntityQueue; }
        const CgsModule::EventQueue<InEventRemoveEntity, 10000>&    GetRemoveEntityQueue() const    { return mRemoveEntityQueue; }
        const CgsModule::EventQueue<InEventSetEntityPosition, 1024>& GetUpdatePositionQueue() const { return mUpdatePositionQueue; }
        const CgsModule::EventQueue<InEventSetEntityRadius, 512>&   GetSetEntityRadiusQueue() const { return mSetEntityRadiusQueue; }

        // ARTIST 0x822E6550 initializes the fixed-capacity queues at these native offsets.
        // DecFIGS CgsSceneManagerIO_SceneUpdate.h:585–609 corroborates their declaration order.
        CgsModule::EventQueue<InEventSetEntityPosition, 1024> mUpdatePositionQueue;  // +0x0
        CgsModule::EventQueue<InEventSetVolumeInstanceTransform, 1024> mSetVolumeInstanceTransformQueue;  // +0x8010
        CgsModule::EventQueue<InEventAddEntity, 5120> mAddEntityQueue;  // +0x1C020
        CgsModule::EventQueue<InEventAddDynamicVolume, 1280> mAddDynamicVolumeQueue;  // +0x44030
        CgsModule::EventQueue<InEventAddForCollision, 1536> mAddForCollisionQueue;  // +0x71040
        CgsModule::EventQueue<InEventAddVolumeInstance, 1280> mAddVolumeInstanceQueue;  // +0x7D050
        CgsModule::EventQueue<InEventForceNoPadding, 64> mForceNoPaddingQueue;  // +0x96060
        CgsModule::EventQueue<InEventRemoveEntity, 10000> mRemoveEntityQueue;  // +0x96270
        CgsModule::EventQueue<InEventRemoveForCollision, 1536> mRemoveForCollisionQueue;  // +0xA9B00
        CgsModule::EventQueue<InEventRemoveVolume, 1344> mRemoveVolumeQueue;  // +0xACB10
        CgsModule::EventQueue<InEventRemoveVolumeInstance, 1280> mRemoveVolumeInstanceQueue;  // +0xAF520
        CgsModule::EventQueue<InEventReplaceDynamicVolume, 64> mReplaceDynamicVolumeQueue;  // +0xB1D30
        CgsModule::EventQueue<InEventSetCullingGroupPair, 64> mSetCullingGroupPairQueue;  // +0xB4140
        CgsModule::EventQueue<InEventClearCullingTable, 64> mClearCullingTableQueue;  // +0xB444C
        CgsModule::EventQueue<InEventSetEntityRadius, 512> mSetEntityRadiusQueue;  // +0xB4498
        CgsModule::EventQueue<InEventSetPadding, 1280> mSetPaddingQueue;  // +0xB54B0
        CgsModule::EventQueue<InEventAddVolumeInstanceForCaching, 64> mAddVolumeInstanceForCachingQueue;  // +0xBF4C0
        CgsModule::EventQueue<InEventClearEntityPadding, 16> mClearEntityPaddingQueue;  // +0xBF8D0
        CgsModule::EventQueue<InEventSetVolumeInstanceCullingGroup, 1280> mSetVolumeInstanceCullingGroupQueue;  // +0xBF920
        CgsModule::EventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache, 298> mAddToCacheQueue;  // +0xC4930
        CgsModule::EventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition, 298> mUpdateCachedPositionQueue;  // +0xC5290
        CgsModule::EventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache, 298> mRemoveFromCacheQueue;  // +0xC77E0
        CgsModule::EventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList, 20> mAddPolySoupListQueue;  // +0xC7C94
        CgsModule::EventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventClearPolySoupLists, 20> mClearPolySoupListsQueue;  // +0xC7DE0
        CgsModule::EventQueue<InEventRemoveAllEntities, 1> mRemoveAllEntitiesQueue;  // +0xC7E3C
    };
}
}
