#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu::{IsValid, operator+, Dot, Cross, Mult, ...}

#include <cmath>   // std::pow (models the VMX exp2/log2 pow-curve in the damp funcs)

// BrnPhysics::ExternalPhysicsBody -- the 7 functions owned by the BrnPhysics-bodies group.
//
// The X360 build implements every one of these with VMX128/AltiVec inline assembly that does
// not exist on the x64 host. Per project convention (cf. BrnTagPoint.cpp) the bodies below are
// the DE-SIMD'd scalar/vector equivalents written against the reconstructed members BY NAME --
// no __asm. The four AddWorldSpace* accumulators and the two transform-space details are
// recovered store-for-store; the collision-impulse solver and the two angular-velocity dampers
// are reconstructed from their recovered DATA FLOW (the VMX poly-approximation internals -- the
// exp2/log2 pow curve and the per-lane select machinery -- are MODELLED, see the flags below).

namespace BrnPhysics
{
    namespace vpu = rw::math::vpu;

    // ---------------------------------------------------------------------------------------
    // AddWorldSpace{Force,Torque,Impulse,AngularImpulse}  @0x825BE710 / .9D0 / .8F8 / .EAA8
    //
    // Each is the same shape: assert the incoming world-space vector is finite, then add it
    // (vaddfp the stored accumulator by the argument and store back). The asm self-compares
    // each of the x/y/z lanes (`vspltw`+`vcmpeqfp.`) to detect NaN -- i.e. RwMathVPU::IsValid
    // -- and fires the matching "Bad <kind> added " assert on failure, but the add then runs
    // UNCONDITIONALLY (the assert is a non-gating tripwire). Store target confirmed by asm
    // `addi r11,this,0x{E0,F0,100,110}` -> mTotalLinearForce/mTotalTorque/mTotalLinearImpulse/
    // mTotalAngularImpulse. The accumulators carry xyz (force/torque/impulse have no w term).
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::AddWorldSpaceForce(Vector3 lvForce)
    {
        CGS_ASSERT(vpu::IsValid(lvForce), "Bad force added ");
        mTotalLinearForce = vpu::Add(mTotalLinearForce, lvForce);
    }

    void ExternalPhysicsBody::AddWorldSpaceTorque(Vector3 lvTorque)
    {
        CGS_ASSERT(vpu::IsValid(lvTorque), "Bad torque added ");
        mTotalTorque = vpu::Add(mTotalTorque, lvTorque);
    }

    void ExternalPhysicsBody::AddWorldSpaceImpulse(Vector3 lvImpulse)
    {
        CGS_ASSERT(vpu::IsValid(lvImpulse), "Bad impulse added ");
        mTotalLinearImpulse = vpu::Add(mTotalLinearImpulse, lvImpulse);
    }

    void ExternalPhysicsBody::AddWorldSpaceAngularImpulse(Vector3 lvImpulse)
    {
        CGS_ASSERT(vpu::IsValid(lvImpulse), "Bad angular impulse added ");
        mTotalAngularImpulse = vpu::Add(mTotalAngularImpulse, lvImpulse);
    }

