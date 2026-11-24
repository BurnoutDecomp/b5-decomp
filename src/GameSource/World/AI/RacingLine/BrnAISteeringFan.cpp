#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cfloat>   // FLT_MAX (0x7F7FFFFF)
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
        f32 lfBest   = -FLT_MAX;
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
        if (lfCube >= KF_STEERING_FAN_SPEED_RATIO_KNEE)
            return 1.0f - (lfCube - KF_STEERING_FAN_SPEED_RATIO_KNEE) / (1.0f - KF_STEERING_FAN_SPEED_RATIO_KNEE);
        return 1.0f;
    }
}

// FOLDED FROM BrnAISteeringFan_Aggression.cpp
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan_AggressionConstants.h"

#include "GameSource/World/AI/BrnAICar.h"                             // AICar getters
#include "GameSource/World/AI/BrnAIDriver.h"                          // NearbyVehicle / NearbyVehicles
#include "GameSource/World/AI/BrnAIUtils.h"                           // DistancePosVelToOrigin
#include "GameSource/World/AI/Route/BrnRacingLine.h"                  // RacingLine
#include "GameSource/Math/BrnMathUtils.h"                             // BrnMath::Flatten
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                    // CGS_ASSERT

#include <cfloat>   // FLT_MAX (flt_8204F664, FindNeabyAIInTraffic's lfBestProximity seed)
#include <cmath>    // std::sqrt / std::fabs (the vrsqrtefp and vandc sequences)

// BrnAI::SteeringFan -- the AGGRESSION contributors (aiwave 2026-09-05 lane F2b). Third partfile
// of the weighting half; BrnAISteeringFan_Weightings.cpp owns the racing-line contributors and
// BrnAISteeringFan_Target.cpp the target/accumulate half. Every body here was parked in
// _Weightings.cpp as a `(void)`-argument stub; those stubs are deleted at mount time.
//
//   BODIED
//     IncludeSmashIntoTarget        @0x82787968  (writes whichever row the caller names)
//     FindNeabyAIInTraffic          @0x82787BE8  (sic: DWARF spelling; NO IDA export -- image bytes)
//     IncludeSmashIntoPlayer        @0x82791230  (row eFan_SmashIntoPlayer)
//     IncludeSmashIntoNearbyAI      @0x82791338  (row eFan_SmashIntoRivals)
//     IncludeDriveCloseToPlayer     @0x82787E58  (row eFan_DriveCloseToPlayer)
//     IncludeDriftDirectionTracking @0x827881B0  (row eFan_DriftFinalDirection)
//     IncludeDriftLocationTracking  @0x82788738  (row eFan_DriftFinalLocation)
//
// SHARED REGISTER MAP. UpdateWeightings @0x82794600 calls these with r3 = this and a permutation
// of r26 = lpCar, r27 = lpRacingLine, r25 = lpNearbyVehicles, r24 = leAggressionVictim; each
// banner below names its own.
//
// .rdata constants this TU reads (image.bin, big-endian, file offset == VA - 0x82000000):
//   flt_82001C98 == 1.0     flt_82001CC0 == 0.0     flt_820C4168 == 0.5
//   flt_820037C8 == -1.0    flt_82004C78 == -0.5    flt_820C3B70 == 1.1920929e-07 (FLT_EPSILON)
//   flt_820047C8 == 0.05    flt_820C4880 == 3.5     flt_820C488C == 5.0
//   flt_820C4844 == 4.5     flt_820C4890 == 20.0    flt_820C8074 == -4.5
//   flt_820C3FAC == 100.0   flt_8204F664 == 3.4028235e38 (FLT_MAX)
// The two .bss tunables live in BrnAISteeringFan_AggressionConstants.h with their initialisers.
//
// FAN GEOMETRY. mUnitDirection[i] (this+0x220, stride 0x10) is ray i's unit 2D direction and
// mfWeighting[row] (this+0x3B4, stride 0x44 == 17*4) is the row each contributor writes. No
// contributor here calls WriteWeightingValues (the console's debug overlay hook); the bodied
// contributors in _Weightings.cpp do not either.

namespace BrnAI
{
namespace
{
    // flt_820C3B70 == 1.1920929e-07 (FLT_EPSILON) -- the per-component epsilon every
    // rw::math::vpu::IsZero the console inlines here tests against (`vandc` to drop the sign bit,
    // then `vcmpgtfp` against the splatted epsilon). Same constant _Weightings.cpp calls
    // KF_FAN_TINY; kept TU-local (internal linkage) so the two partfiles cannot drift apart.
    const f32 KF_AGGRESSION_TINY = 1.1920928955078125e-07f;

    // ARTIST 827879D8..87AA0: dot, rsqrt estimate and two refinements.
    __vector4 LengthSquared2DAggression(const Vector2& lVector)
    {
        const __vector4 lSquare = __vmulfp(lVector.mV, lVector.mV);
        return __vaddfp(__vspltw(lSquare, 0), __vspltw(lSquare, 1));
    }
    f32 Dot2DAggression(const Vector2& lA, const Vector2& lB)
    {
        const __vector4 lProduct = __vmulfp(lA.mV, lB.mV);
        return __vaddfp(__vspltw(lProduct, 0), __vspltw(lProduct, 1)).x;
    }
    __vector4 ReciprocalLength2DAggression(__vector4 lLength)
    {
        const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
        __vector4 lEstimate = __vrsqrtefp(lLength);
        __vector4 lSquare = __vmulfp(lEstimate, lEstimate);
        __vector4 lHalfEstimate = __vmulfp(lEstimate, lHalf);
        __vector4 lCorrection = __vnmsubfp(lLength, lSquare, lOne);
        lEstimate = __vmaddfp(lHalfEstimate, lCorrection, lEstimate);
        lSquare = __vmulfp(lEstimate, lEstimate);
        lHalfEstimate = __vmulfp(lEstimate, lHalf);
        lCorrection = __vnmsubfp(lLength, lSquare, lOne);
        return __vmaddfp(lHalfEstimate, lCorrection, lEstimate);
    }
    Vector2 Normalize2DAggression(const Vector2& lVector)
    {
        Vector2 lOut;
        lOut.mV = __vmulfp(lVector.mV, ReciprocalLength2DAggression(LengthSquared2DAggression(lVector)));
        return lOut;
    }
    f32 Magnitude2DAggression(const Vector2& lVector)
    {
        const __vector4 lLength = LengthSquared2DAggression(lVector);
        const __vector4 lZero = __vspltisw(0);
        return __vsel(__vmulfp(lLength, ReciprocalLength2DAggression(lLength)), lZero,
                      __vcmpeqfp(lZero, lLength)).x;
    }

    // The `vandc` (drop the sign bit) + `vcmpgtfp` pair the console emits for
    // rw::math::vpu::IsZero on a Vector2: true when NEITHER component exceeds the epsilon. Each
    // lane is tested with a non-dot vcmpgtfp, gathered by vperm and `cmpwi ; bne/beq` (e.g.
    // IncludeDriftDirectionTracking 0x8278825C..0x8278828C) -- a NaN lane is never "greater", so
    // it counts as zero. Spelt that way round (crash parity FX-AINAN2; `fabs <= tiny` called a
    // NaN non-zero).
    bool IsZero2DAggression(const Vector2& lVector)
    {
        return !(std::fabs(lVector.mV.x) > KF_AGGRESSION_TINY)
            && !(std::fabs(lVector.mV.y) > KF_AGGRESSION_TINY);
    }

    // The `fsel f12,(lo-v),lo,v ; fsel f12,(hi-v),v,hi` ladder, i.e. rw::math::vpu::Clamp (lo == 0:
    // `fneg ; fsel`), e.g. IncludeDriftDirectionTracking 0x8278831C..0x82788330, IncludeSmashIntoTarget
    // 0x82787AC4..0x82787ADC, IncludeDriveCloseToPlayer 0x82788164..0x82788170. fsel takes its THIRD
    // operand on an unordered test, so a NaN comes back as lfHigh (crash parity FX-AINAN2; the old
    // `if (< lo) .. if (> hi) ..` returned the NaN).
    f32 ClampAggression(f32 lfValue, f32 lfLow, f32 lfHigh)
    {
        const f32 lfFloored = ((lfLow - lfValue) >= 0.0f) ? lfLow : lfValue;
        return ((lfHigh - lfFloored) >= 0.0f) ? lfFloored : lfHigh;
    }

    // The 2D cross product rw::math::vpu::Cross reduces to on a Vector2
    // (IncludeDriveCloseToPlayer @0x8278807C..84).
    f32 Cross2DAggression(const Vector2& lA, const Vector2& lB)
    {
        const __vector4 lXY = __vmulfp(__vspltw(lA.mV, 0), __vspltw(lB.mV, 1));
        const __vector4 lYX = __vmulfp(__vspltw(lA.mV, 1), __vspltw(lB.mV, 0));
        return __vsubfp(lXY, lYX).x;
    }

    // Zero one contributor row. Every Include* below has an arm that does exactly this
    // (`li r10,0x11 ; mtctr ; stw 0,0(r11) ; addi r11,r11,4 ; bdnz`).
    void ZeroContributorRow(f32* lpRow)
    {
        for (s32 liFanIndex = 0; liFanIndex < KI_FAN_STEPS; ++liFanIndex)
            lpRow[liFanIndex] = 0.0f;
    }
}

// ========================================================================================
// IncludeSmashIntoTarget @0x82787968   (159 instructions)
//   r31 = this, r30 = lpCar, r28 = lpTarget, r29 = leContributor,
//   f31 = lfMinAheadness (f1), f30 = lfMaxAheadness (f2).
// DWARF BrnAISteeringFan.cpp:2005; locals :2007..:2015.
//
//   0x827879A0  lCarPosition  = BrnMath::Flatten(lpCar->GetPosition())        -> v127
//   0x827879B8  lCarDirection = BrnMath::Flatten(lpCar->GetDirection())       -> v1
//   0x827879D0  lvx128 v0, r28, 0x10  == lpTarget->mCentre
//   0x827879D8  lRelativeVector = lpTarget->mCentre - lCarPosition            -> v12
//   0x827879C8..A4C  two refined vrsqrtefp chains: v10 = 1/|lCarDirection|, v9 = 1/|lRelativeVector|
//   0x82787A48  v8  = lCarDirection * v10                  == Normalize(lCarDirection)
//   0x82787A54  v10 = lRelativeVector * v9                 == lRelativeDirection
//   0x82787A5C..AA8  lfAheadness = Dot(Normalize(lCarDirection), lRelativeVector)  (a DISTANCE
//               along the car's forward axis, NOT a cosine -- the separation is NOT normalised
//               on this side of the dot, which is why the callers pass metres: -4.5 .. 20.0)
//   0x82787A70..AA0  lfSeparation = Magnitude(lRelativeVector), with the vcmpeqfp/vsel zero guard,
//               stored to var_70 and read back at 0x82787AAC BEFORE 0x82787ABC overwrites var_70
//               with lfAheadness. (Both scalars share the slot; the read order is load-bearing.)
//   0x82787AB0..AE0  lfProximity = 1.0 - Clamp(lfSeparation * flt_820047C8, 0, 1)
//               flt_820047C8 == 0.05 == 1/20 == 1/KF_MAX_SEPERATION_FOR_SLAM (DWARF :2015); the
//               console compiler folded the division into the reciprocal.
//   0x82787AE4  lfProximity == 0.0 -> zero the row
//   0x82787AF0  lfAheadness < lfMinAheadness -> zero the row
//   0x82787AF8  lfAheadness > lfMaxAheadness -> zero the row
//   0x82787B00  row base = this + leContributor*0x44 + 0x3B4 == mfWeighting[leContributor]
//   0x82787B20..BA0  per ray: ((Dot(lRelativeDirection, mUnitDirection[i]) + 1.0) * 0.5) * lfProximity
//               (the 1.0 and the 0.5 are built as one-lane Vector2 scalars on the stack --
//                flt_82001C98 into var_80 and flt_820C4168 into var_70 -- then vspltw'd; that is
//                the VecFloat idiom, not two more constants.)
// The row is written in [0,1] scaled by lfProximity: rays pointing AT the target score highest,
// rays pointing away score 0. kfBias supplies the sign/strength (AccumulateWeightings @0x82779088).
// ========================================================================================
void SteeringFan::IncludeSmashIntoTarget(AICar* lpCar, const NearbyVehicle* lpTarget,
                                         EFan_Contributors leContributor,
                                         f32 lfMinAheadness, f32 lfMaxAheadness)
{
    // DWARF BrnAISteeringFan.cpp:2015 -- declared inside the body, not at file scope.
    const f32 KF_RECIP_MAX_SEPERATION_FOR_SLAM = 0.05f; // ARTIST 82787AB0/AB8

    const Vector2 lCarPosition  = BrnMath::Flatten(lpCar->GetPosition());    // @0x827879A0
    const Vector2 lCarDirection = BrnMath::Flatten(lpCar->GetDirection());   // @0x827879B8

    Vector2 lRelativeVector;
    lRelativeVector.mV = __vsubfp(lpTarget->mCentre.mV, lCarPosition.mV); // ARTIST 827879D8

    const Vector2 lRelativeDirection = Normalize2DAggression(lRelativeVector);
    const f32     lfSeparation       = Magnitude2DAggression(lRelativeVector);
    const f32     lfAheadness        = Dot2DAggression(Normalize2DAggression(lCarDirection),
                                                       lRelativeVector);

    const f32 lfProximity =
        1.0f - ClampAggression(lfSeparation * KF_RECIP_MAX_SEPERATION_FOR_SLAM, 0.0f, 1.0f);

    f32* lpRow = mfWeighting[leContributor];

    if (lfProximity == 0.0f || lfAheadness < lfMinAheadness || lfAheadness > lfMaxAheadness)
    {
        ZeroContributorRow(lpRow);                                          // @0x82787BA8
        return;
    }

    for (s32 liFanIndex = 0; liFanIndex < KI_FAN_STEPS; ++liFanIndex)       // @0x82787B20
    {
        const f32 lfAlignment = Dot2DAggression(lRelativeDirection, mUnitDirection[liFanIndex]);
        lpRow[liFanIndex] = ((lfAlignment + 1.0f) * 0.5f) * lfProximity;
    }
}

// ========================================================================================
// FindNeabyAIInTraffic @0x82787BE8   (sic: the DWARF's spelling, BrnAISteeringFan.cpp:2048)
//   r31 = lpCar, r30 = the walking NearbyVehicle*, r29 = lpBestRival, f30 = lfOurSpeed,
//   f31 = lfBestProximity, v127 = lOurPosition, v126 = lOurMotion.
//
// NO IDA EXPORT EXISTS for 0x82787BE8 -- .ida-exports/BURNOUT_X360_ARTIST.XEX has no
// 0x82787BE8.json and scratch/aiwave2/names.tsv has no row for it, but IDA knows the name (it is
// in IncludeSmashIntoNearbyAI's xrefs_from). The whole body below was disassembled from the image
// bytes 0x82787BE8..0x82787E50 with tools/re/ppcdis.py; it is NOT inlined, IncludeSmashIntoNearbyAI
// really does `bl` it.
//
//   0x82787C20  f31 = flt_8204F664 == FLT_MAX                     (lfBestProximity seed)
//   0x82787C24  f30 = lpCar->GetSpeed()                           (lfOurSpeed)
//   0x82787C34  v127 = BrnMath::Flatten(lpCar->GetPosition())     (lOurPosition)
//   0x82787C4C  v126 = BrnMath::Flatten(lpCar->GetUsefulDirection()) (lOurMotion)
//   0x82787C60  GetCount() == 0 -> return NULL
//   constants hoisted out of the loop: f13 = flt_82001CC0 == 0.0, f10 = flt_820C4890 == 20.0,
//                                      f11 = 0x8300D6F8 (KF_SLAM_AHEADNESS),
//                                      f12 = 0x8300D75C (KF_SLAM_FROM_BEHIND_RELATIVE_SPEED)
//   loop, stride 0x70 from lpNearbyTraffic+0 (== mVehicle[i]):
//     0x82787C8C  mType (entry+0x20) != E_NEARBY_AI (1) -> next
//     0x82787CA8  lRelativePosition = mCentre - lOurPosition
//     0x82787CD0..D00  lfSeparation = Magnitude(lRelativePosition) (zero-guarded)
//     0x82787D04  lfSeparation > KF_AI_SMASH_SEPERATION (flt_820C4890 == 20.0) -> next
//     0x82787D0C  lfSeparation == 0.0 -> next
//     0x82787D50  lRelativePosition = Normalize(lRelativePosition)   <- REASSIGNED IN PLACE
//     0x82787D54..68  lfAheadness = Dot(lRelativePosition, lOurMotion)
//     0x82787D6C  lfAheadness < 0.0 -> next
//     0x82787D74..DCC  lfRelativeSpeed = lfOurSpeed - Magnitude(entry->mVelocity)
//     0x82787DE8  lfRelativeSpeed >  KF_SLAM_FROM_BEHIND_RELATIVE_SPEED -> keep the candidate
//     0x82787DF0  else lfAheadness > KF_SLAM_AHEADNESS -> next
//     0x82787DF8  lfProximity = MagnitudeSquared(lRelativePosition)
//     0x82787E14  lfProximity >= lfBestProximity -> next; else it is the new best
//
// CONSOLE QUIRK, REPRODUCED AS-IS. lfProximity is MagnitudeSquared of the ALREADY-NORMALISED
// lRelativePosition (`vmulfp128 v0, v10, v10` with v10 the unit vector written at 0x82787D50), so
// it is 1.0 for every candidate to within the rsqrt refinement error. With the FLT_MAX seed the
// FIRST qualifying rival therefore wins and no later one can displace it (1.0 >= 1.0 takes the
// `bge` skip). The DWARF confirms the shape -- MagnitudeSquared is called exactly once, and
// lRelativePosition is one variable that Normalize reassigns -- so this is the original source's
// own bug, not a transcription slip. Taking MagnitudeSquared before the Normalize would change
// rival selection; do NOT "fix" it.
// ========================================================================================
const NearbyVehicle* SteeringFan::FindNeabyAIInTraffic(const NearbyVehicles* lpNearbyTraffic,
                                                       AICar* lpCar)
{
    // DWARF BrnAISteeringFan.cpp:2076 -- declared inside the loop body. flt_820C4890 == 20.0.
    const f32 KF_AI_SMASH_SEPERATION = 20.0f;

    const NearbyVehicle* lpBestRival     = 0;                               // r29
    f32                  lfBestProximity = FLT_MAX;                         // flt_8204F664
    const f32            lfOurSpeed      = lpCar->GetSpeed();               // @0x82787C24
    const Vector2        lOurPosition    = BrnMath::Flatten(lpCar->GetPosition());
    const Vector2        lOurMotion      = BrnMath::Flatten(lpCar->GetUsefulDirection());

    const s32 liCount = lpNearbyTraffic->GetCount();                        // @0x82787C60
    for (s32 liTrafficIndex = 0; liTrafficIndex < liCount; ++liTrafficIndex)
    {
        const NearbyVehicle* lpRival = &lpNearbyTraffic->mVehicle[liTrafficIndex];
        if (lpRival->mType != E_NEARBY_AI)                                  // @0x82787C98
            continue;

        Vector2 lRelativePosition;
        lRelativePosition.mV = __vsubfp(lpRival->mCentre.mV, lOurPosition.mV);

        const f32 lfSeparation = Magnitude2DAggression(lRelativePosition);
        if (lfSeparation > KF_AI_SMASH_SEPERATION)                          // @0x82787D04
            continue;
        if (lfSeparation == 0.0f)                                           // @0x82787D0C
            continue;

        lRelativePosition = Normalize2DAggression(lRelativePosition);       // @0x82787D50

        const f32 lfAheadness = Dot2DAggression(lRelativePosition, lOurMotion);
        if (lfAheadness < 0.0f)                                             // @0x82787D6C
            continue;

        // `fcmpu relSpeed, K ; bgt -> keep` @0x82787DE8/0x82787DEC: only an ORDERED relSpeed > K
        // skips the aheadness test, so a NaN relative speed still gets it (FX-AINAN2; `<=` kept
        // the NaN candidate outright).
        const f32 lfRelativeSpeed = lfOurSpeed - Magnitude2DAggression(lpRival->mVelocity);
        if (!(lfRelativeSpeed > KF_SLAM_FROM_BEHIND_RELATIVE_SPEED)         // @0x82787DE8
            && lfAheadness > KF_SLAM_AHEADNESS)                             // @0x82787DF0
        {
            continue;
        }

        // See the CONSOLE QUIRK note above: lRelativePosition is the UNIT vector here.
        const f32 lfProximity = Dot2DAggression(lRelativePosition, lRelativePosition);
        if (lfProximity < lfBestProximity)                                  // @0x82787E14
        {
            lfBestProximity = lfProximity;
            lpBestRival     = lpRival;
        }
    }

    return lpBestRival;
}

// ========================================================================================
// IncludeSmashIntoPlayer @0x82791230   (65 instructions)
//   r28 = this, r25 = lpCar, r26 = lpNearbyTraffic, r27 = leAggressionVictim.
// DWARF BrnAISteeringFan.cpp:1949.
//
//   0x8279124C  leAggressionVictim == -1 -> the console's CGS_ASSERT
//               ("Invalid global race car ID used in slam", BrnAISteeringFan.cpp:1952). It is a
//               StrStream-formatted assert the retail build DID emit, and it is NOT a guard:
//               control falls straight through into FindVictimInTraffic afterwards.
//   0x827912D8  lpPlayer = FindVictimInTraffic(lpNearbyTraffic, leAggressionVictim)
//   0x827912E8  no victim -> zero this+0x6A0 (== mfWeighting[11], eFan_SmashIntoPlayer), return
//   0x8279130C  f1 = flt_820C8074 == -4.5, f2 = flt_820C4890 == 20.0, r6 = 0xB
//               -> IncludeSmashIntoTarget(lpCar, lpPlayer, eFan_SmashIntoPlayer, -4.5, 20.0)
// The aheadness window is asymmetric because IncludeSmashIntoTarget's lfAheadness is in METRES:
// the victim may be up to 20 m ahead along our forward axis but only 4.5 m behind it.
// ========================================================================================
void SteeringFan::IncludeSmashIntoPlayer(AICar* lpCar, const NearbyVehicles* lpNearbyVehicles,
                                         EGlobalRaceCarIndex leAggressionVictim)
{
    CGS_ASSERT(leAggressionVictim != -1, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid global race car ID used in slam\n", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/RacingLine/BrnAISteeringFan.cpp", 1952));

