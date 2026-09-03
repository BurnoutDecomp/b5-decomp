#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cmath>    // std::cos (XMVectorCos), std::fabs

// BrnAI::SteeringFan -- the four small members the AIDriver chain needs every frame / at Prepare.
// The 26 weighting/target members (UpdateWeightings, GetDrivingTarget, the Include* contributors,
// ...) are NOT here; AIDriver gates its calls to them behind BRN_AI_RACINGLINE_STACK_PRESENT.
//
//   Prepare       @0x82778E40
//   SetBiasMode   @0x827693E8
//   GetBestIndex  @0x82768D48
//   GetSpeedRatio @0x82779B90
//
// Constants read from the image: flt_82F30428 == 1.3962634 (mfFanAngle, 80 degrees in radians),
// flt_82F302BC == 0.5 (GetSpeedRatio knee).

namespace BrnAI
{
    // GetSpeedRatio's knee: |((best/16 - 0.5)*2)^3| >= K -> ratio drops from 1.0 to 0.0 as the
    // best fan ray moves from the knee to the fan edge. Initialised .data @0x82F302BC == 0.5.
    const f32 KF_STEERING_FAN_SPEED_RATIO_KNEE = 0.5f;
    // The fan's half-angle. Initialised .data @0x82F30428 == 1.3962634 rad.
    const f32 KF_STEERING_FAN_ANGLE = 1.3962634f;

    // ====================================================================================
    // Prepare @0x82778E40
    //
    // Seed the fan for a new race entry: reciprocal step 1/16, look-ahead radius 10, fan angle,
    // clear all 14x17 weightings + the 17 cumulative slots, reset the bias mode (bumping the
    // state counter if it changes), register the PerfMon monitors once, then build the travel-
    // direction bias table: for ray i, t = i/16, x = ((t - 0.5) * 2)^3 * fanAngle,
    // bias[i] = |cos(x)| (the vcmpgefp/vxor pair negates a negative cosine).
    // ====================================================================================
    void SteeringFan::Prepare()
    {
        mfReciprocalSteps = 0.0625f;                  // +2036 (1 / (KI_FAN_STEPS - 1))
        miStateCounter    = 0;                        // +2052
        mfLookAheadRadius = 10.0f;                    // +2040
        mfFanAngle        = KF_STEERING_FAN_ANGLE;    // +2048 (flt_82F30428)

        // 17 columns x (1 cumulative + 14 contributor rows) cleared (the X360 walks the columns
        // in the outer loop, rows inner; same final state).
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
        {
            mfCumulativeWeighting[liStep] = 0.0f;
            for (s32 liRow = 0; liRow < E_FAN_CONTRIBUTORS_COUNT; ++liRow)
                mfWeighting[liRow][liStep] = 0.0f;
        }

        // Bias mode back to 0; a change bumps the state counter (SetBiasMode's rule, inlined).
        if (meBiasMode != 0)
            ++miStateCounter;
        meBiasMode = static_cast<EBiasMode>(0);

        // X360 @0x82778EC8..: `if (dword_82F302D8 == -1) { AddMonitor("Steering Fan 0" .. "6") }`
        // -- the six CgsDev::PerfMonCpu monitors (miSteeringFanPM[7], DWARF :338). Presentation-only
        // profiling registration; intentionally not reproduced on the host build (no PerfMon page).

        mbPointAheadKnown = false;                    // +2056

        f32 lfT = 0.0f;
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
        {
            const f32 lfInterp = (lfT - 0.5f) * 2.0f;
            const f32 lfAngle  = (lfInterp * lfInterp * lfInterp) * mfFanAngle;
            const f32 lfCos    = std::cos(lfAngle);              // XMVectorCos, lane 0
            // vcmpgefp cos >= 0 ? cos : cos ^ signbit  ==  |cos|
            mTravelDirectionBias[liStep] = (lfCos >= 0.0f) ? lfCos : -lfCos;
            lfT += mfReciprocalSteps;
        }
    }

    // ====================================================================================
    // SetBiasMode @0x827693E8
    //
    // Assert the mode is in range (< 10), store it, and bump the state counter when it changed
    // (the counter invalidates cached fan results).
    // ====================================================================================
    void SteeringFan::SetBiasMode(EBiasMode leBiasMode)
    {
        CGS_ASSERT(static_cast<u32>(leBiasMode) < static_cast<u32>(E_BIAS_MODE_COUNT),
                   "Bad Bias mode set in Steering Fan");

        const EBiasMode lePrevious = meBiasMode;     // this[236]
        meBiasMode = leBiasMode;
        if (lePrevious != leBiasMode)
            ++miStateCounter;                        // this[513]
    }

    // ====================================================================================
    // GetBestIndex @0x82768D48
    //
    // Index of the largest cumulative weighting (first wins on ties; starts from -FLT_MAX). The
    // X360 unrolls the 17-slot scan 8-wide + a 1-wide tail; re-rolled.
    // ====================================================================================
    s32 SteeringFan::GetBestIndex()
    {
        s32 liBest   = 0;
        f32 lfBest   = -3.4028235e38f;
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
        {
            if (mfCumulativeWeighting[liStep] > lfBest)
            {
                lfBest = mfCumulativeWeighting[liStep];
                liBest = liStep;
            }
        }
        return liBest;
    }

    // ====================================================================================
    // GetSpeedRatio @0x82779B90
    //
    // How "straight" the chosen fan ray is, as a 0..1 speed multiplier for CalculateDesiredSpeed:
    // v = ((best * 1/16) - 0.5) * 2 ; c = |v^3| ; ratio = c >= knee ? 1 - (c - knee)/(1 - knee) : 1.
    // ====================================================================================
    f32 SteeringFan::GetSpeedRatio()
    {
        const s32 liBest = GetBestIndex();
        const f32 lfV    = (static_cast<f32>(liBest) * 0.0625f - 0.5f) * 2.0f;
        const f32 lfCube = std::fabs(lfV * lfV * lfV);
        if (lfCube >= KF_STEERING_FAN_SPEED_RATIO_KNEE)
            return 1.0f - (lfCube - KF_STEERING_FAN_SPEED_RATIO_KNEE) / (1.0f - KF_STEERING_FAN_SPEED_RATIO_KNEE);
        return 1.0f;
    }
}
