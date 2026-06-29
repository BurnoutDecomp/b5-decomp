#pragma once

// Scene-manager IO event payloads (the subset the boot-path event queues embed).
// Reconstructed from the DecFIGS DWARF. Events derive from an empty per-module Event
// base (CgsModule event-queue convention).
#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Vector3, EntityId, Matrix44Affine
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // CgsSceneManager::VolumeInstanceId
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"     // CgsSceneManager::SceneQueryId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct Event {};

    // ========================================================================
    // CgsSceneManager::SceneManagerIO::InSceneUpdateInterface -- the write-side handle a
    // module's post-physics/pre-scene output buffer hands out so loaded entities can have
    // their positions / per-volume transforms pushed into the scene graph for the frame.
    //
    // MINIMAL SLICE (declared-only): the prop entity module (PropZoneManager::UpdateInstance
    // @0x822F0920, RemoveAllPropsAndParts) calls only the three updaters below; their bodies
    // live in the scene-manager's own TU. The full interface (the queue it appends to, the
    // remove/add-entity calls) is reconstructed by that TU; this header grows it ADDITIVELY.
    // It is used pointer-only here, so no member layout is asserted.
    //   SetEntityPosition            @ 0x822B1398 -- (EntityId, const Matrix44Affine&)
    //   SetVolumeInstanceTransform   @ 0x822CB7E8 -- (VolumeInstanceId, const Matrix44Affine&)
    //   RemoveAllEntities            @ 0x...      -- clears every entity this frame
    struct InSceneUpdateInterface
    {
        void SetEntityPosition(u32 luEntityId, const Matrix44Affine& lTransform);
        void SetVolumeInstanceTransform(VolumeInstanceId lVolumeInstanceId, const Matrix44Affine& lTransform);
        void RemoveAllEntities();
    };

    // Output event: the nearest hit of a line test.
    struct alignas(16) OutEventLineTestNearestResult : public Event
    {
        Vector3          mPosition;
        Vector3          mNormal;
        VolumeInstanceId mVolumeInstanceId;
        SceneQueryId     mQueryId;
        EntityId         mEntityId;
        f32              mfLineParam;
        u16              mu16MaterialTag;
        u16              mu16GroupTag;
        bool             mbIntersection;
    };

    // MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
    // reconstructed by SceneFineLineTestQueue's own TU (DWARF home
    // CgsSceneManagerModuleIO.h). Size 16400 (DWARF-derived: see below).
    //
    // In BrnRaceCarEntityModuleIO.h, OutputBuffer_PostScene declares
    //   typedef InputBuffer_Query::InFineLineTestQueue SceneFineLineTestQueue;  // :78
    // and embeds it BY VALUE (mSceneFineLineTestQueue, :388). InFineLineTestQueue is
    //   typedef EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestFine,256> ...
    //   (CgsSceneManagerModuleIO.h:261). EventQueue<T,256> : BaseEventQueue<T> adds
    //   T maEvents[256]; BaseEventQueue<T> = { T* mpEvents; s32 miMaxLength; s32 miLength; }
    //   (12 bytes, padded to 16 for the 16-byte-aligned element). InEventLineTestFine
    //   (CgsSceneManagerIO_FineQuery.h:50) = Vector3 mLineStart(16) + Vector3 mLineEnd(16)
    //   + SceneQueryId(4) + EntityTypeFlags u32(4) + EntityId(4) + EExclusionMode enum(4)
    //   + VolumeTypeFlags u8(1) -> 49 bytes, alignas(16) (carries Vector3) -> 64 bytes.
    //   So sizeof == 16 + 256*64 = 16400 bytes. Carries Vector3, so alignas(16).
    // Per the stub rules the real EventQueue/BaseEventQueue generic + the
    // InEventLineTestFine element are intentionally NOT pulled in; a complete sized
    // blob unlocks the buffer (the IO header only takes &member). Full layout belongs
    // to this type's own ledger TU.
    struct alignas(16) SceneFineLineTestQueue
    {
        unsigned char maReserved[16400];
    };
}
}
