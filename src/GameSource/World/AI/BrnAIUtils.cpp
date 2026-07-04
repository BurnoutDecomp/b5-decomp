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
    // v2 = lPosition, v1 = lVelocity. Returns the signed perpendicular distance from the origin
    // to the line through lPosition running in direction lVelocity, i.e.
    // cross(lVelocity, lPosition) / |lPosition.xy|. When |lPosition.xy| is zero the result falls
    // back to |lVelocity.xy| (the asm's `if (|pos|==0) recompute magnitude from v1` branch). The
    // result is asserted finite ('Bad maths!', X360 baked BrnAIUtils.h:141 -- path/line dropped).
    f32 DistancePosVelToOrigin(Vector2 lPosition, Vector2 lVelocity)
    {
        // |lPosition.xy| via len2 * rsqrt(len2); the vsel yields 0 for a zero-length vector.
        const f32 lfPosLenSq = (lPosition.x * lPosition.x) + (lPosition.y * lPosition.y);
        const f32 lfPosLen   = (lfPosLenSq == 0.0f) ? 0.0f : (lfPosLenSq * (1.0f / std::sqrt(lfPosLenSq)));

        f32 lfResult;
        if (lfPosLen == 0.0f)
        {
            // Fallback: |lVelocity.xy| the same way (zero-length -> 0).
            const f32 lfVelLenSq = (lVelocity.x * lVelocity.x) + (lVelocity.y * lVelocity.y);
            lfResult = (lfVelLenSq == 0.0f) ? 0.0f : (lfVelLenSq * (1.0f / std::sqrt(lfVelLenSq)));
        }
        else
        {
            // cross(lVelocity, lPosition) scaled by 1/|lPosition.xy|.
            const f32 lfCross = (lVelocity.x * lPosition.y) - (lVelocity.y * lPosition.x);
            lfResult = lfCross * (1.0f / lfPosLen);
        }

        // vcmpeqfp self-compare: a finite result equals itself; a NaN does not.
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
            if (!(lfSide >= 0.0f))
            {
                return false;
            }
        }
        return true;
    }
}
