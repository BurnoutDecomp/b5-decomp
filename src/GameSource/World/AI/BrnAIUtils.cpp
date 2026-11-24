#include "GameSource/World/AI/BrnAIUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (StepTo negative-step guard)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CgsDev::Assert::MessageAt

#include <cmath>   // std::sqrt (the de-optimised VMX rsqrt paths)

namespace BrnAI
{
    // 0x82766BA8 - move lfCurrent toward lfTarget, capping the move at lfStep.
    // asm: f31=current, f29=target, f30=step; f28 = fabs(target-current). Assert step >= 0.
    // If step > |target-current| the move overshoots, so return the target. Otherwise step toward
    // the target: +step when target >= current, -step otherwise.
    //
    // UNORDERED (NaN) POLARITY (crash parity FX-AINAN2). Every test is `fcmpu` + one CR bit:
    //   0x82766BD4/0x82766BDC  fcmpu step,0.0      ; bge skip-assert  -> only an ORDERED step < 0
    //                                                                    fires "Negative Step"
    //   0x82766C00/0x82766C04  fcmpu step,|delta|  ; ble -> C10       -> return the target only
    //                                                                    on an ordered step > |d|
    //   0x82766C10/0x82766C14  fcmpu target,current; bge -> C20 (+)   -> +step on >= AND unordered
    // so a NaN target (|delta| NaN, the ble falls to C10) steps UP by lfStep. The old
    // `if (target >= current)` stepped a NaN target DOWN, and `CGS_ASSERT(step >= 0)` fired on a
    // NaN step the console lets through. UpdateSteeringAngle feeds this every frame.
    f32 StepTo(f32 lfCurrent, f32 lfTarget, f32 lfStep)
    {
        const f32 lfDelta = lfTarget - lfCurrent;
        const f32 lfAbsDelta = std::fabs(lfDelta);                  // fabs f28, f13 @0x82766BD8

        CGS_ASSERT(!(lfStep < 0.0f), CgsDev::Assert::MessageAt("Negative Step", __FILE__, 176));   // ARTIST 0x82766BE8

        if (lfStep > lfAbsDelta)
            return lfTarget;

        // ARTIST 0x82766C14 `bge` to the +step arm, the -step arm falling through: the test is
        // `target < current` (an unordered compare also takes the +step arm).
        if (lfTarget < lfCurrent)
            return lfCurrent - lfStep;

        return lfCurrent + lfStep;
    }

    // 0x82771800 - 2D line-line intersection parameters for segments (lP1->lP2) and (lQ1->lQ2).
    // asm names the deltas:
    //   dQy = lQ2.mV.y - lQ1.mV.y   dPx = lP2.mV.x - lP1.mV.x
    //   dQx = lQ2.mV.x - lQ1.mV.x   dPy = lP2.mV.y - lP1.mV.y
    //   denom = dPx*dQy - dPy*dQx
    // When denom == 0 the segments are parallel: write 0 to both outputs and return false.
    // Otherwise (with rPx = lP1.mV.x - lQ1.mV.x, rPy = lP1.mV.y - lQ1.mV.y, inv = 1/denom):
    //   *lpfParamA = (rPy*dQx - rPx*dQy) * inv
    //   *lpfParamB = (rPy*dPx - rPx*dPy) * inv
    bool Calc2DIntersectionEquationData(Vector2 lP1,
                                        Vector2 lP2,
                                        Vector2 lQ1,
                                        Vector2 lQ2,
                                        f32*    lpfParamA,
                                        f32*    lpfParamB)
    {
        const f32 lfDQy = lQ2.mV.y - lQ1.mV.y;
        const f32 lfDPx = lP2.mV.x - lP1.mV.x;
        const f32 lfDQx = lQ2.mV.x - lQ1.mV.x;
        const f32 lfDPy = lP2.mV.y - lP1.mV.y;

        const f32 lfDenom = (lfDPx * lfDQy) - (lfDPy * lfDQx);

        if (lfDenom == 0.0f)
        {
            *lpfParamA = 0.0f;
            *lpfParamB = 0.0f;
            return false;
        }

        const f32 lfRPx = lP1.mV.x - lQ1.mV.x;
        const f32 lfRPy = lP1.mV.y - lQ1.mV.y;
        const f32 lfInv = 1.0f / lfDenom;

        *lpfParamA = ((lfRPy * lfDQx) - (lfRPx * lfDQy)) * lfInv;
        *lpfParamB = ((lfRPy * lfDPx) - (lfRPx * lfDPy)) * lfInv;
        return true;
    }

