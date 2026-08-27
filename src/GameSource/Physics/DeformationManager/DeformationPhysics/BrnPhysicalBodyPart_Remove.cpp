#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"   // InSceneUpdateInterface (the four Remove* + mRemoveFromCacheQueue)
#include "vendor/renderware/collision/CollisionVolume.hpp"   // rw::collision::BoxVolume (AddToScene's volume)
#include "rw/rwcore_structs.h"                               // rw::Resource (BoxVolume::Initialize's first argument)
#include "rw/physics/rigidbody.h"                            // rw::physics::ACTIVE_BODY (AddForCollision's body state)

// BrnPhysics::Deformation::PhysicalBodyPart -- the scene MEMBERSHIP slice.
// ⭐ TU CREATED 2026-08-14 (deformation-mount wave), the established slice pattern
// (BrnPhysicalBodyPart_Construct.cpp is the sibling precedent): the home TU
// (BrnPhysicalBodyPart.cpp) still carries its own open closure, and the deformation-manager
// mount needs exactly this one body (ResetDeformation -> RemovePhysicalPartsAndJoints ->
// PhysicalBodyPartPool::RemovePart -> HERE). Fold back into the home TU when it mounts.
//
// RemoveFromScene @ 0x825E7818 (43 instr), reconstructed call-for-call.
// AddToScene     @ 0x8260A938 (157 instr), reconstructed call-for-call (2026-08-27, this wave).
// ⛔ THE "DEAD AT RUNTIME" BANNER THAT STOOD HERE IS RETIRED. Parts detach on any crash since
// 2026-08-27, so both halves are on the live path.

namespace BrnPhysics
{
namespace Deformation
{
    // =============================================================================================
    // maBoxInitialiseBuffer -- DWARF BrnPhysicalBodyPart.h:297 (`extern char[96]`), X360 0x82FB7C30.
    // MOVED HERE 2026-08-27 from BrnPhysicalBodyPart.cpp, where it had sat as a DEAD zero-filled
    // static since the deformation wave. It is not scratch and it is not decorative: it is the
    // block AddToScene's `rw::Resource` points word 0 at, i.e. the storage the console
    // placement-news one `rw::collision::BoxVolume` into. Its only user is AddToScene, so it lives
    // in AddToScene's TU.
    //
    // ⭐⭐ THE SIZE IS 96 AND IT IS MEASURED, NOT ASSUMED (the brief that opened this wave believed
    // the constructed object was 128 bytes, which would have made every initialise a 32-byte WRITE
    // overrun of a file-scope static. It is not; do not widen it). Three independent readings:
    //   1. `static_assert(sizeof(BoxVolume) == 96)` -- CollisionVolume.hpp:466, itself pinned by
    //      the +0x40/+0x44/+0x50/+0x5C store offsets of BoxVolume::BoxVolume @0x82BAA0F0 and by
    //      PropManager::GetPropInertia's i*96 serialised stride.
    //   2. The X360 lays THREE of these volume-initialise statics end to end with a 96-byte
    //      stride, each referenced by exactly one function (grepped over the whole export set):
    //          0x82FB7C30  <- PhysicalBodyPart::AddToScene  @0x8260A938   (this one, box)
    //          0x82FB7C90  <- PhysicalWheel::AddToScene     @0x8260C540   (cylinder)
    //          0x82FB7CF0  <- VehicleManager::PredictCarCarIntersection   (box)
    //      0x82FB7C90 - 0x82FB7C30 == 0x60 == 96 exactly. A 128-byte object would overlap its
    //      neighbour on the console.
    //   3. The DWARF itself says char[96], and the wheel's twin (BrnPhysicalWheel.h:202) says 96.
    //
    // ⛔ WHERE THE 128 IS REAL, AND WHY IT IS A READ AND NOT A WRITE. The CONSUMER over-reads:
    // InSceneUpdateInterface::AddDynamicVolume @0x822B1518 block-copies a fixed 128-byte volume
    // image OUT of the pointer it is handed (`li r5, 0x80`; InEventAddDynamicVolume::maVolumeData
    // is 128 bytes). So the console reads 32 bytes past this buffer, into its 0x82FB7C90
    // neighbour, and always has. That is the game's own behaviour, benign there because the two
    // statics are adjacent. On the host two separate statics are NOT guaranteed adjacent, so the
    // neighbour is modelled EXPLICITLY below rather than left to chance -- reproducing the
    // console's memory neighbourhood instead of committing an out-of-bounds read.
    // (Same fact, same wording, as BrnActiveRaceCar_wQ5_01.cpp's `u8 laVolumeStorage[128]`.)
    //
    // ⚠️ ALIGNMENT, STATED EXPLICITLY BECAUSE NO GATE IN THIS PROJECT CAN SEE IT: the array is
    // `alignas(16)` to match the console's 16-aligned .data placement. sizeof is UNCHANGED (96 is
    // already a multiple of 16) and the declared array is UNCHANGED at 96 bytes, so nothing about
    // any layout, stride or offset moves. Had it been widened to 128 the alignment consequence
    // would have been real; it was not widened.
    // =============================================================================================
    namespace
    {
        struct BoxInitialiseStorage
        {
            alignas(16) char maBoxInitialiseBuffer[96];  // DWARF BrnPhysicalBodyPart.h:297 -- the BoxVolume
            char maConsoleNeighbourTail[32];             // NOT a source member: the 32 bytes the
                                                         // consumer's 128-byte copy runs into (see above)
        };
        BoxInitialiseStorage gBoxInitialiseStorage = {};

