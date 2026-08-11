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

            // ⭐ ADDED 2026-08-11 (create-drain wave). Both are DWARF-declared on this class
            // (DecFIGS BrnDeformationInputInterface.h:58 `uint32_t RemoveDeformationModel(RigidBodyId)`
            // and :76 `uint32_t DeactivateDeformationModel(RigidBodyId, float32_t,
            // BrnPhysics::Deformation::DeformationResetType)`), and neither has a standalone X360
            // symbol -- the console INLINES both inside VehicleManager::ProcessRemoveEvents
            // @0x826160C8, at the two raw seats this class already documents:
            //     0x826163CC  addi r3, lpDeformationInterface, 0xD40   == +3392, the deactivate queue
            //     0x826164AC  addi r3, lpDeformationInterface, 0xC90   == +3216, the remove queue
            // each followed by a 16-byte stack event and a BaseEventQueue<T>::AddEvent. Reached
            // BY NAME here so the drain never spells those two offsets.
            // The return is the queue length after insertion minus one -- the same "slot index"
            // convention AddDeformationModel already uses (and ProcessRemoveEvents discards it,
            // exactly as the console does).
            u32 RemoveDeformationModel(RigidBodyId lHandlingBodyID);
            u32 DeactivateDeformationModel(RigidBodyId lHandlingBodyID,
                                           f32 lfInitialDamageAmount,
                                           DeformationResetType leDeformationResetType);

            // ADDITIVE GROW (flagged by DeformationManager mgr-core group): the manager's
            // per-frame event processors (DeformationManager::ProcessAdd/Remove/Deactivate-
            // DeformationModelEvents, ProcessEvents) drain these request queues every scene
            // update. The X360 reads them at fixed sub-offsets off the interface base
            // (add @+0, remove @+3216, deactivate @+3392, the reset-scratches flag @+5056);
            // these by-name const accessors expose exactly those reads without changing the
            // class layout or its existing Construct/AddDeformationModel semantics. The
            // ValidateRaceCar / SetModelCollision / SetModelCullingGroup queues belong to the
            // sibling processors and gain their own accessors when those TUs land.
            const CgsModule::EventQueue<AddDeformationModelEvent, 20>& GetAddDeformationModelQueue() const
            {
                return mAddDeformationModelQueue;
            }
            const CgsModule::EventQueue<RemoveDeformationModelEvent, 20>& GetRemoveDeformationModelQueue() const
            {
                return mRemoveDeformationModelQueue;
            }
            const CgsModule::EventQueue<DeactivateDeformationModelEvent, 28>& GetDeactivateDeformationModelQueue() const
            {
                return mDeactivateDeformationModelQueue;
            }
            bool ShouldResetPlayerScratches() const { return mbResetPlayerScratches; }

            // Drop every queued event + clear the reset-scratches flag (the X360 zeroes each
            // queue's miLength directly at the tail of DeformationManager::ProcessEvents once the
            // queues are drained). Pure length resets -- the inline buffers are untouched.
            void ClearAllQueues()
            {
                mAddDeformationModelQueue.Clear();
                mRemoveDeformationModelQueue.Clear();
                mDeactivateDeformationModelQueue.Clear();
                mValidateDeformationModelEventQueue.Clear();
                mSetModelCollisionEventQueue.Clear();
                mSetModelCullingGroupEventQueue.Clear();
                mbResetPlayerScratches = false;
            }

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