    // 0x827651F0 -- SEMANTIC reconstruction of the VMX body (vrsqrtefp + 2 Newton-Raphson
    // steps lowered to one 1/sqrt; the trailing vcmpeqfp. self-compare lowered to a NaN guard,
    // per the committed Calc2DIntersectionEquationData / VMX precedent).
    //
    // v1 = lPosition (arg 1), v2 = lVelocity (arg 2) -- the PPC vector ABI order. Returns the
    // signed perpendicular distance from the origin to the line through lPosition running in
    // direction lVelocity, i.e. cross(lPosition, lVelocity) / |lVelocity.xy|. When |lVelocity.xy|
    // is zero the result falls back to |lPosition.xy|. The result is asserted finite ('Bad
    // maths!', X360 baked BrnAIUtils.h:141 -- path/line dropped).
    //
    // CORRECTED 2026-09-05 (aiwave2 lane F2a, re-read against the asm): the earlier body had the
    // two arguments' ROLES swapped -- it divided by |lPosition|, fell back to |lVelocity| and
    // formed cross(lVelocity, lPosition). The asm is unambiguous: 0x82765200 `vmulfp128 v13,v2,v2`
    // (the divisor is |v2| == arg 2), 0x82765270 `vmulfp128 v0,v1,v1` (the fallback is |v1|),
    // 0x827652B8..0x827652F4 `v1.x*v2.y - v1.y*v2.x` (cross(arg1, arg2)). The DWARF twin
    // BrnTraffic::TrafficEntityModule::Avoidance_CalculateDistancePosVelToOrigin(lStart, lVel)
    // agrees. Call sites pass (position, velocity) and were never wrong.
    f32 DistancePosVelToOrigin(Vector2 lPosition, Vector2 lVelocity)
    {
        // |lVelocity.xy| via len2 * rsqrt(len2); the vsel yields 0 for a zero-length vector.
        const f32 lfVelLenSq = (lVelocity.mV.x * lVelocity.mV.x) + (lVelocity.mV.y * lVelocity.mV.y);
        const f32 lfVelLen   = (lfVelLenSq == 0.0f) ? 0.0f : (lfVelLenSq * (1.0f / std::sqrt(lfVelLenSq)));

        f32 lfResult;
        if (lfVelLen == 0.0f)
        {
            // Fallback: |lPosition.xy| the same way (zero-length -> 0).
            const f32 lfPosLenSq = (lPosition.mV.x * lPosition.mV.x) + (lPosition.mV.y * lPosition.mV.y);
            lfResult = (lfPosLenSq == 0.0f) ? 0.0f : (lfPosLenSq * (1.0f / std::sqrt(lfPosLenSq)));
        }
        else
        {
            // cross(lPosition, lVelocity) scaled by 1/|lVelocity.xy|.
            const f32 lfCross = (lPosition.mV.x * lVelocity.mV.y) - (lPosition.mV.y * lVelocity.mV.x);
            lfResult = lfCross * (1.0f / lfVelLen);
        }

        // vcmpeqfp self-compare: a finite result equals itself; a NaN does not.
        CGS_ASSERT(lfResult == lfResult, "Bad maths!");
        return lfResult;
    }

