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
// The five remaining ledger functions of this class are BLOCKED, not emitted here:
//   * Clear / Destruct              -- reset the live count of EVERY embedded queue (25 of them);
//                                      ~14 sibling queues have un-recovered element types (their
//                                      AddEvent callees are still todo, no committed element home),
//                                      so their counters cannot be named/zeroed without fabrication.
//   * SetVolumeInstanceTransform    -- targets mSetVolumeInstanceTransformQueue (X360 +0x8010);
//   * AddVolumeInstanceForCaching   -- targets the +0xBF480 caching queue;
//   * SetVolumeInstanceCullingGroup -- targets the +0xBF920 culling-group queue.
//   The last three queues' element type + capacity are un-recovered in this dossier (AddEvent
//   callees todo, no committed element home / EventQueue_*_N instantiation), so the event record
//   cannot be staged faithfully. Left as honest gaps.
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
}
}
