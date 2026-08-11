#include "GameSource/Physics/PhysicsUtilities/Spring1D.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// BrnPhysics::SuspensionSpring -- the eight state Set* methods.
//
// Every console body is the same shape: the scalar argument arrives in v1, is broadcast across a
// VMX register (vmr128) and lane-inserted (vrlimi128) into exactly one lane of one of the three
// packed Vector4 members, after a NaN guard. The guard is `vcmpeqfp128. v0,v127,v127`; for IEEE
// floats `x == x` is true for every value EXCEPT NaN, so the assert fires only when the caller
// passes a NaN. The original asserts live in PhysicsUtilities/Spring1D.h with bespoke "Setting
// invalid <field>: <value>, please tell Graham D" messages; CGS_ASSERT supplies file/line itself,
// so the messages here are short descriptive equivalents.
//
// The target lane for each setter is pinned by the asm (store base offset + vrlimi mask, mask bit
// 8->lane0/.x, 4->.y, 2->.z, 1->.w) and matches the packed lane name in the member identifier.

namespace BrnPhysics
{
    // @0x8259F318 -- base+0, mask 8 -> .x
    void SuspensionSpring::SetStiffness(f32 lfStiffness)
    {
        CGS_ASSERT(lfStiffness == lfStiffness, "SuspensionSpring: setting invalid (NaN) stiffness");
        mvStiffness_Damping_Mass_Position.x = lfStiffness;
    }

    // @0x8259F410 -- base+0, mask 4 -> .y
    void SuspensionSpring::SetDamping(f32 lfDamping)
    {
        CGS_ASSERT(lfDamping == lfDamping, "SuspensionSpring: setting invalid (NaN) damping");
        mvStiffness_Damping_Mass_Position.y = lfDamping;
    }

    // @0x8259F508 -- base+0, mask 2 -> .z
    void SuspensionSpring::SetMass(f32 lfMass)
    {
        CGS_ASSERT(lfMass == lfMass, "SuspensionSpring: setting invalid (NaN) mass");
        mvStiffness_Damping_Mass_Position.z = lfMass;
    }

    // @0x8259F600 -- base+0, mask 1 -> .w
    void SuspensionSpring::SetPosition(f32 lfPosition)
    {
        CGS_ASSERT(lfPosition == lfPosition, "SuspensionSpring: setting invalid (NaN) position");
        mvStiffness_Damping_Mass_Position.w = lfPosition;
    }

    // @0x825BF8C8 -- base+16, mask 8 -> .x
    void SuspensionSpring::SetVelocity(f32 lfVelocity)
    {
        CGS_ASSERT(lfVelocity == lfVelocity, "SuspensionSpring: setting invalid (NaN) velocity");
        mvVelocity_Acceleration_DampingForce_SpringForce.x = lfVelocity;
    }

    // @0x825BF9C0 -- base+16, mask 4 -> .y
    void SuspensionSpring::SetAcceleration(f32 lfAcceleration)
    {
        CGS_ASSERT(lfAcceleration == lfAcceleration, "SuspensionSpring: setting invalid (NaN) acceleration");
        mvVelocity_Acceleration_DampingForce_SpringForce.y = lfAcceleration;
    }

    // @0x825BFAB8 -- base+16, mask 2 -> .z
    //
    // ⭐ THE NINTH SETTER, ADDED 2026-08-11 (suspension-springs wave). It was declared-only, and
    // UpdateSuspensionSprings @0x825F7AF0 calls it -- so it was an LNK2019 waiting for the first
    // caller. 0x825BFAB8 is a genuine hole in the IDA export set (its four siblings all have a
    // .json; it does not) -- [[ida-export-set-has-holes]] again.
    //
    // ⛔ THE LANE IS NOT INFERRED FROM THE MEMBER NAME. It is read out of the X360 image and
    // calibrated against the three siblings whose masks are already committed. All four bodies are
    // 62 words and differ in exactly 10 of them; word 56 (+0xE0) is the vrlimi128:
    //     SetVelocity     @0x825BF8C8  1808ff13  mask 8 -> .x
    //     SetAcceleration @0x825BF9C0  1804ff13  mask 4 -> .y
    //     SetDampingForce @0x825BFAB8  1802ff13  mask 2 -> .z   <- this one
    //     SetSpringForce  @0x825BFBB0  1801ff13  mask 1 -> .w
    // The store base word is byte-identical across all four (base+0x10). Corroborated by word 49,
    // `li r5,<__LINE__>`: 123 / 169 / 177 / 185 -- strictly ascending, so the console header
    // declares them in exactly this order, which is the order Spring1D.h already carries.
    void SuspensionSpring::SetDampingForce(f32 lfDampingForce)
    {
        CGS_ASSERT(lfDampingForce == lfDampingForce, "SuspensionSpring: setting invalid (NaN) damping force");
        mvVelocity_Acceleration_DampingForce_SpringForce.z = lfDampingForce;
    }

