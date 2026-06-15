#pragma once

// The deformation input interface: a per-frame buffer of fixed-capacity event
// queues that game systems push deformation requests onto (the deformation system
// drains them). Member set, order and types recovered from the DecFIGS DWARF
// (BrnDeformationInputInterface.h) and confirmed against the X360 Construct
// @0x825A95E0 (each queue's byte offset matches: the queues are accessed by name,
// not offset — this PC build is x64 so absolute offsets differ). This header
// currently declares only the two functions in this TU (Construct,
// AddDeformationModel); the remaining accessors/mutators are reconstructed with
// their own TUs.
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"  // Add/Remove/Deactivate/SetModelCollision/SetModelCullingGroup events
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"          // BrnPhysics::Vehicle::ValidateRaceCarEvent
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue

namespace BrnPhysics
{
    namespace Deformation
    {
        class DeformationInputInterface
        {
        public:
            void Construct();

            // Queues a new deformable model for simulation; returns the model's
            // slot index (the queue length after insertion, minus one).
            u32 AddDeformationModel(ResourceHandle lModelHandle,
                                    RigidBodyId lHandlingBodyID,
                                    EntityId lGlobalEntityId,
                                    BrnPhysics::Vehicle::VehiclePhysics* lpVehiclePhysics,
                                    Vector3 lvCOMOffset,
                                    const Matrix44Affine& lInitialWorldSpaceTransform,
                                    Vector3 lvInitialWorldSpaceVelocity,
                                    Vector3 lvInitialWorldSpaceAngularVelocity,
                                    f32 lfInitialDamageAmount,
                                    DeformationResetType leBaseDeformationType,
                                    bool lbUseSweptSphereTests);

        private:
            CgsModule::EventQueue<AddDeformationModelEvent, 20>        mAddDeformationModelQueue;          // X360 +0
            CgsModule::EventQueue<RemoveDeformationModelEvent, 20>     mRemoveDeformationModelQueue;       // X360 +3216
            CgsModule::EventQueue<DeactivateDeformationModelEvent, 28> mDeactivateDeformationModelQueue;   // X360 +3392
            CgsModule::EventQueue<BrnPhysics::Vehicle::ValidateRaceCarEvent, 8> mValidateDeformationModelEventQueue; // X360 +3856
            CgsModule::EventQueue<SetModelCollisionEvent, 28>         mSetModelCollisionEventQueue;        // X360 +4128
            CgsModule::EventQueue<SetModelCullingGroupEvent, 28>      mSetModelCullingGroupEventQueue;     // X360 +4592
            bool mbResetPlayerScratches;                                                                   // X360 +5056
        };
    }
}
