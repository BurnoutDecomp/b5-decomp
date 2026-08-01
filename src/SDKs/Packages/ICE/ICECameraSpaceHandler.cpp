#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44Affine / Vector3
#include "GameSource/Director/Camera/Utils/CameraUtils.h"   // Utils::CreateLookAt (the two takedown spaces)
#include "GameSource/Director/Camera/BrnBehaviourManager.h" // BehaviourHandle<>::GetProducedCamera (gameplay space)
#include "GameSource/Director/Camera/Camera.h"              // Camera::GetTransform
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT

// ============================================================================
// SDKs/Packages/ICE/ICECameraSpaceHandler.cpp
//
// The reference-space half of ICE::CameraSpaceHandler. Both bodies here are attested
// to THIS translation unit by the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/SDKs/Packages/ICE/ICECameraSpaceHandler.cpp), which
// places GetTransformToWorld at :112, TransformToWorld at :51 and the two private
// takedown helpers at :95 / :103 -- i.e. one TU, four functions. They were split apart
// in the reconstruction; this file puts them back together, which is also what makes
// the TU self-consistent enough to mount (GetTransformToWorld was previously the ONLY
// unresolved external TransformToWorld dragged in).
//
//   GetTransformToWorld     @0x82531BA0
//   TransformToWorld(p,sp)  @0x82533DE8
//   GetTakedownToWorld        )  no standalone symbol -- the X360 INLINES both into
//   GetReverseTakedownToWorld )  GetTransformToWorld's two CreateLookAt arms
// ============================================================================

