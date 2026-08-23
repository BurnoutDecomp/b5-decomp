#include "GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"             // rw::math::vpu::{Add, Mult}
#include "rw/math/vpu/matrix44affine_operation.h"      // rw::math::vpu::TransformPoint (COM spawn seat)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // CreatePhysicalTrafficEvent (real layout)
// PreparePhysical dereferences lpDeformSpec for the
// lHalfExtent argument (mHandlingBodyDimensions @+0x40), and VehiclePhysics.h only FORWARD-declares
// StreamedDeformationSpec. The 2026-08-11 create-drain commit that introduced that dereference left
// this TU un-compilable (incomplete type -> the 9-argument call silently reported as 8); this is
// the missing definition, not a new dependency.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"

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
                                         VehicleAttribs* lpAttribs,
                                         const CgsGeometric::AxisAlignedBox& lrAABB,
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

        // mean = (1/4) * sum. The asm forms the reciprocal as `vrefp` + TWO Newton-Raphson
        // refinement steps, then `v3 = recip * sum` (corrected 2026-08-11: this note used to say
        // ONE step). Verbatim: 0x826394B8 vrefp v0,v0 ; 0x826394BC vnmsubfp v11,v0,v13,v11 +
        // 0x826394C0 vmaddfp v0,v0,v0,v11 (step 1) ; 0x826394C4 vnmsubfp v13,v0,v13,v10 +
        // 0x826394C8 vmaddfp v0,v0,v0,v13 (step 2) ; 0x826394CC vmulfp128 v3,v0,v12. Two steps of
        // Newton on a vrefp seed is full f32 precision, which is why the exact 0.25f below is a
        // faithful de-SIMD and not a tightening of the console's answer.
        const Vector3 lvWheelPosMean = vpu::Mult(lvWheelPosSum, 0.25f);

        // The old note here said the wheel-mean was
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
        //
        // VERIFIER CATCH, CORRECTED 2026-08-11 (adversarial verify of this wave): the three
        // instructions the note above stopped short of were console BEHAVIOUR, not scaffolding,
        // and the first pass omitted both:
        //   0x8263949C  lvx128 v9, r0, r10       <- r10 = lpAttribs + 0x20 == mBaseAttribs.mCOMOffset
        //   0x826394D0  vaddfp v0, v9, v3        <- COM += wheelMean
        //   0x826394D4  stvx128 v0, r0, r10      <- WRITE-BACK into the attribs record (a `+=`,
        //                                           reproduced verbatim -- if the record is shared
        //                                           across spawns the console accumulates; that is
        //                                           the console's own semantics either way).
        //   0x82639520..48  row0*P.x + row1*P.y + row2*P.z + event.row3, P = the UPDATED COM
        //                   -> the r4 handed to Prepare is the event transform with its
        //                   TRANSLATION REPLACED by TransformPoint(event, COM) -- NOT
        //                   lpEvent->mInitialTransform verbatim.
        lpAttribs->mBaseAttribs.mCOMOffset =
            vpu::Add(lpAttribs->mBaseAttribs.mCOMOffset, lvWheelPosMean);      // @0x826394D4

        Matrix44Affine lSpawnTransform = lpEvent->mInitialTransform;
        lSpawnTransform.wAxis = vpu::TransformPoint(lpEvent->mInitialTransform,
                                                    lpAttribs->mBaseAttribs.mCOMOffset); // @0x82639548

        VehiclePhysics::Prepare(lSpawnTransform,
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