    const NearbyVehicle* lpPlayer = FindVictimInTraffic(lpNearbyVehicles, leAggressionVictim);
    if (lpPlayer == 0)
    {
        ZeroContributorRow(mfWeighting[eFan_SmashIntoPlayer]);              // @0x827912E8
        return;
    }

    IncludeSmashIntoTarget(lpCar, lpPlayer, eFan_SmashIntoPlayer,
                           -4.5f,          // f1 = flt_820C8074
                           20.0f);         // f2 = flt_820C4890
}

// ========================================================================================
// IncludeSmashIntoNearbyAI @0x82791338   (35 instructions)
//   r31 = this, r30 = lpCar, r5 = lpNearbyTraffic.
// DWARF BrnAISteeringFan.cpp:1976.
//
//   0x8279135C  lpNearbyAI = FindNeabyAIInTraffic(lpNearbyTraffic, lpCar)
//               (the console swaps its own arguments into r4 = r5, r5 = r30 first)
//   0x8279136C  none -> zero this+0x728 (== mfWeighting[13], eFan_SmashIntoRivals), return
//   0x8279138C  f1 = flt_820C8074 == -4.5, f2 = flt_820C4844 == 4.5, r6 = 0xD
//               -> IncludeSmashIntoTarget(lpCar, lpNearbyAI, eFan_SmashIntoRivals, -4.5, 4.5)
// The rival window is SYMMETRIC (+/-4.5 m) where the player window reaches 20 m ahead: a rival is
// only slammed when it is roughly alongside.
// ========================================================================================
void SteeringFan::IncludeSmashIntoNearbyAI(AICar* lpCar, const NearbyVehicles* lpNearbyVehicles)
{
    const NearbyVehicle* lpNearbyAI = FindNeabyAIInTraffic(lpNearbyVehicles, lpCar);
    if (lpNearbyAI == 0)
    {
        ZeroContributorRow(mfWeighting[eFan_SmashIntoRivals]);              // @0x8279136C
        return;
    }

    IncludeSmashIntoTarget(lpCar, lpNearbyAI, eFan_SmashIntoRivals,
                           -4.5f,          // f1 = flt_820C8074
                           4.5f);          // f2 = flt_820C4844
}

// ========================================================================================
// IncludeDriveCloseToPlayer @0x82787E58   (214 instructions)
//   r30 = this, r31 = lpRacingLine, r28 = lpCar, r29 = lpNearbyTraffic then lpPlayer,
//   f26 = lfOurCarSpeed, v126 = lOurCarPosition, v127 = lPlayerUnitVelocity, f27 = lfCarSide.
// DWARF BrnAISteeringFan.cpp:2186; locals :2188..:2264.
//
//   0x82787E90  lfOurCarSpeed = lpCar->GetSpeed()
//   0x82787EA0  lpPlayer = FindPlayerInTraffic(lpNearbyTraffic)
//   0x82787EB0  no player -> zero this+0x6E4 (== mfWeighting[12]), return
//   0x82787ED8  lOurCarPosition: GetPosition(lpCar) then `vrlimi128 v10,v12,8,0` (lane 0 <- x)
//               and `vrlimi128 v126,v12,4,1` (lane 1 <- source lane 2 == z) -- the SAME XZ flatten
//               BrnMath::Flatten performs, written inline instead of called.
//   0x82787F10  lpRacingLine->mfImmmediateApproachSpeedOfTrafficAhead (+0xB20) = flt_82001CC0 == 0.0
//   0x82787F14  lpRacingLine->mfImmediateDistanceToTrafficImpact     (+0xB1C) = flt_820C3FAC == 100.0
//               (the proximity-speed inputs AIDriver::ProximitySpeed @0x82770800 reads; this
//                contributor resets them to "nothing ahead" before doing its own work.)
//   0x82787F1C..FDC  IsZero(lpPlayer->mVelocity) -> zero the row, return
//   0x82787FE0..8030  lPlayerUnitVelocity = Normalize(lpPlayer->mVelocity)
//   0x82788034  lPosA = BrnMath::Flatten(lpCar->GetPosition())   (a SECOND GetPosition call; the
//               DWARF lists AICar::GetPosition twice and BrnMath::Flatten once)
//   0x82788058..88  lPosB = lpPlayer->mCentre, lPosC = lPosB + lPlayerUnitVelocity,
//               lfCarSide = Cross(lPosC - lPosA, lPosB - lPosA)
//   0x827880A4  f27 = (lfCarSide >= 0) ? flt_82001C98 (1.0) : flt_820037C8 (-1.0)
//   loop i = 0..16 from 0x827880AC (r30 walks this+0x220, r25 walks this+0x6E4):
//     0x827880BC  lRelativePosition = lOurCarPosition - lpPlayer->mCentre
//     0x827880E8  lOurCarMotion     = mUnitDirection[i] * lfOurCarSpeed
//     0x827880F0  lRelativeVelocity = lOurCarMotion - lpPlayer->mVelocity            (-> v2)
//     0x827880EC..108  lfAheadness = Dot(lRelativePosition, mUnitDirection[i]) + KF_PROJECT_AHEAD
//                  (flt_820C488C == 5.0, DWARF :2260)
//     0x82788128  v1 = lRelativePosition - lPlayerUnitVelocity * that sum
//     0x82788130  BrnAI::DistancePosVelToOrigin(v1, v2)
//     0x82788138  lfPassingSpace = result * lfCarSide - KF_DESIRED_CLOSE_PASSING_SEPERATION (3.5)
//     0x82788140  lfPassingSpace >= 0 -> row[i] =  1.0 - Clamp(lfPassingSpace * 0.5, 0, 1)
//                 else                   row[i] = -Clamp(lfPassingSpace * -0.5, 0, 1)
//                 (0.5 == 1/KF_CLOSE_PASSING_RANGE; see the constants header.)
// The row is therefore +1 when the ray would put us exactly at the desired separation on the
// correct side and ramps to 0 as we drift KF_CLOSE_PASSING_RANGE beyond it; rays that would put
// us on the WRONG side (or inside the separation) score NEGATIVE.
// ========================================================================================
void SteeringFan::IncludeDriveCloseToPlayer(RacingLine* lpRacingLine, AICar* lpCar,
                                            const NearbyVehicles* lpNearbyVehicles)
{
    // DWARF BrnAISteeringFan.cpp:2260 -- declared inside the loop body. flt_820C488C == 5.0.
    const f32 KF_PROJECT_AHEAD = 5.0f;

    const f32 lfOurCarSpeed = lpCar->GetSpeed();                            // @0x82787E90

    const NearbyVehicle* lpPlayer = FindPlayerInTraffic(lpNearbyVehicles);  // @0x82787EA0
    if (lpPlayer == 0)
    {
        ZeroContributorRow(mfWeighting[eFan_DriveCloseToPlayer]);           // @0x82787EB0
        return;
    }

    // The inline vrlimi128 XZ flatten (@0x82787EFC / @0x82787F2C), not a BrnMath::Flatten call.
    const Vector3 lOurCarPosition3D = lpCar->GetPosition();
    Vector2 lOurCarPosition;
    lOurCarPosition.mV = __vrlimi(lOurCarPosition3D.mV, lOurCarPosition3D.mV, 4, 1);

    lpRacingLine->mfImmmediateApproachSpeedOfTrafficAhead = 0.0f;           // @0x82787F10
    lpRacingLine->mfImmediateDistanceToTrafficImpact      = 100.0f;         // @0x82787F14

    if (IsZero2DAggression(lpPlayer->mVelocity))                            // @0x82787FC4
    {
        ZeroContributorRow(mfWeighting[eFan_DriveCloseToPlayer]);
        return;
    }

    const Vector2 lPlayerUnitVelocity = Normalize2DAggression(lpPlayer->mVelocity);  // DWARF :2207

    // DWARF :2220 -- loop-invariant, so the console recomputes it at 0x827880BC every step from
    // the same two loads; hoisted here to where the source declares it.
    Vector2 lRelativePosition;
    lRelativePosition.mV = __vsubfp(lOurCarPosition.mV, lpPlayer->mCentre.mV);

    const Vector2 lPosA = BrnMath::Flatten(lpCar->GetPosition());           // DWARF :2224, @0x82788034
    const Vector2 lPosB = lpPlayer->mCentre;                                // DWARF :2225

    Vector2 lPosC;                                                          // DWARF :2226
    lPosC.mV = __vaddfp(lPosB.mV, lPlayerUnitVelocity.mV);

    Vector2 lToPosB;
    lToPosB.mV = __vsubfp(lPosB.mV, lPosA.mV);

    Vector2 lToPosC;
    lToPosC.mV = __vsubfp(lPosC.mV, lPosA.mV);

    // DWARF :2232, @0x827880A4 (`fsel f27, lfCarSide, 1.0, -1.0`): +1 on one side of the player's
    // line of travel, -1 on the other, which is what flips the sign of lfPassingSpace below.
    const f32 lfCarSide = (Cross2DAggression(lToPosC, lToPosB) >= 0.0f) ? 1.0f : -1.0f;

    for (s32 liFanIndex = 0; liFanIndex < KI_FAN_STEPS; ++liFanIndex)       // @0x827880AC
    {
        const Vector2& lUnitDirection = mUnitDirection[liFanIndex];

        Vector2 lOurCarMotion;
        __vector4 lSpeed;
        lSpeed.x = lfOurCarSpeed;
        lOurCarMotion.mV = __vmulfp(lUnitDirection.mV, __vspltw(lSpeed, 0));

        Vector2 lRelativeVelocity;
        lRelativeVelocity.mV = __vsubfp(lOurCarMotion.mV, lpPlayer->mVelocity.mV);

        // DWARF :2256 lfAheadness (the raw dot) then :2260 KF_PROJECT_AHEAD; the console adds them
        // at 0x82788108 (`fadds f0, f13, flt_820C488C`) and splats the sum at 0x82788124.
        const f32 lfAheadness   = Dot2DAggression(lRelativePosition, lUnitDirection);
        const f32 lfProjectHere = lfAheadness + KF_PROJECT_AHEAD;

        Vector2 lProjectedPosition;
        __vector4 lProject;
        lProject.x = lfProjectHere;
        lProjectedPosition.mV = __vsubfp(lRelativePosition.mV, __vmulfp(lPlayerUnitVelocity.mV, __vspltw(lProject, 0)));

        const f32 lfPassingSpace =
            DistancePosVelToOrigin(lProjectedPosition, lRelativeVelocity) * lfCarSide
            - KF_DESIRED_CLOSE_PASSING_SEPERATION;                          // @0x82788138

        // `fcmpu ps, 0.0 ; bge` @0x8278813C/0x82788140: a NaN passing space takes this arm, whose
        // clamp then answers 1.0 -> a 0.0 weight (FX-AINAN2; `>=` sent it to the negative arm).
        if (!(lfPassingSpace < 0.0f))                                       // @0x82788140
        {
            mfWeighting[eFan_DriveCloseToPlayer][liFanIndex] =
                1.0f - ClampAggression(lfPassingSpace / KF_CLOSE_PASSING_RANGE, 0.0f, 1.0f);
        }
        else
        {
            mfWeighting[eFan_DriveCloseToPlayer][liFanIndex] =
                -ClampAggression(-lfPassingSpace / KF_CLOSE_PASSING_RANGE, 0.0f, 1.0f);
        }
    }
}

// ========================================================================================
// IncludeDriftDirectionTracking @0x827881B0   (354 instructions -- a fully unrolled 17-step loop)
//   r3 = this, r4 = lpRacingLineGenerator (NEVER READ), r5 = lpRacingLine. Leaf: no prologue,
//   no calls, three `beqlr`/`blr` exits.
// DWARF BrnAISteeringFan.cpp:2327; locals :2339 lVectorToCentre, :2341 lLineVector, :2350 liFanIndex.
//
//   0x827881B0  !lpRacingLine->mbIsInitialised (+0xBD0)  -> return, row LEFT AS IT WAS
//   0x827881BC  !mbPointAheadKnown (this+0x808)          -> return, row LEFT AS IT WAS
//               (neither arm zeroes the row -- `beqlr`, unlike every other contributor here.)
//   0x827881E0  lVectorToCentre = mCentreAheadFarAhead (this+0x380) - mCentreFarAhead (this+0x370)
//   0x827881F8..828C  IsZero(lVectorToCentre): if it IS zero the normalise is SKIPPED
//               (`bne cr6, loc_827882DC`) and the raw zero vector is used for every dot -- the
//               console does not bail out here, so the row is written with 0.5 everywhere.
//   0x82788290..D8  lLineVector = Normalize(lVectorToCentre)
//   0x827882F0..8730  17 unrolled steps over this+0x220 .. +0x320 (mUnitDirection[0..16]) writing
//               this+0x618 .. +0x658 == mfWeighting[9] (eFan_DriftFinalDirection):
//                 lfAheadness = Dot(mUnitDirection[i], lLineVector)
//                 row[i] = (Clamp(lfAheadness, flt_820037C8 == -1.0, flt_82001C98 == 1.0) + 1.0)
//                          * flt_820C4168 (0.5)
//               i.e. the alignment remapped from [-1,1] to [0,1].
//
// PROVABLY UNREACHABLE IN THIS BUILD: kfBias[mode][eFan_DriftFinalDirection] is 0.0 in all ten
// bias modes, so UpdateWeightings @0x827947EC never calls this. Bodied for completeness; if a
// later build re-enables column 9 it is already correct.
// ========================================================================================
void SteeringFan::IncludeDriftDirectionTracking(RacingLineGenerator* lpRacingLineGenerator,
                                                RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;   // r4 -- never read by the console body

    if (!lpRacingLine->mbIsInitialised)                                     // @0x827881B0
        return;
    if (!mbPointAheadKnown)                                                 // @0x827881BC
        return;

    Vector2 lVectorToCentre;                                                // @0x827881E0
    lVectorToCentre.mV = __vsubfp(mCentreAheadFarAhead.mV, mCentreFarAhead.mV);

    Vector2 lLineVector = lVectorToCentre;
    if (!IsZero2DAggression(lVectorToCentre))                               // @0x8278828C
        lLineVector = Normalize2DAggression(lVectorToCentre);

    for (s32 liFanIndex = 0; liFanIndex < KI_FAN_STEPS; ++liFanIndex)
    {
        const f32 lfAheadness = Dot2DAggression(mUnitDirection[liFanIndex], lLineVector);
        mfWeighting[eFan_DriftFinalDirection][liFanIndex] =
            (ClampAggression(lfAheadness, -1.0f, 1.0f) + 1.0f) * 0.5f;
    }
}

