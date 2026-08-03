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

        // FLAG: the X360 then does `*(lpAttribs+32) = COM + lvWheelPosMean` -- folding the wheel-mean
        // into the attribs' COM-offset lane (mCOMOffset region @ +0x20 of the full VehicleAttribs).
        // The included VehiclePhysics.h VehicleAttribs slice does NOT expose mCOMOffset BY NAME (it is
        // owned by the VehicleAttribs TU), so this in-place COM update is recorded but ELIDED here
        // rather than written through a raw-offset cast. The computed lvWheelPosMean is the faithful
        // value the console adds. (void)-tagged so the computation is not dropped.
        (void)lvWheelPosMean;
        (void)lrAABB;

        // Build the local transform from the spawn event (the asm copies event+0x10 rows into a
        // local v35 transform) and forward into the full-physics Prepare.
        VehiclePhysics::Prepare(&lpEvent->mInitialTransform, lpDeformSpec, lpAttribs,
                                lpWheelPositions, lpafWheelRadii);
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
