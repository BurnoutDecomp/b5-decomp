#pragma once

// BrnPhysics::Deformation::ImpulseParams -- the parameter block the deformation system fills in
// to apply one impulse to a collidable body (a sensor / vehicle rigid body). Homed at its mirrored
// DWARF path (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnCollidableBody.h, struct @ :49).
//
// DeformableObject::ApplyCarCarImpulse builds an ImpulseParams and hands it to ApplySensorImpulse,
// which forwards it to the body's ApplyLocalImpulse. The member SEQUENCE + names + types are
// DWARF-authoritative -- every field below is verbatim from the DWARF struct (meImpulseDirection ..
// mbWorldContact). The owning CollidableBody class (the base of VehicleRigidBody) carries only its
// vptr in the layout that matters here and is NOT reconstructed in this slice (it has its own TU);
// ImpulseParams is a standalone POD, so the car-car-impulse group only needs the param block.

#include "types.hpp"            // f32, bool
#include "BrnCommonTypes.h"     // Vector3, VecFloat
#include "rw/physics/rigidbody.h"  // rw::physics::InputSpace
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnSharedDeformationEnums.h"  // ENextSensorDirection
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnAbsorptionTable.h"          // EAbsorptionSets

namespace BrnPhysics
{
namespace Deformation
{
    // The impulse passer is referenced only by pointer (mpImpulsePasser); forward-declare it to
    // avoid pulling its whole header in (it has its own home, BrnImpulsePasser.h).
    struct ImpulsePasser;

    // DWARF BrnCollidableBody.h:49. One impulse to apply to a collidable body. Built by
    // DeformableObject::ApplyCarCarImpulse / ApplyCarWorldImpulse and consumed by
    // CollidableBody::ApplyLocalImpulse.
    struct ImpulseParams
    {
        ENextSensorDirection    meImpulseDirection;            // :51 which body axis to steer along
        VecFloat                mvfImpulseMagnitude;           // :52 scalar impulse magnitude
        Vector3                 mImpulsePosition;              // :53 application point
        Vector3                 mWorldImpulseDirection;        // :54 world-space impulse direction
        Vector3                 mLimitVector;                  // :55 clamp/limit axis
        rw::physics::InputSpace mePositionSpace;               // :56 frame mImpulsePosition is in
        VecFloat                mvfInverseInertia;             // :57 the body's inverse-inertia term
        VecFloat                mvfTimeStep;                   // :58 physics time-step this apply
        VecFloat                mvfVelocityAlongNormal;        // :59 closing speed along the normal
        VecFloat                mvfAllowedCompressionFactor;   // :60 deformation compression budget
        VecFloat                mvfMaximumAllowedAbsorption;   // :61 absorption clamp
        ImpulsePasser*          mpImpulsePasser;               // :62 optional chained impulse passer
        EAbsorptionSets         meAbsorptionSet;               // :63 which absorption profile to use
        bool                    mbWorldContact;                // :64 true if the contact is vs world
    };
}
}
