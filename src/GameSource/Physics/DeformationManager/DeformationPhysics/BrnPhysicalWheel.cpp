#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint (walls leg 4 gates)

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"   // InSceneUpdateInterface::SetVolumeInstanceTransform (pulls in CgsSceneManager::EntityId)
#include "vendor/renderware/collision/CylinderVolume.hpp"   // rw::collision::CylinderVolume (AddToScene's volume)
#include "rw/rwcore_structs.h"                              // rw::Resource (the Initialize slot record)
#include "GameShared/GameClasses/Geometric/Primitives/CgsCylinder.h"   // CgsGeometric::Cylinder (GetCylinder out-param)
#include "rw/physics/rigidbody.h"                           // rw::physics::ACTIVE_BODY (AddForCollision's body state)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"    // InputBuffer::GetAddRigidBodyQueue (AddToSim)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"   // InAddRigidBody / NewRigidBody (AddToSim)
#include "rw/physics/inertia.h"                                             // rw::physics::Inertia setters (AddToSim)

// ============================================================================
// BrnPhysics::Deformation::PhysicalWheel -- per-frame body for the lean detached-wheel
// sibling of PhysicalBodyPart. Reconstructed store-for-store from the X360 ARTIST asm:
//
//   SetRigidBodyTransform @ 0x825E81C0  -- force the cached render transform from a rigid-body
//                                          transform (rotating the COM offset back out of the
//                                          translation) and republish it to the scene.
//   Update                @ 0x826156A0  -- consume one post-physics OutUpdateRigidBody event:
//                                          cache the linear velocity + frozen flag, then either
//                                          remove-from-scene (just-frozen) or push the new
//                                          rigid-body transform + add-to-scene (still live).
//
// X360 LAYOUT NOTE (offset authority): the X360 image uses 32-bit pointers, so the asm indexes
// the wheel's members by raw byte offset. They map onto the frozen-header members as:
//   +0   mRenderTransform   (Matrix44Affine, 64 bytes: xAxis@0, yAxis@16, zAxis@32, wAxis@48)
//   +64  mComOffset         (Vector3)
//   +80  mBoundingBoxDimensions (Vector3)
//   +96  mLinearVelocity    (Vector3)
//   +112 mWheelBodyId       (BurnoutWheelBodyID: muEntityWord@112 u32, muSubA@116 u16, muSubB@118 u16)
//   +120 mfHalfHeight       +124 mfRadius
//   +128 mbFrozen           +129 mbAddedToScene
//
// The OutUpdateRigidBody event passed to Update is owned by an un-homed sim-output TU (it is only
// forward-declared in the frozen header). The asm therefore touches it through raw byte offsets;
// they are reproduced here via pointer arithmetic, which is store-for-store faithful to the X360
// control flow (same branches, same strides, same field reads). The event byte offsets the asm
// uses (proven by this function's loads):
//   +32   the rigid-body translation row (-> render wAxis source)
//   +48   the new linear velocity (-> mLinearVelocity)
//   +80   the rigid-body basis xAxis row
//   +96   the rigid-body basis yAxis row
//   +112  the rigid-body basis zAxis row
//   +156  the flag byte; bit 1 (mask 0x2) == "wheel is now frozen / settled"
//
// MODELLED-vs-asm: in SetRigidBodyTransform the asm forms the COM-rotated translation with a
// scrambled-looking vmulfp128/vmaddfp triple (Hex-Rays leaves the FMA operand order noisy); the
// real operation is the standard 3-axis weighted sum xAxis*com.x + yAxis*com.y + zAxis*com.z
// (the COM offset rotated into world space by the transform's basis), subtracted from the
// translation row. It is written here in that readable form.
//
// SCENE-PUBLISH ARG: the asm calls SetVolumeInstanceTransform(*(this+112), *(this+116), this) --
// the same de-inlining the sibling PhysicalBodyPart::SetRigidBodyTransform shows as
// SetVolumeInstanceTransform(HIDWORD(v11), v11, v16). The committed interface signature
// SetVolumeInstanceTransform(CgsSceneManager::EntityId, const Matrix44Affine&) pins the meaning:
// arg1 is the entity word (the high dword, this+112 == mWheelBodyId.muEntityWord); the second
// Hex-Rays operand (*(this+116), the id's low dword) is the 64-bit-register-pair decoding
// artifact; the transform ref is &mRenderTransform (this+0). No header grow needed here -- the
// interface declaration was already added by the PhysicalBodyPart slice.
//
// Callers (X360 xref): Update <- DetachedWheelManager::UpdatePostPhysics;
//                      SetRigidBodyTransform <- PhysicalWheel::Update.
// ============================================================================