    // ---------------------------------------------------------------------------------------
    // CalculateCollisionImpulseWithInanimateObject  @0x8259C978
    //
    // The textbook impulse magnitude for a body striking an immovable object at contact point
    // r (relative to the centre of mass) with surface normal n, relative velocity vRel and
    // restitution e:
    //
    //     j = -(1 + e) (vRel . n) / ( 1/m + n . ( (I^-1 (r x n)) x r ) )
    //
    // The recovered asm computes exactly this shape: a vmsum3fp dot of the normal with a
    // velocity vector, the cross/inverse-inertia/cross chain assembled through the vpermwi +
    // vnmsubfp sequence, a vrefp reciprocal of the effective mass, and three stores:
    //   * the inverse effective-mass scalar  -> *lpvfInvInertiaOut   (first store, asm r6)
    //   * the impulse magnitude / scaled-n    -> the VecFloat return  (asm r3)
    //   * the impulse vector  j*n             -> *lpvImpulseOut        (asm r5)
    // and asserts `lpvfInvInertiaOut != NULL` before writing it.
    //
    // FLAG (modelled, not bit-verified): the X360 Hex-Rays dropped the argument list (rendered
    // `int(...)`), so the arg->register threading was recovered from the PS3 DecFIGS DWARF
    // prototype (PS3 0x68B130): (Vector3 lPoint, Vector3 lPointVel, Vector3 lCollisionNormal,
    // VecFloat lvfRestitution, Vector3* lpImpulseOut, VecFloat* lpvfInvInertiaOut). The PS3 asm
    // shows lPointVel is the relative velocity (it subtracts the body velocity at this+0x30:
    // `vsubfp v2,v2,v0; lvx v0,this,0x30`), lPoint is the cross-product `r`, lCollisionNormal is
    // the surface normal `n`. The inverse inertia used is this body's mWorldInverseInertia. The
    // data flow, output set and assert match the asm; the precise VMX tensor multiply / reciprocal
    // refinement is the modelled I^-1 row-combination, not proven lane-for-lane.
    // ---------------------------------------------------------------------------------------
    VecFloat ExternalPhysicsBody::CalculateCollisionImpulseWithInanimateObject(
        Vector3 lPoint, Vector3 lPointVel, Vector3 lCollisionNormal,
        VecFloat lvfRestitution, Vector3* lpImpulseOut, VecFloat* lpvfInvInertiaOut)
    {
        CGS_ASSERT(lpvfInvInertiaOut != nullptr, "lpvfInvInertiaOut != NULL");

        const f32 lfRestitution = lvfRestitution.x;   // broadcast VecFloat -> scalar (de-modelled lane)

        // Angular term: I^-1 ( r x n ), then ( I^-1(rxn) ) x r, projected onto n.
        const Vector3 lvRxN = vpu::Cross(lPoint, lCollisionNormal);
        const Vector3 lvAngular = vpu::Add(
            vpu::Add(vpu::Mult(mWorldInverseInertia.xAxis, lvRxN.x),
                     vpu::Mult(mWorldInverseInertia.yAxis, lvRxN.y)),
            vpu::Mult(mWorldInverseInertia.zAxis, lvRxN.z));
        const Vector3 lvAngularAtContact = vpu::Cross(lvAngular, lPoint);

        // Effective inverse mass: 1/m + n . (angular term). m is stored as VecFloat (broadcast).
        const f32 lfInvMass = (mfMass.x != 0.0f) ? (1.0f / mfMass.x) : 0.0f;
        const f32 lfDenominator = lfInvMass + vpu::Dot(lCollisionNormal, lvAngularAtContact);
        const f32 lfInvDenominator = (lfDenominator != 0.0f) ? (1.0f / lfDenominator) : 0.0f;

        // Impulse magnitude and the impulse vector j*n.
        const f32 lfRelativeNormalSpeed = vpu::Dot(lPointVel, lCollisionNormal);
        const f32 lfImpulse = -(1.0f + lfRestitution) * lfRelativeNormalSpeed * lfInvDenominator;
        const Vector3 lvImpulse = vpu::Mult(lCollisionNormal, lfImpulse);

        // Stores (asm order): inverse-effective-mass scalar, then the impulse vector out.
        VecFloat lvfInvInertia; lvfInvInertia.x = lvfInvInertia.y = lvfInvInertia.z = lvfInvInertia.w = lfInvDenominator;
        *lpvfInvInertiaOut = lvfInvInertia;
        if (lpImpulseOut != nullptr)
            *lpImpulseOut = lvImpulse;

        VecFloat lvfResult; lvfResult.x = lvfResult.y = lvfResult.z = lvfResult.w = lfImpulse;
        return lvfResult;
    }