    // 0x8276DDB8 -- SEMANTIC reconstruction (aiwave2 lane R2 + conductor, 2026-09-05), same
    // VMX lowering as DistancePosVelToOrigin above. v1 = lPoint, v2 = lLineStart, v3 = lLineEnd:
    //   0x8276DDC8 vsubfp v9, v3, v2      line    = end - start
    //   0x8276DDD4 vsubfp v8, v1, v2      toPoint = point - start
    //   0x8276DE10.. |line| via rsqrt + 2 Newton steps, vsel -> 0 for a zero-length line
    //   0x8276DE50 fcmpu |line| vs 0.0 ; beq -> fallback |toPoint| (0x8276DE58..)
    //   0x8276DEA0.. cross = line.x*toPoint.y - line.y*toPoint.x (v13 = v9.x*v8.y, v0 = v9.y*v8.x,
    //                vsubfp v0, v13, v0), scaled by 1/|line| (fdivs 1.0/f13)
    //   then the vcmpeqfp self-compare -> 'Bad maths! Point = ..., Start = ..., End = ...'
    //   (BrnAIUtils.cpp:117, StrStream-formatted; the retail build emits it).
    f32 DistancePointToLine(Vector2 lPoint, Vector2 lLineStart, Vector2 lLineEnd)
    {
        const f32 lfLineX    = lLineEnd.mV.x - lLineStart.mV.x;
        const f32 lfLineY    = lLineEnd.mV.y - lLineStart.mV.y;
        const f32 lfToPointX = lPoint.mV.x   - lLineStart.mV.x;
        const f32 lfToPointY = lPoint.mV.y   - lLineStart.mV.y;

        const f32 lfLineLenSq = (lfLineX * lfLineX) + (lfLineY * lfLineY);
        const f32 lfLineLen   = (lfLineLenSq == 0.0f) ? 0.0f : (lfLineLenSq * (1.0f / std::sqrt(lfLineLenSq)));

        f32 lfResult;
        if (lfLineLen == 0.0f)
        {
            const f32 lfToPointLenSq = (lfToPointX * lfToPointX) + (lfToPointY * lfToPointY);
            lfResult = (lfToPointLenSq == 0.0f) ? 0.0f : (lfToPointLenSq * (1.0f / std::sqrt(lfToPointLenSq)));
        }
        else
        {
            const f32 lfCross = (lfLineX * lfToPointY) - (lfLineY * lfToPointX);
            lfResult = lfCross * (1.0f / lfLineLen);
        }

        CGS_ASSERT(lfResult == lfResult, "Bad maths!");
        return lfResult;
    }

    // 0x827653C0 -- the DWARF BrnAIUtils.h:180 overload (locals lB :182, lA :183, lfSelfDot :185,
    // lfLineLength :197, lfDot :198, lfParam :200). ARTIST emits it out of line for its only
    // caller, LineTestTrafficHNG @0x8277A878 (the PS3 build inlines it there). Crash parity G07-D1
    // (FX-AI-RUMBLE, 2026-09-23). v1 = l2DPoint, v2 = l2DStartLine, v3 = l2DEndLine,
    // r3 = &l2DPointOnLine; VMX operands checked with tools/re/vmx128.py:
    //   0x827653C0 vsubfp v12, v3, v2          lB = end - start
    //   0x827653CC vsubfp v10, v1, v2          lA = point - start
    //   0x827653D4..0x827653E0                 lfSelfDot = lB.x*lB.x + lB.y*lB.y
    //   0x827653E4 vcmpeqfp. selfdot == 0 -> the degenerate arm (0x827653F8..0x82765458):
    //       0x82765408 stvx128 v2, r0, r3      *out = the START
    //       return |lA| (vrsqrtefp + 2 Newton steps, vsel -> 0 when |lA|^2 == 0)
    //   else (0x8276545C..0x82765528):
    //       lfParam      = lfDot * (1/lfSelfDot)   (vrefp + 2 Newton; lfDot = lB.x*lA.x + lB.y*lA.y)
    //       lfLineLength = Magnitude(lB)           (lfSelfDot * rsqrt, vsel -> 0 @0x827654E4)
    //       0x827654FC vmaddfp v12, v12, v2, v9    (raw D,A,B,C: lB*lfParam + start) -> *out
    //       0x827654D4..0x82765508                 Cross(lB, lA) = lB.x*lA.y - lB.y*lA.x
    //       return Cross * (1/lfLineLength)        (vrefp + 2 Newton) -- SIGNED, no fabs
    // The estimate + Newton pairs lower to exact reciprocals/sqrt, as in the overload above.
    f32 DistancePointToLine(Vector2 l2DPoint, Vector2 l2DStartLine, Vector2 l2DEndLine,
                            Vector2& l2DPointOnLine)
    {
        const f32 lfBX = l2DEndLine.mV.x - l2DStartLine.mV.x;   // lB
        const f32 lfBY = l2DEndLine.mV.y - l2DStartLine.mV.y;
        const f32 lfAX = l2DPoint.mV.x   - l2DStartLine.mV.x;   // lA
        const f32 lfAY = l2DPoint.mV.y   - l2DStartLine.mV.y;

        const f32 lfSelfDot = (lfBX * lfBX) + (lfBY * lfBY);
        if (lfSelfDot == 0.0f)
        {
            l2DPointOnLine = l2DStartLine;
            const f32 lfALengthSquared = (lfAX * lfAX) + (lfAY * lfAY);
            return (lfALengthSquared == 0.0f) ? 0.0f
                                              : (lfALengthSquared * (1.0f / std::sqrt(lfALengthSquared)));
        }

        const f32 lfLineLength = (lfSelfDot == 0.0f) ? 0.0f : (lfSelfDot * (1.0f / std::sqrt(lfSelfDot)));
        const f32 lfDot        = (lfBX * lfAX) + (lfBY * lfAY);
        const f32 lfParam      = lfDot * (1.0f / lfSelfDot);

        // The whole 16-byte register is stored: every lane is lB * lfParam + start.
        const __vector4 lDelta = __vsubfp(l2DEndLine.mV, l2DStartLine.mV);
        __vector4 lParam;
        lParam.x = lfParam;
        l2DPointOnLine.mV = __vmaddfp(lDelta, __vspltw(lParam, 0), l2DStartLine.mV);


        const f32 lfCross = (lfBX * lfAY) - (lfBY * lfAX);
        return lfCross * (1.0f / lfLineLength);
    }

