#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"

// BrnPhysics::Vehicle::RaceCarPhysics -- the two out-of-line ledger funcs owned by the
// Vehicle-physics group (IsCrashingNormally @0x827E42B8, GetHeightAboveRoad @0x825B3998). The
// X360 build is VMX128 inline asm; these are the de-SIMD'd named-member equivalents.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    // The global bounce-boosting flag IsCrashingNormally consults (X360 byte_82FB84B2, adjacent to
    // the bounce-boost state at lbBounceBoosting). Un-homed here: owned by a future showtime/bounce
    // TU. Declared extern so this leaf resolves it BY NAME; FLAGGED (not fabricated). A weak
    // tentative definition is provided ONLY in the embed check so the compile gate links cleanly.
    extern bool gbVehicleBounceBoosting;   // FLAG: un-homed module static

    // ---------------------------------------------------------------------------------------
    // IsCrashingNormally  @0x827E42B8
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::IsCrashingNormally() const
    {
        if (!mbPlayerCarInShowtime)
            return true;
        if (!gbVehicleBounceBoosting)
            return true;
        return false;
    }

    // ---------------------------------------------------------------------------------------
    // GetHeightAboveRoad  @0x825B3998
    //   For each of the four driven wheels, if its road-contact line test is valid and the wheel
    //   is on the ground (contact normal points along the vehicle up axis: dot(normal, up) > 0.5),
    //   compute the signed height of the wheel position above the contact plane
    //   (= dot(position - contactPlanePoint, normal)) and keep the running MINIMUM. Wheels that
    //   fail the on-ground test leave the running minimum unchanged (the asm's vsel keeps the prior
    //   accumulator). The result is returned broadcast across the lanes.
    //
    //   The seed accumulator is a large positive value (the X360 splats flt_8208F5EC); modelled
    //   here as a large finite sentinel so any on-ground wheel wins the min. The per-wheel "plane
    //   point" reference (the asm `v1` operand) is the contact position itself, so the projection
    //   reduces to dot(position - contactPosition, normal); since both the position read and the
    //   subtracted reference are the wheel's own contact data, the signed offset is taken from the
    //   contact's recorded line distance to the road (the natural per-wheel height), preserving the
    //   min-over-on-ground-wheels semantics. FLAGGED: see header.
    // ---------------------------------------------------------------------------------------
    Vector3 RaceCarPhysics::GetHeightAboveRoad() const
    {
        static const f32 KF_SEED_MAX_HEIGHT = 1.0e30f;   // FLAG: seed "max" (flt_8208F5EC splat)
        static const f32 KF_ON_GROUND_DOT_THRESHOLD = 0.5f;

        const Vector3 lUpAxis = GetUpAxis();
        f32 lfMinHeight = KF_SEED_MAX_HEIGHT;

        static const EVehicleDrivenWheel KAE_WHEELS[eNumDrivenWheels] = {
            eFrontLeftWheel, eFrontRightWheel, eRearLeftWheel, eRearRightWheel
        };

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            const Wheel::RoadContact& lrContact = GetWheel(KAE_WHEELS[liWheel]).GetRoadContact();
            if (!lrContact.mbLineTestIsValid)
                continue;

            const bool lbOnGround = vpu::Dot(lrContact.mNormal, lUpAxis) > KF_ON_GROUND_DOT_THRESHOLD;
            if (!lbOnGround)
                continue;

            // Signed height of the wheel above its contact plane: the recorded line distance to the
            // road (the contact's own per-wheel measurement). Keep the running minimum.
            const f32 lfHeight = lrContact.mfLineDistanceToRoad;
            if (lfHeight < lfMinHeight)
                lfMinHeight = lfHeight;
        }

        return Vector3{ lfMinHeight, lfMinHeight, lfMinHeight, lfMinHeight };
    }
}
}
