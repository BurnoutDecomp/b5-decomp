#include "GameSource/World/AI/BrnAIBoundaryLine.h"

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
}
