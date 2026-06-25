#pragma once

#include "types.hpp"

namespace BrnPhysics
{
namespace Props
{
    extern f32 gfAntiHerdUpwardScale;
    extern f32 gfAntiHerdSideScale;
    extern f32 gfAntiHerdHighSpeedSideScale;
    extern f32 gfMaxSpeedForSideForce;
    extern f32 gfAntiHerdSpeedClamp;
    extern f32 gfInertiaScale;
    extern f32 gfGravityScale;

    extern f32 gfLinearDrag;
    extern f32 gfAngularDrag;
    extern f32 gfMaxLinearVelocity;
    extern f32 gfMaxAngularVelocity;
    extern f32 gfRestitution;
}
}
