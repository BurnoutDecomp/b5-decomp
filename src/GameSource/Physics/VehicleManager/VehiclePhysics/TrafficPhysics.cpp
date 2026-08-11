#include "GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"             // rw::math::vpu::{Add, Mult}
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // CreatePhysicalTrafficEvent (real layout)

// BrnPhysics::Vehicle::TrafficPhysics -- C11_simple_traffic_attribs group.
//   PreparePhysical: bodied (de-SIMD'd from 0x82639380).
//   Update: PARTIAL -- the control-halving + freak-out FSM are bodied (de-SIMD'd from the legible
//   parts of 0x82639590); the inlined per-axis VMX128 angular-velocity damping curve on the
//   crashing path (un-homed rodata coefficient tables unk_82014AC0..AF0 driving a vlogefp/vexptefp
//   powf) is NOT fabricated -- it is delegated to the committed VehiclePhysics::UpdateCrashing
//   entry, which owns that math in the full-physics TU.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    // (The four freak-out FSM constants moved to TrafficPhysics_Construct.cpp with Update, their
    // only consumer -- 2026-08-03.)

    // -------------------------------------------------------------------------------------------
    // PreparePhysical  @0x82639380
    //   asserts the event/attribs/wheel-position/wheel-radii pointers are non-null (debug), sums the
    //   four streamed wheel positions, folds 1/4 of that sum into the attribs' COM offset, builds a
    //   local transform from the spawn event, forwards into VehiclePhysics::Prepare +
    //   SetWheelVelocities, then seeds the freak-out fields.
    // -------------------------------------------------------------------------------------------
    bool TrafficPhysics::PreparePhysical(const CreatePhysicalTrafficEvent* lpEvent,
                                         VehicleAttribs* lpAttribs, const AxisAlignedBox& lrAABB,
                                         const StreamedDeformationSpec* lpDeformSpec,
                                         const Vector3* lpWheelPositions, const f32* lpafWheelRadii)
    {
        CGS_ASSERT(lpEvent != nullptr,          "lpEvent != NULL");
        CGS_ASSERT(lpAttribs != nullptr,        "lpAttribs != NULL");
        CGS_ASSERT(lpWheelPositions != nullptr, "lpWheelPositions != NULL");
        CGS_ASSERT(lpafWheelRadii != nullptr,   "lpafWheelRadii != NULL");

        // Sum the four streamed wheel positions (the `do { v12 += lvx128 } while (v15<4)` loop).
        Vector3 lvWheelPosSum = { 0.0f, 0.0f, 0.0f, 0.0f };
        for (int liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            lvWheelPosSum = vpu::Add(lvWheelPosSum, lpWheelPositions[liWheel]);

        // mean = (1/4) * sum  (the asm forms vrefp(4.0) + one Newton step, then `v3 = recip * sum`).
        const Vector3 lvWheelPosMean = vpu::Mult(lvWheelPosSum, 0.25f);

        // ⭐⭐ CORRECTED 2026-08-11 (create-drain wave). The old note here said the wheel-mean was
        // folded into the attribs' COM lane and then `(void)`-discarded it, and the AABB was
        // discarded too -- both because the VehiclePhysics::Prepare declaration this file called was
        // a FIVE-parameter fork (see that declaration's banner). With the DWARF/PS3 nine-parameter
        // signature in place, the console's own use of both values is plain, read off the asm:
        //   0x826394CC  vmulfp128 v3, v0, v12    <- v3 = 0.25f * sum(wheelPositions), and v3 is NOT
        //                                           written again before the call, i.e. the wheel
        //                                           mean IS the lHandlingBodyOffset argument.
        //   0x826394E8  mr  r5, r25              <- r25 is PreparePhysical's own lrAABB parameter.
        //   0x82639550  lvx128 v4, r24, 0x40     <- r24 == lpDeformSpec; +0x40 == +64 ==
        //                                           StreamedDeformationSpec::mHandlingBodyDimensions
        //                                           (static_asserted in that header) == lHalfExtent.
        //   0x8263950C  lvx128 v1, r0, r29       <- r29 == lpEvent + 0x50 == mInitialVelocity.
        //   0x8263951C  lvx128 v2, r30, 0x60     <- lpEvent + 0x60 == mAngularVelocity.
        //   0x826394E0  addi  r4, r1, var_90     <- the local transform built from the spawn event.
        VehiclePhysics::Prepare(lpEvent->mInitialTransform,
                                lpEvent->mInitialVelocity,
                                lpEvent->mAngularVelocity,
                                lvWheelPosMean,
                                lpDeformSpec->mHandlingBodyDimensions,
                                lrAABB,
                                lpAttribs,
                                lpWheelPositions,
                                lpafWheelRadii);
        // asm: `lvx128 v1, r0, r29` where r29 = lpEvent+0x50 = &lpEvent->mInitialVelocity -- the
        // post-Prepare SetWheelVelocities call passes the spawn event's initial velocity vector,
        // NOT a zero vector (Hex-Rays dropped this VMX128 register argument from the pseudocode).
        VehiclePhysics::SetWheelVelocities(lpEvent->mInitialVelocity);

        // Seed the freak-out fields. mOwnerID = *(event+8) = event->mCrasherID; the others zeroed.
        mOwnerID            = lpEvent->mCrasherID;                            // +5104
        mfFreakOutDirection = 0.0f;                                            // +5112
        mfFreakOutTime      = 0.0f;                                            // +5116
        mu8FreakOutState    = E_FREAK_OUT_STATE_OFF;                           // +5108
        return true;
    }

}
}