// ========================================================================================
// IncludeDriftLocationTracking @0x82788738   (354 instructions -- the same unrolled shape)
//   r3 = this, r4 = lpRacingLineGenerator (NEVER READ), r5 = lpRacingLine. Leaf.
// DWARF BrnAISteeringFan.cpp:2372; locals :2380 lVectorToCentre, :2387 lLineVector,
// :2405 liFanIndex, :2409 lVectorToFanTarget, :2413 lfAheadness.
//
//   0x82788738  !lpRacingLine->mbIsInitialised (+0xBD0)  -> return, row LEFT AS IT WAS
//   0x82788744  !mbPointAheadKnown (this+0x808)          -> return, row LEFT AS IT WAS
//   0x82788768  lVectorToCentre = mCentreFarAhead (this+0x370) - mFanOrigin2D (this+0x350)
//   0x82788814  IsZero(lVectorToCentre) -> `bnelr`, RETURN (this is where the two drift
//               contributors differ: the direction one merely skips the normalise)
//   0x82788818..74  lLineVector = Normalize(lVectorToCentre)
//   0x82788834..8CB8  17 unrolled steps over mUnitDirection[0..16] writing this+0x65C .. +0x69C
//               == mfWeighting[10] (eFan_DriftFinalLocation), identical arithmetic to the
//               direction row: (Clamp(Dot(unit, lLineVector), -1, 1) + 1) * 0.5.
//
// PROVABLY UNREACHABLE IN THIS BUILD for the same reason as the direction row: the
// kfBias[mode][eFan_DriftFinalLocation] column is 0.0 in all ten modes.
// ========================================================================================
void SteeringFan::IncludeDriftLocationTracking(RacingLineGenerator* lpRacingLineGenerator,
                                               RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;   // r4 -- never read by the console body

    if (!lpRacingLine->mbIsInitialised)                                     // @0x82788738
        return;
    if (!mbPointAheadKnown)                                                 // @0x82788744
        return;

    Vector2 lVectorToCentre;                                                // @0x82788768
    lVectorToCentre.mV = __vsubfp(mCentreFarAhead.mV, mFanOrigin2D.mV);

    if (IsZero2DAggression(lVectorToCentre))                                // @0x82788814
        return;

    const Vector2 lLineVector = Normalize2DAggression(lVectorToCentre);

    for (s32 liFanIndex = 0; liFanIndex < KI_FAN_STEPS; ++liFanIndex)
    {
        const f32 lfAheadness = Dot2DAggression(mUnitDirection[liFanIndex], lLineVector);
        mfWeighting[eFan_DriftFinalLocation][liFanIndex] =
            (ClampAggression(lfAheadness, -1.0f, 1.0f) + 1.0f) * 0.5f;
    }
}

}

// FOLDED FROM BrnAISteeringFan_HNG.cpp
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameSource/World/AI/Route/BrnRacingLine.h"                  // RacingLine + SectionData
#include "GameSource/World/AI/RacingLine/BrnHardNoGoMap.h"            // HardNoGoMap
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"    // RacingLineGenerator + the gates

#include <cfloat>   // FLT_MAX (0x7F7FFFFF)
#include <cmath>    // std::sqrt (the vrsqrtefp + two-Newton-step length in FanIntersectsEdge), std::fmaf

// BrnAI::SteeringFan -- the HARD-NO-GO / ROUTE-EDGE contributors, partfile 3 of the weighting
// half. Three functions, all read off the X360 ARTIST image:
//
//   IncludeHardNoGo              @0x82779D98 (284 instr, IDA export)  rows eFan_AvoidHNG(1) /
//                                                                     eFan_ExitHNG(2) /
//                                                                     eFan_FavourHNGDanger(3)
//   FanIntersectsEdge            @0x8277A208 ( 92 instr, IDA export)  pure 2D geometry
//   IncludeRouteEdgeIntersection @0x8277A378 (136 instr, NO IDA EXPORT -- disassembled straight
//                                             out of the image, 0x8277A378..0x8277A590; the next
//                                             symbol is RouteRequestManager::GetFleeVector
//                                             @0x8277A598)            row eFan_AvoidEdges(6)
//
// The parked `(void)` stubs for these three in BrnAISteeringFan_Weightings.cpp are deleted at
// mount time; this file is their only body.
//
// REGISTER MAPS (from the asm, not the pseudocode -- Hex-Rays renders both entry points as
// `int __fastcall f(int, int, int, ..., double a9..a16)` and hides the VMX Vector2 arguments):
//   IncludeHardNoGo              0x82779DAC..DB8: r24 = this, r27/r4 = lpRacingLineGenerator,
//                                r29/r5 = lpRacingLine.
//   IncludeRouteEdgeIntersection 0x8277A38C..398: r24 = this, r25/r4 = lpRacingLineGenerator,
//                                r28/r5 = lpRacingLine.
//   FanIntersectsEdge            0x8277A208..228: r3 = this (NEVER READ), r4 = lpEdge,
//                                r5 = liIndex, v1 = lA, v2 = lB (both spilled to arg_30/arg_40).
// Both Include* take (RacingLineGenerator*, RacingLine*) with the GENERATOR first -- the console
// then calls the generator with r3 = generator, r4 = racingline, which is what pins the order.
//
// RECOVERED CONSTANTS (image bytes, big-endian, file offset = VA - 0x82000000):
//   flt_82F302B8 = 30.0f     the route-edge intersection range (IncludeRouteEdgeIntersection)
//   flt_82F302D0 =  0.0625f  the hard-no-go distance margin    (IncludeHardNoGo)
//   flt_82F302D4 =  0.125f   the hard-no-go danger threshold   (IncludeHardNoGo)
//   flt_8204F664 = +FLT_MAX (0x7F7FFFFF) / flt_82035570 = -FLT_MAX (0xFF7FFFFF)  min/max seeds
//   flt_820C4168 = 0.5f, flt_82001C98 = 1.0f, flt_82001CC0 = 0.0f, flt_820037C8 = -1.0f
//   flt_820C3B70 = +1.1920929e-07f, flt_82002514 = -1.1920929e-07f  (the parallel-edge epsilon)
// None of these lives in the zeroed 0x8300xxxx dynamic-initialiser bank; every one reads
// non-zero straight out of the static image.

namespace BrnAI
{
namespace
{
    // DWARF BrnAICar.cpp:49 `const uint16_t KU_INVALID_SECTION_INDEX = 32767`. Both Include*
    // bodies compare the section id against 0x7FFF as a signed 32-bit value (`cmpwi cr6, rN,
    // 0x7FFF`), so it is spelled s32 here; same value AICar.h already carries.
    const s32 KI_INVALID_SECTION_INDEX = 0x7FFF;

    // IncludeHardNoGo @0x82779F00 / @0x82779F3C -- flt_82F302D0 / flt_82F302D4.
    const f32 KF_HNG_EDGE_MARGIN   = 0.0625f;
    const f32 KF_HNG_DANGER_MARGIN = 0.125f;

    // IncludeHardNoGo @0x82779F98 / @0x82779FA4 -- flt_8204F664 / flt_82035570, the seeds of the
    // eFan_ExitHNG min/max scan. The +FLT_MAX seed is compared against again at 0x8277A0D8 to
    // detect "the row was entirely zero", so it must be this exact value.
    const f32 KF_FAN_FLOAT_MAX = FLT_MAX;

    // IncludeRouteEdgeIntersection @0x8277A4C4 -- flt_82F302B8. An edge hit at this distance or
    // beyond contributes nothing.
    const f32 KF_ROUTE_EDGE_RANGE = 30.0f;

    // IncludeRouteEdgeIntersection: `cmpwi cr6, r30, 0x30` @0x8277A480 caps the gather loop at
    // three 16-byte entries; the tail append at 0x8277A490 can add a fourth, which is why both
    // console stack arrays (r1+0x60 and r1+0xA0) are 0x40 bytes.
    const s32 KI_ROUTE_EDGE_SECTIONS = 3;
    const s32 KI_ROUTE_EDGE_POINTS   = KI_ROUTE_EDGE_SECTIONS + 1;

    // FanIntersectsEdge @0x8277A258 / @0x8277A270 -- flt_820C3B70 / flt_82002514 (FLT_EPSILON).
    const f32 KF_FAN_EDGE_PARALLEL_EPSILON = 1.1920929e-07f;

    // FanIntersectsEdge @0x8277A368 -- flt_820037C8. The "this ray does not cross this edge in
    // front of its target" answer; IncludeRouteEdgeIntersection tests results against it.
    const f32 KF_FAN_EDGE_NO_INTERSECTION = -1.0f;
}

#ifndef BRN_AI_STEERINGFAN_HNG_PRESENT
#define BRN_AI_STEERINGFAN_HNG_PRESENT BRN_AI_RACINGLINE_STACK_PRESENT
#endif

// ============================================================================================
// IncludeHardNoGo @0x82779D98   (this r3/r24, lpRacingLineGenerator r4/r27, lpRacingLine r5/r29)
// Rows eFan_AvoidHNG (this+0x3F8), eFan_ExitHNG (this+0x43C), eFan_FavourHNGDanger (this+0x480);
// mfWeighting is at this+0x3B4 with a 0x44-byte row stride, so those are rows 1, 2 and 3.
//
//   0x82779DB8  !lpRacingLine->mbIsInitialised (racingline+0xBD0) -> return, rows UNTOUCHED.
//   0x82779DC4  liSection = lpRacingLine->miLastKnownSectionID (racingline+0xC00).
//   0x82779DCC  only when that is KI_INVALID_SECTION_INDEX:
//   0x82779DE4     liSection = GetLocalSectionID(lpRacingLine, mFanOrigin2D (this+0x350, loaded
//                              by `lvx128 v1, r24, 0x350`), KI_INVALID_SECTION_INDEX)
//   0x82779DF4     still invalid -> zero ALL THREE rows (three unrolled 17-word `stw 0` loops at
//                              +0x3F8 / +0x43C / +0x480) and return.
//   0x82779E58     otherwise write it back to miLastKnownSectionID.
//   0x82779E80  per fan step i (r26 = 17, r28 = mHNGTarget[i] = this+0x110+16i, r31 = &row3[i]):
//     0x82779E90  liSection = GetNearSectionID(lpRacingLine, mHNGTarget[i], liSection)
//     0x82779EA0  invalid -> row1[i] = 0, row2[i] = 1 (f30 == flt_82001C98), row3[i] = 0, and
//                 liSection is RE-SEEDED from miLastKnownSectionID for the next step.
//     0x82779EC0  else lpSection = GetSectionPointer(lpRacingLine, liSection)
//     0x82779EC4  !lpSection->mHardNoGoMap.mbReady (section+0x98 == map+0x48) -> same 0/1/0 row.
//     0x82779EEC  else lbInside = mHardNoGoMap (section+0x50)
//                                 .DistanceToHardNoGoEdge(mHNGTarget[i], lfDistance)
//     0x82779EFC    !lbInside: lfDistance -= 0.0625; if it went negative, clamp it to 0 AND flip
//                              lbInside to TRUE (`li r3, 1` @0x82779F18 -- the console really does
//                              reclassify the point as inside here).
//     0x82779F28    row3 (FavourHNGDanger) = lbInside ? 0 : ((lfDistance - 0.125 < 0) ? 0
//                                                          : 1.0 - (lfDistance - 0.125))
//                   -- NOT clamped on the far side: 1.125 - d goes negative for d > 1.125 and the
//                   console keeps it.
//     0x82779F5C    lbInside ? (row2 = lfDistance, row1 = 0) : (row1 = lfDistance, row2 = 0)
//                   i.e. row eFan_AvoidHNG carries the distance while OUTSIDE the map and row
//                   eFan_ExitHNG carries it while INSIDE.
//   0x82779F8C  min/max over the NON-ZERO entries of row eFan_ExitHNG ONLY (unrolled 8+8+1;
//               f0 seeded +FLT_MAX, f13 seeded -FLT_MAX, zeros skipped by `fcmpu ... beq`).
//   0x8277A0D0  min == max, or min still +FLT_MAX (nothing non-zero) -> leave the row alone.
//   0x8277A0E0  else rescale every non-zero entry of that row into [0.5, 1.0]:
//                    v = (v - min) * ((1.0 / (max - min)) * 0.5) + 0.5
//               (`fdivs f12, f30(1.0), f13` then `fmuls f12, f12, f13(0.5)` then
//                `fmadds f11, f11, f12, f13(0.5)`). Rows 1 and 3 are NOT rescaled.
// ============================================================================================
void SteeringFan::IncludeHardNoGo(RacingLineGenerator* lpRacingLineGenerator,
                                  RacingLine* lpRacingLine)
{
    if (!lpRacingLine->mbIsInitialised)
        return;                                    // 0x82779DC0 -- rows keep their old values

#if BRN_AI_STEERINGFAN_HNG_PRESENT
    s32 liSection = lpRacingLine->miLastKnownSectionID;
    if (liSection == KI_INVALID_SECTION_INDEX)
    {
        liSection = lpRacingLineGenerator->GetLocalSectionID(lpRacingLine, mFanOrigin2D,
                                                             KI_INVALID_SECTION_INDEX);
        if (liSection == KI_INVALID_SECTION_INDEX)
        {
            for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            {
                mfWeighting[eFan_AvoidHNG][liStep]        = 0.0f;
                mfWeighting[eFan_ExitHNG][liStep]         = 0.0f;
                mfWeighting[eFan_FavourHNGDanger][liStep] = 0.0f;
            }
            return;
        }
        lpRacingLine->miLastKnownSectionID = liSection;
    }

    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        liSection = lpRacingLineGenerator->GetNearSectionID(lpRacingLine, mHNGTarget[liStep],
                                                            liSection);
        if (liSection == KI_INVALID_SECTION_INDEX)
        {
            mfWeighting[eFan_AvoidHNG][liStep]        = 0.0f;
            mfWeighting[eFan_ExitHNG][liStep]         = 1.0f;
            mfWeighting[eFan_FavourHNGDanger][liStep] = 0.0f;
            liSection = lpRacingLine->miLastKnownSectionID;      // 0x82779EAC
            continue;
        }

        SectionData* lpSection = lpRacingLineGenerator->GetSectionPointer(lpRacingLine, liSection);
        if (!lpSection->mHardNoGoMap.IsReady())
        {
            mfWeighting[eFan_AvoidHNG][liStep]        = 0.0f;
            mfWeighting[eFan_ExitHNG][liStep]         = 1.0f;
            mfWeighting[eFan_FavourHNGDanger][liStep] = 0.0f;
            continue;
        }

        f32  lfDistance       = 0.0f;
        bool lbInsideHardNoGo = lpSection->mHardNoGoMap.DistanceToHardNoGoEdge(mHNGTarget[liStep],
                                                                               lfDistance);
        if (!lbInsideHardNoGo)
        {
            lfDistance -= KF_HNG_EDGE_MARGIN;                    // 0x82779F04
            if (lfDistance < 0.0f)
            {
                lfDistance       = 0.0f;                         // 0x82779F14
                lbInsideHardNoGo = true;                         // 0x82779F18 `li r3, 1`
            }
        }

        if (lbInsideHardNoGo)
        {
            mfWeighting[eFan_FavourHNGDanger][liStep] = 0.0f;    // 0x82779F34
            mfWeighting[eFan_ExitHNG][liStep]         = lfDistance;
            mfWeighting[eFan_AvoidHNG][liStep]        = 0.0f;
        }
        else
        {
            const f32 lfOverDanger = lfDistance - KF_HNG_DANGER_MARGIN;
            mfWeighting[eFan_FavourHNGDanger][liStep] =
                (lfOverDanger < 0.0f) ? 0.0f : (1.0f - lfOverDanger);
            mfWeighting[eFan_AvoidHNG][liStep]        = lfDistance;
            mfWeighting[eFan_ExitHNG][liStep]         = 0.0f;
        }
    }

