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

        // Input event: remove a deformation model from simulation.
        struct alignas(16) RemoveDeformationModelEvent
        {
            RigidBodyId mHandlingBodyID;
        };

        // Input event: toggle a deformation model's collision.
        struct alignas(16) SetModelCollisionEvent
        {
            RigidBodyId mHandlingBodyID;
            bool        mbCollide;
        };

        // Output event: a part has detached from a vehicle.
        struct alignas(16) DetachedPartNotificationEvent
        {
            Vector3    mPointOnA;
            EntityId   mVehicleId;
            EBodyParts meType;
        };

        // Output event: current orientation/velocity of a hinged (jointed) part.
        // NOT 16-byte aligned (no SIMD members): DeformationOutputInterface places its
        // queue at byte +116, which is only 4-aligned — proof it is a plain 4-aligned
        // struct, so the queue's inline buffer starts at +12 (not +16).
        struct JointedPartStateEvent
        {
            EntityId   mVehicleId;
            EBodyParts meType;
            f32        mfCurrentOrientation;
            f32        mfHingeVelocity;
        };

        // Input event: set a deformation model's scene culling group.
        struct alignas(16) SetModelCullingGroupEvent
        {
            RigidBodyId  mHandlingBodyID;
            typedef u32  CullingGroup;   // InEventAddForCollision::CullingGroup (CgsSceneManagerTypes.h)
            CullingGroup mCullGroup;
        };
    }
}
