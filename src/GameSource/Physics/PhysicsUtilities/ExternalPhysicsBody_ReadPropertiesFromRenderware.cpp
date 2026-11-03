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
        static const CGS_ALIGN(16) f32 kX[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        static const CGS_ALIGN(16) f32 kY[4] = {0.0f, 1.0f, 0.0f, 0.0f};
        static const CGS_ALIGN(16) f32 kZ[4] = {0.0f, 0.0f, 1.0f, 0.0f};
        static const CGS_ALIGN(16) u32 kRowY[4] = {0x04050607, 0x14151617, 0x18191A1B, 0x0C0D0E0F};
        static const CGS_ALIGN(16) u32 kRowZ[4] = {0x08090A0B, 0x18191A1B, 0x10111213, 0x0C0D0E0F};
        const __vector4 diagonal = lpRigidBody->GetLocalInvInertiaDiagonal().mV;
        mLocalInverseInertia.xAxis = __vmulfp(__lvx(kX, 0), __vspltw(diagonal, 0));
        mLocalInverseInertia.yAxis = __vmulfp(__lvx(kY, 0), __vspltw(diagonal, 1));
        mLocalInverseInertia.zAxis = __vmulfp(__lvx(kZ, 0), __vspltw(diagonal, 2));
        const f32 mass = 1.0f / lpRigidBody->GetInverseMass();
        mfMass.mV = __vspltw(__lvlx(&mass, 0), 0);
        const __vector4 full = __lvx(&lpRigidBody->GetInertiaFull(), 0);
        const __vector4 split = __lvx(&lpRigidBody->GetInertiaSplit(), 0);
        mWorldInverseInertia.xAxis = full;
        mWorldInverseInertia.yAxis = __vperm(full, split, __lvx(kRowY, 0));
        mWorldInverseInertia.zAxis = __vperm(full, split, __lvx(kRowZ, 0));
    }

}
