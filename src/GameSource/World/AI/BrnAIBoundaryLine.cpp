#include "GameSource/World/AI/BrnAIBoundaryLine.h"

#include <cmath>   // sqrtf

// BrnAI::BoundaryLine member bodies.
//
// BrnAI::BoundaryLine::GetInterp @ 0x82676C78.
//
// KEYSTONE -- NOT reconstructed. The X360 body is a hand-vectorised AltiVec/VMX128 pipeline
// built on vrlimi128 permute-immediates:
//   - lvx128 v13, r0, r4              loads the whole 16-byte line (4 packed lanes) at once;
//   - the interp parameter lfT (f1) is stored, reloaded with lvlx and broadcast (vspltw v12);
//   - a cascade of vrlimi128 (rotate-left-immediate, mask-insert: amounts 8/4, masks 2/0)
//     re-packs the four line lanes into two working endpoint vectors (v10, v11/v0);
//   - vsubfp v13, v10, v0             forms the inter-endpoint delta;
//   - vmaddfp v0, v13, v0, v12        fuses a multiply-add across the re-packed lanes;
//   - stvx128 v0, r0, r3             writes the resulting 16-byte Vector2 to the out pointer.
//
// The vrlimi128 lane re-packing does not reliably lower to scalar C++: which lanes carry the
// start/end/parameter after the rotate-mask cascade is exactly the kind of per-lane detail the
// project rule forbids guessing, and a wrong scalar lerp (the naive (end-start)*t+start) does
// NOT match the fused vmaddfp operand assignment the asm actually emits. Inventing a per-lane
// formula here would be fabrication, so the body is left as an honest stub pending a VMX-aware
// lowering pass (or the RW vpu *_operation intrinsics) that decodes the vrlimi128 permutation.
//
// The stub does not fabricate arithmetic: it zero-initialises the out Vector2 (a defined,
// non-corrupting result) rather than emitting a guessed interpolation. The operands are
// referenced so the signature is honest about what it consumes/produces.
//
// BrnAI::BoundaryLine::GetLength @ 0x82676BE8.
//
// The planar length of the boundary line == the distance between its two 2D endpoints. The
// X360 loads the whole 16-byte line (lvx128: start at lanes 0,1; end at lanes 2,3), the
// vrlimi128 cascade re-packs start=(x,y) and end=(x,y), vsubfp forms the (end-start) delta,
// the two squared lanes are summed (vspltw lane0 + lane1, vaddfp), and the same vrsqrtefp +
// 2-step Newton-Raphson + vcmpeqfp/vsel zero-guard magnitude pipeline yields the result
// (lfs f1 returns it). UNLIKE GetInterp, the lane usage here is a plain endpoint-pair
// magnitude that lowers cleanly, so it is bodied (not stubbed): sqrtf((dx*dx)+(dy*dy)),
// the established subsystem convention (BrnAStar.h:167). sqrtf(0)==0 satisfies the asm's
// exact-zero vsel guard. No rodata constant is consumed.

namespace BrnAI
{
    void BoundaryLine::GetInterp(Vector2* lpv2Out, f32 lfT) const
    {
        // KEYSTONE STUB: VMX vrlimi128 lerp pipeline not reconstructed (see file header).
        (void)lfT;
        (void)mfStartX; (void)mfStartY; (void)mfEndX; (void)mfEndY;
        if (lpv2Out != nullptr)
        {
            lpv2Out->SetZero();
        }
    }

    f32 BoundaryLine::GetLength() const
    {
        const f32 lfDX = mfEndX - mfStartX;
        const f32 lfDY = mfEndY - mfStartY;
        return sqrtf((lfDX * lfDX) + (lfDY * lfDY));
    }
}
