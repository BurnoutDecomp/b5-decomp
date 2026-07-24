#pragma once

// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface -- the scene-input write
// interface embedded BY VALUE in the world entity-module IO output buffers
// (RaceCarEntityModuleIO / TriggerEntityModuleIO / PropEntityModuleIO OutputBuffer_*,
// and CgsSceneManager::SceneManagerIO::InputBuffer_Update). It is a large aggregate of
// fixed-capacity EventQueue<> members (mUpdatePositionQueue ... ) plus the producer
// methods that stage one event and append it to the matching queue.
//
// LAYOUT (this TU is the DWARF home CgsSceneManagerIO_SceneUpdate.h). The X360 producers
// address each embedded queue at a large this-relative byte offset and append via the
// per-element BaseEventQueue<T>::AddEvent. The queues below are the ones this TU's own
// producers (0x822B10C8..0x822CB8E4) touch and whose element type + capacity are grounded
// by a committed element home + the matching EventQueue_*_N explicit-instantiation TU. They
// are declared in ascending X360-offset order (== DWARF declaration order). PC layout is the
// baseline (widened pointers): the 8-byte-pointer BaseEventQueue header keeps the absolute
// PC offsets from matching the X360 byte offsets for the <=4-aligned elements, exactly as the
// project models every recovered layout -- so the producers reach each queue BY NAME, not by
// the X360 byte offset. Nothing external reads a raw offset (every consumer takes &member or
// embeds the aggregate by value), so the named-member model is the faithful reconstruction.
//
// COMPLETE LAYOUT (grounded): the whole embedded-queue set is now recovered from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct @ 0x822E6550, which
// constructs all 25 embedded queues in ascending this-relative offset order, naming each
// queue's element type + capacity (EventQueue<T, N>). The whole-interface Clear/Destruct
// (X360 0x822B10C8 / 0x828BA280) each zero the live count (miLength) of those 25 queues; the
// 25 zeroed offsets in the asm are exactly Construct's 25 queue bases + 8. Every element type
// has a committed home in the _Event*.h family, except the three volume-instance element
// records (SetVolumeInstanceTransform / SetVolumeInstanceCullingGroup / AddVolumeInstanceForCaching)
// whose field layout is read directly off their producers' stores (@0x822CB7E8 / 0x822B1A98 /
// 0x8270DA10) and defined below; their capacities are pinned by Construct's offsets.
//
// The 14 additive queue members below are APPENDED after the 11 committed members (never
// shifting the committed front). The named-member reconstruction is order-independent for the
// producers (they reach each queue BY NAME) and for Clear/Destruct (independent per-queue count
// resets), so appending -- rather than interleaving in X360-offset order -- is faithful and
// keeps the committed members' offsets unchanged. X360 offsets are kept in per-member comments.

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
    struct alignas(16) InEventSetVolumeInstanceTransform : public EventBaseSetVolumeInstanceTransform
    {
        VolumeInstanceId mVolumeInstanceId; // +0x00 (std r4, 8B)
        // +0x08..0x0F pad to the 16-byte alignment forced by the affine matrix lane.
        Matrix44Affine   mTransform;        // +0x10 (four 16B lvx/stvx lanes, 64B)
    };

    // mSetVolumeInstanceCullingGroupQueue element (producer @0x822B1A98: `std r4` id @+0x00,
    // `stw r5` culling-group id @+0x08). 8-byte aligned, sizeof 0x10 (16). Capacity 1280.
    struct EventBaseSetVolumeInstanceCullingGroup {};
    struct InEventSetVolumeInstanceCullingGroup : public EventBaseSetVolumeInstanceCullingGroup
    {
        VolumeInstanceId mVolumeInstanceId; // +0x00 (std r4, 8B)
        s32              miCullingGroupId;  // +0x08 (stw r5, 4B)
    };

    // mAddVolumeInstanceForCachingQueue element (producer @0x8270DA10: `std r30` id @+0x00,
    // `stw r31` cache-options enum word @+0x08). 8-byte aligned, sizeof 0x10 (16). Capacity 64.
    struct EventBaseAddVolumeInstanceForCaching {};
    struct InEventAddVolumeInstanceForCaching : public EventBaseAddVolumeInstanceForCaching
    {
        VolumeInstanceId mVolumeInstanceId; // +0x00 (std r30, 8B)
        s32              meCacheOptions;     // +0x08 (stw r31, 4B; E_*_CACHE_OPTIONS enum word)
    };

    struct alignas(16) InSceneUpdateInterface
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
        InSceneUpdateInterface* Clear();
        InSceneUpdateInterface* Destruct();

        // Producer methods (X360 0x822B10C8..0x822CB8E4). Each stages one event on the stack,
        // fires the queue's "too small" tripwire assert when the target queue is full, then
        // appends the event to the matching embedded queue. Signatures for the pre-existing
        // declarations are LOAD-BEARING (committed consumers call them) and left unchanged.
        void AddDynamicVolume(CgsSceneManager::EntityId lEntityId, const void* lpVolumeImage, u8 lu8VolumeTypeFlag);
        void AddEntity(CgsSceneManager::EntityId lEntityId, u32 luEntityTypeFlag, f32 lfBoundingRadius);
        // Full producer signature (X360 @ 0x822B11F8 stages the bounding-sphere CENTRE
        // vmx lane at event +0x00 before the id/flags/radius scalars; the world-entity
        // consumers pass it explicitly). The 3-arg slice above predates this overload.
        void AddEntity(CgsSceneManager::EntityId lEntityId, u32 luEntityTypeFlag,
                       Vector3 lCentre, f32 lfBoundingRadius);
        void AddVolumeInstance(CgsSceneManager::EntityId lEntityId, const Matrix44Affine& lrTransform);

        void RemoveEntity(CgsSceneManager::EntityId lEntityId, u32 luFlags);
        void RemoveForCollision(CgsSceneManager::EntityId lEntityId);
        void RemoveVolumeInstance(CgsSceneManager::EntityId lEntityId);
        void RemoveVolume(CgsSceneManager::EntityId lEntityId);
        void SetVolumeInstanceTransform(CgsSceneManager::EntityId lEntityId, const Matrix44Affine& lrTransform);

        // ADDITIVE GROW: bodies emitted by this TU (X360 producers) -- these were not
        // previously declared. Signatures taken from the producers' asm-attested args.
        void ReplaceDynamicVolume(CgsSceneManager::EntityId lEntityId, const void* lpVolumeImage);
        void SetEntityRadius(CgsSceneManager::EntityId lEntityId, f32 lfBoundingRadius);
        void ClearCullingTable(bool lbCullAll);
        void ClearEntityVolumesPadding(CgsSceneManager::EntityId lEntityId);

        // Merged prop-entity write-side updaters. SetVolumeInstanceTransform(VolumeInstanceId)
        // is the @0x822CB7E8 producer (bodied by this TU -- it targets mSetVolumeInstanceTransformQueue);
        // pointer-/value-used by committed consumers, signatures unchanged.
        void SetEntityPosition(u32 luEntityId, const Matrix44Affine& lTransform);
        void SetVolumeInstanceTransform(VolumeInstanceId lVolumeInstanceId, const Matrix44Affine& lTransform);
        void RemoveAllEntities();

        // ADDITIVE GROW: the two remaining volume-instance producers (bodied by this TU). Args
        // are asm-attested (r4 8-byte VolumeInstanceId + a 32-bit second word each).
        void SetVolumeInstanceCullingGroup(VolumeInstanceId lVolumeInstanceId, s32 liCullingGroupId); // @0x822B1A98
        void AddVolumeInstanceForCaching(VolumeInstanceId lVolumeInstanceId, s32 leCacheOptions);     // @0x8270DA10

        // ADDITIVE GROW (FLAG -- declared-only, body owned by InSceneUpdateInterface's own
        // TU): the whole-interface merge the physics->scene world bridge drives
        // (WorldModule::BridgePhysicsSceneUpdateToScene @0x827ABA40 tail-calls it).
        void Append(const InSceneUpdateInterface& lrOther);

        // ---- Embedded fixed-capacity input queues (ascending X360-offset / DWARF order) ----
        // Capacities are the committed EventQueue_*_N explicit-instantiation TUs; element
        // types are the committed per-queue element homes. Member names are the X360 assert
        // rodata ("SceneManager.<member> too small ...").
        CgsModule::EventQueue<InEventSetEntityPosition, 1024>    mUpdatePositionQueue;       // SetEntityPosition
        CgsModule::EventQueue<InEventAddEntity, 5120>           mAddEntityQueue;            // AddEntity
        CgsModule::EventQueue<InEventAddDynamicVolume, 1280>    mAddDynamicVolumeQueue;     // AddDynamicVolume
        CgsModule::EventQueue<InEventRemoveEntity, 10000>       mRemoveEntityQueue;         // RemoveEntity
        CgsModule::EventQueue<InEventRemoveForCollision, 1536>  mRemoveForCollisionQueue;   // RemoveForCollision
        CgsModule::EventQueue<InEventRemoveVolume, 1344>        mRemoveVolumeQueue;         // RemoveVolume
        CgsModule::EventQueue<InEventRemoveVolumeInstance, 1280> mRemoveVolumeInstanceQueue; // RemoveVolumeInstance
        CgsModule::EventQueue<InEventReplaceDynamicVolume, 64>  mReplaceDynamicVolumeQueue; // ReplaceDynamicVolume
        CgsModule::EventQueue<InEventClearCullingTable, 64>     mClearCullingTableQueue;    // ClearCullingTable
        CgsModule::EventQueue<InEventSetEntityRadius, 512>      mSetEntityRadiusQueue;      // SetEntityRadius
        CgsModule::EventQueue<InEventClearEntityPadding, 16>    mClearEntityPaddingQueue;   // ClearEntityVolumesPadding

        // ---- Additive: the remaining embedded queues (grounded by Construct @0x822E6550) ----
        // Appended after the committed 11 so their offsets are unchanged. Element types +
        // capacities are exactly what Construct passes; the X360 this-relative queue base is in
        // each trailing comment. These are all reset by Clear/Destruct and (where a producer
        // lives in this TU) appended to by name.
        CgsModule::EventQueue<InEventSetVolumeInstanceTransform, 1024>     mSetVolumeInstanceTransformQueue;     // X360 +0x8010
        CgsModule::EventQueue<InEventAddForCollision, 1536>               mAddForCollisionQueue;                // X360 +0x71040
        CgsModule::EventQueue<InEventAddVolumeInstance, 1280>             mAddVolumeInstanceQueue;              // X360 +0x7D050
        CgsModule::EventQueue<InEventForceNoPadding, 64>                  mForceNoPaddingQueue;                 // X360 +0x96060
        CgsModule::EventQueue<InEventSetCullingGroupPair, 64>             mSetCullingGroupPairQueue;            // X360 +0xB4140
        CgsModule::EventQueue<InEventSetPadding, 1280>                    mSetPaddingQueue;                     // X360 +0xB54B0
        CgsModule::EventQueue<InEventAddVolumeInstanceForCaching, 64>     mAddVolumeInstanceForCachingQueue;    // X360 +0xBF4C0
        CgsModule::EventQueue<InEventSetVolumeInstanceCullingGroup, 1280> mSetVolumeInstanceCullingGroupQueue;  // X360 +0xBF920
        CgsModule::EventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache, 298>          mAddToCacheQueue;          // X360 +0xC4930
        CgsModule::EventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition, 298> mUpdateCachedPositionQueue; // X360 +0xC5290
        CgsModule::EventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache, 298>     mRemoveFromCacheQueue;     // X360 +0xC77E0
        CgsModule::EventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList, 20>  mAddPolySoupListQueue;     // X360 +0xC7C94
        CgsModule::EventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventClearPolySoupLists, 20> mClearPolySoupListsQueue; // X360 +0xC7DE0
        CgsModule::EventQueue<InEventRemoveAllEntities, 1>                mRemoveAllEntitiesQueue;              // X360 +0xC7E3C
    };
}
}