        // ---- AddToScene's four literal inputs (all X360-attested) --------------------------------
        // `li r6, 4`  @0x8260A99C -- InEventAddDynamicVolume::VolumeTypeFlags for a dynamic box.
        const u8  KU8_PART_VOLUME_TYPE_FLAG = 4u;
        // `li r5, 4`  @0x8260AA08 -- the scene entity-type flag word (the car's own is 0x484).
        const u32 KU_PART_SCENE_ENTITY_TYPE_FLAG = 4u;
        // `li r5/r8, 9` @0x8260AA84 / @0x8260AB50 -- the culling group both the AddForCollision
        // record and SetVolumeInstanceCullingGroup stamp on a deformable part's volume instance.
        const s32 KI_CULLING_GROUP_DEFORMABLE_PART = 9;
        // `lfs f1, flt_82098EF4` @0x8260AA14 -- the entity bounding radius handed to AddEntity.
        // FLAG (name only): the rodata symbol is unnamed, so the NAME below is role-derived from
        // the call. The VALUE is IDA's own read of the image word at 0x82098EF4 and the address is
        // referenced by exactly one function in the whole export set (this one).
        const f32 KF_PART_SCENE_ENTITY_RADIUS = 1.6636f;
        // `lfs f0, flt_82001C98` @0x8260AB8C -- 1.0f, the byte-verified reciprocal numerator
        // rw/physics/inertia.h:63 already carries. Added to GetSphereRadius() for the cache sphere.
        const f32 KF_PART_CACHE_SPHERE_PADDING = 1.0f;
        // `addi r11, r11, 0x49` -- the triangle-cache slot base. RemoveFromScene drops the SAME
        // slot; the two must never drift, so the one constant serves both.
        const u32 KU_PART_TRIANGLE_CACHE_SLOT_BASE = 0x49u;   // 73

        // The VOLUME key both AddToScene (@0x8260A9C4..0x8260A9EC) and RemoveFromScene
        // (@0x825E7844..0x825E7864) build from the part handle's entity word, instruction for
        // instruction the same repack in both. Factored out so the add and the remove cannot post
        // two different volume keys -- a divergence no gate could see, because both would link.
        //   ((entityWord >> 10) & 0xFF) << 8   -- entity index (low 8 of the 14-bit field) @ 8..15
        // |  (entityWord >> 24) << 16          -- owner byte moved to bits 16..23
        // |  u16(s8(entityWord & 0xFF))        -- low byte sign-extended into the low 16
        u32 PackVolumeWord(u32 luEntityWord)
        {
            return (((luEntityWord >> 10) & 0xFFu) << 8)
                 | ((luEntityWord >> 24) << 16)
                 | (static_cast<u32>(static_cast<u16>(static_cast<s16>(static_cast<s8>(luEntityWord & 0xFFu)))));
        }
    }