namespace BrnPhysics
{
namespace Deformation
{
    // ----- maCylinderInitialiseBuffer (DWARF BrnPhysicalWheel.h:202, `extern char[96]`) -----------
    //
    // ⭐ RE-DESCRIBED 2026-08-27 (detached-part collision wave). The banner that stood here called
    // this "the wheel's cylinder-initialisation scratch buffer ... shared by the lifecycle/geometry
    // methods (Construct / GetCylinder)". It is not scratch and neither of those touches it: it is
    // X360 0x82FB7C90, and it is the memory block PhysicalWheel::AddToScene @0x8260C540 -- its ONLY
    // referrer in the entire export set -- placement-news one rw::collision::CylinderVolume into.
    //
    // SIZE 96, MEASURED: the X360 lays three of these volume-initialise statics end to end with an
    // exact 96-byte stride (0x82FB7C30 the body-part box / 0x82FB7C90 THIS ONE / 0x82FB7CF0
    // VehicleManager's box), and the CylinderVolume record's own static_asserts put its last member
    // muFlags at +0x5C. See BrnPhysicalBodyPart_Remove.cpp's twin banner for the full three-way
    // reading and for why the 128 that appears in this chain is the CONSUMER's over-read
    // (AddDynamicVolume block-copies a fixed 128-byte volume image out of whatever pointer it is
    // handed) and not the constructed object's size. Do NOT widen the array; the neighbour those
    // extra 32 bytes land in is modelled explicitly instead.
    //
    // ⚠️ ALIGNMENT: `alignas(16)` matches the console's .data placement and does not change sizeof
    // (96 is already a multiple of 16), so no offset, stride or layout anywhere moves.
    namespace
    {
        struct CylinderInitialiseStorage
        {
            alignas(16) char maCylinderInitialiseBuffer[96];  // DWARF BrnPhysicalWheel.h:202
            char maConsoleNeighbourTail[32];                  // NOT a source member -- see above
        };
        CylinderInitialiseStorage gCylinderInitialiseStorage = {};

        // ---- AddToScene's literal inputs (X360-attested; the body-part twin's are identical
        //      except for the cache-slot base and the entity radius) --------------------------------
        const u8  KU8_WHEEL_VOLUME_TYPE_FLAG        = 4u;   // li r6, 4  @0x8260C5C0
        const u32 KU_WHEEL_SCENE_ENTITY_TYPE_FLAG   = 4u;   // li r5, 4  @0x8260C64C
        const s32 KI_CULLING_GROUP_DETACHED_WHEEL   = 9;    // li r8, 9  @0x8260C68C / li r5,9 @0x8260C75C
        const f32 KF_WHEEL_CACHE_SPHERE_PADDING     = 1.0f; // flt_82001C98, byte-verified elsewhere
        const u32 KU_WHEEL_TRIANGLE_CACHE_SLOT_BASE = 0x7Bu;// 123 -- the SAME base RemoveFromScene drops

        // Initialize's fatness argument: `lfs f31, flt_82001CC0` @0x8260C568 -> `fmr f3, f31`.
        // flt_82001CC0 is the byte-verified 0.0f this tree already carries in three other places.
        const f32 KF_WHEEL_VOLUME_FATNESS = 0.0f;

        // The two frame rows AddToScene overwrites AFTER Initialize has stamped the identity basis
        // (0x8260C5A4..0x8260C5FC). Rows 1 and 3 keep Initialize's own values (+Y basis, zero
        // centre); only rows 0 and 2 are replaced, which swings the cylinder's local frame a quarter
        // turn about Y so its axis lies along the wheel's spin axis.
        //   maFrame[0] = { 0, 0, -1, 0 }   (flt_820037C8 == -1.0f into lane z; lanes x/y/w = 0)
        //   maFrame[2] = { 1, 0,  0, 0 }   (flt_82001C98 ==  1.0f into lane x; lanes y/z/w = 0)
        const f32 KF_WHEEL_FRAME_MINUS_ONE = -1.0f;   // flt_820037C8
        const f32 KF_WHEEL_FRAME_PLUS_ONE  =  1.0f;   // flt_82001C98

        // The VOLUME key both PhysicalWheel::AddToScene (@0x8260C604..0x8260C62C) and
        // RemoveFromScene (@0x825E8274..0x825E82A0) build from the wheel handle's entity word --
        // instruction for instruction the same repack the body-part pair uses. Factored so the add
        // and the remove can never post two different volume keys.
        u32 PackWheelVolumeWord(u32 luEntityWord)
        {
            return (((luEntityWord >> 10) & 0xFFu) << 8)
                 | ((luEntityWord >> 24) << 16)
                 | (static_cast<u32>(static_cast<u16>(static_cast<s16>(static_cast<s8>(luEntityWord & 0xFFu)))));
        }
    }

