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
    // =============================================================================================
    // RemoveDeformationModel / DeactivateDeformationModel -- NO STANDALONE X360 SYMBOL either.
    // Recovered 2026-08-11 from the one place the console inlines both, VehicleManager::
    // ProcessRemoveEvents @0x826160C8, read off the asm:
    //
    //   deactivate  0x826163C8  addi r4, r1, var_D0            ; a 16-byte stack event
    //               0x826163CC  addi r3, lpDeformationInterface, 0xD40   (== +3392, this queue)
    //               0x82616444  ldx  r11, r9, this             ; maRaceCarHandlingBodyIDs[idx]
    //               0x82616448  std  r11, var_D0(r1)           ; event.mHandlingBodyID
    //               (var_C8 == flt_82001CC0 == 0.0f and var_C4 == -1 are hoisted OUT of the loop,
    //                @0x82616128/0x82616138 -- i.e. the two trailing arguments are loop-invariant
    //                literals, which is exactly what a call with constant arguments looks like)
    //               0x8261644C  bl   BaseEventQueue<DeactivateDeformationModelEvent>::AddEvent
    //   remove      0x826164AC  addi r3, lpDeformationInterface, 0xC90   (== +3216, this queue)
    //               0x826164B0  ldx  r11, r11, this ; std r11, var_E8(r1)
    //               0x826164B8  bl   BaseEventQueue<RemoveDeformationModelEvent>::AddEvent
    //
    // The signatures are the DWARF's (BrnDeformationInputInterface.h:58 / :76), and the bodies are
    // the same three lines AddDeformationModel above already uses.
    // =============================================================================================
    u32 DeformationInputInterface::RemoveDeformationModel(RigidBodyId lHandlingBodyID)
    {
        RemoveDeformationModelEvent lEvent;
        lEvent.mHandlingBodyID = lHandlingBodyID;

        mRemoveDeformationModelQueue.AddEvent(lEvent);
        return mRemoveDeformationModelQueue.GetLength() - 1;
    }

    u32 DeformationInputInterface::DeactivateDeformationModel(RigidBodyId lHandlingBodyID,
                                                              f32 lfInitialDamageAmount,
                                                              DeformationResetType leDeformationResetType)
    {
        DeactivateDeformationModelEvent lEvent;
        lEvent.mHandlingBodyID        = lHandlingBodyID;
        lEvent.mfInitialDamageAmount  = lfInitialDamageAmount;
        lEvent.meDeformationResetType = leDeformationResetType;

        mDeactivateDeformationModelQueue.AddEvent(lEvent);
        return mDeactivateDeformationModelQueue.GetLength() - 1;
    }
}
}
