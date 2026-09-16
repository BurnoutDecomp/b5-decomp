#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cmath>    // std::cos (XMVectorCos), std::fabs
#include <cstdlib>  // [DIAG] getenv (BRN_AI_FAN_DIAG)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] BRN_AI_FAN_DIAG witness

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
        // [DIAG] NOT IN THE X360 BINARY (BRN_AI_FAN_DIAG=1). Prepare is the ONLY code that
        // zeroes mfCumulativeWeighting, and the measurement shows AccumulateWeightings writing
        // ~93 into that array while GetSpeedRatio reads 0 off the SAME object -- so if this is
        // running per-frame rather than once per race entry, it is wiping the fan every frame
        // and pinning every AI car at quarter speed.
        if (getenv("BRN_AI_FAN_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            static u32 suPrepCall = 0;
            ++suPrepCall;
            if (suPrepCall <= 12 || (suPrepCall % 240) == 0)
            {
                f32 lfMaxCumBefore = 0.0f;
                for (s32 liS = 0; liS < KI_FAN_STEPS; ++liS)
                {
                    const f32 lfA = (mfCumulativeWeighting[liS] < 0.0f) ? -mfCumulativeWeighting[liS]
                                                                         :  mfCumulativeWeighting[liS];
                    if (lfA > lfMaxCumBefore) lfMaxCumBefore = lfA;
                }
                *CgsDev::Log::gpDebugPrint
                    << "[aiprep] fan " << static_cast<s32>(reinterpret_cast<intptr_t>(this) & 0xFFFFFF)
                    << " Prepare call " << static_cast<s32>(suPrepCall)
                    << " wiping maxCum " << lfMaxCumBefore << "\n";
            }
        }

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

        // [DIAG] NOT IN THE X360 BINARY (BRN_AI_FAN_DIAG=1). CalculateDesiredSpeed scales the
        // AI's target speed by (ratio * 0.75 + 0.25), so a ratio of 0 is a HARD QUARTER-SPEED
        // CAP on every AI car -- measured as desired 15 mph against an 80 mph top speed, which
        // is both "the AI is too slow" and "the AI never boosts" (CheckForBoosting needs the
        // car to be BELOW its desired speed). ratio 0 means GetBestIndex returned ray 0, and
        // because that scan is "first wins on ties" starting at -FLT_MAX, ray 0 is exactly what
        // an ALL-EQUAL (e.g. all-zero) weighting array yields. So print the array's spread, not
        // just the index. Rate limited to one line per ~2 s across all cars.
        if (getenv("BRN_AI_FAN_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            static u32 suCall = 0;
            if ((suCall++ % 240) == 0)
            {
                f32 lfMin = mfCumulativeWeighting[0];
                f32 lfMax = mfCumulativeWeighting[0];
                s32 liNonZero = 0;
                for (s32 li = 0; li < KI_FAN_STEPS; ++li)
                {
                    const f32 lfW = mfCumulativeWeighting[li];
                    if (lfW < lfMin) lfMin = lfW;
                    if (lfW > lfMax) lfMax = lfW;
                    if (lfW != 0.0f) ++liNonZero;
                }
                *CgsDev::Log::gpDebugPrint
                    << "[aifan] fan " << static_cast<s32>(reinterpret_cast<intptr_t>(this) & 0xFFFFFF)
                    << " best " << liBest << " cube " << lfCube
                    << " weights min " << lfMin << " max " << lfMax
                    << " nonzero " << liNonZero << "/" << static_cast<s32>(KI_FAN_STEPS)
                    << "\n";
            }
        }
        if (lfCube >= KF_STEERING_FAN_SPEED_RATIO_KNEE)
            return 1.0f - (lfCube - KF_STEERING_FAN_SPEED_RATIO_KNEE) / (1.0f - KF_STEERING_FAN_SPEED_RATIO_KNEE);
        return 1.0f;
    }
}
