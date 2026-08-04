// ExternallySimulatedBody embed/compile check. Pulls in the (grown) home + the RigidBody
// vendor type so the gate compiles the header + the ReadFromRenderware body together.
//
// Layout asserts UPGRADED (2026-08-02). The note that stood here said the offsets "are NOT
// reproduced on x64" because the committed home was polymorphic on the host. That polymorphism
// was the bug, not a platform fact: the console class has no vptr (see the LAYOUT note in the
// header -- Construct @0x8259CF28 stores the identity rows at this+0x00..0x30, the two zero
// velocity registers at +0x40/+0x50 and the zero byte at +0x60). With the spurious `virtual`
// removed the class is entirely pointer-free, so every console offset DOES reproduce exactly on
// x64 and can be asserted outright.
//
// ⚠️ These offsets are NOT taken from a console byte-size literal and NOT copied from a header
// comment -- each was derived independently from the X360 asm (Construct's store set; the
// "IsValid(mLinearVelocity)"/"IsValid(mAngularVelocity)" assert pair in CalculateNewVelocity
// @0x825A1B10 naming this+0x40 and this+0x50; and ExternalPhysicsBody's own members continuing
// at +0x70). If a future finding moves one of them, fix the CLASS, not the assert.

#include "GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h"
#include <cstddef>       // offsetof
#include <type_traits>   // std::is_polymorphic

static_assert(sizeof(Matrix44Affine) == 64, "Matrix44Affine == 4 x 16-byte rows");
static_assert(sizeof(Vector3) == 16,        "Vector3 == one 16-byte lane register");

// The console frame of ExternallySimulatedBody itself (NOT the derived frame -- inside a
// SimpleVehiclePhysics/VehiclePhysics object every one of these sits 0x10 higher, behind the
// leaf's vptr). The members are protected, so the offsets are taken through an empty derived
// probe (which adds nothing and therefore shares the base's frame exactly).
static_assert(!std::is_polymorphic<BrnPhysics::ExternallySimulatedBody>::value,
              "ExternallySimulatedBody is non-polymorphic on the console (no vptr at +0)");
static_assert(sizeof(BrnPhysics::ExternallySimulatedBody) == 0x70,
              "sizeof == 0x70 -- ExternalPhysicsBody's mLocalInverseInertia continues at +0x70");

namespace
{
    struct EsbLayoutProbe : public BrnPhysics::ExternallySimulatedBody
    {
        using BrnPhysics::ExternallySimulatedBody::mTransform;
        using BrnPhysics::ExternallySimulatedBody::mLinearVelocity;
        using BrnPhysics::ExternallySimulatedBody::mAngularVelocity;
        using BrnPhysics::ExternallySimulatedBody::mbFrozen;
    };

    static_assert(sizeof(EsbLayoutProbe) == sizeof(BrnPhysics::ExternallySimulatedBody),
                  "the probe adds nothing, so it shares the base's frame");
    static_assert(offsetof(EsbLayoutProbe, mTransform)       == 0x00,
                  "mTransform @ +0x00   (Construct @0x8259CF28 stores the identity rows at r3+0/0x10/0x20/0x30)");
    static_assert(offsetof(EsbLayoutProbe, mLinearVelocity)  == 0x40,
                  "mLinearVelocity @ +0x40  (CalculateNewVelocity asserts IsValid(mLinearVelocity) on this+0x40)");
    static_assert(offsetof(EsbLayoutProbe, mAngularVelocity) == 0x50,
                  "mAngularVelocity @ +0x50 (CalculateNewVelocity asserts IsValid(mAngularVelocity) on this+0x50)");
    static_assert(offsetof(EsbLayoutProbe, mbFrozen)         == 0x60,
                  "mbFrozen @ +0x60  (Construct's `stb r11,0x60(r3)`; ReadFromRenderware's `lbz r11,0x60`)");
}