    // ---------------------------------------------------------------------------------------
    // CalculateCollisionImpulseWithBody  @0x8259CAE8
    //
    // The two-body collision impulse (the car-on-car shunt solver). Identical in shape to the
    // inanimate version above, but the effective inverse mass in the denominator sums BOTH
    // bodies' linear (1/m) and angular (n.((I^-1(r x n)) x r)) terms. The recovered asm computes,
    // per body, the I^-1 row-combination of (r x n), crosses it back with r, dots with n, adds
    // the two reciprocal masses, takes the reciprocal of the sum (vrefp + a Newton refinement)
    // and multiplies by the numerator -(1+e)(vRel.n). It stores each body's inverse
    // effective-mass term separately (asm: `1/mA + (lApart.n)` -> lpfInvInertiaAOut at r31,
    // `1/mB + (lBpart.n)` -> lpfInvInertiaBOut at r29) so the caller (ApplyCarCarImpulse) can
    // split the impulse application between the two cars; j*n -> lpImpulseOut (r27); and returns
    // the impulse magnitude (r28). The DecFIGS body hint (ExternalPhysicsBody.cpp:585-618) names
    // lvfNumerator/lvfDenominator/lvfMassA/lvfMassB/lvfOneOverMassA/lvfOneOverMassB/lApart/lBpart.
    //
    // FLAG (modelled, not bit-verified): the VMX `vrefp` reciprocal + its Newton-Raphson
    // refinement (vnmsubfp/vmaddfp) is reproduced as a plain `1.0f / x` (same modelling as the
    // inanimate solver above); the per-lane permute/select machinery is de-SIMD'd to scalar/
    // Vector3 arithmetic. The data flow, output set and the two asserts match the asm.
    // ---------------------------------------------------------------------------------------
    VecFloat ExternalPhysicsBody::CalculateCollisionImpulseWithBody(
        const ExternalPhysicsBody& lBody2, Vector3 lPoint1, Vector3 lPoint2,
        Vector3 lImpactVel, Vector3 lCollisionNormal, VecFloat lvfRestitution,
        Vector3* lpImpulseOut, VecFloat* lpfInvInertiaAOut, VecFloat* lpfInvInertiaBOut) const
    {
        CGS_ASSERT(lpfInvInertiaAOut != nullptr, "lpfInvInertiaAOut != NULL");
        CGS_ASSERT(lpfInvInertiaBOut != nullptr, "lpfInvInertiaBOut != NULL");

        const f32 lfRestitution = lvfRestitution.x;   // broadcast VecFloat -> scalar

        // Numerator: -(1 + e)(vRel . n).
        const f32 lfClosingSpeed = vpu::Dot(lImpactVel, lCollisionNormal);
        const f32 lfNumerator    = -(1.0f + lfRestitution) * lfClosingSpeed;

        // Body A angular coupling: n . ( (Ia^-1 (rA x n)) x rA ),  rA = lPoint1, Ia^-1 = this body.
        const Vector3 lvRAxN = vpu::Cross(lPoint1, lCollisionNormal);
        const Vector3 lvAngularA = vpu::Add(
            vpu::Add(vpu::Mult(mWorldInverseInertia.xAxis, lvRAxN.x),
                     vpu::Mult(mWorldInverseInertia.yAxis, lvRAxN.y)),
            vpu::Mult(mWorldInverseInertia.zAxis, lvRAxN.z));
        const Vector3 lApart = vpu::Cross(lvAngularA, lPoint1);
        const f32 lfAngularA = vpu::Dot(lCollisionNormal, lApart);

        // Body B angular coupling: rB = lPoint2, Ib^-1 = lBody2's world inverse inertia.
        const Vector3 lvRBxN = vpu::Cross(lPoint2, lCollisionNormal);
        const Vector3 lvAngularB = vpu::Add(
            vpu::Add(vpu::Mult(lBody2.mWorldInverseInertia.xAxis, lvRBxN.x),
                     vpu::Mult(lBody2.mWorldInverseInertia.yAxis, lvRBxN.y)),
            vpu::Mult(lBody2.mWorldInverseInertia.zAxis, lvRBxN.z));
        const Vector3 lBpart = vpu::Cross(lvAngularB, lPoint2);
        const f32 lfAngularB = vpu::Dot(lCollisionNormal, lBpart);

        // Inverse masses (m stored as VecFloat, broadcast).
        const f32 lfOneOverMassA = (mfMass.x != 0.0f)        ? (1.0f / mfMass.x)        : 0.0f;
        const f32 lfOneOverMassB = (lBody2.mfMass.x != 0.0f) ? (1.0f / lBody2.mfMass.x) : 0.0f;

        // Effective inverse mass (both bodies) -> impulse magnitude.
        const f32 lfDenominator    = lfOneOverMassA + lfOneOverMassB + lfAngularA + lfAngularB;
        const f32 lfInvDenominator = (lfDenominator != 0.0f) ? (1.0f / lfDenominator) : 0.0f;
        const f32 lfImpVal         = lfNumerator * lfInvDenominator;

        // Outputs: each body's inverse effective-mass term, then the impulse vector j*n.
        const f32 lfInvInertiaA = lfOneOverMassA + lfAngularA;
        const f32 lfInvInertiaB = lfOneOverMassB + lfAngularB;
        VecFloat lvfInvInertiaA; lvfInvInertiaA.x = lvfInvInertiaA.y = lvfInvInertiaA.z = lvfInvInertiaA.w = lfInvInertiaA;
        VecFloat lvfInvInertiaB; lvfInvInertiaB.x = lvfInvInertiaB.y = lvfInvInertiaB.z = lvfInvInertiaB.w = lfInvInertiaB;
        *lpfInvInertiaAOut = lvfInvInertiaA;
        *lpfInvInertiaBOut = lvfInvInertiaB;
        if (lpImpulseOut != nullptr)
            *lpImpulseOut = vpu::Mult(lCollisionNormal, lfImpVal);

        VecFloat lvfResult; lvfResult.x = lvfResult.y = lvfResult.z = lvfResult.w = lfImpVal;
        return lvfResult;
    }

