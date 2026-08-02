#pragma once

// BrnPhysics::ExternalPhysicsBody -- an externally-simulated rigid body that also accumulates
// per-frame forces/torques/impulses and carries an inverse-inertia tensor, so the game can ask
// it for collision impulses. Derives from ExternallySimulatedBody. Minimal OWNING
// reconstruction: the member SEQUENCE is verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../PhysicsUtilities/ExternalPhysicsBody.h:241-249).
//
// LAYOUT (X360, confirmed against the asm of the four accumulator funcs below, and against
// ReadPropertiesFromRenderware @0x825A2280 which stores all three inertia/mass members):
//   mLocalInverseInertia @ +112 (0x70) (Matrix33)  -- begins right after the base
//   mWorldInverseInertia @ +160 (0xA0) (Matrix33)  -- `addi r11,this,0xA0`
//   mfMass                @ +208 (0xD0) (VecFloat)  -- `li r28,0xD0` / `stvx128 v0,r3,r28`
//   mTotalLinearForce    @ +224 (0xE0)  -- AddWorldSpaceForce          `addi r11,this,0xE0`
//   mTotalTorque         @ +240 (0xF0)  -- AddWorldSpaceTorque         `addi r11,this,0xF0`
//   mTotalLinearImpulse  @ +256 (0x100) -- AddWorldSpaceImpulse        `addi r11,this,0x100`
//   mTotalAngularImpulse @ +272 (0x110) -- AddWorldSpaceAngularImpulse `addi r11,this,0x110`
//
// The four +0xE0..+0x110 stride-16 offsets pin mTotalLinearForce/mTotalTorque/
// mTotalLinearImpulse/mTotalAngularImpulse to the exact DWARF member sequence. The two inertia
// matrices + mfMass fill +0x70..+0xE0 (0x70/0xA0/0xD0, confirmed by ReadPropertiesFromRenderware);
// their exact sub-offsets are not load-bearing for this group's funcs (only the inertia tensor's
// rows are read, via a caller-supplied pointer, in CalculateCollisionImpulseWithInanimateObject).
// Per project rule members are pinned BY NAME + SEQUENCE (no cross-pointer absolute-offset
// static_assert).

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Matrix33, VecFloat
#include "rw/physics/rigidbody.h"   // rw::physics::InputSpace
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
        // @0x8259C978. The X360 Hex-Rays dropped the arg list (rendered `int(...)`); the
        // parameter names/order below are recovered from the PS3 DecFIGS DWARF prototype
        // (._ZN10BrnPhysics19ExternalPhysicsBody44CalculateCollisionImpulseWithInanimateObject...
        //  @ PS3 0x68B130): (Vector3 lPoint, Vector3 lPointVel, Vector3 lCollisionNormal,
        //  VecFloat lvfRestitution, Vector3* lpImpulseOut, VecFloat* lpvfInvInertiaOut). lPoint
        // is the contact point relative to the centre of mass (the cross-product `r`),
        // lPointVel the point's relative velocity, lCollisionNormal the surface normal `n`.
        VecFloat CalculateCollisionImpulseWithInanimateObject(
            Vector3 lPoint, Vector3 lPointVel, Vector3 lCollisionNormal,
            VecFloat lvfRestitution, Vector3* lpImpulseOut, VecFloat* lpvfInvInertiaOut);

        // Solve the collision impulse between THIS body and another movable body -- the two-body
        // form used for car-on-car shunts (DeformableObject::ApplyCarCarImpulse calls it). Same
        // formula as the inanimate case but the effective inverse mass sums BOTH bodies' terms:
        //   j = -(1+e)(vRel . n) / ( 1/mA + 1/mB + n.((Ia^-1(rA x n)) x rA) + n.((Ib^-1(rB x n)) x rB) )
        // rA = lPoint1 (relative to this body's COM), rB = lPoint2 (relative to lBody2's COM),
        // vRel = lImpactVel, n = lCollisionNormal. Writes j*n to lpImpulseOut, and EACH body's
        // inverse effective-mass term to lpfInvInertiaAOut / lpfInvInertiaBOut (the caller splits
        // the impulse application between the two cars by these). Returns the impulse magnitude.
        // @0x8259CAE8. (Signature from the DecFIGS DWARF; X360 Hex-Rays dropped the arg list.)
        VecFloat CalculateCollisionImpulseWithBody(
            const ExternalPhysicsBody& lBody2, Vector3 lPoint1, Vector3 lPoint2,
            Vector3 lImpactVel, Vector3 lCollisionNormal, VecFloat lvfRestitution,
            Vector3* lpImpulseOut, VecFloat* lpfInvInertiaAOut, VecFloat* lpfInvInertiaBOut) const;

        // ADDITIVE GROW (Deformation car-car-impulse group): DECLARE-ONLY sibling. The velocity of a
        // body point given relative to the centre of mass, in the requested input frame
        // (= mLinearVelocity + mAngularVelocity x r). DeformableObject::ApplyCarCarImpulse calls it
        // BY NAME to get each car's contact-point velocity; its body is a separate TU, so only the
        // declaration is needed for the per-TU gate. Signature from the DecFIGS DWARF
        // (ExternalPhysicsBody::GetLocalVelocity(Vector3, rw::physics::InputSpace) const -> Vector3).
        Vector3 GetLocalVelocity(Vector3 lPoint, rw::physics::InputSpace leSpace) const;

        // Damp the angular velocity. DampenAngularVelocity applies one isotropic damping
        // curve; DampPitchYawRoll applies a separate per-axis (pitch/yaw/roll) curve. Both
        // scale mAngularVelocity in place by pow(dampPerSecond, dt)-style factors built from a
        // VMX exp2/log2 polynomial. @0x825B2CD8 / @0x825BE210.
        void DampenAngularVelocity(VecFloat lvfDampingPerSecond, VecFloat lvfDeltaTime); // h:192 @0x825B2CD8
        void DampPitchYawRoll(VecFloat lvfPitchDamping, VecFloat lvfYawDamping,
                              VecFloat lvfRollDamping, VecFloat lvfDeltaTime);            // h:200 @0x825BE210

        // ADDITIVE GROW (PhysicalBodyPartPool::UpdateRWBodies caller): DECLARE-ONLY.
        // Accumulate a force expressed in the body's LOCAL frame (rotated by the body's
        // current orientation before being added to the linear-force accumulator). The X360
        // UpdateRWBodies builds a local-space gravity force vector (0, KF_PART_EXTRA_GRAVITY,
        // 0, 0) scaled by a transform row and calls this on each detached part's body. Its body
        // lives in a separate (not-yet-homed) TU, so only the declaration is needed for the
        // per-TU `cl /c` gate. FLAG: signature reconstructed from the call site (a single
        // Vector3 force arg); the X360 Hex-Rays dropped the arg list.
        void AddLocalSpaceForce(Vector3 lvForce);

        // ⭐ THE INTEGRATOR -- the two functions the whole vehicle force model integrates through.
        // Both are now BODIED in ExternalPhysicsBody.cpp (they were declare-only/absent).

        // @0x825A1B10. Turn the four accumulators into a velocity change for this step, then
        // clear them:
        //     v += (F*dt + J) / m ;   omega += I_world^-1 * (T*dt + L)
        // ⚠️ SIGNATURE CORRECTION (2026-08-02): this was declared `void CalculateNewVelocity()`
        // behind a FLAG reading "void/void signature recovered from the call site". That was the
        // DROPPED-ARGUMENT STUB trap again. The X360 body multiplies mTotalLinearForce and
        // mTotalTorque by the incoming vector register v1 before adding the impulses -- v1 is the
        // timestep -- and the caller proves it: PhysicalBodyPart::UpdateRW @0x825E7998 stashes its
        // own v1 in v127 on entry and replays `vmr128 v1,v127` in the instruction PAIR immediately
        // before `bl CalculateNewVelocity`. A dt-less integrator silently integrates nothing.
        void CalculateNewVelocity(VecFloat lvfDeltaTime);

        // @0x825A7930. Advance the pose by the current velocities and re-orthonormalise:
        //     mTransform.wAxis += mLinearVelocity * dt
        //     each rotation row -= row x (mAngularVelocity * dt)      (the small-angle spin)
        //     mTransform = OrthoNormalize3x3(mTransform)
        //     CalculateWorldIntertia()
        // dt again arrives in the vector register v1 (`vmulfp128 v0, omega, v1`).
        void IntegrateTransform(VecFloat lvfDeltaTime);

        // @0x825A1E30. Rotate the LOCAL inverse-inertia tensor into world space:
        //     mWorldInverseInertia = R * mLocalInverseInertia * R^T,   R = mTransform's 3x3.
        // Run from Prepare, IntegrateTransform, ReadPropertiesFromChangeInertiaEvent and the two
        // VehiclePhysics update spines.
        void CalculateWorldIntertia();

        // @0x825A1670 / @0x825A1878. Accumulate a force / an impulse applied AT A POINT, with each
        // of the two vectors independently tagged WORLD_SPACE or BODY_SPACE.
        //   force/impulse: BODY_SPACE -> rotated into world by the transform's 3x3
        //   position:      WORLD_SPACE -> made relative to the centre of mass (pos - mTransform.wAxis)
        //                  BODY_SPACE  -> rotated into a world-space offset by the 3x3
        // then linear accumulator += v, angular accumulator += r x v.
        // ⚠️ NOTE for the VehiclePhysics re-parenting: VehiclePhysics.h currently declares its own
        // TWO-argument AddLocalForce/AddLocalImpulse on the flat struct. Those drop both InputSpace
        // tags. When VehiclePhysics is re-parented onto this chain those local declarations must be
        // deleted, not left to shadow these.
        void AddLocalForce(Vector3 lForce, rw::physics::InputSpace leForceSpace,
                           Vector3 lPosition, rw::physics::InputSpace lePositionSpace);
        void AddLocalImpulse(Vector3 lImpulse, rw::physics::InputSpace leImpulseSpace,
                             Vector3 lPosition, rw::physics::InputSpace lePositionSpace);

        // @0x825A1A80. The same space-resolution + `r x v` split as AddLocalImpulse, but the two
        // results are written out instead of accumulated (the caller shapes them before banking).
        void GetImpulsesFromLocalImpulse(Vector3 lImpulse, rw::physics::InputSpace leImpulseSpace,
                                         Vector3 lPosition, rw::physics::InputSpace lePositionSpace,
                                         Vector3* lpLinearImpulseOut,
                                         Vector3* lpAngularImpulseOut) const;

        // The lifecycle quartet -- @0x825A1598 / @0x825A15E0 / @0x825A78D0 / @0x825A1628. Each
        // chains to the ExternallySimulatedBody function of the same name and then zeroes the four
        // accumulators; Prepare additionally runs CalculateWorldIntertia and returns 1. (The `bl`
        // to the base's same-named function at the head of each is the console's own confirmation
        // that this class really derives from ExternallySimulatedBody.)
        void Construct();
        void Destruct();
        bool Prepare();
        void Release();

        void ReadPropertiesFromRenderware(const rw::physics::RigidBody* lpRigidBody);

        // ADDITIVE GROW (Deformation PhysicalBodyPart::Construct caller): DECLARE-ONLY. Set the body's
        // mass (mfMass @ +208). The deformation part Construct @0x825B4178 stores 5.0 into the body's
        // mass between ExternalPhysicsBody::Construct() and ::Prepare() (the lvlx/vspltw of a 5.0 stack
        // temp into +208). The authoritative body is owned by ExternalPhysicsBody's own TU; only the
        // declaration is needed for the per-TU `cl /c` gate. FLAG: signature from the Construct call
        // site (a single f32 mass arg).
        void SetMass(f32 lfMass);

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
