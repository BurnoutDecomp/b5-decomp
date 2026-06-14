#pragma once

#include "BrnCommonTypes.h"

namespace BrnPhysics
{
    namespace Vehicle { struct VehiclePhysics; }

    namespace Deformation
    {
        // Recovered from BrnDeformationEvents.h / BrnIKBodyPart.h (DecFIGS DWARF).
        enum EBodyParts : s32
        {
            E_BODY_PART_INVALID = -1
        };

        enum DeformationResetType : s32
        {
            E_DEFORMATION_RESET_NONE = 0
        };

        struct ResourceHandle { u32 muValue; };

        // Input event: spawn a deformable model for a body.
        struct alignas(16) AddDeformationModelEvent
        {
            ResourceHandle             mModelHandle;
            RigidBodyId                mHandlingBodyID;
            EntityId                   mGlobalEntityId;
            Vector3                    mCOMOffset;
            Matrix44Affine             mInitialWorldSpaceTransform;
            Vector3                    mInitialWorldSpaceVelocity;
            Vector3                    mInitialWorldSpaceAngularVelocity;
            BrnPhysics::Vehicle::VehiclePhysics* mpVehiclePhysics;
            f32                        mfInitialDamageAmount;
            DeformationResetType       meBaseDeformationType;
            bool                       mbDoSweptSphereTests;
        };

        // Output event: a joint between two parts has broken.
        struct alignas(16) BrokenJointNotificationEvent
        {
            Vector3    mPointOnA;
            EntityId   mVehicleId;
            EBodyParts meType;
        };

        // Input event: stop simulating a deformable model. The X360 places the
        // owning queue's inline buffer 16-byte aligned.
        struct alignas(16) DeactivateDeformationModelEvent
        {
            RigidBodyId          mHandlingBodyID;
            f32                  mfInitialDamageAmount;
            DeformationResetType meDeformationResetType;
        };
    }
}