    // 0x82768680 -- SEMANTIC reconstruction of the branchless SIMD point-in-section test
    // (the SoA precomputed-edge twin of AISection::IsInside). The X360 body runs all four
    // edge half-plane tests in parallel across the VMX lanes, then AND-reduces (vperm +
    // vcmpequw vs all-ones) to a single inside/outside bit.
    //
    // lpSectionEdges points at four consecutive 4-lane vectors (lane i == edge i):
    //   edgeX0 @+0x00, edgeY0 @+0x10, coefA @+0x20, coefB @+0x30.
    // Per edge the side is  coefA[i]*(lfY - edgeY0[i]) - coefB[i]*(lfX - edgeX0[i]); an ORDERED side >= 0 is OUTSIDE (inside = all four `!(side >= 0)`, see the body).
    // FLAGGED (low): the section-edge SoA field semantics beyond these four attested
    // lane-vectors are not DWARF-attested; the vperm(unk_8327F110)+vcmpequw reduction is
    // modelled as a plain over-all-four-edges AND. Pointer kept opaque (const void*).
    bool IsInsideSectionFast(const void* lpSectionEdges, f32 lfX, f32 lfY)
    {
        const f32* lpfBase   = reinterpret_cast<const f32*>(lpSectionEdges);
        const f32* lpfEdgeX0 = lpfBase + 0;   // +0x00, 4 lanes
        const f32* lpfEdgeY0 = lpfBase + 4;   // +0x10, 4 lanes
        const f32* lpfCoefA  = lpfBase + 8;   // +0x20, 4 lanes
        const f32* lpfCoefB  = lpfBase + 12;  // +0x30, 4 lanes

        for (s32 liEdge = 0; liEdge < 4; ++liEdge)
        {
            const f32 lfSide = (lpfCoefA[liEdge] * (lfY - lpfEdgeY0[liEdge]))
                             - (lpfCoefB[liEdge] * (lfX - lpfEdgeX0[liEdge]));
            // CORRECTED 2026-09-05 (aiwave2 lane R1, re-read by the conductor): inside is
            // ALL FOUR crosses NEGATIVE. asm 0x827686EC..0x82768708: `vcmpgefp v13,cross,0 ;
            // vnot` (lane = cross < 0), `vperm` with the 0x0004080C byte-gather (AND-reduce over
            // the four lanes), `vcmpequw` against ~0, then `vcmpeqfp. vs 0 ; mfocrf ; not ;
            // extrwi 1,24` returns 1 iff the reduced mask is non-zero. The earlier `>= 0.0f`
            // test made every section probe miss, which left the whole racing line inert.
            // Corroborated by AISection::IsInside @0x82677058 (inside == cross <= 0) and the
            // winding SectionData::SetFastSectionCorners builds.
            // NaN polarity (FX-AINAN2): the lane mask is `vnot(vcmpgefp(cross, 0))` @0x827686EC/
            // 0x827686F0 -- "not >= 0" -- and a NaN cross fails vcmpgefp, so its lane counts as
            // INSIDE. Only an ORDERED cross >= 0 puts the point outside; `!(cross < 0)` also sent
            // a NaN outside.
            if (lfSide >= 0.0f)
            {
                return false;
            }
        }
        return true;
    }
}