    // ----- (the OutUpdateRigidBody byte-offset table that used to sit here was RETIRED 2026-09-02:
    //        Update reads the homed CgsPhysics event by name; see its banner for the console offsets
    //        and the x64 ghost the table hid.) --------------------------------------------------

    // X360 @ 0x825E81C0. Force the cached render transform from a rigid-body transform and, if the
    // wheel is currently in the scene, republish it.
    //
    // The basis rows copy straight across (rigid-body == render orientation). The translation is the
    // rigid-body translation with the wheel's centre-of-mass offset rotated back out of it:
    //     mRenderTransform.wAxis = lTransform.wAxis
    //                            - ( lTransform.xAxis*mComOffset.x
    //                              + lTransform.yAxis*mComOffset.y
    //                              + lTransform.zAxis*mComOffset.z )
    // which re-expresses the COM-centred physics origin as the mesh origin the renderer wants.
    void PhysicalWheel::SetRigidBodyTransform(Matrix44Affine lTransform,
                                              CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        // Copy the basis + translation rows across (stvx128 of xAxis/yAxis/zAxis/wAxis to this+0/16/32/48).
        mRenderTransform.xAxis = lTransform.xAxis;
        mRenderTransform.yAxis = lTransform.yAxis;
        mRenderTransform.zAxis = lTransform.zAxis;
        mRenderTransform.wAxis = lTransform.wAxis;

        // Rotate mComOffset into world space by the transform's basis (vmulfp128 + two vmaddfp): the
        // per-axis weighted sum xAxis*com.x + yAxis*com.y + zAxis*com.z.
        const Vector3 lvRotatedCom = {
            lTransform.xAxis.x * mComOffset.x + lTransform.yAxis.x * mComOffset.y + lTransform.zAxis.x * mComOffset.z,
            lTransform.xAxis.y * mComOffset.x + lTransform.yAxis.y * mComOffset.y + lTransform.zAxis.y * mComOffset.z,
            lTransform.xAxis.z * mComOffset.x + lTransform.yAxis.z * mComOffset.y + lTransform.zAxis.z * mComOffset.z,
            lTransform.xAxis.w * mComOffset.x + lTransform.yAxis.w * mComOffset.y + lTransform.zAxis.w * mComOffset.z
        };

        // mRenderTransform.wAxis = wAxis - rotatedCom (vsubfp v0 = v13 - v0; stvx128 -> this+48).
        mRenderTransform.wAxis.x = lTransform.wAxis.x - lvRotatedCom.x;
        mRenderTransform.wAxis.y = lTransform.wAxis.y - lvRotatedCom.y;
        mRenderTransform.wAxis.z = lTransform.wAxis.z - lvRotatedCom.z;
        mRenderTransform.wAxis.w = lTransform.wAxis.w - lvRotatedCom.w;

        // if ( mbAddedToScene ) push the new render transform out to the scene under this wheel's
        // scene-entity id. The asm passes the entity word (this+112 == mWheelBodyId.muEntityWord,
        // surfaced by Hex-Rays as HIDWORD of the 64-bit id) and &mRenderTransform (this+0). This
        // mirrors the sibling PhysicalBodyPart::SetRigidBodyTransform call
        // SetVolumeInstanceTransform(HIDWORD(v11), v11, v16) -- the EntityId is the high dword, the
        // second Hex-Rays operand is the 64-bit-pair decoding artifact.
        if ( mbAddedToScene )
        {
            // walls leg 4: the real bodied SetVolumeInstanceTransform keys on the packed
            // VolumeInstanceId (the wheel handle IS the volume-instance id -- see
            // GetContactVolumeInstanceId's banner); the EntityId overload never existed.
            lpSceneInterface->SetVolumeInstanceTransform(
                GetVolumeInstanceId(), mRenderTransform);
        }
    }

