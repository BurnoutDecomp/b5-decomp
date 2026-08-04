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
// ⭐ 2026-08-04 -- THE BLOCKER THAT KEPT THIS TU OUT OF THE LINK IS RETIRED, AND IT IS MOUNTED.
// The note that stood here said GetLocalInvInertiaDiagonal() "cannot be bodied: the console
// stores that pointer INSIDE the rigid body's mUp.w float lane ... and the committed 64-bit
// rigidbody.h layout types that offset as a float". That was true of the OLD layout. The rw
// physics landing promoted all ten packed console scalars -- including the Inertia pointer in
// the mUp.w lane -- to real named members of RigidBody, so the accessor is now an ordinary
// member read and this TU links.
//
// This function is still not on the vehicle path: its only caller,
// BrnPhysics::Deformation::PhysicalBodyPart::Update, lives in the unmounted
// BrnPhysicalBodyPart.cpp (16 unresolved of its own). Mounting therefore adds a body to the
// image without changing a single frame -- it is here for link closure.
//
// The TU split itself is now BUILD-MECHANICS-ONLY history: this body is byte-identical to the
// one that used to sit at the tail of ExternalPhysicsBody.cpp, and its declared home is still
// ExternalPhysicsBody.h. Folding it back is a pure code move and is deliberately left alone.
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
        const vpu::Vector3& lvLocalDiag = lpRigidBody->GetLocalInvInertiaDiagonal();
        mLocalInverseInertia.xAxis = Vector3{ lvLocalDiag.x, 0.0f, 0.0f, 0.0f };
        mLocalInverseInertia.yAxis = Vector3{ 0.0f, lvLocalDiag.y, 0.0f, 0.0f };
        mLocalInverseInertia.zAxis = Vector3{ 0.0f, 0.0f, lvLocalDiag.z, 0.0f };

        // Mass: 1 / the body's packed inverse mass, splatted across the lane register.
        const f32 lfMass = 1.0f / lpRigidBody->GetInverseMass();
        mfMass.x = mfMass.y = mfMass.z = mfMass.w = lfMass;

        // World inverse inertia: the symmetric tensor from the two packed vectors. The three
        // X360 row stores are whole-register moves, so each row's w lane carries the console's
        // mIfull.w -- which is the INVERSE MASS. On the PC that scalar is its own member, so
        // the lane is written from GetInverseMass() by name instead of riding along in .w.
        const vpu::Vector4& lvA = lpRigidBody->GetInertiaFull();    // {Ixx, Ixy, Ixz}
        const vpu::Vector4& lvB = lpRigidBody->GetInertiaSplit();   // {Izz, Iyy, Iyz}
        const f32 lfInvm = lpRigidBody->GetInverseMass();           // console mIfull.w
        mWorldInverseInertia.xAxis = Vector3{ lvA.x, lvA.y, lvA.z, lfInvm };
        mWorldInverseInertia.yAxis = Vector3{ lvA.y, lvB.y, lvB.z, lfInvm };
        mWorldInverseInertia.zAxis = Vector3{ lvA.z, lvB.z, lvB.x, lfInvm };
    }
}