    // 0x82779F8C..0x8277A0CC -- min/max over the non-zero entries of eFan_ExitHNG only. The
    // console unrolls the 17 steps as 8 + 8 + 1; `fsel f0, (v - min), f0, v` keeps the running
    // minimum and `fsel f13, (v - max), v, f13` the running maximum.
    f32 lfMinimum =  KF_FAN_FLOAT_MAX;
    f32 lfMaximum = -KF_FAN_FLOAT_MAX;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        const f32 lfValue = mfWeighting[eFan_ExitHNG][liStep];
        if (lfValue == 0.0f)
            continue;
        // Spelt as the two fsels (0x82779FC0 / 0x82779FC4 for the first step): fsel takes its
        // THIRD operand on an unordered test, so a NaN entry BECOMES the running minimum (the next
        // entry replaces it again, `v - NaN` being unordered too) and leaves the maximum alone; a
        // NaN in the last folded entry therefore NaNs every rescaled entry below (crash parity
        // FX-AINAN2; the old `if (v < min) ..; if (v > max) ..` skipped a NaN on both).
        lfMinimum = ((lfValue - lfMinimum) >= 0.0f) ? lfMinimum : lfValue;
        lfMaximum = ((lfValue - lfMaximum) >= 0.0f) ? lfValue : lfMaximum;
    }

    // 0x8277A0D0 / 0x8277A0D8 -- a flat row (min == max) and an all-zero row (min still the
    // +FLT_MAX seed) both leave the row exactly as it is.
    if (lfMinimum == lfMaximum || lfMinimum == KF_FAN_FLOAT_MAX)
        return;

    // `fsubs f13, max, min ; fdivs f12, 1.0, f13 ; fmuls f12, f12, 0.5` (0x8277A0E0..0x8277A0F4) --
    // the scale is two roundings, as written.
    const f32 lfScale = (1.0f / (lfMaximum - lfMinimum)) * 0.5f;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        if (mfWeighting[eFan_ExitHNG][liStep] == 0.0f)
            continue;
        // ROUNDING_RULE rule 3 (L6 AIDRIVE, owner list 2026-09-27): every one of the 17 rescales is
        // `fsubs f11, v, min ; fmadds f11, f11, f12 (scale), f13 (0.5)` -- 0x8277A108, 0x8277A120,
        // 0x8277A138, 0x8277A150, 0x8277A168, 0x8277A180, 0x8277A198, 0x8277A1B0 (the 8-wide block,
        // run twice) and 0x8277A1E0 (the 17th) -- ONE rounding of (v - min) * scale + 0.5. The PC
        // spelling rounded the product and the sum separately.
        mfWeighting[eFan_ExitHNG][liStep] =
            (mfWeighting[eFan_ExitHNG][liStep] - lfMinimum) * lfScale + 0.5f;
    }
#else
    // The console's 0x82779DF4 arm: no section can be resolved, so all three rows read zero.
    (void)lpRacingLineGenerator;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        mfWeighting[eFan_AvoidHNG][liStep]        = 0.0f;
        mfWeighting[eFan_ExitHNG][liStep]         = 0.0f;
        mfWeighting[eFan_FavourHNGDanger][liStep] = 0.0f;
    }
#endif
}

// ============================================================================================
// FanIntersectsEdge @0x8277A208   (this r3 -- NEVER READ, lpEdge r4, liIndex r5, lA v1, lB v2)
//
// The 2D segment/segment test the route-edge contributor runs per fan ray. lpEdge is a polyline
// of 16-byte Vector2s; the edge tested is lpEdge[liIndex] -> lpEdge[liIndex + 1] (`slwi r11,
// r5, 4` @0x8277A224 then the 0x00/0x04 and 0x10/0x14 loads). The ray is lA -> lB.
//
//   0x8277A230  D  = lB - lA            (f11 = D.x, f12 = D.y)
//   0x8277A244  E  = e1 - e0            (f6  = E.x, f5  = E.y)
//   0x8277A25C  denominator = D.y*E.x - D.x*E.y      (`fmuls f0, f11, f5` then
//                                                     `fmsubs f0, f12, f6, f0`)
//   0x8277A264  |denominator| <= FLT_EPSILON -> parallel -> return -1.0. The console spells this
//               as `bgt` on +eps then `bge` on -eps, so a NaN denominator ALSO returns -1.0; the
//               !(>) / !(<) form below reproduces that exactly.
//   0x8277A290  R  = e0 - lA            (f9 = R.x, f10 = R.y)
//   0x8277A2A8  lfEdgeParam = (R.y*D.x - R.x*D.y) / denominator   -- the position along the edge
//   0x8277A2B8  outside [0, 1] -> return -1.0
//   0x8277A2C8  lfRayParam  = (R.y*E.x - R.x*E.y) / denominator   -- the position along the ray
//   0x8277A2D4  < 1.0 -> return -1.0, i.e. the crossing must be AT OR BEYOND lB (the fan target),
//               never between the car and its target.
//   0x8277A2E4..0x8277A360  return lfRayParam * |D|. The console builds |D| with vrsqrtefp plus
//               two Newton-Raphson refinements and a `vcmpeqfp` / `vsel` that forces the length
//               to 0 when D.x*D.x + D.y*D.y is exactly 0; std::sqrt plus that same guard is the
//               semantic equal. The result is therefore a DISTANCE in metres from lA.
// ============================================================================================
f32 SteeringFan::FanIntersectsEdge(Vector2* lpEdge, s32 liIndex, Vector2 lA, Vector2 lB)
{
    const f32 lfDirectionX = lB.mV.x - lA.mV.x;
    const f32 lfDirectionY = lB.mV.y - lA.mV.y;

    const f32 lfEdgeX = lpEdge[liIndex + 1].mV.x - lpEdge[liIndex].mV.x;
    const f32 lfEdgeY = lpEdge[liIndex + 1].mV.y - lpEdge[liIndex].mV.y;

    // ROUNDING_RULE rule 3 (L6 AIDRIVE, owner list 2026-09-27): the three 2D crosses are each
    // `fmuls p, a, b ; fmsubs r, c, d, p` -- ONE rounding of c*d - round(a*b):
    //   0x8277A25C/0x8277A260  denominator = D.y*E.x - round(D.x*E.y)
    //   0x8277A2A8/0x8277A2AC  edge param  = R.y*D.x - round(R.x*D.y)
    //   0x8277A2C8/0x8277A2CC  ray param   = R.y*E.x - round(R.x*E.y)
    // The PC spelling rounded both products. (The |D| root below stays exact std::sqrt: rule 5.)
    const f32 lfDenominator = lfDirectionY * lfEdgeX - lfDirectionX * lfEdgeY;
    if (!(lfDenominator >  KF_FAN_EDGE_PARALLEL_EPSILON) &&
        !(lfDenominator < -KF_FAN_EDGE_PARALLEL_EPSILON))
        return KF_FAN_EDGE_NO_INTERSECTION;                 // parallel (or NaN) -- no crossing

    const f32 lfToEdgeX    = lpEdge[liIndex].mV.x - lA.mV.x;
    const f32 lfToEdgeY    = lpEdge[liIndex].mV.y - lA.mV.y;
    const f32 lfReciprocal = 1.0f / lfDenominator;

    const f32 lfEdgeParam = (lfToEdgeY * lfDirectionX - lfToEdgeX * lfDirectionY) * lfReciprocal;
    if (lfEdgeParam < 0.0f || lfEdgeParam > 1.0f)
        return KF_FAN_EDGE_NO_INTERSECTION;                 // the crossing misses the segment

    const f32 lfRayParam = (lfToEdgeY * lfEdgeX - lfToEdgeX * lfEdgeY) * lfReciprocal;
    if (lfRayParam < 1.0f)
        return KF_FAN_EDGE_NO_INTERSECTION;                 // the crossing is short of lB

    const f32 lfLengthSq = lfDirectionX * lfDirectionX + lfDirectionY * lfDirectionY;
    const f32 lfLength   = (lfLengthSq == 0.0f) ? 0.0f : std::sqrt(lfLengthSq);
    return lfRayParam * lfLength;
}

// ============================================================================================
// IncludeRouteEdgeIntersection @0x8277A378  (this r3/r24, lpRacingLineGenerator r4/r25,
//                                            lpRacingLine r5/r28).  Row eFan_AvoidEdges(6) at
//                                            this+0x54C. NO IDA EXPORT -- the listing this body
//                                            was written from was decoded out of the image with
//                                            tools/re/ppcdis.py + tools/re/vmx128.py; the
//                                            function occupies 0x8277A378..0x8277A590 with a
//                                            zero pad word at 0x8277A594.
//
//   0x8277A398  !lpRacingLine->mbIsInitialised -> return, row UNTOUCHED.
//   0x8277A3B8  lCarPosition2D: `lvx128 v1, r0, r1+0x50` (an UNINITIALISED stack slot -- only
//               x/y are ever read downstream) then `lvx128 v0, r28, 0xB30` (lpRacingLine->mCarPos)
//               then `vrlimi128 v1, v0, 8, 0` (mask 8 == x lane, rotate 0 -> .x = mCarPos.x) and
//               `vrlimi128 v1, v0, 4, 1` (mask 4 == y lane, rotate 1 -> .y = mCarPos.z). This is
//               the same XZ flatten GenerateFanVectors @0x82779330/0x82779350 uses.
//   0x8277A3C8  liSection = GetLocalSectionID(lpRacingLine, lCarPosition2D,
//                                             lpRacingLine->miLastKnownSectionID)
//               -- unlike IncludeHardNoGo this call is UNCONDITIONAL, and the last known id is
//               the hint rather than KI_INVALID_SECTION_INDEX.
//   0x8277A3D8  invalid -> zero the 17 words at this+0x54C and return.
//   0x8277A408  lpRacingLine->miLastKnownSectionID = liSection.
//   0x8277A41C  GATHER, at most KI_ROUTE_EDGE_SECTIONS entries (`cmpwi cr6, r30, 0x30`):
//     0x8277A428    lpRacingLine->maSectionCache[liSection & 15].mCachedSectionIndex (the s16 at
//                   racingline + (id & 15)*0xB0 + 0xB8) != liSection -> stop. This inlined test is
//                   the DWARF's RacingLineGenerator::CacheUpToDate (BrnRacingLineGenerator.h:226)
//                   and it is exactly the condition GetSectionPointer @0x827655D0 would otherwise
//                   fire its StrStream assert on.
//     0x8277A444    lpSection = GetSectionPointer(lpRacingLine, liSection)
//     0x8277A448    !lpSection->mHardNoGoMap.mbReady -> stop (lpSection STAYS SET -- see below).
//     0x8277A454    `lvx128 v0, r3, 0x50` / `lvx128 v13, r3, 0x60` load the map's
//                   mCurrentAndPreviousLeft / mCurrentAndPreviousRight, and BOTH are then run
//                   through `vpermwi128 vX, vX, 0xBF`. 0xBF == (2,3,3,3), i.e. z/w -> x/y, which
//                   moves the PREVIOUS point into the Vector2 lanes: GetPreviousLeft() /
//                   GetPreviousRight() (DWARF BrnHardNoGoMap.h:139 / :142).
//                   (The permute immediate is split across the word: uimm = (b23<<7)|(b24<<6)|
//                   (b25<<5)|bits(b11..b15); calibrated against XMMatrixRotationX @0x8220352C and
//                   CreateLookAt @0x8220C72C, whose 0x63 is the yzxw cross-product swizzle.)
//   0x8277A488  AFTER the gather, if lpSection is non-null -- which it is whenever at least one
//               GetSectionPointer ran, INCLUDING the "map not ready" exit -- append that same
//               section's mCurrentAndPreviousLeft/Right with NO permute, i.e. GetCurrentLeft() /
//               GetCurrentRight(). That is what closes the polyline: entry(s0), entry(s1),
//               entry(s2), exit(s_last).
//   0x8277A4E4  per fan step i (r30 = 17, r7 = &row[i], r31 = &mTarget[i]):
//     0x8277A4E4    row[i] = 0.0 unconditionally, then walk the (liEdgeCount - 1) segments:
//     0x8277A504    lfLeft  = FanIntersectsEdge(laLeftEdgePoints,  liSegment, mFanOrigin2D,
//                                               mTarget[i])
//     0x8277A510    lfRight = FanIntersectsEdge(laRightEdgePoints, liSegment, mFanOrigin2D,
//                                               mTarget[i])  -- the second call reuses r3/r5/v1/v2
//                                               untouched, only r4 changes.
//     0x8277A514    both -1 -> next segment; otherwise take the nearer of the two hits and STOP
//                   scanning (the console falls straight through to the next fan step).
//     0x8277A548    lfRange = clamp(distance / 30.0, 0, 1)  (`fneg`+`fsel` low clamp,
//                   `fsubs`+`fsel` high clamp), then
//                   row[i] = (1 - lfRange)^2 * mTravelDirectionBias[i]   (this+0x54C + 0x264 ==
//                   this+0x7B0, mTravelDirectionBias).
//               Because FanIntersectsEdge only reports crossings at or beyond mTarget[i], the
//               distance is never below mfLookAheadRadius (10.0), so the row peaks at
//               (1 - 10/30)^2 == 0.444 and reaches 0 at 30 m. kfBias[eBiasMode_Race]
//               [eFan_AvoidEdges] == -200, so it is a strong penalty for rays that run into the
//               road edge close ahead.
// ============================================================================================
void SteeringFan::IncludeRouteEdgeIntersection(RacingLineGenerator* lpRacingLineGenerator,
                                               RacingLine* lpRacingLine)
{
    if (!lpRacingLine->mbIsInitialised)
        return;                                    // 0x8277A3A0 -- row keeps its old values

#if BRN_AI_STEERINGFAN_HNG_PRESENT
    // 0x8277A3B8 -- the XZ flatten of lpRacingLine->mCarPos. The console leaves z/w as whatever
    // the stack slot held; nothing downstream reads them, so they are zeroed here.
    Vector2 lCarPosition2D;
    lCarPosition2D.mV.x = lpRacingLine->mCarPos.mV.x;
    lCarPosition2D.mV.y = lpRacingLine->mCarPos.mV.z;
    lCarPosition2D.mV.z = 0.0f;
    lCarPosition2D.mV.w = 0.0f;

    s32 liSection = lpRacingLineGenerator->GetLocalSectionID(lpRacingLine, lCarPosition2D,
                                                             lpRacingLine->miLastKnownSectionID);
    if (liSection == KI_INVALID_SECTION_INDEX)
    {
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            mfWeighting[eFan_AvoidEdges][liStep] = 0.0f;
        return;
    }
    lpRacingLine->miLastKnownSectionID = liSection;

    Vector2      laLeftEdgePoints[KI_ROUTE_EDGE_POINTS];
    Vector2      laRightEdgePoints[KI_ROUTE_EDGE_POINTS];
    s32          liEdgeCount = 0;
    SectionData* lpSection   = 0;

    for (s32 liSlot = 0; liSlot < KI_ROUTE_EDGE_SECTIONS; ++liSlot)
    {
        // The inlined RacingLineGenerator::CacheUpToDate (DWARF BrnRacingLineGenerator.h:226).
        const s32 liCacheSlot = liSection & (RacingLine::KI_SECTION_CACHE_COUNT - 1);
        if (static_cast<s32>(lpRacingLine->maSectionCache[liCacheSlot].mCachedSectionIndex)
                != liSection)
            break;

        lpSection = lpRacingLineGenerator->GetSectionPointer(lpRacingLine, liSection);
        if (!lpSection->mHardNoGoMap.IsReady())
            break;

        laLeftEdgePoints[liEdgeCount]  = lpSection->mHardNoGoMap.GetPreviousLeft();
        laRightEdgePoints[liEdgeCount] = lpSection->mHardNoGoMap.GetPreviousRight();
        ++liEdgeCount;
        ++liSection;
    }

    // 0x8277A488 -- close the polyline with the last resolved section's CURRENT edge points.
    // lpSection is only null when the very first cache test failed.
    if (lpSection != 0)
    {
        laLeftEdgePoints[liEdgeCount]  = lpSection->mHardNoGoMap.GetCurrentLeft();
        laRightEdgePoints[liEdgeCount] = lpSection->mHardNoGoMap.GetCurrentRight();
        ++liEdgeCount;
    }

    const s32 liSegmentCount = liEdgeCount - 1;

    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        mfWeighting[eFan_AvoidEdges][liStep] = 0.0f;

        for (s32 liSegment = 0; liSegment < liSegmentCount; ++liSegment)
        {
            const f32 lfLeftHit  = FanIntersectsEdge(laLeftEdgePoints,  liSegment,
                                                     mFanOrigin2D, mTarget[liStep]);
            const f32 lfRightHit = FanIntersectsEdge(laRightEdgePoints, liSegment,
                                                     mFanOrigin2D, mTarget[liStep]);

            f32 lfDistance;
            if (lfLeftHit == KF_FAN_EDGE_NO_INTERSECTION)
            {
                if (lfRightHit == KF_FAN_EDGE_NO_INTERSECTION)
                    continue;                                    // 0x8277A524 -- next segment
                lfDistance = lfRightHit;
            }
            else if (lfRightHit == KF_FAN_EDGE_NO_INTERSECTION)
            {
                lfDistance = lfLeftHit;                          // 0x8277A538
            }
            else
            {
                lfDistance = (lfLeftHit < lfRightHit) ? lfLeftHit : lfRightHit;
            }

            // The high clamp is `fsubs t,1.0,r ; fsel r,t,r,1.0` 0x8277A558/0x8277A55C: fsel takes
            // its THIRD operand on an unordered test, so a NaN range is 1.0 (closeness 0, a zero
            // weight) -- spelt as the fsel (FX-AINAN2; `if (r > 1) r = 1` kept the NaN). The low
            // clamp (fneg + fsel 0x8277A550/0x8277A554) keeps a NaN either way.
            f32 lfRange = lfDistance / KF_ROUTE_EDGE_RANGE;
            if (lfRange <= 0.0f) lfRange = 0.0f;                 // 0x8277A550 fneg + fsel
            lfRange = ((1.0f - lfRange) >= 0.0f) ? lfRange : 1.0f;   // 0x8277A558 fsubs + fsel
            const f32 lfCloseness = 1.0f - lfRange;
            mfWeighting[eFan_AvoidEdges][liStep] =
                lfCloseness * lfCloseness * mTravelDirectionBias[liStep];
            break;                                               // the first hit wins
        }
    }