    // =============================================================================================
    // RemoveFromScene @ 0x825E7818 -- tear down the part's scene presence: the entity, its
    // collision registration, its volume instance, its volume, and its triangle-cache slot; then
    // clear mbAddedToScene.
    //
    // The console `ld`s the packed 8-byte part handle (mRigidBodyId, part+0x1D0) once and derives
    // every id from it:
    //   * entity word  = the HIGH dword (muEntityWord)                       -> RemoveEntity(id, 0)
    //   * the WHOLE 64-bit handle (r29, untouched)                           -> RemoveForCollision
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
        const CgsSceneManager::VolumeInstanceId lVolumeInstanceId = GetContactVolumeInstanceId();
        const u64 luHandle     = lVolumeInstanceId.muId;              // ld part+0x1D0 (the packed id)
        const u32 luEntityWord = static_cast<u32>(luHandle >> 32);    // muEntityWord

        // The repacked volume word (bit-exact transcription of 0x825E7844..0x825E7864); the shared
        // helper is the same repack AddToScene posts, so the two can never disagree.
        const CgsSceneManager::VolumeId lVolumeId(static_cast<u64>(PackVolumeWord(luEntityWord)));

        // ⛔⛔ WIDTH DEFECT FIXED 2026-08-27 (detached-part collision wave). The two middle posts
        // used to pass `EntityId(luHandle & 0xFFFFFFFF)` -- the handle's LOW dword -- through the
        // 32-bit overloads, on the strength of CgsSceneManagerIO_SceneUpdate.h:199-207's own note
        // that those fitted 32-bit forms "were fitted to their one caller (PhysicalBodyPart::
        // RemoveFromScene, whose ids really are 32-bit words)". THE ASM SAYS OTHERWISE. At
        // 0x825E786C and 0x825E7878 the console does `mr r4, r29` -- r29 is the untouched result of
        // `ld r29, 0x1D0(r30)`, i.e. the WHOLE 8-byte handle -- exactly as the wheel twin
        // RemoveFromScene @0x825E8258 does at 0x825E8278/0x825E82A8, and exactly as the DWARF
        // spells both producers (VolumeInstanceId, :393 / :423). Only RemoveEntity (r4 = the
        // `srdi`-extracted high dword) and RemoveVolume (r4 = the repack) take 32-bit words.
        // ⇒ THE CONSEQUENCE, and why it had to be fixed in the same commit as AddToScene: the
        // remove posted the low dword while AddToScene @0x8260A938 registers under the FULL handle.
        // Add and remove would have keyed on different values, so a part could never be taken out
        // of the collision set it had just been put into -- and no gate could see it, because both
        // spellings compile, link, and post a well-formed event. [[serialized-slots-stay-32-bit]].
        lpSceneInput->RemoveEntity(CgsSceneManager::EntityId(luEntityWord), 0u);  // srdi r11 (high dword)
        lpSceneInput->RemoveForCollision(lVolumeInstanceId);                      // mr r4, r29 (all 64)
        lpSceneInput->RemoveVolumeInstance(lVolumeInstanceId);                    // mr r4, r29 (all 64)
        lpSceneInput->RemoveVolume(lVolumeId);                                    // mr r4, r28 (repack)

        // Triangle-cache eviction: slot = (handle low byte) + 73, posted on the scene interface's
        // own remove-from-cache queue (the AddEvent instantiation TU is mounted).
        CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveEvent;
        lRemoveEvent.miCacheSlot =
            static_cast<s32>((luHandle & 0xFFull) + KU_PART_TRIANGLE_CACHE_SLOT_BASE);
        lpSceneInput->mRemoveFromCacheQueue.AddEvent(lRemoveEvent);

        mbAddedToScene = false;   // stb 0 -> part+0x1E5
    }

