#pragma once

// BrnPhysics::Deformation::VehicleRigidBody -- MINIMAL OWNING SLICE.
//
// The collidable body that wraps a car's VehiclePhysics for the deformation system: it is the
// CollidableBody the deformation sensors hand impulses to, and it routes those impulses into the
// underlying vehicle physics (linear + angular momentum) through the three VehiclePhysics contact
// handlers. The full class (Construct/Destruct/Prepare/Release/ClearVariables/ApplyShowtimeContact
// Impulse/GetVehiclePhysics; DWARF BrnVehicleRigidBody.h:51) is several TUs. THIS slice reconstructs
// only what the two impulse-apply virtuals reach BY NAME:
//   * ApplyLocalImpulse        @0x8260E068 (BrnVehicleRigidBody.cpp:186)
//   * RecievePassedOnImpulse   @0x8260DFA0 (BrnVehicleRigidBody.cpp:131)
// so the only state pinned is the attached vehicle pointer (console +0x4) plus the per-direction
// unit-axis lookup table the two functions index (X360 rodata &unk_82FB9680).
//
// LAYOUT NOTE (project rule -- members BY NAME, no raw-offset padding): VehicleRigidBody derives the
// canonical CollidableBody (BrnCollidableBody.h), whose console layout is just a vptr at +0x0;
// mpAttachedVehicle follows at console +0x4 (the asm's `*(this+4)`). GROW this slice into the full
// member sequence + the remaining methods as the VehicleRigidBody TUs land; do not fork.
//
// HOMING NOTE: this header previously carried a LOCAL minimal copy of CollidableBody. That copy is
// now REMOVED -- the canonical base is included from BrnCollidableBody.h (Task 1) so there is a single
// ODR-correct CollidableBody shared by VehicleRigidBody and DeformationSensor.

#include "types.hpp"
#include "BrnCommonTypes.h"     // Vector3, Vector4, VecFloat
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnCollidableBody.h"          // CollidableBody (canonical base) + ImpulseParams
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnSharedDeformationEnums.h"  // ENextSensorDirection
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"                     // BrnPhysics::Vehicle::VehiclePhysics

namespace BrnPhysics
{
namespace Deformation
{
    // VehicleRigidBody : CollidableBody. Wraps the attached car's VehiclePhysics and converts a
    // deformation ImpulseParams into one of the three VehiclePhysics contact-impulse applies.
    struct VehicleRigidBody : public CollidableBody
    {
        // console +0x4 (the asm's `*(this+4)`). The car whose physics body receives the impulse.
        // DWARF BrnVehicleRigidBody.h:92.
        BrnPhysics::Vehicle::VehiclePhysics* mpAttachedVehicle;

        BrnPhysics::Vehicle::VehiclePhysics*       GetVehiclePhysics()       { return mpAttachedVehicle; }
        const BrnPhysics::Vehicle::VehiclePhysics* GetVehiclePhysics() const { return mpAttachedVehicle; }

        // @0x8260E068 (BrnVehicleRigidBody.cpp:186). Build the local impulse vector from the params'
        // direction + magnitude and route it into the attached vehicle: crashed -> crashed-contact,
        // else world-contact -> wall-contact, else car-contact.
        virtual void ApplyLocalImpulse(ImpulseParams* lpImpulseParams);

        // @0x8260DFA0 (BrnVehicleRigidBody.cpp:131). Receive an impulse passed on from a neighbouring
        // body: query the attached vehicle's impulse-passing gate first and early-out if it is set,
        // otherwise apply the impulse exactly as ApplyLocalImpulse does.
        virtual void RecievePassedOnImpulse(const ImpulseParams* lpImpulseParams, VecFloat lvfPassedMagnitude);
    };
}
}
