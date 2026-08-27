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

    // =============================================================================================
    // AddToScene -- ⚠️ LOG-ONCE GATE 2026-08-14 (walls leg 4). The INVERSE of RemoveFromScene
    // above (the four scene adds + the cache add) is NOT reconstructed yet; the only caller is
    // the pool's AddPartsToScene walk.
    // ⛔ "which is empty until a part detaches" -- PARTS DETACH NOW (2026-08-27): that walk has
    // live slots on any crash, so this gate is on the LIVE path and is what keeps a shed panel
    // out of the scene (it renders, but it has no scene presence and no collision).
    // ⇒ PROMOTED IN PRIORITY -- AND SCOPED 2026-08-27 (detach-2 wave), so the next attempt does
    // not start by re-measuring it. It is @0x8260A938, 157 instructions, and it is NOT the mirror of
    // RemoveFromScene: it builds a collision VOLUME first and then makes five separate calls.
    //   0x8260A97C  a 20-byte record seeded with a descriptor pointer (unk_82FB7C30) and
    //               `bl rw::collision::BoxVolume::Initialize` with v1 == mBoundingBoxHalfDimensions
    //   0x8260A984..0x8260A9BC  copy the four mBBoxOrientation rows (this+0x120..0x150) into the
    //               volume the call returned
    //   0x8260A9F4  InSceneUpdateInterface::AddDynamicVolume(id32, volume, 4)
    //   0x8260AA1C  InSceneUpdateInterface::AddEntity(mRigidBodyId's entity word, v1 = this+0x30,
    //               f1 = flt_82098EF4, 4)
    //   0x8260AA60  InSceneUpdateInterface::AddVolumeInstance(id64, id32, the four rows)
    //   0x8260AB4C  InEventAddForCollision::AddEvent onto sceneInput + 0x71040
    //   0x8260AB5C  SetVolumeInstanceCullingGroup(id64, 9)
    //   0x8260AB78  GetSphereRadius() + 1.0 -> InEventAddToCache::AddEvent (cache slot
    //               (handle & 0xFF) + 0x49 -- the SAME slot RemoveFromScene drops, above)
    //   0x8260AB9C  mbAddedToScene = 1
    // ⛔ THE REAL BLOCKER IS NOT ANY OF THOSE CALLS -- IT IS A TYPE. `rw::collision::BoxVolume`
    // DOES NOT EXIST ANYWHERE IN THIS TREE: no header, no body, and no other producer of
    // InEventAddDynamicVolume in the whole game side (only the queue's own AddEvent/Append
    // instantiations). Its 20-byte seed + Initialize + the volume record all have to land first.
    // ⛔⛔ AND LANDING IT ALONE BUYS NOTHING OBSERVABLE. `EmitUpdateTriangleCacheEvent`
    // (BrnDetachedWheelManager.cpp:315) is ALSO a log-once gate, and it is the only emitter
    // DetachedPartManager::UpdateTriangleCache has -- so a scene-added part would still publish no
    // cached position, and the world triangles around it would never be fetched. Collision for a
    // shed panel needs BOTH, plus whatever the contact bridge then wants. Do not half-land this and
    // report "collision"; the panel would look identical and fall through the road exactly as it
    // does today (measured: slot 0 reaches y = -752 and is still going).
    // =============================================================================================
    void PhysicalBodyPart::AddToScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* /*lpSceneInput*/)
    {
        static bool sbLoggedAddToSceneGate = false;
        if ( !sbLoggedAddToSceneGate )
        {
            sbLoggedAddToSceneGate = true;
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: PhysicalBodyPart::AddToScene reached but not "
                       "reconstructed -- detached part NOT added to scene [FLAG PC boot gate]\n";
        }
    }

}
}