    // ---------------------------------------------------------------------------------------
    // DampenAngularVelocity  @0x825B2CD8   /   DampPitchYawRoll  @0x825BE210
    //
    // Both scale mAngularVelocity in place (asm `addi r11,this,0x50` -> mAngularVelocity load,
    // poly chain, store back). The poly chain is the EARenderWare VMX pow(base, exp) lane
    // approximation: vlogefp (log2) + a Chebyshev-style polynomial (coefficient tables at
    // 0x82014AC0..0x82014AF0) + vexptefp (exp2), combined per axis. The net effect is a
    // frame-rate-correct exponential decay of the angular velocity:
    //
    //     omega *= pow(dampingPerSecond, dt)
    //
    // DampenAngularVelocity uses one isotropic damping coefficient for all three axes;
    // DampPitchYawRoll uses a separate coefficient per body axis (pitch=x, yaw=y, roll=z).
    //
    // FLAG (modelled, not bit-verified): the exact per-lane select machinery and the polynomial
    // coefficients (unk_82014A* / unk_82FB9AF0) are NOT reproduced -- they are the SDK's
    // internal pow() approximation, with no project home. The recovered DATA FLOW (load
    // mAngularVelocity, raise a damping base to the dt power per axis, multiply, store back) is
    // reproduced with std::pow. Faithful in behaviour and store target; the bit pattern of the
    // approximation is intentionally NOT fabricated.
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::DampenAngularVelocity(VecFloat lvfDampingPerSecond, VecFloat lvfDeltaTime)
    {
        const f32 lfFactor = std::pow(lvfDampingPerSecond.x, lvfDeltaTime.x);
        mAngularVelocity = vpu::Mult(mAngularVelocity, lfFactor);
    }

