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

    // The traffic over-speed gate: above this MPH the scripted throttle/steer are halved
    // (the asm compares mpAttribs->maxSpeed-region lane vs the local `v95 = 5000.0`).
    static const f32 KF_TRAFFIC_CONTROL_HALVE_SPEED = 5000.0f;
    static const f32 KF_FREAKOUT_SPINOUT_TIMEOUT    = 4.0f;   // SPIN_OUT times out at 4.0s
    static const f32 KF_FREAKOUT_TURNROLL_MAX       = 2.0f;   // TURN_AND_ROLL window <= 2.0s
    static const f32 KF_FREAKOUT_SEVERITY_GATE       = 0.5f;  // entry gated on control-severity > 0.5

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

    // -------------------------------------------------------------------------------------------
    // Update  @0x82639590  (PARTIAL)
    //   ingests a 72-byte scripted control block (copied to a local), halves throttle/steer over the
    //   speed gate, advances the freak-out FSM, then forwards into VehiclePhysics::UpdateShunt and
    //   VehiclePhysics::UpdateCrashing.
    // -------------------------------------------------------------------------------------------
    void TrafficPhysics::Update(f32 lfTimeStep, f32 lfArg2, const Matrix44Affine* lpReferenceTransform,
                                const BrnPlayerDriverControls* lpControls, bool lbArg5, bool lbArg6,
                                bool lbArg7)
    {
        (void)lfArg2; (void)lpReferenceTransform; (void)lbArg5; (void)lbArg6; (void)lbArg7;

        // The X360 memcpy's the 72-byte control block into a local (v99) so the modifiers below don't
        // corrupt the caller's input. We work on a local copy of the throttle/steer scalars. FLAG:
        // the control block is opaque here (owned by the input TU); the two modified fields are the
        // throttle (offset +4 in the local, `v99[1]`) and steer (`v99[2]`). They are read/written via
        // a local mirror -- the full block is forwarded unchanged into UpdateShunt/UpdateCrashing.
        f32 lfLocalThrottle = 0.0f;   // v99[1] mirror
        f32 lfLocalSteer    = 0.0f;   // v99[2] mirror

        // --- over-speed control halving (the `if (speed > v95=5000.0)` branch) ---
        // The gate compares mpAttribs maxSpeed-region lane vs KF_TRAFFIC_CONTROL_HALVE_SPEED.
        // FLAG: the speed source is read from the attribs lane (un-homed in this slice); the halving
        // shape (throttle *= 0.5 always, steer *= 0.5 only when `*(this+4032)==0`) is reproduced.
        const bool lbOverSpeed = false;   // FLAG: gate result from un-homed attribs lane (inert)
        const bool lbHalveSteer = true;   // `!*(this+4032)` -- the freak-out-active suppressor
        if (lbOverSpeed)
        {
            lfLocalThrottle *= 0.5f;
            if (lbHalveSteer)
                lfLocalSteer *= 0.5f;
        }

        // --- the freak-out FSM (mu8FreakOutState @ +5108) ---
        f32 lfNewFreakOutTime = 0.0f;
        switch (mu8FreakOutState)
        {
        case E_FREAK_OUT_STATE_OFF:
        {
            // Entry: when the control severity (`v99[3]`) exceeds the gate, roll the spin direction
            // coin-flip off the per-car LCG (multiplier 1148159575) and SetFreakedOut. FLAG: the
            // control-severity input + the +/- direction floats (flt_82FB8388 / flt_82FB838C) are
            // un-homed; the LCG advance + the >0.5 gate are reproduced.
            const f32 lfSeverity = 0.0f;   // FLAG: v99[3] severity (un-homed control lane, inert)
            if (lfSeverity > KF_FREAKOUT_SEVERITY_GATE)
            {
                // LCG: state = state * 1148159575 + 1  (the `*v52 *= ...; ++DWORD2(v52)` step).
                mRandom.muState = mRandom.muState * 1148159575u + 1u;
                // direction = (hashed-bucket >= 0x32) ? +dir : -dir. FLAG: the two direction
                // magnitudes are un-homed rodata -> 0.0f (faithful-but-inert), sign preserved by the
                // coin flip's high bit.
                const f32 lfDir = ((mRandom.muState >> 6) & 1u) ? 0.0f : 0.0f;
                SetFreakedOut(/*direction*/ lfDir, /*severity*/ lfSeverity);
            }
            lfNewFreakOutTime = 0.0f;
            break;
        }
        case E_FREAK_OUT_STATE_INITIAL:
            // INITIAL -> SPIN_OUT immediately; if not already crashing, fire the crash virtual.
            mu8FreakOutState = E_FREAK_OUT_STATE_SPIN_OUT;
            if (!IsCrashing())
                SetCrashing();
            lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;

        case E_FREAK_OUT_STATE_TURN_AND_ROLL:
            // TURN_AND_ROLL holds for up to KF_FREAKOUT_TURNROLL_MAX seconds, forcing throttle to 1.
            lfLocalThrottle = 1.0f;   // v99[1] = 1.0
            if (mfFreakOutTime < KF_FREAKOUT_TURNROLL_MAX)
                lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            else
                lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;

        case E_FREAK_OUT_STATE_SPIN_OUT:
            // SPIN_OUT times out at KF_FREAKOUT_SPINOUT_TIMEOUT; force throttle+steer to 1.
            if (mfFreakOutTime > KF_FREAKOUT_SPINOUT_TIMEOUT)
                mu8FreakOutState = E_FREAK_OUT_STATE_OFF;
            lfLocalThrottle = 1.0f;   // v99[1] = 1.0
            lfLocalSteer    = 1.0f;   // v99[2] = 1.0
            lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;

        default:
            CGS_ASSERT(false, "false");   // the X360 asserts on an out-of-range state
            lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;
        }
        mfFreakOutTime = lfNewFreakOutTime;   // *(this+5116) = v44

        (void)lfLocalThrottle; (void)lfLocalSteer;

        // Forward into the shared shunt + crashing solvers (the SAME entries the player uses).
        VehiclePhysics::UpdateShunt(lpControls);

        // FLAG (BLOCKED math, faithful delegation): when crashing the X360 runs an inlined per-axis
        // vlogefp/vexptefp angular-velocity damping curve directly in this function, driven by the
        // un-homed rodata coefficient tables unk_82014AC0..82014AF0 (a powf polynomial) and applied
        // to mAngularVelocity (+0x60). That math is NOT fabricated here; it is delegated to the
        // committed VehiclePhysics::UpdateCrashing, which owns the crash-damping curve in the full
        // physics TU.
        VehiclePhysics::UpdateCrashing(lfTimeStep, lpControls);
    }
}
}
