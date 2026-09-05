#include "GameSource/World/AI/BrnAIUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (StepTo negative-step guard)

#include <cmath>   // std::sqrt (the de-optimised VMX rsqrt paths)

namespace BrnAI
{
    // 0x82766BA8 - move lfCurrent toward lfTarget, capping the move at lfStep.
    // asm: f31=current, f29=target, f30=step; f28 = fabs(target-current). Assert step >= 0.
    // If step > |target-current| the move overshoots, so return the target. Otherwise step toward
    // the target: +step when target >= current, -step otherwise.
    f32 StepTo(f32 lfCurrent, f32 lfTarget, f32 lfStep)
    {
        const f32 lfDelta = lfTarget - lfCurrent;
        const f32 lfAbsDelta = (lfDelta < 0.0f) ? -lfDelta : lfDelta;

        CGS_ASSERT(lfStep >= 0.0f, "Negative Step");

        if (lfStep > lfAbsDelta)
            return lfTarget;

        if (lfTarget >= lfCurrent)
            return lfCurrent + lfStep;

        return lfCurrent - lfStep;
    }

    // 0x82771800 - 2D line-line intersection parameters for segments (lP1->lP2) and (lQ1->lQ2).
    // asm names the deltas:
    //   dQy = lQ2.y - lQ1.y   dPx = lP2.x - lP1.x
    //   dQx = lQ2.x - lQ1.x   dPy = lP2.y - lP1.y
    //   denom = dPx*dQy - dPy*dQx
    // When denom == 0 the segments are parallel: write 0 to both outputs and return false.
    // Otherwise (with rPx = lP1.x - lQ1.x, rPy = lP1.y - lQ1.y, inv = 1/denom):
    //   *lpfParamA = (rPy*dQx - rPx*dQy) * inv
    //   *lpfParamB = (rPy*dPx - rPx*dPy) * inv
    bool Calc2DIntersectionEquationData(Vector2 lP1,
                                        Vector2 lP2,
                                        Vector2 lQ1,
                                        Vector2 lQ2,
                                        f32*    lpfParamA,
                                        f32*    lpfParamB)
    {
        const f32 lfDQy = lQ2.y - lQ1.y;
        const f32 lfDPx = lP2.x - lP1.x;
        const f32 lfDQx = lQ2.x - lQ1.x;
        const f32 lfDPy = lP2.y - lP1.y;

        const f32 lfDenom = (lfDPx * lfDQy) - (lfDPy * lfDQx);

        if (lfDenom == 0.0f)
        {
            *lpfParamA = 0.0f;
            *lpfParamB = 0.0f;
            return false;
        }

        const f32 lfRPx = lP1.x - lQ1.x;
        const f32 lfRPy = lP1.y - lQ1.y;
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
        const f32 lfVelLenSq = (lVelocity.x * lVelocity.x) + (lVelocity.y * lVelocity.y);
        const f32 lfVelLen   = (lfVelLenSq == 0.0f) ? 0.0f : (lfVelLenSq * (1.0f / std::sqrt(lfVelLenSq)));

        f32 lfResult;
        if (lfVelLen == 0.0f)
        {
            // Fallback: |lPosition.xy| the same way (zero-length -> 0).
            const f32 lfPosLenSq = (lPosition.x * lPosition.x) + (lPosition.y * lPosition.y);
            lfResult = (lfPosLenSq == 0.0f) ? 0.0f : (lfPosLenSq * (1.0f / std::sqrt(lfPosLenSq)));
        }
        else
        {
            // cross(lPosition, lVelocity) scaled by 1/|lVelocity.xy|.
            const f32 lfCross = (lPosition.x * lVelocity.y) - (lPosition.y * lVelocity.x);
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
        const f32 lfLineX    = lLineEnd.x - lLineStart.x;
        const f32 lfLineY    = lLineEnd.y - lLineStart.y;
        const f32 lfToPointX = lPoint.x   - lLineStart.x;
        const f32 lfToPointY = lPoint.y   - lLineStart.y;

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

    // 0x82768680 -- SEMANTIC reconstruction of the branchless SIMD point-in-section test
    // (the SoA precomputed-edge twin of AISection::IsInside). The X360 body runs all four
    // edge half-plane tests in parallel across the VMX lanes, then AND-reduces (vperm +
    // vcmpequw vs all-ones) to a single inside/outside bit.
    //
    // lpSectionEdges points at four consecutive 4-lane vectors (lane i == edge i):
    //   edgeX0 @+0x00, edgeY0 @+0x10, coefA @+0x20, coefB @+0x30.
    // Per edge the inside test is:  coefA[i]*(lfY - edgeY0[i]) - coefB[i]*(lfX - edgeX0[i]) >= 0.
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
            if (!(lfSide < 0.0f))
            {
                return false;
            }
        }
        return true;
    }
}