    // X360 @ 0x826156A0. Apply one post-physics rigid-body update event.
    //
    // Caches the event's frozen flag + linear velocity, then forks on the frozen bit:
    //   frozen  : if the wheel is in the scene, remove it (it has settled, sim is done).
    //   live    : reassemble the rigid-body transform from the event's basis/translation rows,
    //             push it through SetRigidBodyTransform, and add the wheel to the scene if it is
    //             not in it yet.
    // ⛔⛔ CORRECTED 2026-09-02 (deform close-out wave) -- an x64-WIDENING GHOST, measured. The
    // console reads the frozen bit as `lwz r11, 0x9C(event) ; extrwi r11, r11, 1, 30` @0x826156B4
    // == bit 1 (FROZEN_BODY) of the RigidBody's mState, which the CONSOLE packs into mIsplt.w at
    // RigidBody+0x8C (event+0x9C). This body used to dereference that console byte offset on the
    // PC event -- but the PC rw::physics::RigidBody PROMOTES mState to its own member after the
    // eleven registers (rigidbody.h:292), so event+0x9C on the PC is mIsplt.w == unused padding
    // == 0, and mbFrozen could never become true. MEASURED (run wheelw_r3, the first wheel ever
    // to detach on this build): the wheel rolled 12 m and settled at (3166.17, -3.96, -2015.12),
    // the sim put its body to sleep, DoDetachedWheelWorldContactGeneration (which skips on
    // IsFrozen, like the console) kept generating wheel-vs-world pairs for a sleeping body, and
    // PhysicsSimulationModule::ProcessAddContactQueue's "Rigid Body A: " both-asleep tripwire
    // (CgsPhysicsSimulationModule.cpp:1302) fired 839,983 times in one run and starved the
    // harness. Same shape as PhysicalBodyPart::Update, which already reads the state BY NAME.
    // Every field is read by name now; the event IS the CgsPhysics OutUpdateRigidBody (the
    // Deformation-namespace forward declaration on the header is the same type plumbing the
    // body-part twin carries).
    void PhysicalWheel::Update(const OutUpdateRigidBody* lpUpdateEvent,
                               CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        const rw::physics::RigidBody& lrRigidBody =
            reinterpret_cast<const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody*>(lpUpdateEvent)
                ->mRigidBody;

        // 0x826156B4  lwz 0x9C(event) ; extrwi 1,30 ; stb 0x80(this)  -- FROZEN_BODY (bit 1) of mState.
        const bool lbFrozen = (lrRigidBody.GetState() & rw::physics::FROZEN_BODY) != 0;
        mbFrozen = lbFrozen;

        // 0x826156D4  lvx128 v0, event, 0x30 ; stvx128 this+0x60  -- the body's linear velocity (mVel).
        mLinearVelocity = lrRigidBody.GetLinearVelocity();

        if ( lbFrozen )
        {
            // Just settled: drop it out of the scene if it is currently in it.
            if ( mbAddedToScene )
            {
                RemoveFromScene(lpSceneInterface);
                return;
            }
        }
        else
        {
            // 0x826156F8..0x82615724: the basis rows (event+0x50/+0x60/+0x70 == mRi/mUp/mAt) and
            // the translation (event+0x20 == mCom) staged on the stack -- RigidBody::GetTransform.
            const Matrix44Affine lRigidBodyTransform = lrRigidBody.GetTransform();

            SetRigidBodyTransform(lRigidBodyTransform, lpSceneInterface);

            // if ( !mbAddedToScene ) add the wheel's volume instance to the scene.
            if ( !mbAddedToScene )
            {
                AddToScene(lpSceneInterface);
                return;
            }
        }
    }

    // =============================================================================================
    // RemoveFromScene @ 0x825E8258 (43 instructions) -- ⭐ LANDED 2026-08-27, replacing the
    // log-once gate. The exact twin of PhysicalBodyPart::RemoveFromScene @0x825E7818, with the
    // wheel's own +0x7B cache-slot base. Every id comes from the ONE `ld r29, 0x70(r30)`:
    //   RemoveForCollision  (r4 = r29, the WHOLE 64-bit handle)
    //   RemoveVolumeInstance(r4 = r29, the WHOLE 64-bit handle)
    //   RemoveVolume        (r4 = r27, the repacked volume word)
    //   RemoveEntity        (r4 = r28, the `srdi`-extracted entity word; r5 = 0)
    //   mRemoveFromCacheQueue.AddEvent{ slot = (handle & 0xFF) + 0x7B }   (scene + 0xC77E0)
    //   mbAddedToScene = 0
    // ⚠️ CALL ORDER DIFFERS from the body part's (the wheel removes the entity LAST, the part
    // FIRST) -- transcribed as the asm has it, not made uniform.
    // =============================================================================================
    void PhysicalWheel::RemoveFromScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput)
    {
        if ( lpSceneInput == 0 )
        {
            return;   // PC-safety guard (the console has no null test here)
        }

        const CgsSceneManager::VolumeInstanceId lVolumeInstanceId = GetVolumeInstanceId();
        const u64 luHandle     = lVolumeInstanceId.muId;
        const u32 luEntityWord = static_cast<u32>(luHandle >> 32);
        const CgsSceneManager::VolumeId lVolumeId(
            static_cast<u64>(PackWheelVolumeWord(luEntityWord)));

        lpSceneInput->RemoveForCollision(lVolumeInstanceId);                       // mr r4, r29
        lpSceneInput->RemoveVolumeInstance(lVolumeInstanceId);                     // mr r4, r29
        lpSceneInput->RemoveVolume(lVolumeId);                                     // mr r4, r27
        lpSceneInput->RemoveEntity(CgsSceneManager::EntityId(luEntityWord), 0u);   // mr r4, r28 ; li r5,0

        CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveEvent;
        lRemoveEvent.miCacheSlot =
            static_cast<s32>((luHandle & 0xFFull) + KU_WHEEL_TRIANGLE_CACHE_SLOT_BASE);
        lpSceneInput->mRemoveFromCacheQueue.AddEvent(lRemoveEvent);

        mbAddedToScene = false;   // stb 0 -> wheel+0x81
    }

