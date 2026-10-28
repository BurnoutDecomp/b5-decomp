#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnVehicleRigidBody.h"

// ARTIST ApplyLocalImpulse 0x8260E068 and RecievePassedOnImpulse 0x8260DFA0.
// Both route the params' impulse through the same state/contact-kind selection.
// The passed-on magnitude argument is overwritten by the original params load.
namespace BrnPhysics {
namespace Deformation {
    static __forceinline void ApplyImpulseToVehicle(
        BrnPhysics::Vehicle::VehiclePhysics* lpVehicle,
        const ImpulseParams* lpImpulseParams)
    {
        const Vector3& lrDirection = KA_IMPULSE_DIRECTIONS[lpImpulseParams->meImpulseDirection];
        const f32 lfMagnitude = lpImpulseParams->mvfImpulseMagnitude.x;
        const Vector3 liImpulse = {
            lrDirection.x * lfMagnitude, lrDirection.y * lfMagnitude,
            lrDirection.z * lfMagnitude, lrDirection.w * lfMagnitude
        };
        if (lpVehicle->IsCrashing())
        {
            // ARTIST 0x8260E0B4/0x8260E018 loads the world-contact byte into r6.
            lpVehicle->ApplyCrashedContactImpulse(liImpulse, rw::physics::BODY_SPACE,
                lpImpulseParams->mImpulsePosition, lpImpulseParams->mePositionSpace,
                lpImpulseParams->mbWorldContact);
        }
        else if (lpImpulseParams->mbWorldContact)
        {
            lpVehicle->ApplyWallContactImpulse(liImpulse, rw::physics::BODY_SPACE,
                lpImpulseParams->mWorldImpulseDirection, lpImpulseParams->mImpulsePosition,
                lpImpulseParams->mePositionSpace);
        }
        else
        {
            lpVehicle->ApplyCarContactImpulse(liImpulse, rw::physics::BODY_SPACE,
                lpImpulseParams->mWorldImpulseDirection, lpImpulseParams->mImpulsePosition,
                lpImpulseParams->mePositionSpace);
        }
    }

    void VehicleRigidBody::ApplyLocalImpulse(ImpulseParams* lpImpulseParams)
    {
        ApplyImpulseToVehicle(mpAttachedVehicle, lpImpulseParams);
    }

    void VehicleRigidBody::RecievePassedOnImpulse(const ImpulseParams* lpImpulseParams,
                                                 VecFloat /*lvfPassedMagnitude*/)
    {
        if (mpAttachedVehicle->IsPlayerVehicleInShowtime())
            return;
        ApplyImpulseToVehicle(mpAttachedVehicle, lpImpulseParams);
    }
}
}
