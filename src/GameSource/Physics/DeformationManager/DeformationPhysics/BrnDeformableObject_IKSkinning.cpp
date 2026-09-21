#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "rw/math/vpu/matrix44affine_operation.h"

namespace BrnPhysics
{
namespace Deformation
{
    // ARTIST 0x825E0EC8: start at the streamed rest frame, apply driven-point skinning,
    // then follow a hinged physical part in the vehicle's graphics frame.
    void DeformableObject::UpdateLocator(Matrix44Affine& lrTransform, ETagPointType& lreType,
                                         const LocatorPointSpec* lpSpec,
                                         const rw::math::vpu::Matrix44Affine& lrInverseVehicleGraphics,
                                         DetachedPartManager* lpPartMgr)
    {
        CGS_ASSERT(lpSpec != nullptr, "lpLocator");
        CGS_ASSERT(lpSpec->mu8SkinPoint < 128, "lpLocator->mu8SkinPoint < 128");
        CGS_ASSERT(lpPartMgr != nullptr, "lpDetachedPartManager");

        const s32 liPartIndex = lpSpec->miIkPartIndex;
        CGS_ASSERT(liPartIndex < mpDeformationSpec->GetNumberOfIKParts(),
                   "liPartIndex < mpDeformationSpec->GetNumIKParts()");

        lrTransform = lpSpec->mLocatorMatrix;
        lrTransform.wAxis = rw::math::vpu::Add(
            lrTransform.wAxis, maVerletOffsets_Scratch[lpSpec->mu8SkinPoint].GetVector3());

        if (liPartIndex == -1)
            return;

        if (maPartStates[liPartIndex] == E_PART_STATE_HINGED)
        {
            // Console reloads the rest matrix in this arm, then adds the streamed mesh
            // offset and subtracts both the part graphics position and vehicle COM offset.
            const Vector3& lrMeshOffset = mpDeformationSpec->mMeshOffset;
            const Vector3& lrPartGraphicsPosition =
                maIKParts[liPartIndex].GetSpec()->GetPartGraphicsTransform().wAxis;
            const Vector3& lrCOMOffset =
                GetVehiclePhysics()->GetAttribs()->mBaseAttribs.mCOMOffset;
            lrTransform.wAxis.x += (lrMeshOffset.x - lrCOMOffset.x) - lrPartGraphicsPosition.x;
            lrTransform.wAxis.y += (lrMeshOffset.y - lrCOMOffset.y) - lrPartGraphicsPosition.y;
            lrTransform.wAxis.z += (lrMeshOffset.z - lrCOMOffset.z) - lrPartGraphicsPosition.z;

            const PhysicalBodyPart* lpPart =
                lpPartMgr->GetPartFromIndex(static_cast<u16>(maIKParts[liPartIndex].GetPartPoolIndex()));
            // First product: physical part transform with local graphics minus initial COM
            // offset rotated into world. Second product: world to vehicle graphics space.
            lrTransform = rw::math::vpu::Mult(lrTransform, lpPart->GetEventRenderTransform());
            lrTransform = rw::math::vpu::Mult(lrTransform, lrInverseVehicleGraphics);
        }

        if (maPartStates[liPartIndex] == E_PART_STATE_DETATCHED)
            lreType = E_TAGPOINT_COUNT;
    }
}
}