    // =============================================================================================
    // AddToScene @ 0x8260A938  (157 instructions) -- THE DETACHED PART'S SCENE REGISTRATION.
    // ⭐ LANDED 2026-08-27 (detached-part collision wave). The log-once gate that stood here since
    // 2026-08-14 is GONE. Its stated blocker -- "`rw::collision::BoxVolume` DOES NOT EXIST ANYWHERE
    // IN THIS TREE: no header, no body" -- went stale in the helpful direction on 2026-08-18, when
    // wave Q5 replaced CollisionVolume.hpp's 128-byte placeholder with the real DWARF/asm record
    // and homed BoxVolume::Initialize @0x82BAA188 in BoxVolume.cpp. Both are mounted
    // (build_game_exe.bat: vendor/renderware/collision/BoxVolume.cpp). Nothing else was blocking.
    //
    // It is NOT the mirror of RemoveFromScene: it BUILDS the collision volume first, then makes
    // six posts. Transcribed call-for-call from the raw assembly:
    //
    //   0x8260A94C..0x8260A980  stage a 5-word rw::Resource on the stack, zero every word, put
    //                           maBoxInitialiseBuffer in word 0, and
    //                           BoxVolume::Initialize(resource, v1 = mBoundingBoxHalfDimensions).
    //                           (v1 is `lvx128 v1, r31, 0x1A0` -- part+416, the half extents.)
    //   0x8260A984..0x8260A9BC  copy the four mBBoxOrientation rows (part+0x120/0x130/0x140/0x150)
    //                           into the returned volume's transform rows +0x00/+0x10/+0x20/+0x30.
    //   0x8260A9F4  AddDynamicVolume(volumeWord, volume, 4)
    //   0x8260AA1C  AddEntity(entityWord, 4, v1 = part+0x30 (the body's world position), 1.6636)
    //   0x8260AA60  AddVolumeInstance(handle64, volumeWord, the body's four transform rows)
    //   0x8260AB4C  AddForCollision(handle64, group 9, ACTIVE_BODY, zero padding, cache-opt 2)
    //               -- INLINED in the console (the "mAddForCollisionQueue too small" tripwire and
    //               its baked CgsSceneManagerIO_SceneUpdate.h:1070 are AddForCollision's own);
    //               spelled through the real producer here, which carries that same tripwire.
    //   0x8260AB5C  SetVolumeInstanceCullingGroup(handle64, 9)
    //   0x8260AB78..0x8260AB98  mAddToCacheQueue.AddEvent{ slot = (handle & 0xFF) + 0x49,
    //               radius = GetSphereRadius() + 1.0 } -- the SAME slot RemoveFromScene drops.
    //               MEASURED: this post emits NO "queue too small" tripwire (no length compare
    //               anywhere between the GetSphereRadius call and the AddEvent), unlike the five
    //               posts above it. Transcribed as-is rather than made uniform.
    //   0x8260AB9C  mbAddedToScene = 1
    //
    // ⛔ THE OLD BANNER'S SECOND CLAIM WAS RIGHT AND IS NOW ALSO DISCHARGED: landing this alone
    // buys nothing, because DetachedPartManager::UpdateTriangleCache had no real emitter. That
    // emitter (`EmitUpdateTriangleCacheEvent`) was a FABRICATED free function and is retired in
    // the same commit -- see BrnDetachedWheelManager.cpp.
    //
    // PC-SAFETY GUARD (not console behaviour, stated so): the console has no null test on the
    // scene interface here. The early return mirrors the one BrnActiveRaceCar_wQ5_01.cpp's
    // AddToScene already carries, same convention.
    // =============================================================================================
    void PhysicalBodyPart::AddToScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput)
    {
        if ( lpSceneInput == 0 )
        {
            return;
        }

        // ---- 1. build the oriented box ----------------------------------------------------------
        // The console stages the resource record on the stack, zeroes all five words, and stores
        // the static buffer into word 0; Initialize reads only word 0 (`lwz r3, 0(r3)`), so the
        // X360's five-entry BaseResources vs the host's four is inert here (the same <4>-vs-<5>
        // drift CollisionVolume.hpp:75-81 records).
        rw::Resource lVolumeResource = {};
        lVolumeResource.m_baseResources[0] = gBoxInitialiseStorage.maBoxInitialiseBuffer;

        // The ctor spills v1 and re-reads it as three scalar floats, so the 3-float overload is the
        // faithful spelling (the same one BrnActiveRaceCar_wQ5_01.cpp's AddToScene uses).
        rw::collision::BoxVolume* lpVolume = rw::collision::BoxVolume::Initialize(
            lVolumeResource,
            mBoundingBoxHalfDimensions.x, mBoundingBoxHalfDimensions.y, mBoundingBoxHalfDimensions.z);
        if ( lpVolume == 0 )
        {
            return;   // `lwz r3,0(r3); beq -> li r3,0` -- Initialize's own empty-slot arm
        }

        // The four `lvx128 part+0x120.. / stvx128 volume+0x00..` row copies: the box is expressed
        // in the part's own oriented-bbox basis, replacing the identity frame the ctor stamped.
        const Vector3* lapOrientationRows[4] = {
            &mBBoxOrientation.xAxis, &mBBoxOrientation.yAxis,
            &mBBoxOrientation.zAxis, &mBBoxOrientation.wAxis
        };
        for ( s32 liRow = 0; liRow < 4; ++liRow )
        {
            lpVolume->maTransform[liRow].x = lapOrientationRows[liRow]->x;
            lpVolume->maTransform[liRow].y = lapOrientationRows[liRow]->y;
            lpVolume->maTransform[liRow].z = lapOrientationRows[liRow]->z;
            lpVolume->maTransform[liRow].w = lapOrientationRows[liRow]->w;
        }

        // ---- 2. the five scene posts -------------------------------------------------------------
        // Every id derives from the ONE `ld 0x1D0(part)` the console performs (three times, but it
        // is the same packed handle each time).
        const CgsSceneManager::VolumeInstanceId lVolumeInstanceId = GetContactVolumeInstanceId();
        const u64 luHandle     = lVolumeInstanceId.muId;
        const u32 luEntityWord = static_cast<u32>(luHandle >> 32);
        const CgsSceneManager::VolumeId lVolumeId(static_cast<u64>(PackVolumeWord(luEntityWord)));

        // The console's r4 here is the 32-bit repacked volume word left in a 64-bit register whose
        // upper dword the repack has already cleared, so the DWARF's 64-bit VolumeId form is the
        // faithful spelling and posts exactly those bits.
        lpSceneInput->AddDynamicVolume(lVolumeId, lpVolume, KU8_PART_VOLUME_TYPE_FLAG);

        // The bounding-sphere CENTRE is the body's world translation row (part+0x30), i.e. the
        // rigid-body transform's wAxis -- NOT the bbox orientation's.
        const Matrix44Affine lBodyTransform = GetRigidBodyTransform();
        lpSceneInput->AddEntity(CgsSceneManager::EntityId(luEntityWord),
                                KU_PART_SCENE_ENTITY_TYPE_FLAG,
                                lBodyTransform.wAxis,
                                KF_PART_SCENE_ENTITY_RADIUS);

        // The volume instance carries the body's four transform rows (part+0x00/0x10/0x20/0x30).
        lpSceneInput->AddVolumeInstance(lVolumeInstanceId, lVolumeId, lBodyTransform);

        lpSceneInput->AddForCollision(
            lVolumeInstanceId,
            static_cast<CgsSceneManager::SceneManagerIO::InEventAddForCollision::CullingGroup>(
                KI_CULLING_GROUP_DEFORMABLE_PART),                           // li r8, 9
            rw::physics::ACTIVE_BODY,                                        // li r8, 4
            Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },                               // the zeroed vmx lane
            CgsSceneManager::SceneManagerIO::E_DO_NOT_ADD_TO_CACHE_MANAGER); // li r8, 2

        lpSceneInput->SetVolumeInstanceCullingGroup(lVolumeInstanceId,
                                                    KI_CULLING_GROUP_DEFORMABLE_PART);

        // ---- 3. claim the triangle-cache slot ----------------------------------------------------
        // The part tells the cache manager to start fetching world triangles around it. Without
        // this the broad phase has a volume instance with no cached geometry to test against, which
        // is precisely why a shed panel used to pass through the road.
        CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache lAddEvent;
        lAddEvent.miCacheSlot =
            static_cast<s32>((luHandle & 0xFFull) + KU_PART_TRIANGLE_CACHE_SLOT_BASE);
        lAddEvent.mfCacheSphereRadius = GetSphereRadius() + KF_PART_CACHE_SPHERE_PADDING;
        lpSceneInput->mAddToCacheQueue.AddEvent(lAddEvent);

        mbAddedToScene = true;   // stb 1 -> part+0x1E5
    }

}
}