namespace ICE
{

// ---------------------------------------------------------------------------
// GetTakedownToWorld / GetReverseTakedownToWorld (DWARF ICECameraSpaceHandler.cpp:95
// and :103; private). The takedown frame looks from one car to the other. Both are
// INLINED into GetTransformToWorld on the console -- cases 6 and 8 of its switch each
// emit a direct `bl BrnDirector::Camera::Utils::CreateLookAt` with the two POSITION
// rows loaded straight out of the member matrices (v1 = eye, v2 = target):
//
//   case 6  (TAKEDOWN)          v1 = [this+0x30] = mCarToWorld.wAxis   (eye)
//                               v2 = [this+0x70] = mCar2ToWorld.wAxis  (target)
//   case 8  (REVERSE_TAKEDOWN)  v1 = [this+0x70] = mCar2ToWorld.wAxis  (eye)
//                               v2 = [this+0x30] = mCarToWorld.wAxis   (target)
//
// (0x30 is mCarToWorld's fourth row; 0x40 + 0x30 = 0x70 is mCar2ToWorld's.) The
// two-argument CreateLookAt supplies the world up axis itself.
// ---------------------------------------------------------------------------
const Matrix44Affine CameraSpaceHandler::GetTakedownToWorld() const
{
    return BrnDirector::Camera::Utils::CreateLookAt(mCarToWorld.wAxis, mCar2ToWorld.wAxis);
}

const Matrix44Affine CameraSpaceHandler::GetReverseTakedownToWorld() const
{
    return BrnDirector::Camera::Utils::CreateLookAt(mCar2ToWorld.wAxis, mCarToWorld.wAxis);
}

// ---------------------------------------------------------------------------
// GetTransformToWorld (DWARF ICECameraSpaceHandler.cpp:112) -- X360 @0x82531BA0.
//
// Hand back the reference-space-to-world affine for one eICESpace. The console body is
// a fourteen-case jump table (`cmplwi r5, 0xD` + `bctr` over jpt_82531BD8); every arm
// either flat-copies one of the eight member matrices (four lvx128/stvx128 pairs over
// offsets 0x00/0x10/0x20/0x30 of a member base) or synthesises a frame. Case-by-case,
// with the member base each arm loads from -- the offsets land exactly on the layout
// ICECameraSpaceHandler.hpp documents:
//
//   0,11 CAR / BYSTANDER    this+0x000  mCarToWorld           (one jump-table slot: the
//                                                              two spaces SHARE an arm)
//   1    WORLD              identity basis + ZERO translation
//   2    HYBRID             identity basis + mCarToWorld.wAxis translation
//   3    SCENE              this+0x0C0  mSceneToWorld
//   4    CAR2               this+0x040  mCar2ToWorld
//   5    TRAFFIC_LIGHT      this+0x080  mTrafficLightToWorld
//   6    TAKEDOWN           GetTakedownToWorld()          (inlined CreateLookAt)
//   7    IMPACT             this+0x100  mImpactToWorld
//   8    REVERSE_TAKEDOWN   GetReverseTakedownToWorld()   (inlined CreateLookAt)
//   9    GAMEPLAY           the live gameplay camera's transform, through mpGamePlayCam
//   10   HEADING            this+0x140  mHeadingToWorld
//   12   HEADING2           this+0x1C0  mHeading2ToWorld
//   13   LOOSE_HEADING      this+0x180  mLooseHeadingToWorld
//   def  assert + identity basis + ZERO translation
//
// The identity arms build their rows from the SDK's rodata basis block -- gIVector
// @0x82181500 {1,0,0,0} into row 0, unk_82181510 {0,1,0,0} into row 1, unk_82181520
// {0,0,1,0} into row 2 (that block is dumped and named in CameraUtils.cpp) -- and a
// `vspltisw v11, 0` zero into row 3. That is precisely Matrix44Affine::SetIdentity(),
// whose wAxis is {0,0,0,0}, so the arms are spelled with it rather than by poking rows.
//
// The two asserts are the console's own, verbatim, with its file/line
// (SDKs\Packages\ICE\ICECameraSpaceHandler.cpp:176 and :207 -- CGS_ASSERT supplies
// __FILE__/__LINE__ itself, so only the expression text is carried across).
// ⚠️ Note the console does NOT return early on the unknown-space assert: it falls into
// the identity build, so a dev build that clicks past the assert still gets a sane frame.
// ---------------------------------------------------------------------------
const Matrix44Affine CameraSpaceHandler::GetTransformToWorld(eICESpace leSpace) const
{
    Matrix44Affine lMatrix;

    switch (leSpace)
    {
    case eICE_CAR_SPACE:
    case eICE_BYSTANDER_SPACE:
        // Both spaces share one jump-table slot on the console.
        lMatrix = mCarToWorld;
        break;

    case eICE_WORLD_SPACE:
        lMatrix.SetIdentity();
        break;

    case eICE_HYBRID_SPACE:
        // World-aligned axes carried on the car's position.
        lMatrix.SetIdentity();
        lMatrix.wAxis = mCarToWorld.wAxis;
        break;

    case eICE_SCENE_SPACE:
        lMatrix = mSceneToWorld;
        break;

    case eICE_CAR2_SPACE:
        lMatrix = mCar2ToWorld;
        break;

    case eICE_TRAFFIC_LIGHT_SPACE:
        lMatrix = mTrafficLightToWorld;
        break;

    case eICE_TAKEDOWN_SPACE:
        lMatrix = GetTakedownToWorld();
        break;

    case eICE_IMPACT_SPACE:
        lMatrix = mImpactToWorld;
        break;

    case eICE_REVERSE_TAKEDOWN_SPACE:
        lMatrix = GetReverseTakedownToWorld();
        break;

    case eICE_GAMEPLAY_SPACE:
        // The live gameplay camera's own transform. The X360 asserts the back-pointer,
        // then calls the handle's produced-camera accessor (sub_82212288, pinned by its
        // "IsAllocated()" tripwire at BrnBehaviourManager.h:610 = BehaviourHandle::
        // GetProducedCamera) and reads the camera's transform -- the `addi r3, r3, 0x10`
        // on the returned pointer is Camera::GetTransform's own member offset, i.e. the
        // console spells this as two chained accessors, not one raw address.
        CGS_ASSERT(mpGamePlayCam != 0, "mpGamePlayCam != NULL");
        lMatrix = mpGamePlayCam->GetProducedCamera().GetTransform();
        break;

    case eICE_HEADING_SPACE:
        lMatrix = mHeadingToWorld;
        break;

    case eICE_HEADING2_SPACE:
        lMatrix = mHeading2ToWorld;
        break;

    case eICE_LOOSE_HEADING_SPACE:
        lMatrix = mLooseHeadingToWorld;
        break;

    default:
        CGS_ASSERT(false, "Unknown Camera Space");
        lMatrix.SetIdentity();
        break;
    }

    return lMatrix;
}

// ---------------------------------------------------------------------------
// TransformToWorld (DWARF ICECameraSpaceHandler.cpp:51) -- X360 @0x82533DE8.
//
// Map a point from the given ICE reference space into world space: fetch that space's
// affine (GetTransformToWorld, returned BY VALUE through the hidden sret pointer the
// console passes in r3) and apply the affine point transform
//
//     world = xAxis * p.x + yAxis * p.y + zAxis * p.z + wAxis
//
// The X360 broadcasts the point's lanes (vspltw128 v,vec,0/1/2) and accumulates the four
// 16-byte matrix rows loaded at 0x00/0x10/0x20/0x30 with three vmaddfp. Reading those in
// IDA's RAW FIELD ORDER (VD,VA,VB,VC => VD = VA*VC + VB):
//
//     vmaddfp v0, v10, v11, v0   ->  v0 = row0 * p.x + row3
//     vmaddfp v0, v11, v0,  v13  ->  v0 = row1 * p.y + v0
//     vmaddfp v0, v10, v0,  v12  ->  v0 = row2 * p.z + v0
//
// i.e. rotate by the 3x3 (rows xAxis/yAxis/zAxis) and add the translation row (wAxis).
// The DWARF renders the whole body as a single rw::math::vpu::TransformPoint(...) call;
// that vendor free function is not present in our reconstructed rwmath type header
// (which carries the layout/vocabulary only -- see vendor/renderware/include/rw/math/
// vpu/types.h), so we reproduce its semantics here by the named-lane affine transform
// rather than forking the vendor header. Per-lane f32 math gives exact semantic parity
// with the vspltw128 + vmaddfp accumulation.
//
// ⚠️ The FOURTH LANE is part of the accumulation, not a pass-through. The console's
// three vmaddfp are full 128-bit ops and the result is stored with a single stvx128, so
// w = row0.w*p.x + row1.w*p.y + row2.w*p.z + row3.w -- for a well-formed affine that is
// wAxis.w. An earlier revision of this file carried `lvWorld.w = lvPoint.w` instead,
// which is a different value for any matrix whose rows are not w-zero.
// ---------------------------------------------------------------------------
Vector3 CameraSpaceHandler::TransformToWorld(Vector3 lvPoint, eICESpace leSpace) const
{
    // The reference-space-to-world affine for the requested space (by value).
    const Matrix44Affine lMatrix = GetTransformToWorld(leSpace);

    const f32 lfX = lvPoint.x;
    const f32 lfY = lvPoint.y;
    const f32 lfZ = lvPoint.z;

    Vector3 lvWorld;
    lvWorld.x = lMatrix.xAxis.x * lfX + lMatrix.yAxis.x * lfY + lMatrix.zAxis.x * lfZ + lMatrix.wAxis.x;
    lvWorld.y = lMatrix.xAxis.y * lfX + lMatrix.yAxis.y * lfY + lMatrix.zAxis.y * lfZ + lMatrix.wAxis.y;
    lvWorld.z = lMatrix.xAxis.z * lfX + lMatrix.yAxis.z * lfY + lMatrix.zAxis.z * lfZ + lMatrix.wAxis.z;
    lvWorld.w = lMatrix.xAxis.w * lfX + lMatrix.yAxis.w * lfY + lMatrix.zAxis.w * lfZ + lMatrix.wAxis.w;

    return lvWorld;
}

} // namespace ICE