    void ExternalPhysicsBody::DampPitchYawRoll(VecFloat lvfPitchDamping, VecFloat lvfYawDamping,
                                               VecFloat lvfRollDamping, VecFloat lvfDeltaTime)
    {
        const f32 lfDt = lvfDeltaTime.x;
        mAngularVelocity.x *= std::pow(lvfPitchDamping.x, lfDt);   // pitch about body x
        mAngularVelocity.y *= std::pow(lvfYawDamping.x,   lfDt);   // yaw   about body y
        mAngularVelocity.z *= std::pow(lvfRollDamping.x,  lfDt);   // roll  about body z
    }

    // ---------------------------------------------------------------------------------------
    // ReadPropertiesFromRenderware  @0x825A2280  (ledger TU SDKs/EATech/include/rw/math/vpu/
    // detail/matrix33_type_inline.h -- a DWARF file-attribution artifact; this is the declared
    // home). Called by BrnPhysics::Deformation::PhysicalBodyPart::Update.
    //
    // Mirrors the RW rigid body's mass/inertia properties back into this body. X360 asm walk:
    //   * mLocalInverseInertia (this+0x70): the three basis rows (gIVector / {0,1,0,0} /
    //     {0,0,1,0}, rodata-verified) each scaled by the matching splatted lane of the
    //     body's LOCAL inverse-inertia diagonal (reached through the pointer the console
    //     packs into the rigid body's +0x5C scalar lane) == diag(localInvInertia).
    //   * mfMass (this+0xD0): splat(1.0f / rigidBody.mInvm)  (flt_82001C98 == 1.0 verified;
    //     mInvm is the inverse mass packed in mIfull.w) == the body's MASS, matching the raw
    //     5.0f SetMass store the deformation Construct makes into the same member.
    //   * mWorldInverseInertia (this+0xA0): reassemble the symmetric tensor from the two
    //     packed vectors mIfull == {Ixx,Ixy,Ixz|invm} and mIsplt == {Izz,Iyy,Iyz|state}:
    //       row0 = mIfull (raw 16-byte store; the dead w lane rides along)
    //       row1 = vperm mask 0x8208C2B0 == {A.y, B.y, B.z, A.w} == {Ixy, Iyy, Iyz, invm}
    //       row2 = vperm mask 0x8208C2A0 == {A.z, B.z, B.x, A.w} == {Ixz, Iyz, Izz, invm}
    //     (masks read from the decrypted XEX; the per-lane f32 assembly below is exact).
    // ---------------------------------------------------------------------------------------
    void ExternalPhysicsBody::ReadPropertiesFromRenderware(const rw::physics::RigidBody* lpRigidBody)
    {
        // Local inverse inertia: the diagonal matrix of the body's local diagonal vector.
        const vpu::Vector4& lvLocalDiag = *lpRigidBody->GetLocalInvInertiaDiagonal();
        mLocalInverseInertia.xAxis = Vector3{ lvLocalDiag.x, 0.0f, 0.0f, 0.0f };
        mLocalInverseInertia.yAxis = Vector3{ 0.0f, lvLocalDiag.y, 0.0f, 0.0f };
        mLocalInverseInertia.zAxis = Vector3{ 0.0f, 0.0f, lvLocalDiag.z, 0.0f };

        // Mass: 1 / the body's packed inverse mass, splatted across the lane register.
        const f32 lfMass = 1.0f / lpRigidBody->GetInverseMass();
        mfMass.x = mfMass.y = mfMass.z = mfMass.w = lfMass;

        // World inverse inertia: the symmetric tensor from the two packed vectors (the
        // dead w lanes carry the packed scalars along, exactly as the X360 row stores do).
        const vpu::Vector4& lvA = lpRigidBody->GetInertiaFull();    // {Ixx, Ixy, Ixz | invm}
        const vpu::Vector4& lvB = lpRigidBody->GetInertiaSplit();   // {Izz, Iyy, Iyz | state}
        mWorldInverseInertia.xAxis = Vector3{ lvA.x, lvA.y, lvA.z, lvA.w };
        mWorldInverseInertia.yAxis = Vector3{ lvA.y, lvB.y, lvB.z, lvA.w };
        mWorldInverseInertia.zAxis = Vector3{ lvA.z, lvB.z, lvB.x, lvA.w };
    }
}
