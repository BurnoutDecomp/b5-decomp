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

            // ⭐ ADDED 2026-08-11 (create-drain wave) -- NON-CONST overloads of four accessors the
            // DWARF already declares const (BrnDeformationInputInterface.h:131 / :134 / :137 /
            // :140 / :143). ⚠ FLAG: the const forms are DWARF-attested; these WRITE forms are
            // ADDITIVE. They exist because the vehicle manager's maintenance arms POST into these
            // queues, and the X360 emits every one of those posts as a bare
            // `addi r3, <interface>, <offset>` + `bl BaseEventQueue<T>::AddEvent` -- i.e. the
            // console reaches the queue itself, not a wrapper. The offsets it uses are exactly the
            // seats below:
            //     +3216  ProcessRemoveEvents @0x826164AC  (`addi r3,r10,0xC90`)
            //     +3392  ProcessRemoveEvents @0x826163CC  (`addi r3,r10,0xD40`)
            //     +3856  ProcessValidationEvents @0x825E903C (`addi r29,r5,0xF10`)
            //     +4128  ProcessCollisionEvents @0x825E8F54 (`addi r29,r27,0x1020`)
            //     +4592  ProcessCollisionEvents @0x825E8FB8 (`addi r29,r27,0x11F0`)
            // Reached BY NAME here so those console byte offsets never appear in a body.
            //
            // ⛔ WHY NOT THE DWARF's TYPED MUTATORS at these five seats: some of the console call
            // sites DO NOT WRITE EVERY FIELD of the event they post -- the validate post writes
            // only mbValidate / mVolumeInstanceID / mModelHandle. A typed mutator would have to
            // invent values for the rest, which is the "compensating for a partial reconstruction"
            // shape. The call sites build the event exactly as the console does instead, and say so.
            // Note also that the console itself reaches the QUEUE at these seats -- every one of
            // the five is a bare `addi r3,<interface>,<offset>` + `bl BaseEventQueue<T>::AddEvent`,
            // with no wrapper frame -- so a queue accessor is the literal shape, not a shortcut.
            //
            // ⚠️ ONE HALF OF THAT ARGUMENT WAS RETIRED 2026-08-11 (merge of the two create-drain
            // waves). It used to also claim "the deactivate post writes only mHandlingBodyID". It
            // does not: mfInitialDamageAmount (0.0f) and meDeformationResetType (-1) are
            // LOOP-INVARIANT and the compiler HOISTED their stores out of the drain loop into
            // ProcessRemoveEvents' prologue (`lfs f31, flt_82001CC0` @0x82616118 / `li r11,-1`
            // @0x8261611C, stored to var_C8/var_C4 @0x82616128/0x82616138 -- the same 16-byte
            // stack event var_D0 the loop then fills field 0 of). The console posts a FULLY
            // initialised deactivate event, and the caller now writes all three fields. That makes
            // `DeactivateDeformationModel` above -- the DWARF's own typed mutator, taking exactly
            // those three -- a faithful alternative spelling for that one seat, not an inventing
            // one. It is kept declared and bodied; the drain's queue-accessor spelling is left as
            // committed and boot-verified, and the two must not both be used for the same post.
            CgsModule::EventQueue<RemoveDeformationModelEvent, 20>& GetRemoveDeformationModelQueue()
            {
                return mRemoveDeformationModelQueue;
            }
            CgsModule::EventQueue<DeactivateDeformationModelEvent, 28>& GetDeactivateDeformationModelQueue()
            {
                return mDeactivateDeformationModelQueue;
            }
            CgsModule::EventQueue<BrnPhysics::Vehicle::ValidateRaceCarEvent, 8>& GetValidateDeformationModelEvents()
            {
                return mValidateDeformationModelEventQueue;
            }
            CgsModule::EventQueue<SetModelCollisionEvent, 28>& GetSetModelCollisionEvents()
            {
                return mSetModelCollisionEventQueue;
            }
            CgsModule::EventQueue<SetModelCullingGroupEvent, 28>& GetSetModelCullingGroupEvents()
            {
                return mSetModelCullingGroupEventQueue;
            }

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