// FOLDED FROM BrnAIUtils_Angles.cpp
#include "GameSource/World/AI/BrnAIUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include "SDKs/XboxMath/XMVectorACos.h"         // XboxMath::XMVectorACos (X360 0x821F0980)

#include <cmath>   // std::fabs
                   // (RwMath::IsValid is a NaN SELF-COMPARE on the console, not isfinite -- see below)

// BrnAIUtils partfile -- the two planar-angle helpers the AIDriver steering chain calls
// (CalculateSteeringAngle / CorneringTopSpeed / DetermineDriftSteeringAngle / GetQuickTurnSteering).
//
//   BrnAI::FindUnsignedAngleBetween2DVectors @0x82766B20  (IDA export; DWARF BrnAIUtils.cpp:19)
//   BrnAI::FindSignedAngleBetween2DVectors   @0x827716A8  (NO IDA export -- the .json is absent
//        and names.tsv lacks the row; the body below is read straight from the image bytes
//        0x827716A8..0x827717FC, disassembled with capstone; it is the only caller-attested
//        symbol at that address: CalculateSteeringAngle bl 0x827716A8.)
//
// Both take their Vector2 args in v1/v2 and return in f1. XMVectorACos is the XDK math library's
// arccos polynomial (not a correctly rounded acos: XMVectorACos(1) = 4.8e-7), written out in the
// shared XboxMath::XMVectorACos (crash parity FX-GATE; std::acos stood in for it before).

namespace BrnAI
{
    f32 FindUnsignedAngleBetween2DVectors(Vector2 lA, Vector2 lB)
    {
        // vmulfp128 v0=v1*v2 ; vspltw/vaddfp lanes 0+1 -> dot over (x,y) only.
        const f32 lfDot = lA.mV.x * lB.mV.x + lA.mV.y * lB.mV.y;

        // ⛔⛔ ISSUE #31 -- THE BRANCH SENSE FOR THE **UNORDERED** CASE WAS INVERTED, AND THAT ONE
        // BIT IS THE WHOLE ASSERT STORM. This used to read `if (fabs(dot) >= 1.0f) return 0.0f;`,
        // which is the same thing as the console's test ONLY while `dot` is a number. The console:
        //   0x82766B4C  fabs   f13, f0                 ; f13 = |dot|
        //   0x82766B54  lfs    f0, flt_82001C98        ; f0  = 1.0
        //   0x82766B58  fcmpu  cr6, f13, f0
        //   0x82766B5C  blt    cr6, loc_82766B78       ; ONLY a strict, ORDERED less-than reaches
        //   0x82766B60  lfs    f1, flt_82001CC0        ;   XMVectorACos; everything else...
        //   0x82766B74  blr                            ;   ...returns 0.0f
        // `fcmpu` raises the UNORDERED bit (CR[FU]) for a NaN operand and leaves LT/GT/EQ CLEAR, so
        // `blt` is NOT taken and ARTIST **returns 0.0f for a NaN dot**. `!(|dot| >= 1.0f)` is TRUE
        // for that same NaN, so this file used to fall through into `std::acos(NaN) == NaN` --
        // the exact opposite answer, and the only place in the chain where the NaN survived.
        // Consequence of the old sense (the reported bug): FindSignedAngleBetween2DVectors got a
        // NaN angle instead of 0.0, so it missed its `fcmpu f31,0.0 ; beq` early-out @0x827716D8,
        // computed the 2D cross, fired BOTH of its NaN asserts, and handed NaN to
        // SteeringFan::GenerateFanVectors -- NaN mUnitDirection/mTarget/mCentreTarget, hence the
        // GetSectionInterpPosition (:2616) / GetIterativeHermite (:1849) / GetSimpleHermite (:1808,
        // :1826) / :1899 / :1906 cascade in the issue's log. With the console's sense the base angle
        // is 0.0, every fan ray stays finite and none of those asserts is reachable.
        // Written the way the asm branches so the unordered case cannot be re-inverted by rewriting
        // the condition: acos is reached ONLY on an ordered `|dot| < 1.0`.
        if (std::fabs(lfDot) < 1.0f)
            return XboxMath::XMVectorACos(lfDot);   // bl XMVectorACos @0x82766B84, lane 0 out

        return 0.0f;                   // @0x82766B60..B74 (flt_82001CC0 == 0.0) -- and the NaN arm
    }