    // =============================================================================================
    // AddToScene @ 0x8260C540 (157 instructions) -- THE DETACHED WHEEL'S SCENE REGISTRATION.
    // ⭐ LANDED 2026-08-27 (detached-part collision wave), replacing the log-once gate. Structurally
    // identical to the body-part twin @0x8260A938 -- same six posts, same order, same culling group
    // 9, same ACTIVE_BODY / E_DO_NOT_ADD_TO_CACHE_MANAGER pair, same zero collision padding -- with
    // four wheel-specific differences, all measured:
    //
    //   * the volume is a CYLINDER, not a box:
    //       CylinderVolume::Initialize(slot, f1 = mfRadius (+0x7C), f2 = mfHalfHeight (+0x78),
    //                                  f3 = 0.0f)                                    @0x8260C59C
    //   * only frame rows 0 and 2 are overwritten afterwards, to { 0,0,-1,0 } and { 1,0,0,0 }
    //     (rows 1 and 3 keep Initialize's +Y basis and zero centre)                  @0x8260C5EC..FC
    //   * AddEntity's bounding radius is mfHalfHeight + mfRadius (`fadds f1, f0, f13`, NOT a
    //     rodata constant -- the body part uses the flt_82098EF4 literal instead)    @0x8260C650
    //   * the triangle-cache slot base is 0x7B (123), and the cache sphere radius is the wheel's
    //     RAW mfRadius + 1.0 (the body part uses GetSphereRadius())                  @0x8260C77C
    //
    // The volume instance and the scene entity are both keyed off the SAME `ld r11, 0x70(r31)`
    // wheel handle, and the AddVolumeInstance transform argument is `mr r6, r31` -- the wheel
    // pointer itself, i.e. &mRenderTransform, which is the class's first member.
    //
    // PC-SAFETY GUARD (not console behaviour, stated so): the early return on a null scene
    // interface mirrors the sibling body-part AddToScene and BrnActiveRaceCar_wQ5_01.cpp.
    // =============================================================================================
    void PhysicalWheel::AddToScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput)
    {
        if ( lpSceneInput == 0 )
        {
            return;
        }

        // ---- 1. build the cylinder ---------------------------------------------------------------
        // The console stages a 5-word rw::Resource on the stack, zeroes every word, and puts the
        // static buffer in word 0; Initialize reads only word 0 (`lwz r11, 0(r3)`), so the X360's
        // five-entry BaseResources vs the host's four is inert (the <4>-vs-<5> drift AGENTS.md
        // records). CylinderVolume::Initialize's own DWARF/asm signature takes that slot directly.
        rw::Resource lVolumeResource = {};
        lVolumeResource.m_baseResources[0] = gCylinderInitialiseStorage.maCylinderInitialiseBuffer;

        rw::collision::CylinderVolume* lpVolume = rw::collision::CylinderVolume::Initialize(
            reinterpret_cast<rw::collision::CylinderVolume**>(&lVolumeResource.m_baseResources[0]),
            mfRadius, mfHalfHeight, KF_WHEEL_VOLUME_FATNESS);
        if ( lpVolume == 0 )
        {
            return;   // Initialize's own empty-slot arm (`lwz r11,0(r3) ; beq -> return 0`)
        }

        // Rows 0 and 2 only (rows 1 and 3 keep what Initialize stamped).
        lpVolume->maFrame[0].x = 0.0f;
        lpVolume->maFrame[0].y = 0.0f;
        lpVolume->maFrame[0].z = KF_WHEEL_FRAME_MINUS_ONE;
        lpVolume->maFrame[0].w = 0.0f;

        lpVolume->maFrame[2].x = KF_WHEEL_FRAME_PLUS_ONE;
        lpVolume->maFrame[2].y = 0.0f;
        lpVolume->maFrame[2].z = 0.0f;
        lpVolume->maFrame[2].w = 0.0f;

        // ---- 2. the five scene posts -------------------------------------------------------------
        const CgsSceneManager::VolumeInstanceId lVolumeInstanceId = GetVolumeInstanceId();
        const u64 luHandle     = lVolumeInstanceId.muId;
        const u32 luEntityWord = static_cast<u32>(luHandle >> 32);
        const CgsSceneManager::VolumeId lVolumeId(
            static_cast<u64>(PackWheelVolumeWord(luEntityWord)));

        lpSceneInput->AddDynamicVolume(lVolumeId, lpVolume, KU8_WHEEL_VOLUME_TYPE_FLAG);

        lpSceneInput->AddEntity(CgsSceneManager::EntityId(luEntityWord),
                                KU_WHEEL_SCENE_ENTITY_TYPE_FLAG,
                                mRenderTransform.wAxis,          // lvx128 v1, r31, 0x30
                                mfHalfHeight + mfRadius);        // fadds f1, f0, f13

        lpSceneInput->AddVolumeInstance(lVolumeInstanceId, lVolumeId, mRenderTransform);  // mr r6, r31

        lpSceneInput->AddForCollision(
            lVolumeInstanceId,
            static_cast<CgsSceneManager::SceneManagerIO::InEventAddForCollision::CullingGroup>(
                KI_CULLING_GROUP_DETACHED_WHEEL),                                // li r8, 9
            rw::physics::ACTIVE_BODY,                                            // li r8, 4
            Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },                                   // the zeroed vmx lane
            CgsSceneManager::SceneManagerIO::E_DO_NOT_ADD_TO_CACHE_MANAGER);     // li r8, 2

        lpSceneInput->SetVolumeInstanceCullingGroup(lVolumeInstanceId,
                                                    KI_CULLING_GROUP_DETACHED_WHEEL);

        // ---- 3. claim the triangle-cache slot ----------------------------------------------------
        // MEASURED: like the body-part twin, this post emits NO "queue too small" tripwire.
        CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache lAddEvent;
        lAddEvent.miCacheSlot =
            static_cast<s32>((luHandle & 0xFFull) + KU_WHEEL_TRIANGLE_CACHE_SLOT_BASE);
        lAddEvent.mfCacheSphereRadius = mfRadius + KF_WHEEL_CACHE_SPHERE_PADDING;
        lpSceneInput->mAddToCacheQueue.AddEvent(lAddEvent);

        mbAddedToScene = true;   // stb 1 -> wheel+0x81
    }

    // ==============================================================================================
    // GetCylinder -- BrnPhysicalWheel.h:191, DECLARED here since 2026-08-06 and BODYLESS until now.
    //
    // ⭐ 2026-08-27 (detach-3 wave). There is NO standalone X360 symbol for this: the console folds
    // it into its one caller, DeformableObject::DoDetachedWheelWorldContactGeneration @0x82609878,
    // where the expansion is unmistakable (0x826099xx, the block between the TriangleList validation
    // and the AddPrimitive(Cylinder*) call):
    //     lvx128 v13, r0,  wheel        ; row 0 of mRenderTransform
    //     lvx128 v12, wheel+0x10        ; row 1
    //     lvx128 v10, wheel+0x20        ; row 2
    //     lvx128 v11, wheel+0x30        ; row 3 (translation)
    //     vspltisw v0,-1 ; vslw v0,v0,v0 ; vxor v0, v10, v0     ; row 2 NEGATED (the 0x80000000 splat)
    //     stvx128 v0  -> cyl+0x00       ; -row2
    //     stvx128 v12 -> cyl+0x10       ;  row1
    //     stvx128 v13 -> cyl+0x20       ;  row0
    //     stvx128 v11 -> cyl+0x30       ;  row3
    //     lfs/stfs wheel+0x7C -> cyl+0x40 ; mfRadius
    //     lfs/stfs wheel+0x78 -> cyl+0x44 ; mfHalfHeight
    // So the collision cylinder's local frame is the wheel's render frame turned a quarter turn
    // about Y -- basis (-at, up, right) -- which puts the cylinder's own axis (its X row) along the
    // wheel's SPIN axis. That is the same quarter-turn AddToScene bakes into the shared
    // CylinderVolume's frame rows 0 and 2 above; the two agree, which is the cross-check that this
    // de-inlining is the real GetCylinder and not a local scratch build.
    //
    // ⚠️ mfLength takes mfHalfHeight VERBATIM -- the console does not double it. Not "corrected":
    // the tyre is as wide as the console makes it, and inventing a 2x here would be exactly the
    // "our code is better than the console's" defect this project treats as a bug report.
    // ==============================================================================================
    void PhysicalWheel::GetCylinder(CgsGeometric::Cylinder& lCylinderOut) const
    {
        Matrix44Affine lCylinderTransform;
        // The vxor sign-flip is a full 16-byte lane operation on the console -- all four lanes,
        // w included -- so it is spelled that way here rather than as a Vector3 negate.
        lCylinderTransform.xAxis.x = -mRenderTransform.zAxis.x;   // row 0 <- NEGATED render row 2
        lCylinderTransform.xAxis.y = -mRenderTransform.zAxis.y;
        lCylinderTransform.xAxis.z = -mRenderTransform.zAxis.z;
        lCylinderTransform.xAxis.w = -mRenderTransform.zAxis.w;
        lCylinderTransform.yAxis   =  mRenderTransform.yAxis;     // row 1 <- render row 1
        lCylinderTransform.zAxis   =  mRenderTransform.xAxis;     // row 2 <- render row 0
        lCylinderTransform.wAxis   =  mRenderTransform.wAxis;     // row 3 <- render row 3

        lCylinderOut.Set(lCylinderTransform, mfRadius, mfHalfHeight);
    }

    // ==============================================================================================
    // Prepare (DWARF BrnPhysicalWheel.h:132 / .cpp) -- no standalone X360 emission: the console
    // inlines it into DetachedWheelManager::DetachWheel @0x8260E8FC..0x8260E930, seven stores on
    // the 144-byte record:
    //   std   r7,  0x70(wheel)      mWheelBodyId       (the packed id Set just built)
    //   stvx128 x4 0x00..0x30       mRenderTransform   (the four rows off the lRenderTransform ptr)
    //   stfs  f30, 0x78(wheel)      mfHalfHeight       (f30 == f1 == lfHalfHeight)
    //   stfs  f29, 0x7C(wheel)      mfRadius           (f29 == f2 == lfRadius)
    //   stb   0,   0x80 / 0x81      mbFrozen / mbAddedToScene
    //   stvx128 zero, 0x60(wheel)   mLinearVelocity
    // mComOffset (+0x40) and mBoundingBoxDimensions (+0x50) are NOT written here.
    // ==============================================================================================
    void PhysicalWheel::Prepare(BurnoutWheelBodyID lWheelBodyId, f32 lfRadius, f32 lfHalfHeight,
                                Matrix44Affine lRenderTransform)
    {
        mWheelBodyId     = lWheelBodyId;
        mRenderTransform = lRenderTransform;
        mfHalfHeight     = lfHalfHeight;
        mfRadius         = lfRadius;
        mbFrozen         = false;
        mbAddedToScene   = false;
        mLinearVelocity  = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    }

    // ==============================================================================================
    // File-scope constants of BrnPhysicalWheel.cpp (DWARF :116-:119, names verbatim). All four are
    // .bss dyn-init splats on the X360 (unk_82FB9610 / unk_82FB9640 / unk_82FB9510 / unk_82FB9710
    // read 0 in the image BY DEFINITION); their thunks are export holes, lifted from the raw image:
    //   0x82C5DDF0  lfs flt_82004A20 -> unk_82FB9610   0x41200000 = 10.0        kvfMass
    //   0x82C5DE18  lfs flt_82094724 -> unk_82FB9640   0x3DAAAAAB = 1/12        kvfOneOverTwelve
    //   0x82C5DE40  lfs flt_8208F834 -> unk_82FB9510   0x3E800000 = 0.25        kvfOneOverFour
    //   0x82C5DE68  lfs flt_82020A90 -> unk_82FB9710   0x3F7AE148 = 0.98        kvfInnerRadiusProportion
    // ==============================================================================================
    static const VecFloat kvfMass                  = { 10.0f, 10.0f, 10.0f, 10.0f };                              // :116
    static const VecFloat kvfOneOverTwelve         = { 0.0833333358f, 0.0833333358f, 0.0833333358f, 0.0833333358f }; // :117
    static const VecFloat kvfOneOverFour           = { 0.25f, 0.25f, 0.25f, 0.25f };                              // :118
    static const VecFloat kvfInnerRadiusProportion = { 0.98f, 0.98f, 0.98f, 0.98f };                              // :119

    // ==============================================================================================
    // AddToSim @ 0x8260C7B8 (156 insns) -- bodied 2026-09-02 (deform close-out wave). Register the
    // detached wheel as a free rigid body: a HOLLOW cylinder (tube) about the car's x axis, inner
    // radius = kvfInnerRadiusProportion * mfRadius. X360, store for store (event at r1+0x90):
    //   lvfOuterRadiusSquared = r*r                     (lfs 0x7C ; fmuls)
    //   lvfInnerRadius        = kvfInnerRadiusProportion * splat(r)      (vmulfp128 v11)
    //   lvfInnerRadiusSquared + outer: vmaddfp v10 = v11*v11 + v8(r*r)   (radii sum, "A")
    //   lvfHeightSquared      = (2*hh) * (2*hh)          (vcfsx(2) twice, vmulfp128)
    //   axial      I_x  = (0.5 * kvfMass) * A                            (v12)
    //   transverse I_yz = (kvfOneOverTwelve*kvfMass)*h^2 + (kvfOneOverFour*kvfMass)*A   (v13)
    //     == the tube formulas 1/2 m (r1^2+r2^2) and 1/12 m (3(r1^2+r2^2) + h^2)
    //   +0x70 inverse inertia = (1/I_x, 1/I_yz, 1/I_yz, 0)   (vrefp + two Newton steps per lane,
    //     assembled by vrlimi128 8 / 4 / 2 into the {r,0,0,0} scratch -> exact reciprocals here)
    //   +0x84 spherical = 1 / min(the three inverse lanes)   (two fcmpu/fmr picks, fdivs 1.0/min)
    //     -- Inertia::SetInverseInertia maintains exactly that (see inertia.h), so one call
    //   +0x00 mID        = mWheelBodyId (ld 0x70)
    //   +0x10..+0x40     = mRenderTransform (lvx128 this+0/0x10/0x20/0x30) -- NOT the matrix
    //                      argument: lVehicleTransform is never read by the body (dead param)
    //   +0x50 / +0x60    = v1 / v2 (the two velocity args)
    //   +0x80 inv mass   = flt_82004014 = 0.1     (mass 10 == kvfMass)
    //   +0x88 / +0x8C    = flt_82092BC4 = 60.0 max linear, flt_82004A20 = 10.0 max angular
    //   +0x90 / +0x94    = flt_82013F90 = 0.001 linear + angular drag
    //   +0xA0 spy        = 1 ; +0xB0 state = 4 (ACTIVE_BODY)
    //   GetAddRigidBodyQueue(simIn)->AddEvent(&event)   (0x8260CA0C / 0x8260CA14)
    // ==============================================================================================
    void PhysicalWheel::AddToSim(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput, Matrix44Affine lVehicleTransform,
                                 Vector3 lLinearVelocity, Vector3 lAngularVelocity)
    {
        (void)lVehicleTransform;   // dead on the console: the body reads mRenderTransform (this+0..0x30)

        CgsPhysics::PhysicsSimulationIO::InAddRigidBody lAddRigidBodyEvent;   // :126

        const f32 lfOuterRadiusSquared = mfRadius * mfRadius;                                   // :128
        const f32 lfInnerRadius        = kvfInnerRadiusProportion.x * mfRadius;                 // :129
        const f32 lfInnerRadiusSquared = lfInnerRadius * lfInnerRadius;                         // :130
        const f32 lfHeight             = 2.0f * mfHalfHeight;
        const f32 lfHeightSquared      = lfHeight * lfHeight;                                   // :127
        const f32 lfRadiiSquaredSum    = lfInnerRadiusSquared + lfOuterRadiusSquared;           // vmaddfp v10

        const f32 lfAxialInertia      = (0.5f * kvfMass.x) * lfRadiiSquaredSum;                 // v12
        const f32 lfTransverseInertia = (kvfOneOverTwelve.x * kvfMass.x) * lfHeightSquared
                                      + (kvfOneOverFour.x   * kvfMass.x) * lfRadiiSquaredSum;   // v13

        // The three vrefp + Newton reciprocals, converged (see inertia.h precedent).
        const Vector3 lInverseInertia = { 1.0f / lfAxialInertia, 1.0f / lfTransverseInertia,
                                          1.0f / lfTransverseInertia, 0.0f };

        lAddRigidBodyEvent.mID = GetVolumeInstanceId().muId;                 // ld 0x70 -> +0x00 (the packed id IS the rigid-body id)
        lAddRigidBodyEvent.mRigidBody.mTransform       = mRenderTransform;   // +0x10..+0x40
        lAddRigidBodyEvent.mRigidBody.mVelocity        = lLinearVelocity;    // +0x50 (v1)
        lAddRigidBodyEvent.mRigidBody.mAngularVelocity = lAngularVelocity;   // +0x60 (v2)
        lAddRigidBodyEvent.mRigidBody.mInertia.SetInverseInertia(lInverseInertia);   // +0x70 (+ the +0x84 spherical)
        lAddRigidBodyEvent.mRigidBody.mInertia.SetInverseMass(0.1f);                 // +0x80  flt_82004014
        lAddRigidBodyEvent.mRigidBody.mInertia.SetMaxLinearVelocity(60.0f);          // +0x88  flt_82092BC4
        lAddRigidBodyEvent.mRigidBody.mInertia.SetMaxAngularVelocity(10.0f);         // +0x8C  flt_82004A20
        lAddRigidBodyEvent.mRigidBody.mInertia.SetLinearDrag(0.001f);                // +0x90  flt_82013F90
        lAddRigidBodyEvent.mRigidBody.mInertia.SetAngularDrag(0.001f);               // +0x94  flt_82013F90
        lAddRigidBodyEvent.mRigidBody.mbSpy = true;                                   // +0xA0  stb 1
        lAddRigidBodyEvent.meState = rw::physics::ACTIVE_BODY;                        // +0xB0  li 4

        lpSimInput->GetAddRigidBodyQueue()->AddEvent(lAddRigidBodyEvent);
    }

}
}
