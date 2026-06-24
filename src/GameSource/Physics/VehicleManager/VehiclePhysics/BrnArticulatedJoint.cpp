#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJoint.h"

#include "rw/math/vpu/vector3_operation.h"               // rw::math::vpu::IsValid(Vector3)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

// BrnPhysics::Vehicle::ArticulatedJoint::Set -- reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX @ 0x825D7BC0.
//
// The X360 validates the parent->joint transform (an RwMathVPU::IsValid NaN check over
// the matrix's three populated lanes of each of its four rows -- the asm splats lanes
// 0/1/2 of each row register and self-compares with vcmpeqfp., AND-ing the four row
// results), fires the baked assert "RwMathVPU::IsValid( lParentToJointTransform )" on
// failure, then copies the four 16-byte rows (4x lvx128/stvx128 @ +0x00/+0x10/+0x20/
// +0x30) and stores the 64-bit joint id (`std r26, 0x40(r28)`).
//
// IsValid(Matrix44Affine) is reproduced as the per-row IsValid(Vector3) self-equality
// over the four rows (the canonical rw::math::vpu::IsValid(Vector3) = x==x && y==y &&
// z==z). The matrix copy is a memberwise Matrix44Affine assignment (the four-row vector
// copy), and the id is stored as the full u64.

namespace BrnPhysics
{
namespace Vehicle
{
    void ArticulatedJoint::Set(const rw::math::vpu::Matrix44Affine& lrParentToJointTransform,
                               ArticulatedJointId lJointId)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lrParentToJointTransform.xAxis)
                   && rw::math::vpu::IsValid(lrParentToJointTransform.yAxis)
                   && rw::math::vpu::IsValid(lrParentToJointTransform.zAxis)
                   && rw::math::vpu::IsValid(lrParentToJointTransform.wAxis),
                   "RwMathVPU::IsValid( lParentToJointTransform )");

        mParentToJointTransform = lrParentToJointTransform;  // 4x 16B row copy (lvx128/stvx128)
        mJointId = lJointId;                                 // std r26, 0x40(r28) -- full u64
    }
}
}
