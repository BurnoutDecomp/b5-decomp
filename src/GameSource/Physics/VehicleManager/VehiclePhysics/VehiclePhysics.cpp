#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "rw/math/vpu/vector3_operation.h"            // rw::math::vpu::{MagnitudeSquared, Normalize}
#include "rw/math/vpu/matrix44affine_operation.h"     // rw::math::vpu::{InverseOfMatrixWithOrthonormal3x3, operator*}

// BrnPhysics::Vehicle::VehiclePhysics -- the out-of-line ledger funcs owned by the Vehicle-physics
// group (class TU). The header-homed leaf methods (GetShowtimeDeformationScale,
// IsCounterSteeringAtLowSpeed) and the nested SlamEffect/ShuntEffect bodies live elsewhere; this
// .cpp is the home for the three scalar predicates plus the three VMX128 functions (the latter
// lowered to faithful scalar / canonical rw::math::vpu math) whose X360 addresses the ledger lists.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;
    // @0x825B2FE0  BrnPhysics::Vehicle::VehiclePhysics::GetNumberOfWheelsOnTheGround
    //   The X360 reads the four driven wheels' road-contact on-ground flags at +0x158/+0x238/+0x318/
    //   +0x3F8 (maWheels stride 0xE0, RoadContact::mbIsOnGround +0x28) and counts the nonzero ones:
    //     v1  = maWheels[0].onGround ? 1 : 0
    //     if (maWheels[1].onGround) ++v1
    //     if (maWheels[2].onGround) ++v1
    //     return maWheels[3].onGround ? v1 + 1 : v1
    //   i.e. the count of driven wheels on the ground.
    s32 VehiclePhysics::GetNumberOfWheelsOnTheGround() const
    {
        s32 liCount = 0;
        if (maWheels[eFrontLeftWheel].GetRoadContact().mbIsOnGround)
            ++liCount;
        if (maWheels[eFrontRightWheel].GetRoadContact().mbIsOnGround)
            ++liCount;
        if (maWheels[eRearLeftWheel].GetRoadContact().mbIsOnGround)
            ++liCount;
        if (maWheels[eRearRightWheel].GetRoadContact().mbIsOnGround)
            ++liCount;
        return liCount;
    }

    // @0x825E6D50  BrnPhysics::Vehicle::VehiclePhysics::IsBeingSlamedOrShunted
    //   lfs f13,0x111C(r3) ; fcmpu f13, 0.0 ; bgt -> return 1     (slam in progress)
    //   addi r3,r3,0x1130 ; bl ShuntEffect::IsActive ; -> result  (otherwise: shunt active?)
    bool VehiclePhysics::IsBeingSlamedOrShunted() const
    {
        if (mfSlamLife > 0.0f)
            return true;
        return mShuntEffect.IsActive();
    }

    // @0x82615290  BrnPhysics::Vehicle::VehiclePhysics::IsBeingSlamedOrShuntedByRaceCar
    //   lbz r11,0x13E0(r3) ; extsb r11 ; extsb a2 ; cmpw -> if (a2 != mi8SlammingRaceCarId) return 0
    //   bl IsBeingSlamedOrShunted ; -> return (that ? 1 : 0)
    //   i.e. true only when the slamming/shunting race car is the queried one AND a slam/shunt is live.
    bool VehiclePhysics::IsBeingSlamedOrShuntedByRaceCar(s8 li8RaceCarId) const
    {
        if (li8RaceCarId != mi8SlammingRaceCarId)
            return false;
        return IsBeingSlamedOrShunted();
    }

    // @0x825C0100  BrnPhysics::Vehicle::VehiclePhysics::GetCarGroundDistanceCheck
    //   addi   r11,r3,0x20 ; lvx128 v0,r11 ; vspltw v0,v0,1   ; load mUpAxis, splat .y lane
    //   vspltisw v13,0      ; vcmpgtfp. v0,v13,v0             ; test 0 > up.y  (car inverted?)
    //   lfs    f1,flt_82001DA0                                ; result = 0.5
    //   beqlr  cr6                                            ; up.y >= 0 -> return 0.5
    //   lfs    f13,0x6A4(r3) ; lfs f0,flt_82001D9C            ; mfCarGroundCheckExtent, multiplier
    //   fmadds f1,f13,f0,f1                                   ; return extent*K + 0.5
    // i.e. normally 0.5, but when the up axis points downward (the car is upside down) the result
    // grows by the car's own vertical extent scaled by a constant.
    //
    // Constants: flt_82001DA0 = 0.5 and flt_82001D9C = 2.0, both resolved .rdata literals
    // (flt_82001D9C is homed as 2.0f in CgsQuat.cpp and BrnEmitter3dControl.cpp). The math op
    // (extent * 2.0 + 0.5) and the member stores it touches are exact.
    f64 VehiclePhysics::GetCarGroundDistanceCheck() const
    {
        static const f32 KF_GROUND_DISTANCE_BASE = 0.5f;                  // flt_82001DA0 (resolved)
        static const f32 KF_CAR_GROUND_DISTANCE_INVERTED_SCALE = 2.0f;   // flt_82001D9C = 2.0 (resolved)

        if (!(0.0f > mUpAxis.y))   // up axis not pointing down -> car is upright
            return KF_GROUND_DISTANCE_BASE;

        return mfCarGroundCheckExtent * KF_CAR_GROUND_DISTANCE_INVERTED_SCALE + KF_GROUND_DISTANCE_BASE;
    }

    // @0x825B2EF8  BrnPhysics::Vehicle::VehiclePhysics::GetTransformDelta
    //   r11 = this+0x1370 (mPreviousTransform) ; r10 = this+0x10 (mTransform)
    //   The X360 builds the inverse of mPreviousTransform inline: the orthonormal 3x3 is transposed
    //   with vmrglw/vmrghw lane merges, and the inverse translation is the transpose applied to the
    //   negated position (vsubfp v10,0,wAxis then the vmaddfp cascade). That inverse is then matrix-
    //   multiplied by the current mTransform and the four affine rows are stored to the return buffer
    //   (stvx128 -> result+0/+0x10/+0x20/+0x30).
    //   So: delta = inverse(mPreviousTransform) * mTransform, in mPreviousTransform's local space.
    Matrix44Affine VehiclePhysics::GetTransformDelta() const
    {
        return vpu::InverseOfMatrixWithOrthonormal3x3(mPreviousTransform) * mTransform;
    }

    // @0x825C0000  BrnPhysics::Vehicle::VehiclePhysics::UpdateLinearVelocityMagnitude
    //   r9 = this+0x50 (mLinearVelocity) ; r10 = this+0x1340 (mNormLinearVelocityMag)
    //   vmsum3fp128 v0,v10,v10  -> |v|^2 (dot3) ; the cached vector is zeroed first (stvx128 v12=0).
    //   vrsqrtefp + Newton refinement -> 1/|v| ; v0 = |v|^2 * (1/|v|) = |v| (the speed magnitude),
    //   guarded by vsel/vcmpeqfp-against-zero so a zero-speed input yields a zero (no NaN).
    //   The unit direction is written into the xyz lanes and the magnitude into the "plus" (w) lane
    //   of mNormLinearVelocityMag (vrlimi128 packs the magnitude lane into the direction register).
    // Lowered here to the canonical Normalize + scalar magnitude; the member stores (direction in
    // xyz, speed in the w lane) match the asm's stvx128 to this+0x1340.
    void VehiclePhysics::UpdateLinearVelocityMagnitude()
    {
        const f32 lfSpeedSquared = vpu::MagnitudeSquared(mLinearVelocity);

        Vector3 lvDirection = vpu::Normalize(mLinearVelocity);   // zero vector when speed is zero
        const f32 lfSpeed = (lfSpeedSquared > 0.0f) ? std::sqrt(lfSpeedSquared) : 0.0f;

        mNormLinearVelocityMag.SetVector3(lvDirection);   // unit direction -> xyz lanes
        mNormLinearVelocityMag.SetPlus(lfSpeed);          // speed magnitude -> w / "plus" lane
    }
}
}