#else
    // The console's 0x8277A3D8 arm: no section can be resolved, so the row reads zero.
    (void)lpRacingLineGenerator;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
        mfWeighting[eFan_AvoidEdges][liStep] = 0.0f;
#endif
}

}

// FOLDED FROM BrnAISteeringFan_Target.cpp
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameSource/World/AI/BrnAICar.h"            // AICar::IsPlayerCar
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cfloat>   // FLT_MAX (the -3.4028235e38 / +3.4028235e38 seeds)

// BrnAI::SteeringFan -- partfile 1 of 2 for the weighting/target half (aiwave R6 lane).
// This TU owns the TARGET SELECTION path plus the kfBias table every contributor is scaled by:
//
//   kfBias                @flt_82F30468 (data; see the table banner below)
//   AccumulateWeightings  @0x82779088
//   BestTargetInArea      @0x82768E50
//   AccumulateInArea      @0x82769000
//   GetMinMaxWeightings   @0x82768AC8
//   GetDrivingTarget      @0x82779C30   <- AIDriver::GetTargetPosition's console arm
//   WriteWeightingValues  @0x827691E0   (debug overlay -- parked, see below)
//   RenderEdge            @0x82779770   (debug render -- parked)
//   RenderContributor     @0x82779830   (debug render -- parked)
//   RenderFanVectors      @0x82779A20   (debug render -- parked)
//
// Fan-vector generation + the 13 contributor rows live in BrnAISteeringFan_Weightings.cpp.
//
// Constants read from the image (scratch/postfx_step9_final/envfix/work/image.bin, big-endian,
// file offset = VA - 0x82000000):
//   flt_82001C98 == 1.0   flt_82001CC0 == 0.0   flt_820C4168 == 0.5   flt_82035570 == -FLT_MAX

namespace BrnAI
{

// ========================================================================================
// kfBias -- DWARF BrnAISteeringFan.cpp:68 `float32_t[10][14] kfBias`, image flt_82F30468.
//
// EVIDENCE / HOW THIS TABLE WAS RECOVERED (read this before "correcting" any cell):
//   The static .data image at 0x82F30468 is only HALF the table. 61 of the 140 cells are written
//   at run time by this TU's dynamic initialiser at 0x82C69380..0x82C6946C -- a straight-line
//   lis/addi/lfs/stfs block that copies the file-scope `float32_t KF_*_MAX` constants (which the
//   C++ initialiser list names, so the compiler could not fold them) out of the pool at
//   0x82F30298..0x82F302B4 into kfBias. That initialiser has NO IDA export, which is exactly why
//   a plain image read of 0x82F30468 shows eBiasMode_Race as an all-zero row: it is NOT zero.
//   The pool, decoded store-by-store from 0x82C69380 and matched to the DWARF's file-scope
//   declaration order (BrnAISteeringFan.cpp:33..43):
//     0x82F30298 KF_CENTRE_TRACKING_MAX    =   20      -> column eFan_SteerToCentre
//     0x82F3029C KF_HARD_NO_GO_BAD_MAX     = -300      -> column eFan_ExitHNG
//     0x82F302A0 KF_HARD_NO_GO_GOOD_MAX    =   50      -> column eFan_AvoidHNG
//     0x82F302A4 KF_TRAFFIC_MAX            = -100      -> column eFan_AvoidTraffic
//     0x82F302A8 KF_ONCOMING_TRAFFIC_MAX   = -400      -> column eFan_AvoidOncomingTraffic
//     0x82F302AC KF_EDGE_INTERSECTION_MAX  = -200      -> column eFan_AvoidEdges
//     0x82F302B0 KF_PARALLEL_MAX           = -200      -> column eFan_DriveParallel
//     0x82F302B4 KF_SLAM_PLAYER_MAX        =  300      -> column eFan_SmashIntoPlayer
//   Every other non-zero cell (1 / 5 / 50 / 60 / 90 / 100 / 140 / 500 / 1000) IS present in the
//   static image and is read straight out of image.bin.
//   The table is NOT const on the console (the DWARF spells it `float32_t`, and the dynamic
//   initialiser writes it), so it is not const here either.
// ========================================================================================
const f32 KF_CENTRE_TRACKING_MAX   =   20.0f;   // 0x82F30298
const f32 KF_HARD_NO_GO_BAD_MAX    = -300.0f;   // 0x82F3029C
const f32 KF_HARD_NO_GO_GOOD_MAX   =   50.0f;   // 0x82F302A0
const f32 KF_TRAFFIC_MAX           = -100.0f;   // 0x82F302A4
const f32 KF_ONCOMING_TRAFFIC_MAX  = -400.0f;   // 0x82F302A8
const f32 KF_EDGE_INTERSECTION_MAX = -200.0f;   // 0x82F302AC
const f32 KF_PARALLEL_MAX          = -200.0f;   // 0x82F302B0
const f32 KF_SLAM_PLAYER_MAX       =  300.0f;   // 0x82F302B4

f32 kfBias[E_BIAS_MODE_COUNT][E_FAN_CONTRIBUTORS_COUNT] =
{
    // column order == EFan_Contributors:
    //   0 SteerToCentre  1 AvoidHNG  2 ExitHNG  3 FavourHNGDanger  4 AvoidTraffic
    //   5 AvoidOncomingTraffic  6 AvoidEdges  7 DriveParallel  8 PreferCurrentDirection
    //   9 DriftFinalDirection  10 DriftFinalLocation  11 SmashIntoPlayer
    //  12 DriveCloseToPlayer  13 SmashIntoRivals
    // eBiasMode_Race
    { KF_CENTRE_TRACKING_MAX, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      KF_TRAFFIC_MAX, KF_ONCOMING_TRAFFIC_MAX, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    // eBiasMode_RaceDangerous
    { KF_CENTRE_TRACKING_MAX, 1.0f, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      5.0f, 100.0f, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 500.0f },
    // eBiasMode_Slam
    { 0.0f, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      0.0f, KF_ONCOMING_TRAFFIC_MAX, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, KF_SLAM_PLAYER_MAX, 0.0f, 0.0f },
    // eBiasMode_RoadRage
    { 0.0f, 0.0f, 0.0f, 50.0f,
      KF_TRAFFIC_MAX, KF_ONCOMING_TRAFFIC_MAX, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    // eBiasMode_CloseToPlayer
    { 0.0f, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      KF_TRAFFIC_MAX, KF_ONCOMING_TRAFFIC_MAX, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 60.0f, 0.0f },
    // eBiasMode_SlamRivals
    { KF_CENTRE_TRACKING_MAX, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      KF_TRAFFIC_MAX, KF_ONCOMING_TRAFFIC_MAX, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f },
    // eBiasMode_HitOncoming
    { KF_CENTRE_TRACKING_MAX, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      140.0f, 140.0f, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 90.0f },
    // eBiasMode_SlamDangerous
    { 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, KF_SLAM_PLAYER_MAX, 0.0f, 0.0f },
    // eBiasMode_DontCentreWithinHNG
    { KF_CENTRE_TRACKING_MAX, KF_HARD_NO_GO_GOOD_MAX, KF_HARD_NO_GO_BAD_MAX, 0.0f,
      KF_TRAFFIC_MAX, KF_ONCOMING_TRAFFIC_MAX, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    // eBiasMode_VeerAwayFromPlayer
    { 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, KF_EDGE_INTERSECTION_MAX, KF_PARALLEL_MAX,
      0.0f, 0.0f, 0.0f, 0.0f, 50.0f, 0.0f },
};

// ========================================================================================
// AccumulateWeightings @0x82779088   (returns `this`; the X360 threads r3 straight back out)
//
// Fold the 14 contributor rows into one 17-slot column, scaled by the current bias mode's kfBias
// row, then LERP the persistent mfCumulativeWeighting[] halfway toward it.
//   r10 = *(this+0x3B0) == meBiasMode ; r5 = flt_82F30468 + meBiasMode*0x38 (14 floats per mode)
//   r8  = this+0x3B8 / r7 = this+0x3F4 -- mfWeighting[row] walked with the console's 8-wide + 1
//         unrolling (row stride 0x44 == 17 floats); re-rolled here.
//   The `if (bias != 0.0)` skip at 0x827790D0 is the console's own (flt_82001CC0 == 0.0).
//   r11 = this+0x770 == &mfCumulativeWeighting[1] and r10 = &lafAccumulated[1]: the store at
//         -4(r11) is slot 0. flt_820C4168 == 0.5 -> cum = cum + (accum - cum) * 0.5.
// ========================================================================================
SteeringFan* SteeringFan::AccumulateWeightings()
{
    f32 lafAccumulated[KI_FAN_STEPS];
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
        lafAccumulated[liStep] = 0.0f;

    // ROUNDING_RULE rule 3 (L6 AIDRIVE, owner list 2026-09-27): every fold is ONE rounding --
    // `fmadds acc, weight, bias, acc` at 0x827790FC, 0x82779100, 0x82779114, 0x82779118, 0x8277912C,
    // 0x82779130, 0x82779144, 0x82779148 (the 8-wide block, run twice) and 0x82779194 (the 17th);
    // two of them spell the product bias * weight, which fmaf's exact product does not tell apart.
    // The PC spelling rounded each product before the add, so near-equal rays could swap places in
    // GetBestIndex's max. The DWARF scope (BrnAISteeringFan.cpp:393-398) has no bias local: the
    // row's bias is read straight from kfBias in both the test and the fold, which gives ARTIST's
    // operand order. Remaining difference: ARTIST bumps the bias pointer (`addi r5,r5,4` at
    // 0x827791AC) before the row pointer; the XDK schedules it one slot later.
    for (s32 liContributor = 0; liContributor < E_FAN_CONTRIBUTORS_COUNT; ++liContributor)
    {
        if (kfBias[meBiasMode][liContributor] != 0.0f)
        {
            for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
                lafAccumulated[liStep] += mfWeighting[liContributor][liStep] * kfBias[meBiasMode][liContributor];
        }
    }

    // `fsubs f12, acc, cum ; fmadds f12, f12, f0 (0.5), cum` at 0x82779208 .. 0x82779278 (x2) and
    // 0x827792AC -- one rounding too (value-neutral here: the halving is exact above the subnormals).
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        const f32 lfCumulative = mfCumulativeWeighting[liStep];
        mfCumulativeWeighting[liStep] = (lafAccumulated[liStep] - lfCumulative) * 0.5f + lfCumulative;
    }

    return this;
}

// ========================================================================================
// BestTargetInArea @0x82768E50
//
// Index of the largest mfCumulativeWeighting in [liFrom, liTo] INCLUSIVE (the console's
// `4*(liFrom + 475) + this` is &mfCumulativeWeighting[liFrom]; 475*4 == 1900 == 0x76C).
// Seeds with -FLT_MAX (flt_82035570) and returns liFrom when nothing beats it, so an all-equal
// window resolves to its LOW end. Three range asserts, all "Off end of target fan\n"
// (BrnAISteeringFan.cpp:807/808/809).
// ========================================================================================
s32 SteeringFan::BestTargetInArea(s32 liFrom, s32 liTo)
{
    CGS_ASSERT(liTo < KI_FAN_STEPS, "Off end of target fan\n");
    CGS_ASSERT(liFrom >= 0,         "Off end of target fan\n");
    CGS_ASSERT(liFrom <= liTo,      "Off end of target fan\n");

    s32 liBest = liFrom;
    f32 lfBest = -FLT_MAX;
    for (s32 liStep = liFrom; liStep <= liTo; ++liStep)
    {
        if (mfCumulativeWeighting[liStep] > lfBest)
        {
            lfBest = mfCumulativeWeighting[liStep];
            liBest = liStep;
        }
    }
    return liBest;
}

// ========================================================================================
// AccumulateInArea @0x82769000
//
// MEAN of mfCumulativeWeighting over [liFrom, liTo] inclusive: the console sums the slots into
// f31 while adding flt_82001C98 (1.0) into a float counter, then `fdivs f1, f31, count` -- but
// only when the counter is non-zero (flt_82001CC0 == 0.0); otherwise it returns the raw sum
// (0.0 on that path, because the loop never ran). Same three range asserts as BestTargetInArea
// (BrnAISteeringFan.cpp:837/838/839).
// ========================================================================================
f32 SteeringFan::AccumulateInArea(s32 liFrom, s32 liTo)
{
    CGS_ASSERT(liTo < KI_FAN_STEPS, "Off end of target fan\n");
    CGS_ASSERT(liFrom >= 0,         "Off end of target fan\n");
    CGS_ASSERT(liFrom <= liTo,      "Off end of target fan\n");

    f32 lfTotal = 0.0f;
    f32 lfCount = 0.0f;
    for (s32 liStep = liFrom; liStep <= liTo; ++liStep)
    {
        lfTotal += mfCumulativeWeighting[liStep];
        lfCount += 1.0f;
    }

    if (lfCount != 0.0f)
        return lfTotal / lfCount;
    return lfTotal;
}

// ========================================================================================
// GetMinMaxWeightings @0x82768AC8
//
// Walk lpWeightings[0..16] (the console unrolls 8-wide to 16 then a 1-wide tail), writing the
// max to lrBestWeighting (seeded -FLT_MAX) and the min to lrWorstWeighting (seeded +FLT_MAX).
// Returns the index of the max, or -1 when no slot ever beat the seed. Strict `>` / `<`, so
// ties keep the FIRST index.
// ========================================================================================
s32 SteeringFan::GetMinMaxWeightings(f32* lpWeightings, f32& lrBestWeighting, f32& lrWorstWeighting)
{
    s32 liBest = -1;
    lrBestWeighting  = -FLT_MAX;
    lrWorstWeighting =  FLT_MAX;

    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        if (lpWeightings[liStep] > lrBestWeighting)
        {
            lrBestWeighting = lpWeightings[liStep];
            liBest = liStep;
        }
        if (lpWeightings[liStep] < lrWorstWeighting)
            lrWorstWeighting = lpWeightings[liStep];
    }
    return liBest;
}

// ========================================================================================
// GetDrivingTarget @0x82779C30  (sret Vector2)
//
// THE steering target AIDriver::GetTargetPosition @0x8277CBF8 reads every frame.
// Register map (the sret is not shown in the pseudocode's arg list):
//   r3 = &out Vector2, r4 = this, r5 = lpCar, r6 = lpRacingLine, r7 = lpRacingLineGenerator,
//   r8 = lpNearbyTraffic, r9 = lbRenderThinking.
// r6/r7/r8 are NEVER READ by this body -- they exist for the DWARF signature only.
//
//   AccumulateWeightings();
//   liHigh = BestTargetInArea(8, 16);      liLow  = BestTargetInArea(0, 8);
//   lfLow  = AccumulateInArea(liLow, 8);   lfHigh = AccumulateInArea(8, liHigh);
//   pick liHigh when lfHigh > lfLow (fcmpu/ble at 0x82779CC4), else liLow;
//   return mTarget[picked]   (`slwi r11, idx, 4` + `lvx128 v0, r11, r31`: this + idx*16 is
//   mTarget[idx], the FIRST member of the class).
// i.e. the fan is split into the [0..8] half and the [8..16] half, the half with the better MEAN
// cumulative weighting wins, and within it its own best ray is the target. A FLAT fan therefore
// resolves to ray 0 -- see the [FLAG] block in BrnAIDriver.h about why that matters on this host.
// ========================================================================================
Vector2 SteeringFan::GetDrivingTarget(AICar* lpCar, RacingLine* lpRacingLine,
                                      RacingLineGenerator* lpRacingLineGenerator,
                                      const NearbyVehicles* lpNearbyVehicles,
                                      bool lbRenderThinking)
{
    (void)lpRacingLine;             // r6 -- not read by the console body
    (void)lpRacingLineGenerator;    // r7 -- not read by the console body
    (void)lpNearbyVehicles;         // r8 -- not read by the console body

    AccumulateWeightings();

    const s32 liBestHigh = BestTargetInArea(KI_FAN_STEPS / 2, KI_FAN_STEPS - 1);   // (8, 16)
    const s32 liBestLow  = BestTargetInArea(0, KI_FAN_STEPS / 2);                  // (0, 8)

    const f32 lfLowArea  = AccumulateInArea(liBestLow, KI_FAN_STEPS / 2);
    const f32 lfHighArea = AccumulateInArea(KI_FAN_STEPS / 2, liBestHigh);

    if (lbRenderThinking)
        RenderEdge(KI_FAN_STEPS / 2, 0xFFFFFFFFu);          // li r5, -1

    s32 liChosen;
    if (lfHighArea > lfLowArea)
    {
        if (lbRenderThinking && lpCar->IsPlayerCar())        // lbz 0x1549(car)
        {
            WriteWeightingValues(liBestHigh);
            RenderFanVectors(liBestHigh);
            RenderEdge(liBestHigh, 0xFFA0A000u);
            RenderEdge(liBestLow,  0xFFFFA0A0u);            // li r5, -0x5F60 == 0xFFFFA0A0
        }
        liChosen = liBestHigh;
    }
    else
    {
        if (lbRenderThinking && lpCar->IsPlayerCar())
        {
            WriteWeightingValues(liBestLow);
            RenderFanVectors(liBestHigh);                   // the console passes r30 == liBestHigh here
            RenderEdge(liBestHigh, 0xFFA0A0FFu);
            RenderEdge(liBestLow,  0xFFA0A000u);
        }
        liChosen = liBestLow;
    }

    return mTarget[liChosen];
}

void SteeringFan::WriteWeightingValues(s32 liContributor)
{
    (void)liContributor;
}

void SteeringFan::RenderEdge(s32 liIndex, u32 luColour)
{
    (void)liIndex;
    (void)luColour;
}

void SteeringFan::RenderContributor(s32 liContributor)
{
    (void)liContributor;
}

void SteeringFan::RenderFanVectors(s32 liIndex)
{
    (void)liIndex;
}

}

// FOLDED FROM BrnAISteeringFan_Traffic.cpp
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameSource/World/AI/RacingLine/BrnAISteeringFan_TrafficConstants.h" // the recovered tunables
#include "GameSource/World/AI/BrnAICar.h"                             // AICar::GetSpeed/GetPosition/GetDirection
#include "GameSource/World/AI/BrnAIDriver.h"                          // NearbyVehicle / NearbyVehicles / ENearbyType
#include "GameSource/World/AI/BrnAIUtils.h"                           // BrnAI::DistancePosVelToOrigin
#include "GameSource/World/AI/Route/BrnRacingLine.h"                  // RacingLine
#include "GameSource/Math/BrnMathUtils.h"                             // BrnMath::Flatten (XZ ground plane)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT

#include <cmath>    // std::sqrt (the vrsqrtefp + Newton chains), std::fabs, std::fmaf (the row lerps)

// BrnAI::SteeringFan -- the traffic-avoidance contributor, split out of the weighting partfile.
//
//   IncludeConstantBearing @0x827873A0   rows eFan_AvoidTraffic(4) / eFan_AvoidOncomingTraffic(5)
//
// The parked stub for this function in BrnAISteeringFan_Weightings.cpp is superseded by the body
// below; it must be deleted when this TU is mounted (one body per symbol).
//
// "Constant bearing" is the sailor's collision test: for each of the 17 fan rays, pretend the car
// drives along that ray at its current speed, then ask, per nearby vehicle, whether the RELATIVE
// motion closes on the origin inside KF_TRAFFIC_IMPACT_TIME and passes closer than the allowed
// passing space. Rays that fail get a positive penalty in row 4 (same-direction traffic) or row 5
// (oncoming); AccumulateWeightings then multiplies those rows by kfBias, which is NEGATIVE for
// both (-100 / -400 in eBiasMode_Race), so a penalised ray loses.
//
// EVIDENCE. Every operand below was read from tools/re/vmx128.py --func 0x827873A0 (the raw VMX128
// register fields), not from IDA's listing: this body is full of vA operands IDA prints in the
// v86..v95 band. The Hex-Rays pseudocode is a hint only, and where it lies the lie is named.
//
// X360 REGISTER MAP (prologue 0x827873BC..0x827873D0)
//   r3/r19 = this          r4/r17 = lpRacingLine    r5/r27 = lpCar    r6/r24 = lpNearbyTraffic
//   f21 = lfPlayerSpeed    f31 = 1.0                f29 = 0.0         f28 = lfAllowedPassingSpace
//   f27 = lfReciprocalAllowedPassingSpace           f24 = lfClosestCollision
//   f26 = 80.0             f25 = 0.0125             f20 = 0.2         f23/f22 = the two old rows
//   r16 = &KF_CLOSENESS_TO_BRAKE (0x82F302CC; the three .data tunables are all reached off it)
//   r18 = lpRacingLine + 0xB1C = &mfImmediateDistanceToTrafficImpact
//   r30 = &mfWeighting[5][i]  (so r30 - 0x44 == &mfWeighting[4][i])
//   r27 (REUSED after GetSpeed) = &mUnitDirection[i]        r29 = &lpNearbyTraffic->mVehicle[j]
//   v121 = lPlayerPosition   v119 = lPlayerDirection   v122 = lPlayerMotion
//   v123 = lRelativeVelocity v1   = lRelativePosition  v120 = lImmmediateApproachSpeedOfTrafficAhead
//   v124 = lRelativePosition componentwise squared     v125 = 0
//
// LOCAL NAMES are the DecFIGS DWARF's (dwarfdump/.../BrnAISteeringFan.cpp:979, source lines
// :1505..:1660); the source-line numbers are quoted per declaration below.

namespace BrnAI
{
namespace
{
    // ---- the inlined VMX idioms, spelled once ------------------------------------------------
    // Named with a "Traffic" suffix so they stay distinct from the "...Fan" set in
    // BrnAISteeringFan_Weightings.cpp; both live in anonymous namespaces (internal linkage).

