#pragma once

// BrnPhysics::ExternalPhysicsBody -- an externally-simulated rigid body that also accumulates
// per-frame forces/torques/impulses and carries an inverse-inertia tensor, so the game can ask
// it for collision impulses. Derives from ExternallySimulatedBody. Minimal OWNING
// reconstruction: the member SEQUENCE is verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../PhysicsUtilities/ExternalPhysicsBody.h:241-249).
//
// LAYOUT (X360, confirmed against the asm of the four accumulator funcs below):
//   mLocalInverseInertia @ +128 (Matrix33)   -- begins right after the 128-byte base
//   mWorldInverseInertia              (Matrix33)
//   mfMass                            (VecFloat)
//   mTotalLinearForce    @ +224 (0xE0)  -- AddWorldSpaceForce          `addi r11,this,0xE0`
//   mTotalTorque         @ +240 (0xF0)  -- AddWorldSpaceTorque         `addi r11,this,0xF0`
//   mTotalLinearImpulse  @ +256 (0x100) -- AddWorldSpaceImpulse        `addi r11,this,0x100`
//   mTotalAngularImpulse @ +272 (0x110) -- AddWorldSpaceAngularImpulse `addi r11,this,0x110`
//
// The four +0xE0..+0x110 stride-16 offsets pin mTotalLinearForce/mTotalTorque/
// mTotalLinearImpulse/mTotalAngularImpulse to the exact DWARF member sequence. The two inertia
// matrices + mfMass fill +128..+224; their exact sub-offsets are not load-bearing for this
// group's funcs (only the inertia tensor's rows are read, via a caller-supplied pointer, in
// CalculateCollisionImpulseWithInanimateObject). Per project rule members are pinned BY NAME +
// SEQUENCE (no cross-pointer absolute-offset static_assert).

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Matrix33, VecFloat
#include "GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h"

namespace BrnPhysics
{
    class ExternalPhysicsBody : public ExternallySimulatedBody
    {
    public:
        // --- BrnPhysics-bodies group's 7 ledger functions (bodied in ExternalPhysicsBody.cpp) ---

        // Accumulate a world-space force/torque/(linear)impulse/angular-impulse. Each asserts
        // the input vector is finite (RwMathVPU::IsValid) then adds it to the matching
        // accumulator (vaddfp the member by the input). @0x825BE710 / .9D0 / .8F8 / .EAA8.
        void AddWorldSpaceForce(Vector3 lvForce);            // h:87  @0x825BE710
        void AddWorldSpaceTorque(Vector3 lvTorque);          // h:93  @0x825BE9D0
        void AddWorldSpaceImpulse(Vector3 lvImpulse);        // h:90  @0x825BE8F8
        void AddWorldSpaceAngularImpulse(Vector3 lvImpulse); // h:96  @0x825BEAA8

        // Solve the collision impulse for a hit against an immovable object:
        //   j = -(1+e)(vRel . n) / ( 1/m + n . ((I^-1 (r x n)) x r) )
        // writes the impulse vector, the inverse effective mass scalar, and j*n out-params.
        // @0x8259C978 (DWARF: VecFloat CalculateCollisionImpulseWithInanimateObject(
        //   Vector3 lvNormal, Vector3 lvContactPointRel, Vector3 lvRelativeVelocity,
        //   VecFloat lvfRestitution, Vector3* lpvImpulseOut, VecFloat* lpvfInvInertiaOut)).
        VecFloat CalculateCollisionImpulseWithInanimateObject(
            Vector3 lvNormal, Vector3 lvContactPointRel, Vector3 lvRelativeVelocity,
            VecFloat lvfRestitution, Vector3* lpvImpulseOut, VecFloat* lpvfInvInertiaOut);

        // Damp the angular velocity. DampenAngularVelocity applies one isotropic damping
        // curve; DampPitchYawRoll applies a separate per-axis (pitch/yaw/roll) curve. Both
        // scale mAngularVelocity in place by pow(dampPerSecond, dt)-style factors built from a
        // VMX exp2/log2 polynomial. @0x825B2CD8 / @0x825BE210.
        void DampenAngularVelocity(VecFloat lvfDampingPerSecond, VecFloat lvfDeltaTime); // h:192 @0x825B2CD8
        void DampPitchYawRoll(VecFloat lvfPitchDamping, VecFloat lvfYawDamping,
                              VecFloat lvfRollDamping, VecFloat lvfDeltaTime);            // h:200 @0x825BE210

    protected:
        Matrix33 mLocalInverseInertia;   // :241
        Matrix33 mWorldInverseInertia;   // :242
        VecFloat mfMass;                 // :243
        Vector3  mTotalLinearForce;      // :246  (+0xE0)
        Vector3  mTotalTorque;           // :247  (+0xF0)
        Vector3  mTotalLinearImpulse;    // :248  (+0x100)
        Vector3  mTotalAngularImpulse;   // :249  (+0x110)
    };
}
