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
// GAP (honest): several sibling queues that this interface also embeds (e.g. the
// set-volume-instance-transform / add-volume-instance-for-caching / set-culling-group
// queues, and the query/collision-add/padding families owned by other interface methods)
// are NOT modeled here -- their element type/capacity are un-recovered in this TU's dossier
// (their AddEvent callees are still todo, with no committed element home). They are left out
// rather than guessed; the whole-interface Clear/Destruct (which reset EVERY embedded queue's
// live count) and the three producers that target those un-recovered queues are therefore not
// emitted by this TU (blocked on those siblings' element types).

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

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct alignas(16) InSceneUpdateInterface
    {
        // ADDITIVE GROW (FLAG -- body owned by InSceneUpdateInterface's own TU): the
        // X360 trigger/prop output-buffer Construct bodies (e.g.
        // BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene::Construct @0x822EED90)
        // tail-call InSceneUpdateInterface::Construct on this aggregate. Declared-only here
        // so consuming TUs compile; the owning TU emits the body. No layout change.
        void Construct();

        // Producer methods (X360 0x822B10C8..0x822CB8E4). Each stages one event on the stack,
        // fires the queue's "too small" tripwire assert when the target queue is full, then
        // appends the event to the matching embedded queue. Signatures for the pre-existing
        // declarations are LOAD-BEARING (committed consumers call them) and left unchanged.
        void AddDynamicVolume(CgsSceneManager::EntityId lEntityId, const void* lpVolumeImage, u8 lu8VolumeTypeFlag);
        void AddEntity(CgsSceneManager::EntityId lEntityId, u32 luEntityTypeFlag, f32 lfBoundingRadius);
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

        // Merged prop-entity write-side updaters (bodies owned by this TU where grounded;
        // SetVolumeInstanceTransform(VolumeInstanceId) targets an un-recovered queue and stays
        // declared-only). Pointer-/value-used by committed consumers; signatures unchanged.
        void SetEntityPosition(u32 luEntityId, const Matrix44Affine& lTransform);
        void SetVolumeInstanceTransform(VolumeInstanceId lVolumeInstanceId, const Matrix44Affine& lTransform);
        void RemoveAllEntities();

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
    };
}
}
