#pragma once

// BrnPhysics::Deformation -- the canonical CollidableBody base + its ImpulseParams param block.
// Homed at the mirrored DWARF path
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnCollidableBody.h; ImpulseParams @ :49,
// CollidableBody @ :80).
//
// CollidableBody is the shared base of every body the deformation system applies impulses to --
// DeformationSensor (BrnDeformationSensor.h) and VehicleRigidBody (BrnVehicleRigidBody.h) both
// derive it. Per the DWARF it has NO data members: just a vptr, two pure-virtual impulse applies,
// and one non-virtual GetDirectionVector helper.
//
// DeformableObject::ApplyCarCarImpulse builds an ImpulseParams and hands it to ApplySensorImpulse,
// which forwards it to the body's ApplyLocalImpulse. The ImpulseParams member SEQUENCE + names +
// types are DWARF-authoritative -- every field below is verbatim from the DWARF struct
// (meImpulseDirection .. mbWorldContact). ImpulseParams is a standalone POD.

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

    // GetDirectionVector returns a unit body-axis vector by Vector3 value; it is declared with the
    // ENextSensorDirection enum already included above. Vector3 comes from BrnCommonTypes.h.

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

    // DWARF BrnCollidableBody.h:80. The canonical base of every body the deformation system applies
    // impulses to (DeformationSensor and VehicleRigidBody both derive it). Its console layout is just
    // the vptr -- it carries NO data members (DWARF lists only `_vptr.CollidableBody`). The two impulse
    // applies are PURE virtuals (each concrete body supplies its own routing); GetDirectionVector is a
    // non-virtual shared helper whose body lives in the future BrnCollidableBody.cpp TU (DWARF
    // BrnCollidableBody.cpp:43) and is DECLARED-ONLY here.
    //
    // HOMING NOTE: this REPLACES the minimal local CollidableBody copies that previously lived inside
    // the derived headers (BrnVehicleRigidBody.h). Derived classes now `#include` THIS header and
    // derive this canonical base -- do not fork a local copy.
    struct CollidableBody
    {
        // DWARF BrnCollidableBody.h:85 -- apply one fully-specified impulse to this body. Pure: each
        // concrete body (sensor / vehicle rigid body) routes the impulse into its own physics state.
        virtual void ApplyLocalImpulse(ImpulseParams* lpImpulseParams) = 0;

        // DWARF BrnCollidableBody.h:90 -- receive an impulse passed on from a neighbouring body in the
        // impulse-passing chain (the trailing VecFloat is the chain's remaining/scaled magnitude). Pure.
        virtual void RecievePassedOnImpulse(const ImpulseParams* lpImpulseParams, VecFloat lvfPassedMagnitude) = 0;

        // DWARF BrnCollidableBody.cpp:43 -- non-virtual helper returning the unit body-axis vector for a
        // given signed-axis direction. DECLARED-ONLY: its body is owned by the BrnCollidableBody TU.
        Vector3 GetDirectionVector(ENextSensorDirection leDirection);
    };
}
}
