#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"

// BrnPhysics::Vehicle::VehiclePhysics -- the out-of-line scalar predicates owned by the
// Vehicle-physics group (class TU). The header-homed leaf methods (GetShowtimeDeformationScale,
// IsCounterSteeringAtLowSpeed) and the nested SlamEffect/ShuntEffect bodies live elsewhere; this
// .cpp is the home for the three non-VMX predicates whose X360 addresses the ledger lists here.
//
// The remaining three class-TU functions are VMX128 lowerings and are blocked (GetCarGroundDistanceCheck
// @0x825C0100, GetTransformDelta @0x825B2EF8, UpdateLinearVelocityMagnitude @0x825C0000) -- see the
// group report.

namespace BrnPhysics
{
namespace Vehicle
{
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
}
}
