#include "GameSource/World/AI/BrnHNGTest.h"

#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISection (mpaNoGoLines @+4, muNumNoGoLines @+18)
#include "GameSource/World/AI/BrnAIBoundaryLine.h"      // BrnAI::BoundaryLine (16-byte packed 2D segment)
#include "GameSource/World/AI/BrnAIDriver.h"            // BrnAI::NearbyVehicles / NearbyVehicle (LineTestTrafficHNG)
#include "GameSource/World/AI/BrnAIUtils.h"             // Calc2DIntersectionEquationData, DistancePointToLine (4-arg)
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

    // =============================================================================================
    // BrnAI::LineTestTrafficHNG @ 0x8277A878 (DWARF BrnHNGTest.cpp:104; PS3 twin 0x9B9718).
    // Crash parity G07-D1 (FX-AI-RUMBLE, 2026-09-23) -- it had no body anywhere, so both traffic
    // legs of ResetOnTrackManager::TestCarHNG were parked and a reset pose was never swept off a
    // vehicle standing on it.
    //
    // r3 = lpNearbyTraffic, v1 = lAttemptStartPos (A), v2 = lAttemptEndPos (B). DWARF locals:
    // lfR :106, lfS :107, lfAX :109, lfAY :110, lfBXMinuslfAX :111, lfBYMinuslfAY :112,
    // liTrafficIndex :115; per vehicle lpTraffic :118, lu16HNGIndex :119, lPointOnLine :121,
    // lfDistance :122, KF_TOO_CLOSE_TOO_TRAFFIC :125; per line lpHNGLine :139,
    // lBoundryLineStartPos :141 (C), lBoundryLineEndPos :142 (D), lfCX :147, lfCY :148,
    // lfDYMinusCY :150, lfDXMinusCX :151, lfDenominator :153.
    //
    //   0x8277A8BC/C0      f31 = A.x, f30 = A.y; 0x8277A8C8/DC B - A splatted -> f29 / f28
    //   0x8277A910..74     GetCount() inlined with its two asserts (BrnAIDriver.cpp:2912/:2913),
    //                      re-read every pass; 0x8277A980 i >= count -> return 0 (0x8277AD80)
    //   0x8277A98C..90     DistancePointToLine(mVehicle[i].mCentre (+0x10), A, B, lPointOnLine)
    //                      -- the 4-arg overload @0x827653C0, which returns a SIGNED distance
    //   0x8277A998..AC     lfs flt_82F30698 ; fcmpu ; bge next -- only dist < 5.0 goes on. The
    //                      distance is not fabs'd, so a vehicle on the negative (right-hand) side
    //                      of A->B always passes this gate (console behaviour, kept).
    //   0x8277A9B4..AA04   Dot(lPointOnLine - A, B - A) > 0, else next vehicle
    //   0x8277AA08..AA50   Dot(lPointOnLine - B, B - A) < 0, else next vehicle
    //   0x8277AA54..AD5C   for (u16 j = 0; j < 4; ++j): line = mVehicle[i] + 0x30 + 16*j,
    //                      C = (line.x, line.y), D = (line.z, line.w)
    //     0x8277AB30..44   lfDenominator = (Dy-Cy)*(Bx-Ax) - (Dx-Cx)*(By-Ay); == 0 -> next line
    //     0x8277ABA8..C4   lfR = ((Ay-Cy)*(Dx-Cx) - (Ax-Cx)*(Dy-Cy)) / lfDenominator   (fdivs)
    //     0x8277ABE4       vcmpgtfp. 0 > lfR  -> next line
    //     0x8277AC24       vcmpgtfp. lfR > 1  -> next line      (a NaN lfR passes both, as here)
    //     0x8277ACC8..E4   lfS = ((Ay-Cy)*(Bx-Ax) - (Ax-Cx)*(By-Ay)) / lfDenominator   (fdivs)
    //     0x8277AD04       vcmpgefp. lfS >= 0 -> else next line
    //     0x8277AD3C       vcmpgefp. 1 >= lfS -> return 1 (0x8277AD6C)
    //   0x8277AD60..68     next vehicle (r25 += 1, r30 += 0x70)
    // lfR is the parameter along A->B, lfS the parameter along C->D (the comp.graphics "r and s"
    // segment test, spelled with the console's own local names).
    // =============================================================================================
    bool LineTestTrafficHNG(const NearbyVehicles* lpNearbyTraffic,
                            Vector2 lAttemptStartPos, Vector2 lAttemptEndPos)
    {
        // flt_82F30698 == 0x40A00000 (x360rd): a function-local static in .data; findinit finds no
        // CRT writer, only this function's reader at 0x8277A998.
        static const f32 KF_TOO_CLOSE_TOO_TRAFFIC = 5.0f;

        const f32 lfAX          = lAttemptStartPos.x;
        const f32 lfAY          = lAttemptStartPos.y;
        const f32 lfBXMinuslfAX = lAttemptEndPos.x - lAttemptStartPos.x;
        const f32 lfBYMinuslfAY = lAttemptEndPos.y - lAttemptStartPos.y;

        for (s32 liTrafficIndex = 0; liTrafficIndex < lpNearbyTraffic->GetCount(); ++liTrafficIndex)
        {
            const NearbyVehicle* lpTraffic = &lpNearbyTraffic->mVehicle[liTrafficIndex];

            Vector2 lPointOnLine;
            const f32 lfDistance = DistancePointToLine(lpTraffic->mCentre, lAttemptStartPos,
                                                       lAttemptEndPos, lPointOnLine);
            if (!(lfDistance < KF_TOO_CLOSE_TOO_TRAFFIC))
            {
                continue;
            }

            // The foot of the perpendicular must lie strictly between A and B.
            const f32 lfAlongFromStart = (lPointOnLine.x - lAttemptStartPos.x) * lfBXMinuslfAX
                                       + (lPointOnLine.y - lAttemptStartPos.y) * lfBYMinuslfAY;
            if (!(lfAlongFromStart > 0.0f))
            {
                continue;
            }
            const f32 lfAlongFromEnd = (lPointOnLine.x - lAttemptEndPos.x) * lfBXMinuslfAX
                                     + (lPointOnLine.y - lAttemptEndPos.y) * lfBYMinuslfAY;
            if (!(lfAlongFromEnd < 0.0f))
            {
                continue;
            }

            for (u16 lu16HNGIndex = 0; lu16HNGIndex < NearbyVehicle::KI_TRAFFIC_NUM_HNG_LINES; ++lu16HNGIndex)
            {
                const BoundaryLine* lpHNGLine = &lpTraffic->maHNGLines[lu16HNGIndex];

                const f32 lfCX        = lpHNGLine->mfStartX;
                const f32 lfCY        = lpHNGLine->mfStartY;
                const f32 lfDYMinusCY = lpHNGLine->mfEndY - lfCY;
                const f32 lfDXMinusCX = lpHNGLine->mfEndX - lfCX;

                const f32 lfDenominator = (lfDYMinusCY * lfBXMinuslfAX) - (lfDXMinusCX * lfBYMinuslfAY);
                if (lfDenominator == 0.0f)
                {
                    continue;   // parallel lines
                }

                const f32 lfR = (((lfAY - lfCY) * lfDXMinusCX) - ((lfAX - lfCX) * lfDYMinusCY)) / lfDenominator;
                if (lfR < 0.0f || lfR > 1.0f)
                {
                    continue;
                }

                const f32 lfS = (((lfAY - lfCY) * lfBXMinuslfAX) - ((lfAX - lfCX) * lfBYMinuslfAY)) / lfDenominator;
                if (lfS >= 0.0f && lfS <= 1.0f)
                {
                    return true;
                }
            }
        }

        return false;
    }
}