    // vmulfp128 + vspltw 0 + vspltw 1 + vaddfp: the 2-lane dot the console spells inline eight
    // times in this body. Lanes z/w are never summed here (that is vmsum3fp128, below).
    f32 Dot2DTraffic(const Vector2& lA, const Vector2& lB)
    {
        const __vector4 lProduct = __vmulfp(lA.mV, lB.mV);
        const __vector4 lSum = __vaddfp(__vspltw(lProduct, 0), __vspltw(lProduct, 1));
        return lSum.x;
    }

    // ARTIST 82787704..28 uses one refinement; 82787758..90 and
    // 827878B0..82787940 use two. Zero is selected after the multiply.
    __vector4 SqrtVectorTraffic(__vector4 lValue, bool lbSecondStep)
    {
        const __vector4 lZero = __vspltisw(0);
        const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
        __vector4 lEstimate = __vrsqrtefp(lValue);
        __vector4 lSquare = __vmulfp(lEstimate, lEstimate);
        __vector4 lHalfEstimate = __vmulfp(lEstimate, lHalf);
        __vector4 lCorrection = __vnmsubfp(lValue, lSquare, lOne);
        lEstimate = __vmaddfp(lHalfEstimate, lCorrection, lEstimate);
        if (lbSecondStep)
        {
            lSquare = __vmulfp(lEstimate, lEstimate);
            lHalfEstimate = __vmulfp(lEstimate, lHalf);
            lCorrection = __vnmsubfp(lValue, lSquare, lOne);
            lEstimate = __vmaddfp(lHalfEstimate, lCorrection, lEstimate);
        }
        return __vsel(__vmulfp(lValue, lEstimate), lZero, __vcmpeqfp(lZero, lValue));
    }

    f32 SqrtTraffic(f32 lfValue, bool lbSecondStep)
    {
        __vector4 lValue;
        lValue.x = lfValue;
        return SqrtVectorTraffic(__vspltw(lValue, 0), lbSecondStep).x;
    }

    // fsel f0, f13, f29, f0 / fsubs+fsel f0, f13, f0, f31 -- the console's branchless clamp of a
    // ratio into [0,1] (0x827873F0..0x8278740C and again at 0x827877E0..0x82787804). Written as
    // fsel's own predicate (>= 0 picks the second operand) so the -0.0 and NaN behaviour matches.
    f32 ClampUnitTraffic(f32 lfValue)
    {
        const f32 lfFloored = ((-lfValue) >= 0.0f) ? 0.0f : lfValue;
        return ((1.0f - lfFloored) >= 0.0f) ? lfFloored : 1.0f;
    }

    // fsubs f11, f12, f0 / fsel f0, f11, f12, f0 == max(lfA, lfB) with fsel's tie/NaN rule.
    f32 MaxTraffic(f32 lfA, f32 lfB)
    {
        return ((lfA - lfB) >= 0.0f) ? lfA : lfB;
    }

    // fsubs + fmadds at 0x82787888..0x827878A8: from + (to - from) * time.
    // This is SteeringFan::Interpolate (DWARF BrnAISteeringFan.cpp:276), which has no IDA export
    // because the compiler inlined it at every call site. It is spelled TU-locally here because
    // BrnAISteeringFan.h belongs to another lane -- see the header_request in this lane's report.
    // ROUNDING_RULE rule 3 (L6 AIDRIVE, owner list 2026-09-27): the console fuses the multiply-add,
    // `fsubs f0, new, old ; fmadds f0, f0, f20 (0.2, flt_820C4300), old` @0x82787890/0x82787894 (row
    // eFan_AvoidTraffic) and @0x827878A0/0x827878A4 (row eFan_AvoidOncomingTraffic) -- ONE rounding
    // of (to - from) * time + from, with the operands in that order. The PC spelling rounded the
    // product and the sum separately.
    f32 InterpolateTraffic(f32 lfFrom, f32 lfTo, f32 lfTime)
    {
        return (lfTo - lfFrom) * lfTime + lfFrom;
    }

    // vmsum3fp128 v0, v120, v120 then the two-step rsqrt chain at 0x827878B0..0x827878FC.
    // THREE lanes, not two: the console really does include z, and lImmmediateApproachSpeedOfTrafficAhead
    // is a Vector3 in the DWARF (:1519). With mUnitDirection[i].z == 0 (GenerateFanVectors) the z
    // term reduces to the traffic entity's own velocity z lane.
    f32 Magnitude3Traffic(const Vector3& lVector)
    {
        return SqrtVectorTraffic(__vmsum3fp(lVector.mV, lVector.mV), true).x;
    }
}

// ================================================================================================
// IncludeConstantBearing @0x827873A0
//   (this r3, lpRacingLine r4, lpCar r5, lpNearbyTraffic r6 -- UpdateWeightings' call @0x8279476C)
//
// 0x827873D0  f21 = AICar::GetSpeed(lpCar)
// 0x827873E8  f0  = KF_GUESSED_MAX_SPEED (.bss flt_8300D78C, recovered -- see the constants header)
// 0x827873F0  f0  = f21 / f0, then the fsel pair clamps it into [0,1]        -> lfSpeedRatio
// 0x82787414  f28 = lfSpeedRatio * 0.0 + KF_SLOW_PASSING_SPACE               -> lfAllowedPassingSpace
//             (the multiplier is f29, the SHARED 0.0 literal flt_82001CC0 -- the same register the
//              clamp floor and the assert compare use -- i.e. a folded compile-time zero, NOT an
//              unrecovered .bss slot. Kept so the shape of the console expression survives.)
// 0x82787418  CGS_ASSERT(f28 != 0.0)  "Passing Space is zero"  (BrnAISteeringFan.cpp:1512)
// 0x82787498  f27 = 1.0 / f28
// 0x827874A0  GetPosition(lpCar); the vrlimi128 pair at 0x827874C0/CC packs (pos.x, pos.z) into
//             v121 -- lane 0 from mask 8 shift 0, lane 1 from mask 4 shift 1 (rotate-left-one
//             puts .z in .y). That is the same XZ flatten BrnMath::Flatten performs; lanes z/w of
//             v121 keep the raw position and are DEAD (every consumer of v1 splats lane 0 or 1).
// 0x827874D0  v119 = BrnMath::Flatten(GetDirection(lpCar))
// 0x8278751C  lpRacingLine->mfImmediateDistanceToTrafficImpact = 10000.0
// 0x8278753C  the fan loop. r26 counts 17 iterations while r27/r30 walk DOWNWARDS, so the console
//             visits liFanIndex 16..0. THAT ORDER IS LOAD-BEARING and is preserved below: the
//             "closest collision" test at 0x8278786C is a strict `<`, and |lRelativePosition|^2
//             does not depend on the fan index, so when one vehicle qualifies on several rays the
//             FIRST ray visited keeps the record -- and the recorded value
//             (lImmmediateApproachSpeedOfTrafficAhead = lRelativeVelocity) DOES depend on the ray.
//             Running the loop forwards would silently pick a different ray's relative velocity.
// 0x827878B0  mfImmmediateApproachSpeedOfTrafficAhead = |lImmmediateApproachSpeedOfTrafficAhead|
// 0x8278790C  mfImmediateDistanceToTrafficImpact = sqrt(itself)  -- UNCONDITIONAL, which is why
//             the 10000.0 seed above reports as 100.0 m when no traffic is found.
//             +0xB18 (mfImmediateTimeToTrafficImpact) is NOT written by this function.
//
// PSEUDOCODE LIES CORRECTED (all resolved from the asm / vmx128 fields):
//   * Hex-Rays renders the prototype as `int IncludeConstantBearing()` with no parameters at all;
//     the four arguments are the mr chain at 0x827873BC..0x827873D0.
//   * `v13 = ((_FP13 * 0.0) + flt_82F302C4)` hides that flt_82F302C4 is KF_SLOW_PASSING_SPACE and
//     that the 0.0 is a register, not an immediate.
//   * it prints `BrnAI::DistancePosVelToOrigin(v30)` -- an integer argument. The real arguments are
//     the two VECTOR registers live across the call: v1 = lRelativePosition, v2 = lRelativeVelocity
//     (v2 is set by the otherwise-dead `vmr128 v2, v123` at 0x827876EC, which is what identifies it).
//   * it shows the vcmpgtfp. CR6 extraction as an opaque bit shuffle; `extrwi r11, r11, 1, 24`
//     takes CR6 bit 0, the "all lanes true" bit, and the compared lanes are uniform splats, so the
//     tests are the plain scalar `< 0.0f` comparisons written below.
// ================================================================================================
void SteeringFan::IncludeConstantBearing(RacingLine* lpRacingLine, AICar* lpCar,
                                         const NearbyVehicles* lpNearbyTraffic)
{
    const f32 lfPlayerSpeed = lpCar->GetSpeed();                                      // :1506

    // :1509 -- rw::math::vpu::Clamp in the DWARF call list; the fsel pair in the asm.
    const f32 lfSpeedRatio = ClampUnitTraffic(lfPlayerSpeed / KF_GUESSED_MAX_SPEED);

    // :1511 -- see the banner: KF_PASSING_SPACE_SPEED_TERM is the console's folded 0.0.
    const f32 lfAllowedPassingSpace = lfSpeedRatio * KF_PASSING_SPACE_SPEED_TERM + KF_SLOW_PASSING_SPACE;
    CGS_ASSERT(lfAllowedPassingSpace != 0.0f, "Passing Space is zero\n");             // :1512
    const f32 lfReciprocalAllowedPassingSpace = 1.0f / lfAllowedPassingSpace;         // :1513

    // ARTIST 827874C0/CC: flatten XZ, retaining the original Z/W lanes.
    const Vector3 lCarPosition = lpCar->GetPosition();
    Vector2 lPlayerPosition;
    lPlayerPosition.mV = __vrlimi(lCarPosition.mV, lCarPosition.mV, 4, 1);

    const Vector2 lPlayerDirection = BrnMath::Flatten(lpCar->GetDirection());         // :1521

    f32 lfClosestCollision = KF_NO_CLOSEST_COLLISION;                                 // :1517
    Vector3 lImmmediateApproachSpeedOfTrafficAhead;                                   // :1519 (sic)
    lImmmediateApproachSpeedOfTrafficAhead.SetZero();

    lpRacingLine->mfImmediateDistanceToTrafficImpact = KF_NO_TRAFFIC_IMPACT_DISTANCE_SQUARED;

    // 16 -> 0; see the banner for why the direction is preserved.                    // :1505
    for (s32 liFanIndex = KI_FAN_STEPS - 1; liFanIndex >= 0; --liFanIndex)
    {
        // :1529 -- the velocity the car would have if it drove along this ray.
        // vmulfp128 v122, v0, v13 @0x82787588 (all four lanes; z feeds the Vector3 magnitude at
        // the very end via lRelativeVelocity).
        const Vector2& lUnitDirection = mUnitDirection[liFanIndex];
        Vector2 lPlayerMotion;
        __vector4 lSpeed;
        lSpeed.x = lfPlayerSpeed;
        lPlayerMotion.mV = __vmulfp(lUnitDirection.mV, __vspltw(lSpeed, 0));

        f32* lfAvoidTraffic         = &mfWeighting[eFan_AvoidTraffic][liFanIndex];         // :1537
        f32* lfAvoidOncomingTraffic = &mfWeighting[eFan_AvoidOncomingTraffic][liFanIndex]; // :1538

        // 0x8278755C..0x82787574: both rows are read out, then zeroed, then rebuilt from scratch
        // by the max-accumulate below, and finally interpolated back toward the saved values.
        const f32 lfOldTraffic  = *lfAvoidTraffic;                                    // :1540
        const f32 lfOldOncoming = *lfAvoidOncomingTraffic;                            // :1541
        *lfAvoidTraffic         = 0.0f;
        *lfAvoidOncomingTraffic = 0.0f;

        // 0x8278757C..0x827875CC: the console re-reads miCount three times here because
        // NearbyVehicles::GetCount @0x82766748 is inlined WITH its two asserts
        // (BrnAIDriver.cpp:2912 "miCount >= 0" and :2913 "miCount <= KI_MAX_NEARBY_TRAFFIC").
        // Those asserts belong to GetCount, not to this function -- see this lane's
        // header_request for restoring them there rather than duplicating them here.
        const s32 liTrafficCount = lpNearbyTraffic->GetCount();

        for (s32 liTrafficIndex = 0; liTrafficIndex < liTrafficCount; ++liTrafficIndex)  // :1533
        {
            const NearbyVehicle* lpTraffic = &lpNearbyTraffic->mVehicle[liTrafficIndex]; // :1535

            // vsubfp128 v123, v0, v122 @0x827875E0 -- all four lanes (see Magnitude3Traffic).
            Vector2 lRelativeVelocity;                                                // :1550
            lRelativeVelocity.mV = __vsubfp(lpTraffic->mVelocity.mV, lPlayerMotion.mV);

            // vsubfp128 v1, v0, v121 @0x827875F0 (mCentre is entry+0x10, reached as r11 - 0x10
            // with r11 == entry+0x20). Lanes z/w are dead in every consumer.
            Vector2 lRelativePosition;                                                // :1551
            lRelativePosition.mV = __vsubfp(lpTraffic->mCentre.mV, lPlayerPosition.mV);

            // 0x827875F4..0x82787610: a positive dot means the gap is opening. Only closing
            // vehicles are considered.
            const f32 lfAheadness = Dot2DTraffic(lRelativeVelocity, lRelativePosition); // :1553
            if (lfAheadness > 0.0f)
                continue;

            // 0x82787614..0x82787670 -- the player gets two extra outs.
            if (lpTraffic->mType == E_NEARBY_PLAYER)
            {
                // 0x82787620: while slamming, the player is a target, not an obstacle.
                if (meBiasMode == eBiasMode_Slam)
                    continue;
                // 0x8278762C: and a player who is BEHIND the car is not avoided either.
                if (Dot2DTraffic(lPlayerDirection, lRelativePosition) < 0.0f)
                    continue;
            }

            // 0x82787674..0x827876B4. Both magnitudes are 2-lane (vspltw 0 + vspltw 1).
            const f32 lfRelativeSpeedSquared = Dot2DTraffic(lRelativeVelocity, lRelativeVelocity);
            const f32 lfDistanceSquared = Dot2DTraffic(lRelativePosition, lRelativePosition);
            const f32 lfTimeToCollisionSquared = lfDistanceSquared / lfRelativeSpeedSquared;

            // 0x827876C4: the .bss constant this lane recovered. Note the console tests the
            // SQUARED time before taking the root -- that is what KF_TRAFFIC_IMPACT_TIME_SQUARED
            // exists for, and it is why the fdivs above is not guarded (see this lane's risks).
            if (lfTimeToCollisionSquared > KF_TRAFFIC_IMPACT_TIME_SQUARED)
                continue;

            const f32 lfTimeToCollision = SqrtTraffic(lfTimeToCollisionSquared, false);      // :1579

            // 0x82787730..0x82787738: 1 at the moment of impact, 0 at KF_TRAFFIC_IMPACT_TIME out.
            const f32 lfRisk = 1.0f - (lfTimeToCollision / KF_TRAFFIC_IMPACT_TIME);

            // 0x8278773C: v1 = lRelativePosition, v2 = lRelativeVelocity. The perpendicular
            // distance from the car to the line the relative motion sweeps, i.e. how close this
            // vehicle passes if neither party changes course.
            const f32 lfClosestApproach = DistancePosVelToOrigin(lRelativePosition, lRelativeVelocity);

            // 0x82787740..0x827877A0 (two Newton steps -- rw::math::vpu::Magnitude in the DWARF).
            const f32 lfSeparation = SqrtTraffic(lfDistanceSquared, true);                  // :1615
            if (lfSeparation > KF_MAX_TRAFFIC_CONSIDERATION_RANGE)
                continue;

            // 0x827877A4..0x827877B4: the required clearance grows with range, so a vehicle 80 m
            // away must miss by a full passing space while one at touching distance need only
            // miss at all.
            const f32 lfPassingSpace = lfSeparation * KF_RECIPROCAL_MAX_TRAFFIC_CONSIDERATION_RANGE
                                           * lfAllowedPassingSpace
                                     + std::fabs(lfClosestApproach);                  // :1608
            if (lfPassingSpace > lfAllowedPassingSpace)
                continue;

            // 0x827877B8..0x8278780C.
            const f32 lfCloseness = 1.0f - ClampUnitTraffic(lfPassingSpace * lfReciprocalAllowedPassingSpace);
            const f32 lfNewBias   = lfCloseness * lfRisk;                             // :1643

            // 0x82787800/0x8278781C: the vehicle's own velocity opposing the ray we are testing
            // is what makes it "oncoming"; anything else lands in the same-direction row.
            if (Dot2DTraffic(lpTraffic->mVelocity, lPlayerMotion) < 0.0f)
                *lfAvoidOncomingTraffic = MaxTraffic(*lfAvoidOncomingTraffic, lfNewBias);
            else
                *lfAvoidTraffic         = MaxTraffic(*lfAvoidTraffic, lfNewBias);

            // 0x82787844..0x82787878: the braking feed. Note the test is on lfCloseness, BEFORE
            // it is scaled by lfRisk.
            if (lfCloseness > KF_CLOSENESS_TO_BRAKE)
            {
                // :1660 -- MagnitudeSquared, and the member keeps the squared value until the
                // single sqrt at the end of the function.
                const f32 lfDistance = lfDistanceSquared;
                if (lfDistance < lfClosestCollision)
                {
                    lpRacingLine->mfImmediateDistanceToTrafficImpact = lfDistance;
                    lfClosestCollision = lfDistance;
                    lImmmediateApproachSpeedOfTrafficAhead.mV = lRelativeVelocity.mV;
                }
            }
        }

        // 0x82787888..0x827878A8 -- SteeringFan::Interpolate, inlined twice. Unconditional: with
        // no traffic at all both rows decay toward 0 by KF_TRAFFIC_BIAS_LERP per tick.
        *lfAvoidTraffic         = InterpolateTraffic(lfOldTraffic,  *lfAvoidTraffic,  KF_TRAFFIC_BIAS_LERP);
        *lfAvoidOncomingTraffic = InterpolateTraffic(lfOldOncoming, *lfAvoidOncomingTraffic, KF_TRAFFIC_BIAS_LERP);
    }

    // 0x827878B0..0x82787908 -- vmsum3fp128, i.e. all THREE lanes.
    lpRacingLine->mfImmmediateApproachSpeedOfTrafficAhead =
        Magnitude3Traffic(lImmmediateApproachSpeedOfTrafficAhead);

    // 0x8278790C..0x8278794C -- the squared distance accumulated above becomes a real distance.
    lpRacingLine->mfImmediateDistanceToTrafficImpact =
        SqrtTraffic(lpRacingLine->mfImmediateDistanceToTrafficImpact, true);
}

}

// FOLDED FROM BrnAISteeringFan_Weightings.cpp
#include <xtl.h>
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameSource/World/AI/BrnAICar.h"                             // AICar getters
#include "GameSource/World/AI/BrnAIDriver.h"                          // NearbyVehicle / NearbyVehicles
#include "GameSource/World/AI/BrnAIUtils.h"                           // FindSignedAngleBetween2DVectors
#include "GameSource/World/AI/Route/BrnRacingLine.h"                  // RacingLine
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"    // RacingLineGenerator + the two gates
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan_TrafficConstants.h" // KF_GUESSED_MAX_SPEED (CalculateFanAngle)


#include "SDKs/XboxMath/XMVectorSinCos.h"  // XboxMath::XMVectorSinCos (GenerateFanVectors' inlined SinCos)

#include <cmath>    // std::fmaf (the fmadds / vmaddfp sites), std::fabs, std::sqrt

// BrnAI::SteeringFan -- partfile 2 of 2 for the weighting half (aiwave R6 lane). This TU owns the
// per-round-robin weighting pass AIDriver::DoRoundRobinWork @0x82796340 drives:
//
//   BODIED
//     GenerateFanVectors            @0x827792C0
//     CachePointAhead               @0x827913C8
//     IncludeCentreLineTracking     @0x82786BC8   (row eFan_SteerToCentre)
//     IncludeRouteParallelTracking  @0x82786DB8   (row eFan_DriveParallel)
//     IncludePreferCurrentDirection @0x827694A8   (row eFan_PreferCurrentDirection)
//     FindPlayerInTraffic           @0x82769510
//     FindVictimInTraffic           @0x82769580
//     UpdateWeightings              @0x82794600
//     CalculateFanAngle             @0x82768CB0   (no IDA export -- decoded from image bytes, aiwave2)
//   BODIED ELSEWHERE (aiwave2 2026-09-05 partfiles, see the trailer note at the end of this file)
//     BrnAISteeringFan_HNG.cpp        IncludeHardNoGo / IncludeRouteEdgeIntersection / FanIntersectsEdge
//     BrnAISteeringFan_Traffic.cpp    IncludeConstantBearing
//     BrnAISteeringFan_Aggression.cpp IncludeSmashIntoTarget / IncludeSmashIntoPlayer / IncludeSmashIntoNearbyAI /
//                                     FindNeabyAIInTraffic / IncludeDriveCloseToPlayer / IncludeDrift*Tracking
//
// SHARED REGISTER MAP for UpdateWeightings @0x82794600 (the console's, read off 0x82794614..30):
//   r3/r31 = this, r4/r26 = lpCar, r5/r27 = lpRacingLine, r6/r28 = lpRacingLineGenerator,
//   r7/r25 = lpNearbyTraffic, r8/r24 = leAggressionVictim.
// Every Include* below is called with a permutation of those; the per-function banners name it.

namespace BrnAI
{
namespace
{
    // ARTIST 82779328..58: replace Y with Z while retaining the other lanes.
    Vector2 To2DFan(Vector3 lVector)
    {
        Vector2 lOut;
        lOut.mV = __vrlimi(lVector.mV, lVector.mV, 4, 1);
        return lOut;
    }

