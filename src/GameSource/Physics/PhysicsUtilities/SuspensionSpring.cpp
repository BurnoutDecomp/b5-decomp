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
}
