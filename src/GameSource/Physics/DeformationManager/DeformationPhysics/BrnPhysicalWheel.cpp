#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint (walls leg 4 gates)

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"   // InSceneUpdateInterface::SetVolumeInstanceTransform (pulls in CgsSceneManager::EntityId)

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
    // ----- file-scope scratch (DWARF BrnPhysicalWheel.h:202, `extern char[96]`) ------------------
    //
    // maCylinderInitialiseBuffer is the wheel's cylinder-initialisation scratch buffer. It is a
    // file-scope STATIC (NOT an instance member -- the frozen header deliberately omits it from the
    // layout) shared by the lifecycle/geometry methods (Construct / GetCylinder) authored elsewhere
    // in this TU. It is declared here, in the .cpp data home, exactly as the sibling
    // BrnPhysicalBodyPart.cpp keeps maBoxInitialiseBuffer. The two functions bodied below do not
    // touch it.
    static char maCylinderInitialiseBuffer[96];

    // ----- OutUpdateRigidBody byte offsets the asm indexes (event type un-homed) -----------------
    namespace
    {
        // The rigid-body update event's field offsets (asm-authoritative; struct not yet homed).
        static const u32 KU_EVENT_TRANSLATION   = 32;   // rigid-body translation row -> render wAxis
        static const u32 KU_EVENT_LINEAR_VEL    = 48;   // new linear velocity -> mLinearVelocity
        static const u32 KU_EVENT_BASIS_X_AXIS  = 80;   // rigid-body basis xAxis row
        static const u32 KU_EVENT_BASIS_Y_AXIS  = 96;   // rigid-body basis yAxis row
        static const u32 KU_EVENT_BASIS_Z_AXIS  = 112;  // rigid-body basis zAxis row
        static const u32 KU_EVENT_FLAGS         = 156;  // flag byte
        static const u8  KU_EVENT_FLAG_FROZEN   = 0x2;  // bit 1: wheel is now frozen / settled
    }

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
    void PhysicalWheel::Update(const OutUpdateRigidBody* lpUpdateEvent,
                               CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        const char* lpEvent = reinterpret_cast<const char*>(lpUpdateEvent);

        // v6 = (flags >> 1) & 1 ; mbFrozen = (flags & 0x2) != 0  -- both are flag bit 1.
        const u8 lu8Flags = *reinterpret_cast<const u8*>(lpEvent + KU_EVENT_FLAGS);
        const bool lbFrozen = (lu8Flags & KU_EVENT_FLAG_FROZEN) != 0;
        mbFrozen = lbFrozen;

        // mLinearVelocity = event linear velocity (lvx128 event+48 ; stvx128 this+96).
        mLinearVelocity = *reinterpret_cast<const Vector3*>(lpEvent + KU_EVENT_LINEAR_VEL);

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
            // Reassemble the rigid-body transform from the event rows the asm staged on the stack:
            //   xAxis = event+80, yAxis = event+96, zAxis = event+112, wAxis = event+32.
            Matrix44Affine lRigidBodyTransform;
            lRigidBodyTransform.xAxis = *reinterpret_cast<const Vector3*>(lpEvent + KU_EVENT_BASIS_X_AXIS);
            lRigidBodyTransform.yAxis = *reinterpret_cast<const Vector3*>(lpEvent + KU_EVENT_BASIS_Y_AXIS);
            lRigidBodyTransform.zAxis = *reinterpret_cast<const Vector3*>(lpEvent + KU_EVENT_BASIS_Z_AXIS);
            lRigidBodyTransform.wAxis = *reinterpret_cast<const Vector3*>(lpEvent + KU_EVENT_TRANSLATION);

            SetRigidBodyTransform(lRigidBodyTransform, lpSceneInterface);

            // if ( !mbAddedToScene ) add the wheel's volume instance to the scene.
            if ( !mbAddedToScene )
            {
                AddToScene(lpSceneInterface);
                return;
            }
        }
    }

    void PhysicalWheel::RemoveFromScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* /*lpSceneInput*/)
    {
        static bool sbLoggedWRFS = false;
        if ( !sbLoggedWRFS )
        {
            sbLoggedWRFS = true;
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                *CgsDev::Log::gpDebugPrint << "conductor gate: PhysicalWheel::RemoveFromScene reached but not "
                                              "reconstructed [FLAG PC boot gate]\n";
        }
        
    }


    // LOG-ONCE GATE 2026-08-14 (walls leg 4): the wheel's scene ADD (the RemoveFromScene twin's
    // inverse) is not reconstructed; dead today (0 detached wheels). Reconstruct and DELETE.
    void PhysicalWheel::AddToScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* /*lpSceneInput*/)
    {
        static bool sbLoggedWATS = false;
        if ( !sbLoggedWATS )
        {
            sbLoggedWATS = true;
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                *CgsDev::Log::gpDebugPrint << "conductor gate: PhysicalWheel::AddToScene reached "
                                              "but not reconstructed [FLAG PC boot gate]\n";
        }
    }

}
}