    // ARTIST 82779370..B0, 82786CF4..D58, 82786EF8..F58.
    // Keep the estimate, two VMX refinements and the complete register.
    Vector2 Normalize2DFan(Vector2 lVector)
    {
        const __vector4 lSquares = __vmulfp(lVector.mV, lVector.mV);
        const __vector4 lLength = __vaddfp(__vspltw(lSquares, 0), __vspltw(lSquares, 1));
        const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
        __vector4 lEstimate = __vrsqrtefp(lLength);
        __vector4 lSquare = __vmulfp(lEstimate, lEstimate);
        __vector4 lHalfEstimate = __vmulfp(lEstimate, lHalf);
        __vector4 lCorrection = __vnmsubfp(lLength, lSquare, lOne);
        lEstimate = __vmaddfp(lHalfEstimate, lCorrection, lEstimate);
        lSquare = __vmulfp(lEstimate, lEstimate);
        lHalfEstimate = __vmulfp(lEstimate, lHalf);
        lCorrection = __vnmsubfp(lLength, lSquare, lOne);
        lEstimate = __vmaddfp(lHalfEstimate, lCorrection, lEstimate);
        Vector2 lOut;
        lOut.mV = __vmulfp(lVector.mV, lEstimate);
        return lOut;
    }

    f32 Dot2DFan(const Vector2& lA, const Vector2& lB)
    {
        const __vector4 lProduct = __vmulfp(lA.mV, lB.mV);
        const __vector4 lSum = __vaddfp(__vspltw(lProduct, 0), __vspltw(lProduct, 1));
        return lSum.x;
    }

    // flt_820C3B70 == 1.1920929e-07 (FLT_EPSILON) -- the "is this delta vector zero" epsilon both
    // centre-line contributors test each component of the delta against before normalising.
    const f32 KF_FAN_TINY = 1.1920929e-07f;

    // The console's own look-ahead distance for the drift point cache: flt_82004D0C == 40.0
    // (CachePointAhead @0x82791440 `lfs f1, flt_82004D0C`).
    const f32 KF_FAN_POINT_AHEAD_DISTANCE = 40.0f;
}

// ========================================================================================
// CalculateFanAngle @0x82768CB0 -- NO IDA export; decoded from image bytes (tools/re/ppcdis.py,
// 38 instructions, 0x82768CB0..0x82768D44) by the conductor, aiwave2 2026-09-05.
//
//   bl AICar::GetSpeed                     -> f1
//   f0  = flt_82F30428  (1.3962634 = 80 deg)          KF_STEER_AT_LOW_SPEED   (DWARF :24)
//   f13 = flt_820C4168  (0.5)                          KF_STEER_AT_HIGH_SPEED  (DWARF :25)
//   f10 = f13 - f0
//   f11 = flt_820C4238  (15.0)                         look-ahead growth over the speed range
//   stfs flt_82F302C0 (15.0) -> +0x7FC                  mfLookAheadHNGRadius (every tick)
//   f13 = f1 / [0x8300D78C]                            KF_GUESSED_MAX_SPEED (DWARF :26; .bss,
//                                                       recovered by lane F2a = 100 mph)
//   fsel x2 -> saturate(f13) into [0, 1]
//   stfs (f10 * ratio + f0)        -> +0x800            mfFanAngle: 80 deg at rest -> 0.5 rad flat out
//   stfs (ratio * 15.0 + 10.0)     -> +0x7F8            mfLookAheadRadius: 10 m -> 25 m
// ========================================================================================
namespace
{
    const f32 KF_STEER_AT_LOW_SPEED    = 1.3962633609771729f;   // flt_82F30428 (80 deg)
    const f32 KF_STEER_AT_HIGH_SPEED   = 0.5f;                  // flt_820C4168
    const f32 KF_FAN_LOOK_AHEAD_BASE   = 10.0f;                 // flt_820C4150
    const f32 KF_FAN_LOOK_AHEAD_GROWTH = 15.0f;                 // flt_820C4238
    const f32 KF_FAN_LOOK_AHEAD_HNG    = 15.0f;                 // flt_82F302C0
}

void SteeringFan::CalculateFanAngle(AICar* lpCar)
{
    const f32 lfSpeed = lpCar->GetSpeed();

    mfLookAheadHNGRadius = KF_FAN_LOOK_AHEAD_HNG;                                   // stfs 0x7FC

    // The saturate is `fneg t,r ; fsel r,t,0.0,r` 0x82768D0C/0x82768D10 then `fsubs u,1.0,r ;
    // fsel r,u,r,1.0` 0x82768D18/0x82768D1C: fsel takes its THIRD operand on an unordered test, so
    // a NaN speed ratio is 1.0 (flat-out fan) -- spelt as the fsels (FX-AINAN2; the if/if kept NaN).
    f32 lfRatio = lfSpeed / KF_GUESSED_MAX_SPEED;                                    // fdivs
    lfRatio = (-lfRatio >= 0.0f) ? 0.0f : lfRatio;                                   // fsel vs 0.0
    lfRatio = ((1.0f - lfRatio) >= 0.0f) ? lfRatio : 1.0f;                           // fsel vs 1.0

    // Both lerps are ONE rounding (ROUNDING_RULE 3): 0x82768D20 `fmadds f0, f10, f13, f0` (f10 = high - low, the
    // fsubs at 0x82768CE4, rule 4; f13 = the ratio; f0 = low) and 0x82768D2C `fmadds f0, f13, f11, f0` (the
    // ratio, 15.0, 10.0). fmadds frD = frA*frC + frB == std::fmaf(frA, frC, frB).
    volatile f32 lfFanRange = KF_STEER_AT_HIGH_SPEED - KF_STEER_AT_LOW_SPEED;
    mfFanAngle        = lfFanRange * lfRatio + KF_STEER_AT_LOW_SPEED;                            // stfs 0x800
    mfLookAheadRadius = lfRatio * KF_FAN_LOOK_AHEAD_GROWTH + KF_FAN_LOOK_AHEAD_BASE; // stfs 0x7F8
}

// ========================================================================================
// GenerateFanVectors @0x827792C0
//
// Rebuild the 17 fan rays around the car for this tick.
//   r29 = this, r31 = lpCar.
//   0x827792F0 GetPosition(lpCar)         -> v127 ; stvx128 v127, r29, 0x340 -> mFanOrigin
//   0x82779304 GetUsefulDirection(lpCar)  -> v13  (XZ is flattened via the vrlimi128 pair)
//   0x8277933C mFanOrigin2D.xy = carPos.xz (vrlimi128 into the loaded this+0x350)
//   0x827793B4 f28 = FindSignedAngleBetween2DVectors(normalize2D(useful.xz), (0,1))
//              -- var_110/var_10C are stored 0.0 (flt_82001CC0) / 1.0 (flt_82001C98) to build
//              the (0,1) reference vector the angle is measured from.
//   loop i = 0..16 with lfT += mfReciprocalSteps (this+0x7F4, == 1/16):
//     0x82779474..90  angle = ((lfT - 0.5) * 2)^3 * mfFanAngle(this+0x800) + f28
//                     (f30 = flt_820C4168 == 0.5, f29 = flt_820C41F4 == 2.0)
//     0x82779498      CGS_ASSERT(i < 17, "Fan index out of range at ")
//     ...             XMVectorSinCos polynomial + vperm/vsldoi -> mUnitDirection[i] (this+0x220)
//     0x827796F4      mTarget[i]    = mFanOrigin2D + mUnitDirection[i] * mfLookAheadRadius
//                     (vmaddfp vD,vA,vB,vC == vA*vC + vB: vA = unit dir, vB = origin, vC = radius)
//     0x82779724      mHNGTarget[i] = mFanOrigin2D + mUnitDirection[i] * mfLookAheadHNGRadius
//   0x8277973C      mCentreTarget (this+0x330) = mTarget[8] (lvx this+0x80)
//
// ARTIST 827796C0..C8: vperm with bytes at 82CDA3C0
// {00010203,00010203,00010203,14151617} selects {sin,sin,sin,cos}.
// vsldoi by 8 bytes then stores {sin,cos,0,0}; there is no sine negation.
// This agrees with FindSignedAngleBetween2DVectors(A,(0,1)): sign(cross) = sign(A.x).
// Ray 8 follows the useful heading; ray 0 is left and ray 16 is right.
// ========================================================================================
void SteeringFan::GenerateFanVectors(AICar* lpCar)
{
    const Vector3 lCarPosition = lpCar->GetPosition();            // @0x8276B1F0
    const Vector3 lUseful      = lpCar->GetUsefulDirection();     // @0x82770028

    mFanOrigin      = lCarPosition;                               // stvx128 v127, r29, 0x340
    // ARTIST 82779340/54 updates only X and Y, retaining Z and W.
    mFanOrigin2D.mV = __vrlimi(mFanOrigin2D.mV, lCarPosition.mV, 8, 0);
    mFanOrigin2D.mV = __vrlimi(mFanOrigin2D.mV, lCarPosition.mV, 4, 1);
    const Vector2 lReference(0.0f, 1.0f);

    const Vector2 lCarDirection2DRaw = To2DFan(lUseful);

    const Vector2 lCarDirection2D = Normalize2DFan(lCarDirection2DRaw);
    const f32 lfBaseAngle = FindSignedAngleBetween2DVectors(lCarDirection2D, lReference);

    f32 lfT = 0.0f;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        // 0x82779474 fsubs, 0x82779480 fmuls 2.0, the cube as (x * x) * x (0x82779484 / 0x82779488), each rounded
        // (ROUNDING_RULE 4); then 0x8277948C `fmadds f0, f0, f13, f28` = cube * mfFanAngle + base, ONE rounding
        // (rule 3).
        const f32 lfInterp = (lfT - 0.5f) * 2.0f;
        volatile f32 lfSquared = lfInterp * lfInterp;
        volatile f32 lfCubed = lfSquared * lfInterp;
        const f32 lfAngle = lfCubed * mfFanAngle + lfBaseAngle;

        CGS_ASSERT(liStep < KI_FAN_STEPS, "Fan index out of range at ");

        // 0x8277955C..0x827796BC: the XDK's XMVectorSinCos, inlined (tables 0x82000BD0..0x82000C6F; sine in v13,
        // cosine in v0 at 0x827796C0). It is XboxMath::XMVectorSinCos bit for bit: the real words run on emu64
        // agree with it on 1500 angles (FX-GATE, crash parity 2026-09-25). std::sin / std::cos stood in before.
        f32 lfSin;
        f32 lfCos;
        XboxMath::XMVectorSinCos(&lfSin, &lfCos, lfAngle);

        const Vector2 lUnit(lfSin, lfCos);
        mUnitDirection[liStep] = lUnit;

        // 0x827796F4 / 0x82779724 `vmaddfp` (raw D,A,B,C = v13, v0, v13, v12 / v0, v0, v13, v12) = unit * radius +
        // origin, ONE rounding per lane (ROUNDING_RULE 3).
        const XMVECTOR lDirection = __lvx(&lUnit, 0);
        const XMVECTOR lOrigin = __lvx(&mFanOrigin2D, 0);
        __stvx(__vmaddfp(lDirection, XMVectorReplicate(mfLookAheadRadius), lOrigin), &mTarget[liStep], 0);
        __stvx(__vmaddfp(lDirection, XMVectorReplicate(mfLookAheadHNGRadius), lOrigin), &mHNGTarget[liStep], 0);

        lfT += mfReciprocalSteps;
    }

