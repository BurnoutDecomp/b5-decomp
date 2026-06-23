#pragma once

#include "types.hpp"

// ===========================================================================
// rw::physics::Jacobian_RQD -- the "rotational quaternion-derivative" Jacobian
// block of a RenderWare physics constraint. JointJacobian::Build and
// DriveJacobian::Build call Create() to fill a 4x4 (16-float) Jacobian from two
// quaternion-shaped 4-vectors.
//
// OWNING HOME for the single function the X360 binary defines:
//     rw::physics::Jacobian_RQD::Create  @ 0x82BC0FA8
//
// No DWARF hints exist for this TU. Create is pure scalar
// FPU arithmetic (fmuls/fadds/fsubs) in the X360 asm, so it is transcribed
// store-for-store as portable float maths. The asm passes r3 = output Jacobian,
// r4 = first 4-vector, r5 = second 4-vector (each four contiguous floats); the
// output is a 4x4 block of 16 floats at +0x00..+0x3C.
// ===========================================================================

namespace rw
{
namespace physics
{

class Jacobian_RQD
{
public:
    // A quaternion-shaped 4-vector input (four contiguous floats / lanes).
    struct Vec4
    {
        f32 lane0;
        f32 lane1;
        f32 lane2;
        f32 lane3;
    };

    // @ 0x82BC0FA8 -- build the 16-float (4x4) Jacobian into mData from the two
    // input 4-vectors. Returns this. X360: r3=this, r4=lpA, r5=lpB.
    Jacobian_RQD* Create(const Vec4* lpA, const Vec4* lpB);

    f32 mData[16];   // +0x00..+0x3C  the 4x4 Jacobian block
};

} // namespace physics
} // namespace rw
