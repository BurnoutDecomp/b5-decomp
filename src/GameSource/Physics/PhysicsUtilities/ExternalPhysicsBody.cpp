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
    // FLAG (modelled, not bit-verified): Hex-Rays dropped the argument list (rendered `()`),
    // so the exact arg->register threading and the inverse-inertia tensor application are
    // reconstructed from the DWARF prototype + the standard formula above rather than proven
    // lane-for-lane. The inverse inertia used is this body's mWorldInverseInertia. The data
    // flow, output set and assert match the asm; the precise tensor multiply is the modelled
    // I^-1 row-combination.
    // ---------------------------------------------------------------------------------------
    VecFloat ExternalPhysicsBody::CalculateCollisionImpulseWithInanimateObject(
        Vector3 lvNormal, Vector3 lvContactPointRel, Vector3 lvRelativeVelocity,
        VecFloat lvfRestitution, Vector3* lpvImpulseOut, VecFloat* lpvfInvInertiaOut)
    {
        CGS_ASSERT(lpvfInvInertiaOut != nullptr, "lpvfInvInertiaOut != NULL");

        const f32 lfRestitution = lvfRestitution.x;   // broadcast VecFloat -> scalar (de-modelled lane)

        // Angular term: I^-1 ( r x n ), then ( I^-1(rxn) ) x r, projected onto n.
        const Vector3 lvRxN = vpu::Cross(lvContactPointRel, lvNormal);
        const Vector3 lvAngular = vpu::Add(
            vpu::Add(vpu::Mult(mWorldInverseInertia.xAxis, lvRxN.x),
                     vpu::Mult(mWorldInverseInertia.yAxis, lvRxN.y)),
            vpu::Mult(mWorldInverseInertia.zAxis, lvRxN.z));
        const Vector3 lvAngularAtContact = vpu::Cross(lvAngular, lvContactPointRel);

        // Effective inverse mass: 1/m + n . (angular term). m is stored as VecFloat (broadcast).
        const f32 lfInvMass = (mfMass.x != 0.0f) ? (1.0f / mfMass.x) : 0.0f;
        const f32 lfDenominator = lfInvMass + vpu::Dot(lvNormal, lvAngularAtContact);
        const f32 lfInvDenominator = (lfDenominator != 0.0f) ? (1.0f / lfDenominator) : 0.0f;

        // Impulse magnitude and the impulse vector j*n.
        const f32 lfRelativeNormalSpeed = vpu::Dot(lvRelativeVelocity, lvNormal);
        const f32 lfImpulse = -(1.0f + lfRestitution) * lfRelativeNormalSpeed * lfInvDenominator;
        const Vector3 lvImpulse = vpu::Mult(lvNormal, lfImpulse);

        // Stores (asm order): inverse-effective-mass scalar, then the impulse vector out.
        VecFloat lvfInvInertia; lvfInvInertia.x = lvfInvInertia.y = lvfInvInertia.z = lvfInvInertia.w = lfInvDenominator;
        *lpvfInvInertiaOut = lvfInvInertia;
        if (lpvImpulseOut != nullptr)
            *lpvImpulseOut = lvImpulse;

        VecFloat lvfResult; lvfResult.x = lvfResult.y = lvfResult.z = lvfResult.w = lfImpulse;
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
}