    f32 FindSignedAngleBetween2DVectors(Vector2 lA, Vector2 lB)
    {
        f32 lfAngle = FindUnsignedAngleBetween2DVectors(lA, lB);   // bl @0x827716C8 -> f31

        // fcmpu f31, 0.0 ; beq -> return the (zero) angle unchanged (@0x827716D8/DC).
        if (lfAngle == 0.0f)
            return lfAngle;

        // 2D cross product a.x*b.y - a.y*b.x (@0x827716F8..1718: splat a.y / b.x / b.y / a.x,
        // vmulfp a.y*b.x, vmulfp a.x*b.y, vsubfp) ; assert it is not NaN (BrnAIUtils.cpp:65).
        const f32 lfCross = lA.mV.x * lB.mV.y - lA.mV.y * lB.mV.x;
        // ⛔ PREDICATE FIXED (issue #31): this read `std::isfinite(lfCross)`, which ALSO rejects
        // +-Inf. The console's test is a NaN SELF-COMPARE and nothing else --
        //   0x8277172C vspltw v0,v0,0 ; 0x82771730 vcmpeqfp. v0,v0,v0 ; 0x82771734 mfocrf ;
        //   0x82771750 bne -> skip ; else BeginAssert/FireAssert(li r5,0x41 == :65)/EndAssert
        // -- and `Inf == Inf` is TRUE, so an infinite cross product does NOT assert on ARTIST.
        // `isfinite` was a stricter predicate than the binary has, i.e. an assert we invented.
        CGS_ASSERT(lfCross == lfCross, "NAN error in AIDriver::FindSignedAngleBetween2DVectors");

        // The sign is two vector selects over two masks, (cross > 0) and (cross >= 0):
        //   sign = (cross >= 0) ? ((cross > 0) ? +1.0 : 0.0) : -1.0
        // POSITIVE when cross(lA,lB) = lA.x*lB.y - lA.y*lB.x > 0. Both masks are clear for a NaN
        // cross, so a NaN takes the -1.0 arm (not 0.0): written as the selects so the unordered
        // case keeps that arm.
        const f32 lfPositiveSign = (lfCross > 0.0f) ? 1.0f : 0.0f;
        const f32 lfSign        = (lfCross >= 0.0f) ? lfPositiveSign : -1.0f;
        lfAngle = lfSign * lfAngle;

        // assert the signed result is not NaN (BrnAIUtils.cpp:70) -- the same self-compare, at
        // 0x827717BC vspltw / 0x827717C0 vcmpeqfp. v0,v0,v0 / 0x827717D8 li r5,0x46 (== :70).
        CGS_ASSERT(lfAngle == lfAngle, "NAN error in AIDriver::FindSignedAngleBetween2DVectors");
        return lfAngle;
    }
}