    mCentreTarget = mTarget[KI_FAN_STEPS / 2];                    // lvx this+0x80 -> stvx this+0x330
}

// ========================================================================================
// CachePointAhead @0x827913C8   (this r3, lpRacingLineGenerator r4, lpRacingLine r5)
//
//   0x827913E0  if (!lpRacingLine->mbIsInitialised (racingline+0xBD0))
//                   { mbPointAheadKnown = false; mbCentreHereKnown = false; return; }
//   0x827913EC  if (kfBias[mode][eFan_DriftFinalLocation] != 0 ||
//                   kfBias[mode][eFan_DriftFinalDirection] != 0)
//                   mbPointAheadKnown = GetPointFarAhead(lpRacingLine, 40.0 (flt_82004D0C),
//                                                        mFanOrigin2D, mCentreFarAhead,
//                                                        mCentreAheadFarAhead);
//   0x8279144C  mbCentreHereKnown = GetCentreCentreLineHere(lpRacingLine, mCentreTarget,
//                                                           mCentreTarget, mCentreHere,
//                                                           mCentreAhead);
//               (the console loads v2 from this+0x330 and then `vmr v1, v2` -- BOTH vector
//                arguments are mCentreTarget; that is not a transcription slip.)
// ========================================================================================
void SteeringFan::CachePointAhead(RacingLineGenerator* lpRacingLineGenerator,
                                  RacingLine* lpRacingLine)
{
    if (lpRacingLine->mbIsInitialised)
    {
        if (kfBias[meBiasMode][eFan_DriftFinalLocation]  != 0.0f ||
            kfBias[meBiasMode][eFan_DriftFinalDirection] != 0.0f)
        {
#if BRN_AI_RACINGLINE_STACK_PRESENT
            mbPointAheadKnown = lpRacingLineGenerator->GetPointFarAhead(lpRacingLine,
                                                                        KF_FAN_POINT_AHEAD_DISTANCE,
                                                                        mFanOrigin2D,
                                                                        mCentreFarAhead,
                                                                        mCentreAheadFarAhead);
#else
#endif
        }

#if BRN_AI_RACINGLINE_STACK_PRESENT
        mbCentreHereKnown = lpRacingLineGenerator->GetCentreCentreLineHere(lpRacingLine,
                                                                           mCentreTarget,
                                                                           mCentreTarget,
                                                                           mCentreHere,
                                                                           mCentreAhead);
#else
        mbCentreHereKnown = false;
#endif
    }
    else
    {
        mbPointAheadKnown = false;      // stb 0, 0x808
        mbCentreHereKnown = false;      // stb 0, 0x809
    }
}

// ========================================================================================
// IncludeCentreLineTracking @0x82786BC8   (this r3, lpRacingLineGenerator r4 UNUSED, lpRacingLine r5)
// Row eFan_SteerToCentre (this+0x3B4 == mfWeighting[0]).
//
//   0x82786BC8  racing line not initialised (racingline+0xBD0) -> zero the row, return.
//   0x82786BF4  !mbCentreHereKnown (this+0x809)                -> zero the row, return.
//   0x82786C20  delta = mCentreHere (this+0x390) - Flatten(mFanOrigin) (this+0x340)
//   0x82786C64..CF0  if |delta.x| <= FLT_EPSILON && |delta.y| <= FLT_EPSILON -> return with the
//               row LEFT AS IT WAS (`bnelr`, not a zero-then-return -- unlike the two arms above).
//   0x82786D00  f13 = lpRacingLine->mfCentreLineAhead (racingline+0xC04)
//               f12 = lpRacingLine->mfCentreLineAheadRecip (racingline+0xC08)
//   loop i: dot = dot2D(mUnitDirection[i], normalize2D(delta));
//           row[i] = (dot > mfCentreLineAhead) ? -((dot - mfCentreLineAhead) * recip) : 0.0
//           (the `fneg` at 0x82786D8C is the console's -- the weight is NEGATIVE, so with
//            kfBias[eBiasMode_Race][eFan_SteerToCentre] == +20 the fan PENALISES rays that point
//            more directly at the centre line than the mfCentreLineAhead threshold.)
// ========================================================================================
void SteeringFan::IncludeCentreLineTracking(RacingLineGenerator* lpRacingLineGenerator,
                                            RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;   // r4 -- never read by the console body

    if (!lpRacingLine->mbIsInitialised || !mbCentreHereKnown)
    {
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            mfWeighting[eFan_SteerToCentre][liStep] = 0.0f;
        return;
    }

    Vector2 lDelta;
    lDelta.mV.x = mCentreHere.mV.x - mFanOrigin.mV.x;
    lDelta.mV.y = mCentreHere.mV.y - mFanOrigin.mV.z; // ARTIST 82786C4C: world Z into planar Y
    lDelta.mV.z = 0.0f;
    lDelta.mV.w = 0.0f;

    // IsZero is a per-lane non-dot vcmpgtfp |d| > eps gathered by vperm + `cmpwi ; bne/beq`
    // (0x82786C88..0x82786CF0): a NaN lane is never "greater", so a NaN delta counts as zero and
    // returns here (FX-AINAN2; `fabs <= tiny` called it non-zero and wrote a 0.0 row).
    if (!(std::fabs(lDelta.mV.x) > KF_FAN_TINY) && !(std::fabs(lDelta.mV.y) > KF_FAN_TINY))
        return;                                            // bnelr: the row keeps its old values

    const f32 lfAhead      = lpRacingLine->mfCentreLineAhead;
    const f32 lfAheadRecip = lpRacingLine->mfCentreLineAheadRecip;
    const Vector2 lToCentre = Normalize2DFan(lDelta);

    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        const f32 lfDot = Dot2DFan(mUnitDirection[liStep], lToCentre);
        if (lfDot > lfAhead)
            mfWeighting[eFan_SteerToCentre][liStep] = -((lfDot - lfAhead) * lfAheadRecip);
        else
            mfWeighting[eFan_SteerToCentre][liStep] = 0.0f;
    }


}

// ========================================================================================
// IncludeRouteParallelTracking @0x82786DB8  (this r3, lpRacingLineGenerator r4 UNUSED, lpRacingLine r5)
// Row eFan_DriveParallel (this+0x590 == mfWeighting[7]). NO external calls at all.
//
//   0x82786DB8  racing line not initialised -> zero the row, return.
//   0x82786DE4  !mbCentreHereKnown          -> zero the row, return.
//   0x82786E10  delta = mCentreHere (this+0x390) - mCentreAhead (this+0x3A0)
//               (vsubfp v13, v12, v13 with v12 = +0x390 and v13 = +0x3A0), i.e. it points BACK
//               along the road.
//   0x82786E58..ED4  if |delta.x| <= FLT_EPSILON && |delta.y| <= FLT_EPSILON -> zero the row, return.
//   loop i:   d = dot2D(mUnitDirection[i], normalize2D(delta));
//             d = max(d, -1.0)  (flt_820037C8 == -1.0, fsubs+fsel at 0x82786F80/84)
//             d = min(d,  1.0)  (flt_82001C98 ==  1.0, fsubs+fsel at 0x82786F90/94)
//             row[i] = (d + 1.0) * 0.5      (flt_820C4168 == 0.5)
//   So the row is 1 for a ray pointing straight BACK down the road and 0 for one pointing along
//   it; with kfBias[eBiasMode_Race][eFan_DriveParallel] == -200 that is a large penalty for
//   turning round.
// ========================================================================================
void SteeringFan::IncludeRouteParallelTracking(RacingLineGenerator* lpRacingLineGenerator,
                                               RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;   // r4 -- never read by the console body

    if (!lpRacingLine->mbIsInitialised || !mbCentreHereKnown)
    {
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            mfWeighting[eFan_DriveParallel][liStep] = 0.0f;
        return;
    }

    Vector2 lDelta;
    lDelta.mV = __vsubfp(mCentreHere.mV, mCentreAhead.mV); // ARTIST 82786E38

    // IsZero: the same vperm-gathered vcmpgtfp (0x82786E6C / 0x82786EA4) -- a NaN delta counts as
    // zero and zeroes the row (FX-AINAN2; `fabs <= tiny` called it non-zero).
    if (!(std::fabs(lDelta.mV.x) > KF_FAN_TINY) && !(std::fabs(lDelta.mV.y) > KF_FAN_TINY))
    {
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            mfWeighting[eFan_DriveParallel][liStep] = 0.0f;
        return;
    }

    const Vector2 lBackAlongRoad = Normalize2DFan(lDelta);
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        // Clamp(d, -1, 1): `fsubs t,-1.0,d ; fsel d,t,-1.0,d` 0x82786F80/0x82786F84 then
        // `fsubs u,1.0,d ; fsel d,u,d,1.0` 0x82786F90/0x82786F94 -- a NaN dot is 1.0 (FX-AINAN2).
        f32 lfDot = Dot2DFan(mUnitDirection[liStep], lBackAlongRoad);
        lfDot = ((-1.0f - lfDot) >= 0.0f) ? -1.0f : lfDot;
        lfDot = ((1.0f - lfDot) >= 0.0f) ? lfDot : 1.0f;
        mfWeighting[eFan_DriveParallel][liStep] = (lfDot + 1.0f) * 0.5f;
    }
}

// ========================================================================================
// IncludePreferCurrentDirection @0x827694A8   (this r3 only -- a leaf, no calls, no branches)
// Row eFan_PreferCurrentDirection (this+0x5D4 == mfWeighting[8]).
//
//   flt_82046E00 == 0.0625, flt_820C4168 == 0.5, flt_820C41F4 == 2.0, flt_82001C98 == 1.0.
//   row[i] = (1 - |((i * 0.0625) - 0.5) * 2|) ^ 2
// i.e. a squared tent peaking at 1 on the centre ray and falling to 0 at both fan edges. Note the
// console uses the literal 0.0625 here, NOT mfReciprocalSteps.
// (kfBias's eFan_PreferCurrentDirection column is 0.0 in all 10 bias modes in this build, so
// UpdateWeightings never actually calls it -- the body is here because the guard is the console's,
// not ours, and a later build/mode could re-enable the column.)
// ========================================================================================
void SteeringFan::IncludePreferCurrentDirection()
{
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        const f32 lfV      = std::fabs(((static_cast<f32>(liStep) * 0.0625f) - 0.5f) * 2.0f);
        const f32 lfWeight = 1.0f - lfV;
        mfWeighting[eFan_PreferCurrentDirection][liStep] = lfWeight * lfWeight;
    }
}

// ========================================================================================
// FindPlayerInTraffic @0x82769510   (this r3 UNUSED, lpNearbyTraffic r4)
// First entry of the avoidance list whose mType (entry+0x20) is E_NEARBY_PLAYER (2); NULL when the
// list is empty or holds no player. The console walks the NearbyVehicles object itself with stride
// 0x70 from +0x00, which is mVehicle[i] -- named here, never offset-hacked.
// ========================================================================================
const NearbyVehicle* SteeringFan::FindPlayerInTraffic(const NearbyVehicles* lpNearbyTraffic)
{
    const s32 liCount = lpNearbyTraffic->GetCount();       // bl NearbyVehicles::GetCount
    for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
    {
        if (lpNearbyTraffic->mVehicle[liEntry].mType == E_NEARBY_PLAYER)
            return &lpNearbyTraffic->mVehicle[liEntry];
    }
    return 0;
}

// ========================================================================================
// FindVictimInTraffic @0x82769580   (this r3 UNUSED, lpNearbyTraffic r4, leAggressionVictim r5)
// First entry whose meGlobalRaceCarIndex (entry+0x24) equals the aggression victim; NULL if none.
// ========================================================================================
const NearbyVehicle* SteeringFan::FindVictimInTraffic(const NearbyVehicles* lpNearbyTraffic,
                                                      EGlobalRaceCarIndex leAggressionVictim)
{
    const s32 liCount = lpNearbyTraffic->GetCount();
    for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
    {
        if (lpNearbyTraffic->mVehicle[liEntry].meGlobalRaceCarIndex == leAggressionVictim)
            return &lpNearbyTraffic->mVehicle[liEntry];
    }
    return 0;
}

// ========================================================================================
// UpdateWeightings @0x82794600 -- the whole round-robin weighting pass.
//
// Refresh the fan geometry, cache the racing-line samples, then run every contributor whose
// kfBias column is non-zero for the current bias mode. The kfBias tests ARE the console's own
// dispatch (`lwz r11,0x3B0(r31)` / `mulli r11,r11,0x38` / `lfsx` / `fcmpu` against
// flt_82001CC0 == 0.0 before each `bl`), not a host optimisation -- reproducing them is what keeps
// the parked contributors below unreachable in the modes whose column is zero.
//
// The six CgsDev::PerfMonCpu::StartMonitor/StopMonitor pairs around the blocks (ids dword_82F302D8
// .. 0x82F302F0, the miSteeringFanPM[7] the DWARF declares) are the same presentation-only
// profiling SteeringFan::Prepare already declines to register on this host; not reproduced.
// ========================================================================================
void SteeringFan::UpdateWeightings(AICar* lpCar, RacingLine* lpRacingLine,
                                   RacingLineGenerator* lpRacingLineGenerator,
                                   const NearbyVehicles* lpNearbyVehicles,
                                   EGlobalRaceCarIndex leVictim)
{
    const f32* lpfBias = kfBias[meBiasMode];

    CalculateFanAngle(lpCar);                                        // @0x82794640
    GenerateFanVectors(lpCar);                                       // @0x8279464C
    CachePointAhead(lpRacingLineGenerator, lpRacingLine);            // @0x82794664

    if (lpfBias[eFan_SteerToCentre] != 0.0f)
        IncludeCentreLineTracking(lpRacingLineGenerator, lpRacingLine);          // @0x827946A0

    if (lpfBias[eFan_DriveParallel] != 0.0f)
        IncludeRouteParallelTracking(lpRacingLineGenerator, lpRacingLine);       // @0x827946D8

    if (lpfBias[eFan_AvoidHNG] != 0.0f || lpfBias[eFan_ExitHNG] != 0.0f)
        IncludeHardNoGo(lpRacingLineGenerator, lpRacingLine);                    // @0x82794720

    if (lpfBias[eFan_AvoidTraffic] != 0.0f || lpfBias[eFan_AvoidOncomingTraffic] != 0.0f)
        IncludeConstantBearing(lpRacingLine, lpCar, lpNearbyVehicles);           // @0x8279476C

    if (lpfBias[eFan_AvoidEdges] != 0.0f)
        IncludeRouteEdgeIntersection(lpRacingLineGenerator, lpRacingLine);       // @0x8279479C

    if (lpfBias[eFan_PreferCurrentDirection] != 0.0f)
        IncludePreferCurrentDirection();                                         // @0x827947C4

    if (lpfBias[eFan_DriftFinalDirection] != 0.0f)
        IncludeDriftDirectionTracking(lpRacingLineGenerator, lpRacingLine);      // @0x827947EC

    if (lpfBias[eFan_DriftFinalLocation] != 0.0f)
        IncludeDriftLocationTracking(lpRacingLineGenerator, lpRacingLine);       // @0x82794814

    if (lpfBias[eFan_SmashIntoPlayer] != 0.0f)
        IncludeSmashIntoPlayer(lpCar, lpNearbyVehicles, leVictim);               // @0x82794840

    if (lpfBias[eFan_SmashIntoRivals] != 0.0f)
        IncludeSmashIntoNearbyAI(lpCar, lpNearbyVehicles);                       // @0x82794868

    if (lpfBias[eFan_DriveCloseToPlayer] != 0.0f)
        IncludeDriveCloseToPlayer(lpRacingLine, lpCar, lpNearbyVehicles);        // @0x82794894
}

// ========================================================================================
// The eleven contributors that used to be parked here (IncludeHardNoGo, IncludeConstantBearing,
// IncludeRouteEdgeIntersection, IncludeDriftDirectionTracking, IncludeDriftLocationTracking,
// IncludeDriveCloseToPlayer, FindNeabyAIInTraffic, IncludeSmashIntoTarget, IncludeSmashIntoPlayer,
// IncludeSmashIntoNearbyAI, FanIntersectsEdge) were bodied by aiwave2 (2026-09-05) in their own
// partfiles: BrnAISteeringFan_HNG.cpp (HNG / route-edge / FanIntersectsEdge),
// BrnAISteeringFan_Traffic.cpp (constant bearing + BrnAISteeringFan_TrafficConstants.h) and
// BrnAISteeringFan_Aggression.cpp (smash / drive-close / drift + BrnAISteeringFan_AggressionConstants.h).
// ========================================================================================

}
