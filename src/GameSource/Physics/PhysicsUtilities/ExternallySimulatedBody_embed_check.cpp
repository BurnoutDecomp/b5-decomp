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

// The RigidBody pose/velocity fields are 16-byte SIMD registers in DWARF sequence; assert the
// reconstructed type carries the eleven-register run at 16-byte stride (11 * 16 == 176).
static_assert(sizeof(rw::physics::RigidBody) == 11 * 16,
              "RigidBody == eleven 16-byte SIMD register fields (DWARF rigidbody.h:419..459)");

namespace
{
    void TouchExternallySimulatedBody(BrnPhysics::ExternallySimulatedBody& lrBody,
                                      const rw::physics::RigidBody* lpRigidBody)
    {
        lrBody.ReadFromRenderware(lpRigidBody);
    }
}