// =====================================================================================
// rw::physics::RigidBody LAYOUT TRIPWIRE.
//
// ⚠️⚠️ THE ASSERT THAT USED TO STAND HERE WAS GREEN *BECAUSE OF* A DEFECT, AND WOULD HAVE
// REJECTED THE FIX. It read:
//     static_assert(sizeof(rw::physics::RigidBody) == 11 * 16, ...)
// 176 is the CONSOLE size. It only held while all ten packed console scalars were modelled
// as float lanes of the eleven pose registers -- including FIVE that are pointers. The
// moment those became real x64 members (which they had to, because a 64-bit pointer does not
// fit in a float lane) the host type grew and the assert would have fired. The shipping x64
// build settles which side is right: on Xbox One `mOmega` moved +0x30 -> +0x38 precisely
// because the intrusive `next` widened 4 -> 8, so a faithful x64 RigidBody MUST be larger
// than 176 bytes. The assert was the thing that was wrong.
//
// ⭐ THIS GATE IS CONSOLE ARITHMETIC, NOT HOST `offsetof` -- the
// BrnPhysicsModule_layout_check.cpp / VehiclePhysics_layout_check.cpp pattern, and for the
// same reason: the host type is DELIBERATELY not byte-identical (five widened pointers), so a
// host offsetof would either be false or a tautology. What is asserted instead is the thing
// that was actually recovered: **walking the DWARF member sequence (rigidbody.h:419..459)
// with the console's own widths reproduces every independently asm-literal ANCHOR, and closes
// exactly on 0xB0.** Every right-hand side below is an X360 literal quoted from an
// instruction; none is computed from the left-hand side.
//
// THE ANCHORS, each quoted from a named instruction:
//   +0x1C  `lwz r11,0x1C(r3)` then used AS A BASE (`addi r28,r11,0x10`)   DynamicUpdate
//   +0x20  `lvx r4+0x20 -> this+0x40`                    ExternallySimulatedBody::ReadFromRenderware
//   +0x2C / +0x3C  the doubly-linked unlink                    Simulation::RemoveRigidBody @0x82BC2950
//   +0x30  `lvx r4+0x30 -> this+0x50`                    ExternallySimulatedBody::ReadFromRenderware
//   +0x40/+0x50/+0x60  the basis rows                                  RigidBody::GetTransform
//   +0x4C  `lwz r10,0x4C(r3)` then `lfs f0,0xA0(r10)`     DynamicUpdate  (the Simulation)
//   +0x5C  `lwz r30,0x5C(r3)` then `lfs f0,0x1C(r30)`     DynamicUpdate  (the Inertia)
//   +0x70/+0x80  the two packed inertia vectors      ExternalPhysicsBody::ReadPropertiesFromRenderware
//   +0x7C  `lfs f11,0x7C(r3)` (the inverse mass)          DynamicUpdate @0x82BC2F20
//   +0x8C  `lwz r11,0x8C(r4)` + `clrlwi r11,r11,29`       Simulation::RemoveRigidBody @0x82BC2988
//   +0x90 / +0xA0  `addi r6,r3,0x90` / `addi r4,r3,0xA0`  DynamicUpdate @0x82BC2B88 / 0x82BC2B98
//   +0x9C  `lfs f13,0x9C(r3)` / `stfs f0,0x9C(r3)`        DynamicUpdate @0x82BC2F38 / 0x82BC2F6C
//   +0xAC  `lwz r9,0xAC(r3)` / `stw r10,0xAC(r3)`         DynamicUpdate @0x82BC2F44 / 0x82BC2F68
//   0xB0   RigidBody::operator= @0x825E3410 copies +0x00..+0xAC and stops
//
// ⚠️ THE BLIND SPOT, stated rather than hidden. This gate measures the RECOVERED CONSOLE
// layout, so it cannot see a host-side regression (someone re-packing a scalar back into a
// float lane on x64 would not trip it). What guards that is PART 2 below -- named-member
// existence checks through the accessors, which fail to compile if a member is deleted or
// renamed -- plus the fact that a pointer in a float lane is a compile error on x64 anyway.
//
// TAMPER-TESTED 2026-08-04, six cases, ALL SIX FIRE:
//   FIRES  KU_VEC = 12 -> 16 in the walk (i.e. "the scalars are separate 16-byte fields")
//   FIRES  drop mId from the walk (mVel then lands at +0x1C, breaking six later anchors)
//   FIRES  swap mIfull and mIsplt
//   FIRES  mState widened to 8 (the run closes on 0xB4, not 0xB0)
//   FIRES  move mTag before mAt
//   FIRES  delete the GetInverseMass accessor (PART 2 stops compiling)
// =====================================================================================
namespace
{
    // PART 1 -- CONSOLE arithmetic. Pure integer maths over the DWARF sequence with the
    // console's own widths; it does not touch the host type at all.
    namespace RigidBodyX360Layout
    {
        const std::size_t KU_VEC = 12;   // Vector3U_32 -- the PACKED 12-byte triple
        const std::size_t KU_S   = 4;    // one scalar lane

