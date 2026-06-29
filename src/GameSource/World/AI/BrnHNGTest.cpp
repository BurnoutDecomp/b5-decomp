#include "GameSource/World/AI/BrnHNGTest.h"

#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISection (mpaNoGoLines @+4, muNumNoGoLines @+18)
#include "GameSource/World/AI/BrnAIBoundaryLine.h"      // BrnAI::BoundaryLine (16-byte packed 2D segment)
#include "GameSource/World/AI/BrnAIUtils.h"             // BrnAI::Calc2DIntersectionEquationData (the inlined segment-intersection solver)
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT

// BrnAI::LineTestSectionHNG @ 0x8277A650.
//
// Tests whether a 2D query segment (lStart -> lEnd) crosses any of an AI-section's no-go
// (HNG) boundary lines. The X360 body is hand-vectorised VMX128, but every op is a 2-lane
// scalar computation done in vector registers: it forms the query-segment delta and, for each
// no-go line, the inlined form of BrnAI::Calc2DIntersectionEquationData (the same solver the
// committed BrnAI::AISection::PassesThrough inlines) -- a 2D line-line intersection that yields
// the parameter along the no-go edge and the parameter along the query segment. Reversing that
// inlining back into the real call:
//
//   * v8 = lEnd - lStart is the query-segment delta (vsubfp128 v8, v126, v125), and v124 is its
//     (-dSy, dSx) perpendicular (the vperm128 with the &unk_82CDA350 control = the cross-product
//     lane shuffle the solver performs internally).
//   * Per line, lvx128 loads the whole 16-byte BoundaryLine; vpermwi128 ...,0xBF re-packs end into
//     (end,end,end,end) so vsubfp yields the edge delta dE = end - start, and vsubfp128 v9 yields
//     rP = lStart - start. These are exactly Calc2DIntersectionEquationData(lStart, lEnd,
//     line.start, line.end, &paramA, &paramB): paramA is the parameter along the no-go edge,
//     paramB the parameter along the query segment.
//   * The denominator (edge x segment) cull is the leading `vcmpeqfp denom == 0` -> skip-line
//     branch, which is exactly the solver's parallel-segments `return false` early-out, so the
//     line only counts when Calc2DIntersectionEquationData returns true.
//   * The remaining predicate chain (vcmpgtfp 0 > paramA / paramA > 1 ; vcmpgefp paramB >= 0 /
//     1 >= paramB) requires BOTH parameters in the inclusive [0,1] range -- a true
//     segment-vs-segment crossing (not the ray-style u >= 0 of PassesThrough). The reciprocal
//     1/denom is the vrefp + Newton-Raphson hardware estimate, which lowers to an exact divide
//     inside the solver.
//
// The per-iteration "luHNGLineIndex < muNumNoGoLines" assert is the inlined GetHNGLine bounds
// guard (it can never trip given the loop bound, matching the per-edge corner asserts in
// AISection::IsInside / PassesThrough). The leading "lpSection != NULL" assert and the empty
// no-go list (muNumNoGoLines == 0 -> no crossing) are both faithful to the asm.
//
// Called by BrnAI::ResetOnTrackManager::TestSectionHNG and
// BrnAI::ResetOnTrackDebugComponent::RenderWorld.

namespace BrnAI
{
    bool LineTestSectionHNG(const AISection* lpSection, Vector2 lStart, Vector2 lEnd)
    {
        CGS_ASSERT(lpSection != NULL, "lpSection != NULL");

        const u16 luNumNoGoLines = lpSection->muNumNoGoLines;

        for (u16 luHNGLineIndex = 0; luHNGLineIndex < luNumNoGoLines; ++luHNGLineIndex)
        {
            CGS_ASSERT(luHNGLineIndex < lpSection->muNumNoGoLines,
                       "luHNGLineIndex < muNumNoGoLines");

            const BoundaryLine& lrLine = lpSection->mpaNoGoLines[luHNGLineIndex];

            const Vector2 lLineStart = { lrLine.mfStartX, lrLine.mfStartY, 0.0f, 0.0f };
            const Vector2 lLineEnd   = { lrLine.mfEndX,   lrLine.mfEndY,   0.0f, 0.0f };

            // Parameter along the no-go edge (lfParamAlongEdge) and along the query segment
            // (lfParamAlongSegment). A parallel pair (zero denominator) returns false and is
            // skipped, exactly like the asm's `denom == 0` cull.
            f32 lfParamAlongEdge    = 0.0f;
            f32 lfParamAlongSegment = 0.0f;
            if (!Calc2DIntersectionEquationData(lStart, lEnd, lLineStart, lLineEnd,
                                                &lfParamAlongEdge, &lfParamAlongSegment))
            {
                continue;
            }

            // Both parameters must lie within the inclusive [0,1] range for the query segment to
            // actually cross this no-go line (not merely the infinite lines to meet).
            if (lfParamAlongEdge    >= 0.0f && lfParamAlongEdge    <= 1.0f &&
                lfParamAlongSegment >= 0.0f && lfParamAlongSegment <= 1.0f)
            {
                return true;
            }
        }

        return false;
    }
}
