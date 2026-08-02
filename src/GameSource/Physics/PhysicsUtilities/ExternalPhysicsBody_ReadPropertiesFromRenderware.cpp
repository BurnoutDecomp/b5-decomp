#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu::{IsValid, Mult, ...}

// ============================================================================================
// BrnPhysics::ExternalPhysicsBody::ReadPropertiesFromRenderware @0x825A2280 -- SPLIT OUT of
// ExternalPhysicsBody.cpp on 2026-08-02 (physics wave 3). BUILD-MECHANICS SPLIT ONLY: the body
// below is byte-identical to the one that used to sit at the tail of that file, and this
// function's declared home is still ExternalPhysicsBody.h / the DWARF's matrix33_type_inline.h
// attribution.
//
// ⛔ THIS TU IS DELIBERATELY **NOT MOUNTED** in tools/build/build_game_exe.bat.
// It calls rw::physics::RigidBody::GetLocalInvInertiaDiagonal(), which is declared-only and
// cannot be bodied: the console stores that pointer INSIDE the rigid body's mUp.w float lane
// (the caller loads it as `lwz +0x5C`), and the committed 64-bit rigidbody.h layout types that
// offset as a float. Widening it would retype a vendor home that all eleven lane-registers'
// packing depends on, and fabricating a value is forbidden. So the reference stays unresolved
// and this one function stays out of the link.
//
// Everything else in ExternalPhysicsBody -- the four world-space accumulators, AddLocalForce /
// AddLocalImpulse / GetImpulsesFromLocalImpulse, CalculateWorldIntertia, IntegrateTransform,
// CalculateNewVelocity and the lifecycle quartet, i.e. the whole integrator -- IS mounted.
// This function is not on the vehicle path (its only caller is
// BrnPhysics::Deformation::PhysicalBodyPart::Update).
//
// TO RE-MERGE: body GetLocalInvInertiaDiagonal in src/vendor/renderware/physics/RigidBody.cpp
// once the packed lane has a PC-side storage answer, then move this body back and delete the TU.
// ============================================================================================

namespace BrnPhysics
{
    namespace vpu = rw::math::vpu;

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