    // @0x825BFBB0 -- base+16, mask 1 -> .w
    void SuspensionSpring::SetSpringForce(f32 lfSpringForce)
    {
        CGS_ASSERT(lfSpringForce == lfSpringForce, "SuspensionSpring: setting invalid (NaN) force");
        mvVelocity_Acceleration_DampingForce_SpringForce.w = lfSpringForce;
    }

    // @0x825BFCA8 -- base+32, mask 8 -> .x
    void SuspensionSpring::SetExternalForce(f32 lfExternalForce)
    {
        CGS_ASSERT(lfExternalForce == lfExternalForce, "SuspensionSpring: setting invalid (NaN) external force");
        mvExternalForce.x = lfExternalForce;
    }

    // @0x825A7A28 (35 instrs) -- seed stiffness/damping/mass, zero the position, and clear the two
    // trailing registers wholesale.
    //
    // The three VecFloat arguments arrive in v1/v2/v3 and the body immediately parks v2 and v3
    // (`vmr128 v127,v2` / `vmr128 v126,v3`) so it can reuse v1 as the single-argument register for
    // each setter -- so the parameter ORDER is asm-literal: v1 -> SetStiffness, v2 -> SetDamping,
    // v3 -> SetMass. Each setter inserts lane 0, so passing `.x` is store-for-store equivalent to
    // the console's broadcast register.
    //
    // The last two stores are FULL 16-byte `stvx128 v127(==0)` at base+0x10 and base+0x20, i.e. all
    // four lanes of mvVelocity_Acceleration_DampingForce_SpringForce and all four of mvExternalForce
    // -- not the single-lane setters. Written as whole-register clears, which is what the asm does.
    //
    // Its two callers are VehiclePhysics::SetupSuspension @0x825CF718 and VehiclePhysics::Construct
    // @0x8262DBD0 (the latter passes 0/0/0 -- SetupSuspension installs the real values later).
    void SuspensionSpring::Prepare(VecFloat lvStiffness, VecFloat lvDamping, VecFloat lvMass)
    {
        SetStiffness(lvStiffness.x);                            // 0x825A7A54
        SetDamping(lvDamping.x);                                // 0x825A7A60  (v1 <- v127 == v2)
        SetMass(lvMass.x);                                      // 0x825A7A6C  (v1 <- v126 == v3)
        SetPosition(0.0f);                                      // 0x825A7A7C  (vspltisw128 v127,0)
        mvVelocity_Acceleration_DampingForce_SpringForce.SetZero();   // 0x825A7A88  stvx128 base+0x10
        mvExternalForce.SetZero();                                    // 0x825A7A8C  stvx128 base+0x20
    }

    // @0x825A2E30 (21 instrs) -- the same tail as Prepare without the three seeds: zero the
    // position lane through the setter, then clear the two trailing registers wholesale.
    // Its ONLY caller in the image is VehiclePhysics::Reset @0x825FDD78 (four times, once per
    // driven wheel, stepping the 0x30 stride of VehiclePhysics::maSprings).
    void SuspensionSpring::Reset()
    {
        SetPosition(0.0f);                                      // 0x825A2E54  (vspltisw128 v127,0)
        mvVelocity_Acceleration_DampingForce_SpringForce.SetZero();   // 0x825A2E60  stvx128 base+0x10
        mvExternalForce.SetZero();                                    // 0x825A2E64  stvx128 base+0x20
    }
}
