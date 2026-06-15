#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h"

// BrnPhysics::Deformation::DeformationInputInterface::Construct           @ 0x825A95E0
// BrnPhysics::Deformation::DeformationInputInterface::AddDeformationModel @ 0x825E7248
// Reconstructed from BURNOUT_X360_ARTIST.XEX (member names/shape from the DecFIGS
// DWARF, BrnDeformationInputInterface.h). Construct builds the six fixed-capacity
// request queues (each points itself at its inline buffer, sets the capacity, and
// clears the live count) and clears the reset-scratches flag; the X360 pseudocode's
// trailing per-queue length re-zeroes are redundant (Construct already cleared them)
// and are omitted. AddDeformationModel fills an event from its arguments and pushes
// it onto the add queue (the X360 builds the event in-register via VMX copies; we
// restore the by-name member assignments).

namespace BrnPhysics
{
namespace Deformation
{
    void DeformationInputInterface::Construct()
    {
        mAddDeformationModelQueue.Construct();
        mRemoveDeformationModelQueue.Construct();
        mDeactivateDeformationModelQueue.Construct();
        mValidateDeformationModelEventQueue.Construct();
        mSetModelCollisionEventQueue.Construct();
        mSetModelCullingGroupEventQueue.Construct();

        mbResetPlayerScratches = false;
    }

    u32 DeformationInputInterface::AddDeformationModel(ResourceHandle lModelHandle,
                                                       RigidBodyId lHandlingBodyID,
                                                       EntityId lGlobalEntityId,
                                                       BrnPhysics::Vehicle::VehiclePhysics* lpVehiclePhysics,
                                                       Vector3 lvCOMOffset,
                                                       const Matrix44Affine& lInitialWorldSpaceTransform,
                                                       Vector3 lvInitialWorldSpaceVelocity,
                                                       Vector3 lvInitialWorldSpaceAngularVelocity,
                                                       f32 lfInitialDamageAmount,
                                                       DeformationResetType leBaseDeformationType,
                                                       bool lbUseSweptSphereTests)
    {
        AddDeformationModelEvent lEvent;
        lEvent.mModelHandle                      = lModelHandle;
        lEvent.mHandlingBodyID                   = lHandlingBodyID;
        lEvent.mGlobalEntityId                   = lGlobalEntityId;
        lEvent.mCOMOffset                        = lvCOMOffset;
        lEvent.mInitialWorldSpaceTransform       = lInitialWorldSpaceTransform;
        lEvent.mInitialWorldSpaceVelocity        = lvInitialWorldSpaceVelocity;
        lEvent.mInitialWorldSpaceAngularVelocity = lvInitialWorldSpaceAngularVelocity;
        lEvent.mpVehiclePhysics                  = lpVehiclePhysics;
        lEvent.mfInitialDamageAmount             = lfInitialDamageAmount;
        lEvent.meBaseDeformationType             = leBaseDeformationType;
        lEvent.mbDoSweptSphereTests              = lbUseSweptSphereTests;

        mAddDeformationModelQueue.AddEvent(lEvent);
        return mAddDeformationModelQueue.GetLength() - 1;
    }
}
}
