#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"   // InSceneUpdateInterface (the four Remove* + mRemoveFromCacheQueue)

// BrnPhysics::Deformation::PhysicalBodyPart -- the scene-removal slice.
// ⭐ TU CREATED 2026-08-14 (deformation-mount wave), the established slice pattern
// (BrnPhysicalBodyPart_Construct.cpp is the sibling precedent): the home TU
// (BrnPhysicalBodyPart.cpp) still carries its own open closure, and the deformation-manager
// mount needs exactly this one body (ResetDeformation -> RemovePhysicalPartsAndJoints ->
// PhysicalBodyPartPool::RemovePart -> HERE). Fold back into the home TU when it mounts.
//
// RemoveFromScene @ 0x825E7818 (43 instr), reconstructed call-for-call. NOTE: this whole path is
// DEAD AT RUNTIME this wave (no part ever becomes physical until contact generation lands);
// it exists for LINK closure and for the day parts detach.

namespace BrnPhysics
{
namespace Deformation
{
    // =============================================================================================
    // RemoveFromScene @ 0x825E7818 -- tear down the part's scene presence: the entity, its
    // collision registration, its volume instance, its volume, and its triangle-cache slot; then
    // clear mbAddedToScene.
    //
    // The console `ld`s the packed 8-byte part handle (mRigidBodyId, part+0x1D0) once and derives
    // every id from it:
    //   * entity word  = the HIGH dword (muEntityWord)                       -> RemoveEntity(id, 0)
    //   * instance word = the LOW dword ((muSubA << 16) | muSubB)            -> RemoveForCollision
    //                                                                        -> RemoveVolumeInstance
    //   * volume word  = a repack of the entity word (0x825E7844..0x825E7864):
    //         ((entityWord >> 10) & 0xFF) << 8   -- entity index (low 8 of the 14-bit field) at bits 8..15
    //       |  (entityWord >> 24) << 16          -- owner byte moved to bits 16..23
    //       |  u16(s8(entityWord & 0xFF))        -- low byte sign-extended into the low 16
    //                                                                        -> RemoveVolume
    //   * cache slot   = (handle & 0xFF) + 0x49 (73)                         -> mRemoveFromCacheQueue
    //     (`clrlwi r11,r11,24` on the loaded handle == its LOW byte, i.e. muSubB's low byte; the
    //      +73 rebases the part's sub-id into the triangle-cache slot range -- the queue member at
    //      scene+0xC77E0 is the named mRemoveFromCacheQueue, `addis r3,r31,0xC ; addi r3,r3,0x77E0`).
    // =============================================================================================
    void PhysicalBodyPart::RemoveFromScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput)
    {
        const u64 luHandle     = GetContactVolumeInstanceId().muId;   // ld part+0x1D0 (the packed id)
        const u32 luEntityWord = static_cast<u32>(luHandle >> 32);    // muEntityWord
        const u32 luInstanceWord = static_cast<u32>(luHandle & 0xFFFFFFFFull);   // (muSubA<<16)|muSubB

        // The repacked volume word (bit-exact transcription of 0x825E7844..0x825E7864).
        const u32 luVolumeWord =
              (((luEntityWord >> 10) & 0xFFu) << 8)
            | ((luEntityWord >> 24) << 16)
            | (static_cast<u32>(static_cast<u16>(static_cast<s16>(static_cast<s8>(luEntityWord & 0xFFu)))));

        lpSceneInput->RemoveEntity(CgsSceneManager::EntityId(luEntityWord), 0u);
        lpSceneInput->RemoveForCollision(CgsSceneManager::EntityId(luInstanceWord));
        lpSceneInput->RemoveVolumeInstance(CgsSceneManager::EntityId(luInstanceWord));
        lpSceneInput->RemoveVolume(CgsSceneManager::EntityId(luVolumeWord));

        // Triangle-cache eviction: slot = (handle low byte) + 73, posted on the scene interface's
        // own remove-from-cache queue (the AddEvent instantiation TU is mounted).
        CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveEvent;
        lRemoveEvent.miCacheSlot = static_cast<s32>((luHandle & 0xFFull) + 0x49ull);
        lpSceneInput->mRemoveFromCacheQueue.AddEvent(lRemoveEvent);

        mbAddedToScene = false;   // stb 0 -> part+0x1E5
    }
}
}
