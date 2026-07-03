#include "GameSource/World/AI/BrnAIBoundaryLine.h"

#include <cmath>   // sqrtf

// BrnAI::BoundaryLine member bodies.
//
// BrnAI::BoundaryLine::GetInterp @ 0x82676C78.
//
// Interpolate a point along the boundary line by parameter lfT, writing the full 16-byte
// result Vector2 to *lpv2Out (X360 ABI: out in r3, this in r4, lfT in f1). The console body
// is a hand-vectorised VMX128 pipeline, decoded lane-by-lane in the wave-2 VMX pass:
//   - lvx128 loads the whole 16-byte line (start in lanes 0,1; end in lanes 2,3);
//   - lfT is spilled/reloaded and broadcast to all lanes (stfs + lvlx + vspltw);
//   - two vrlimi128 pairs re-pack the four line lanes into zero-padded endpoint registers:
//     start = [startX, startY, 0, 0] (masks 8/4, rotate 0) and end = [endX, endY, 0, 0]
//     (masks 8/4, rotate 2 words);
//   - vsubfp forms delta = end - start;
//   - vmaddfp v0, v13, v0, v12 fuses v0 = v13*v12 + v0 == delta*t + start (the multiplier
//     is the LAST listed operand, the addend the THIRD -- the documented IDA field order);
//   - stvx128 writes all 16 bytes to the out pointer, unconditionally (no null guard).
// Lanes 2,3 are structurally (0-0)*t+0 == +0.0f for all finite t, written as literal zeros
// (the same simplification the sibling GetLength body already applies to its structurally-
// zero lanes). No rodata constant is consumed (only vspltisw-zero immediates).
//
// BrnAI::BoundaryLine::GetLength @ 0x82676BE8.
//
// The planar length of the boundary line == the distance between its two 2D endpoints. The
// X360 loads the whole 16-byte line (lvx128: start at lanes 0,1; end at lanes 2,3), the
// vrlimi128 cascade re-packs start=(x,y) and end=(x,y), vsubfp forms the (end-start) delta,
// the two squared lanes are summed (vspltw lane0 + lane1, vaddfp), and the same vrsqrtefp +
// 2-step Newton-Raphson + vcmpeqfp/vsel zero-guard magnitude pipeline yields the result
// (lfs f1 returns it). The lane usage is a plain endpoint-pair magnitude that lowers
// cleanly: sqrtf((dx*dx)+(dy*dy)), the established subsystem convention (BrnAStar.h:167).
// sqrtf(0)==0 satisfies the asm's exact-zero vsel guard. No rodata constant is consumed.

namespace BrnAI
{
    void BoundaryLine::GetInterp(Vector2* lpv2Out, f32 lfT) const
    {
        // vsubfp lanes 0,1: delta = end - start.
        const f32 lfDeltaX = mfEndX - mfStartX;
        const f32 lfDeltaY = mfEndY - mfStartY;

        // vmaddfp: result = delta*t + start; stvx128 writes all four lanes (the console
        // stores through r3 with no null check -- kept unconditional to match).
        lpv2Out->x = (lfDeltaX * lfT) + mfStartX;
        lpv2Out->y = (lfDeltaY * lfT) + mfStartY;
        lpv2Out->z = 0.0f;   // lane 2: (0-0)*t + 0 on console
        lpv2Out->w = 0.0f;   // lane 3: (0-0)*t + 0 on console
    }

    f32 BoundaryLine::GetLength() const
    {
        const f32 lfDX = mfEndX - mfStartX;
        const f32 lfDY = mfEndY - mfStartY;
        return sqrtf((lfDX * lfDX) + (lfDY * lfDY));
    }
}
