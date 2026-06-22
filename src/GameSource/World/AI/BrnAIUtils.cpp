#include "GameSource/World/AI/BrnAIUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (StepTo negative-step guard)

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
}
