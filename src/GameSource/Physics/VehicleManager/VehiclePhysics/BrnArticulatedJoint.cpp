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

    // @0x825B4690  BrnPhysics::Vehicle::ArticulatedJointId::Set
    //   Packs the cab EntityId (high dword), the trailer's 14-bit entity index (bits[29:16]) and
    //   the vehicle (joint-pool) index (low 16 bits) into mu64RawId. Asserts both halves are
    //   traffic vehicles (owner byte == 2) and that they are distinct (their 14-bit entity indices
    //   at bit 10 differ).
    void ArticulatedJointId::Set(u16 luVehicleIndex, EntityId lCab, EntityId lTrailer)
    {
        CGS_ASSERT((lCab.muValue >> 24) == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
                   "lCabPhysicsEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
        CGS_ASSERT((lTrailer.muValue >> 24) == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
                   "lTrailerPhysicsEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
        CGS_ASSERT((((lCab.muValue >> 10) ^ (lTrailer.muValue >> 10)) & 0x3FFF) != 0,
                   "lCabPhysicsEntityId.GetEntityIndex() != lTrailerPhysicsEntityId.GetEntityIndex()");

        const u16 lu16TrailerEntityIndex = static_cast<u16>((lTrailer.muValue >> 10) & 0x3FFF);

        mu64RawId = (static_cast<u64>(lCab.muValue) << 32)
                  | (static_cast<u64>(lu16TrailerEntityIndex) << 16)
                  | static_cast<u64>(luVehicleIndex);
    }

    // @0x825B4798  BrnPhysics::Vehicle::ArticulatedJointId::GetCabEntityId
    //   The cab EntityId is the high 32 bits of the packed 64-bit joint id
    //   (`ld r11,0(r4); srdi r11,r11,32; stw r11,0(out)`). Asserts the extracted id owns a traffic
    //   vehicle (owner byte == 2).
    EntityId ArticulatedJointId::GetCabEntityId() const
    {
        EntityId lCabEntityId;
        lCabEntityId.muValue = static_cast<u32>(mu64RawId >> 32);

        CGS_ASSERT((lCabEntityId.muValue >> 24) == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
                   "lCabEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
        return lCabEntityId;
    }
}
}