        const std::size_t KU_QUAT    = 0;                          // :419  (a full 16-byte quat)
        const std::size_t KU_COM     = KU_QUAT   + 16;             // :421
        const std::size_t KU_ID      = KU_COM    + KU_VEC;         // :423
        const std::size_t KU_VEL     = KU_ID     + KU_S;           // :425
        const std::size_t KU_RIGHT   = KU_VEL    + KU_VEC;         // :427
        const std::size_t KU_OMEGA   = KU_RIGHT  + KU_S;           // :429
        const std::size_t KU_LEFT    = KU_OMEGA  + KU_VEC;         // :431
        const std::size_t KU_RI      = KU_LEFT   + KU_S;           // :433
        const std::size_t KU_PAD0    = KU_RI     + KU_VEC;         // :435  (holds mStasis on X360)
        const std::size_t KU_UP      = KU_PAD0   + KU_S;           // :437
        const std::size_t KU_PAD1    = KU_UP     + KU_VEC;         // :439  (holds mInertia on X360)
        const std::size_t KU_AT      = KU_PAD1   + KU_S;           // :441
        const std::size_t KU_TAG     = KU_AT     + KU_VEC;         // :443
        const std::size_t KU_IFULL   = KU_TAG    + KU_S;           // :445
        const std::size_t KU_INVM    = KU_IFULL  + KU_VEC;         // :447
        const std::size_t KU_ISPLT   = KU_INVM   + KU_S;           // :449
        const std::size_t KU_STATE   = KU_ISPLT  + KU_VEC;         // :451
        const std::size_t KU_FORCE   = KU_STATE  + KU_S;           // :453
        const std::size_t KU_KINE    = KU_FORCE  + KU_VEC;         // :455
        const std::size_t KU_TORQUE  = KU_KINE   + KU_S;           // :457
        const std::size_t KU_COOL    = KU_TORQUE + KU_VEC;         // :459
        const std::size_t KU_END     = KU_COOL   + KU_S;

        static_assert(KU_COM    == 0x10, "mCom @ +0x10   (GetTransform's translation row)");
        static_assert(KU_ID     == 0x1C, "mId @ +0x1C    (DynamicUpdate uses it AS A BASE)");
        static_assert(KU_VEL    == 0x20, "mVel @ +0x20   (ReadFromRenderware `lvx r4+0x20`)");
        static_assert(KU_RIGHT  == 0x2C, "mRight @ +0x2C (RemoveRigidBody's `next`)");
        static_assert(KU_OMEGA  == 0x30, "mOmega @ +0x30 (ReadFromRenderware `lvx r4+0x30`)");
        static_assert(KU_LEFT   == 0x3C, "mLeft @ +0x3C  (RemoveRigidBody's `prev`)");
        static_assert(KU_RI     == 0x40, "mRi @ +0x40    (GetTransform's xAxis)");
        static_assert(KU_PAD0   == 0x4C, "mStasis @ +0x4C in the mPad0 lane (`lwz r10,0x4C(r3)`)");
        static_assert(KU_UP     == 0x50, "mUp @ +0x50    (GetTransform's yAxis)");
        static_assert(KU_PAD1   == 0x5C, "mInertia @ +0x5C in the mPad1 lane (`lwz r30,0x5C(r3)`)");
        static_assert(KU_AT     == 0x60, "mAt @ +0x60    (GetTransform's zAxis)");
        static_assert(KU_IFULL  == 0x70, "mIfull @ +0x70 (ReadPropertiesFromRenderware row 0)");
        static_assert(KU_INVM   == 0x7C, "mInvm @ +0x7C  (`lfs f11,0x7C(r3)`)");
        static_assert(KU_ISPLT  == 0x80, "mIsplt @ +0x80 (ReadPropertiesFromRenderware rows 1/2)");
        static_assert(KU_STATE  == 0x8C, "mState @ +0x8C (`clrlwi r11,r11,29` in RemoveRigidBody)");
        static_assert(KU_FORCE  == 0x90, "mForce @ +0x90 (`addi r6,r3,0x90`)");
        static_assert(KU_KINE   == 0x9C, "mKine @ +0x9C  (`stfs f0,0x9C(r3)`)");
        static_assert(KU_TORQUE == 0xA0, "mTorque @ +0xA0 (`addi r4,r3,0xA0`)");
        static_assert(KU_COOL   == 0xAC, "mCool @ +0xAC  (`stw r10,0xAC(r3)`)");
        static_assert(KU_END    == 0xB0, "the run closes on 0xB0 -- operator= copies +0x00..+0xAC");
    }

    // PART 2 -- named-member existence. These are compile-time only; each one stops
    // compiling if the member behind it is deleted or renamed, which is the half of the
    // check PART 1's arithmetic cannot see.
    void TouchRigidBodyMembers(const rw::physics::RigidBody& lrBody)
    {
        (void)lrBody.GetInverseMass();
        (void)lrBody.GetInertia();
        (void)lrBody.GetState();
        (void)lrBody.GetSimulation();
        (void)lrBody.GetRight();
        (void)lrBody.GetLeft();
        (void)lrBody.GetReactionForcesId();
        (void)lrBody.GetCoolDown();
        (void)lrBody.GetKineticEnergy();
        (void)lrBody.GetTag();
        (void)lrBody.GetInertiaFull();
        (void)lrBody.GetInertiaSplit();
    }
}

namespace
{
    void TouchExternallySimulatedBody(BrnPhysics::ExternallySimulatedBody& lrBody,
                                      const rw::physics::RigidBody* lpRigidBody)
    {
        lrBody.ReadFromRenderware(lpRigidBody);
    }
}
