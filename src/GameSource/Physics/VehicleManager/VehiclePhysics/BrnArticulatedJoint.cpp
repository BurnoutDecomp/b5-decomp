#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJoint.h"

#include "rw/math/vpu/vector3_operation.h"               // rw::math::vpu::IsValid(Vector3)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

#include <cstddef>                                       // offsetof (layout gate)

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
    // ---------------------------------------------------------------------------------------
    // _AssertLayout -- never called; the gate that keeps this class on its console seats.
    //
    // AN ABSOLUTE offsetof GATE IS LEGITIMATE HERE, unlike the console-arithmetic gates one
    // level up (VehiclePhysics_layout_check.cpp): this class owns NO pointers and embeds no
    // reconstruction whose host size can drift -- Matrix44Affine is four 16-byte float rows and
    // ArticulatedJointId is one u64 -- so the host layout must reproduce the X360's byte for byte.
    //
    // THE FIRST VERSION OF THIS GATE WAS VACUOUS AND THE TAMPER TEST CAUGHT IT. It was
    // `static_assert(sizeof(ArticulatedJoint) == 80)` alone, in BrnArticulatedJointPool.cpp.
    // Adding a spare `u32` member to this class did NOT fail it: 64 + 8 == 72 and the type is
    // alignas(16) via Matrix44Affine, so there are EIGHT bytes of tail padding and anything up to
    // that size slides in free. The sizeof assert can only ever catch growth PAST 80; what
    // actually bites is DISPLACEMENT, and only the offsetof line below catches that (re-tested by
    // inserting a u32 before mJointId: `error C2338` on the +0x40 assert, as required).
    // ---------------------------------------------------------------------------------------
    void ArticulatedJoint::_AssertLayout()
    {
        static_assert(sizeof(rw::math::vpu::Matrix44Affine) == 64,
                      "Matrix44Affine is four 16-byte rows; ArticulatedJoint::Set @0x825D7BC0 "
                      "copies it as 4x lvx128/stvx128 at +0x00/+0x10/+0x20/+0x30");
        static_assert(sizeof(ArticulatedJointId) == 8,
                      "ArticulatedJointId is one packed u64 (`std r26, 0x40(r28)`)");
        static_assert(offsetof(ArticulatedJoint, mParentToJointTransform) == 0x00,
                      "mParentToJointTransform @+0x00 -- Construct @0x825B8DC0 stores row 0 with "
                      "`stvx128 v0, r0, r3` (zero displacement from this)");
        static_assert(offsetof(ArticulatedJoint, mJointId) == 0x40,
                      "mJointId @+0x40 -- Construct @0x825B8DC0 `std r10, 0x40(r3)` and Set "
                      "@0x825D7BC0 `std r26, 0x40(r28)`");
        static_assert(sizeof(ArticulatedJoint) == 80,
                      "ArticulatedJointPool::Construct @0x82600938 indexes maJoints with i*80 "
                      "(slwi r11,r31,2 ; add r11,r31,r11 ; slwi r11,r11,4), and the embedded pool "
                      "must stay 832 bytes for BrnPhysicalTrafficManager.h's +103616..+104448 span");
        static_assert(alignof(ArticulatedJoint) == 16,
                      "16-aligned via Matrix44Affine -- that alignment is what rounds 72 up to 80");
    }

    // ---------------------------------------------------------------------------------------
    // ArticulatedJointId::Construct  --  INLINED into ArticulatedJoint::Construct @0x825B8DC0
    //   (0x825B8E5C..0x825B8E8C). Recovered as its own function because the DWARF declares it
    //   (BrnArticulatedJoint.h:52) and the project reverses compiler inlining.
    //
    // The console writes the 8-byte id FOUR times and every byte of the first write is replaced:
    //     ld  r10, qword_82F2A3B0 ; std r10, 0x40(r3)   <- seed with the invalid-joint constant
    //     stw r11(=0),      0x40(r3)                    <- clears bytes 0x40..0x43 (BE: bits 63..32)
    //     ld  r9, 0x40(r3) ; lwz r10, dword_82F2A3A4 ;
    //     extldi r10,r10,64,32 ; or r10,r10,r9 ; std   <- KU_INVALID_ENTITY_ID into bits 63..32
    //     sth r11(=0),      0x44(r3)                    <- bits 31..16 (trailer entity index)
    //     sth r11(=0),      0x46(r3)                    <- bits 15..0  (vehicle / joint-pool index)
    // so the whole seed qword_82F2A3B0 is dead and the surviving value is exactly
    //     (u64)KU_INVALID_ENTITY_ID << 32.
    //
    // Written as a SHIFT on the named u64, not as byte stores, because "the first four bytes"
    // is the HIGH dword on the big-endian console and the LOW dword on the little-endian host.
    // The shift form is endian-neutral and matches the field packing ArticulatedJointId::Set
    // (below) already commits to: cab EntityId in bits 63..32, trailer entity index in 31..16,
    // vehicle index in 15..0. So this is "an invalid cab id and both indices zero".
    // ---------------------------------------------------------------------------------------
    void ArticulatedJointId::Construct()
    {
        mu64RawId = static_cast<u64>(KU_INVALID_ENTITY_ID) << 32;
    }

    // ---------------------------------------------------------------------------------------
    // ArticulatedJoint::Construct  @0x825B8DC0  (54 instructions)
    //   Builds an identity Matrix44Affine in a stack frame and stores it as four 16-byte rows
    //   (lvx128/stvx128 to r3+0x00/+0x10/+0x20/+0x30), then constructs the joint id.
    //
    // The identity is proven by the literals: flt_82001C98 == 1.0f is stored at the three
    // diagonal lanes only (var_40 / var_2C / var_18, i.e. row0.x, row1.y, row2.z) and
    // flt_82001CC0 == 0.0f at every other populated lane, with four `stw r11(=0)` covering the
    // four rows' .w lanes. That is Matrix44Affine::SetIdentity() -- which sets wAxis to
    // (0,0,0,0), matching the console (an AFFINE matrix's implicit last column is not stored).
    //
    // RETURN TYPE. IDA types this `int __fastcall(int result)` and the pseudocode "returns"
    // r3. There is no return value: r3 is simply still live at the `blr` because it is the
    // implicit `this`. The DWARF declares `void Construct()` (BrnArticulatedJoint.h:100) and the
    // ladder puts DWARF above Hex-Rays for declaration shape, so `void` is what is committed.
    // ---------------------------------------------------------------------------------------
    void ArticulatedJoint::Construct()
    {
        mParentToJointTransform.SetIdentity();
        mJointId.Construct();
    }

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
