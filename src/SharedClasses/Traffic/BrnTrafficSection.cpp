#include "SharedClasses/Traffic/BrnTrafficSection.h"

#include <cmath>                                          // std::floor (CalcPositionAtParameter)

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"               // operator-, IsZero, Normalize, MultAdd

namespace BrnTraffic
{
    // -------------------------------------------------------------------------
    // BrnTraffic::LaneRung::GetRightVector  @ 0x821F4A88  -> Vector3 (by value)
    //   called by BrnTraffic::Section::CalcTransformAtParameter and
    //             BrnTraffic::DebugComponent::DrawNeighbours
    //
    // The lateral ("right") direction of the rung = the normalized vector from the
    // first endpoint to the second.
    //
    // ASM walk-through (hand-vectorised VMX128; r3 = hidden sret slot, r4 = this):
    //   lvx128 v13,r0,r4         -> load maPoints[0]                      (this+0x00)
    //   lvx128 v12,r4,r10(0x10)  -> load maPoints[1]                      (this+0x10)
    //   vsubfp128 v127,v12,v13   -> lDiff = maPoints[1] - maPoints[0]
    //   lvlx/vspltw flt_82001770 -> broadcast a small epsilon tolerance
    //   vandc v0,v127,signmask   -> |lDiff| per lane (clear sign bit)
    //   vcmpgtfp. v0,|lDiff|,eps -> CR: any lane > eps ?  (the !IsZero predicate)
    //   mfocrf/extrwi/beq        -> if NOT(any lane > eps), i.e. IsZero(lDiff):
    //                                 fire assert "!RwMath::IsZero( lDiff )"  (line 328)
    //   vmsum3fp128 v0,v127,v127 -> dot(lDiff,lDiff) = |lDiff|^2
    //   vrsqrtefp + 2 Newton NR  -> 1/|lDiff|        (rsqrt estimate refined twice)
    //   vmulfp128 v0,v127,v0     -> lDiff * (1/|lDiff|) = Normalize(lDiff)
    //   stvx128 v0,r0,r31(=sret) -> store the result into the return slot
    //
    // FLAG (VMX->portable): the rsqrt-estimate + 2 Newton-Raphson refinement steps are
    //   reconstructed as the committed rw::math::vpu::Normalize (exact 1/std::sqrt);
    //   numerically tighter than the console estimate, not a placeholder. The store
    //   order (subtract -> assert-not-zero -> normalize -> return) is preserved.
    // FLAG (IsZero epsilon): the broadcast tolerance flt_82001770 is an un-valued
    //   .rdata float not present in the exports; rw::math::vpu::IsZero's committed
    //   default (1.0e-6f) supplies the small-epsilon |lane| <= eps test the asm's
    //   vandc+vcmpgtfp pair computes. The assert STRUCTURE is faithful; the literal
    //   epsilon is the committed shared default.
    Vector3 LaneRung::GetRightVector() const
    {
        const Vector3 lDiff = maPoints[1] - maPoints[0];

        // !RwMath::IsZero( lDiff )  -- baked at BrnTrafficSection.h:328 (asm li r5,0x148).
        CGS_ASSERT(!rw::math::vpu::IsZero(lDiff), "!RwMath::IsZero( lDiff )");

        return rw::math::vpu::Normalize(lDiff);
    }

    // -------------------------------------------------------------------------
    // BrnTraffic::Section accessor family (@0x821F4B78 / 0x821F4BD8 / 0x821F5068 /
    // 0x82705BC0). Section is homed in BrnTrafficSection.h.
    // -------------------------------------------------------------------------

    // BrnTraffic::Section::GetNumSegments  @ 0x821F4B78  -> uint32_t
    // A section with muNumRungs rungs has muNumRungs-1 inter-rung segments.
    u32 Section::GetNumSegments() const
    {
        CGS_ASSERT(muNumRungs > 0, "muNumRungs > 0");
        return static_cast<u32>(muNumRungs) - 1;
    }

    // BrnTraffic::Section::CalcPositionAtParameter  @ 0x821F4BD8  -> void
    // Interpolate the lane position at the fractional parameter lfParam that lies inside
    // local segment luSegment, from the whole-graph rung table lpaGlobalRungs. The rung pair
    // straddling the segment is lpaGlobalRungs[muRungOffset + luSegment] (A) and the next (B);
    // the section-local fraction is the parameter minus its floor.
    //
    // FLAG (store-for-store VMX): the X360 hand-vectorised inner block is reproduced op for op
    // with the SDK Vector3 helpers (Mult/MultAdd = vmaddfp), NOT re-derived into a named lerp --
    // the 0.5 broadcast and the exact multiply/add/subtract graph are preserved verbatim.
    void Section::CalcPositionAtParameter(const LaneRung* lpaGlobalRungs, VecFloat lfParam,
                                          u32 luSegment, Vector3& lrResult) const
    {
        CGS_ASSERT(lpaGlobalRungs != NULL, "lpaGlobalRungs != NULL");
        CGS_ASSERT(muNumRungs > 0, "muNumRungs > 0");
        CGS_ASSERT(luSegment < GetNumSegments(), "luSegment < GetNumSegments()");

        // fctidz -> truncate the parameter's first lane toward zero; the segment index must
        // equal that whole part.
        const f32 lfParamScalar = lfParam.x;
        CGS_ASSERT(luSegment == static_cast<u32>(static_cast<s32>(lfParamScalar)),
                   "Mismatched segment & param values: seg=");

        // Section-local fraction = param - floor(param)  (vrfim128 = round toward -inf).
        const f32     lfFrac = lfParamScalar - std::floor(lfParamScalar);
        const Vector3 lvFrac{ lfFrac, lfFrac, lfFrac, lfFrac };
        const Vector3 lvHalf{ 0.5f, 0.5f, 0.5f, 0.5f };

        // The rung pair straddling this segment (32-byte LaneRung stride).
        const LaneRung& lrRungA = lpaGlobalRungs[muRungOffset + luSegment];
        const LaneRung& lrRungB = lpaGlobalRungs[muRungOffset + luSegment + 1];

        const Vector3 lvSpanA = lrRungA.maPoints[1] - lrRungA.maPoints[0];  // vsubfp v10
        const Vector3 lvSpanB = lrRungB.maPoints[1] - lrRungB.maPoints[0];  // vsubfp v9

        // ⚠️ OPERAND ROLES CORRECTED 2026-07-29 (bug class (c): a fused-multiply-add's operands
        // read in the wrong roles). This body used to compute
        //     lvA = MultAdd(spanA, A.p0, half)  ==  spanA * A.p0 + 0.5
        //     lrResult = MultAdd(delta, lvA, frac) == (B-A) * A + frac
        // -- i.e. it MULTIPLIED two positions and ADDED a constant, which is not a geometric
        // operation at all, and it never used `frac` as an interpolation weight. Measured
        // consequence on real shipped lane data: the first lane seat the fly-by took came back
        // with position.y == 0.000000 exactly.
        //
        // The correct roles are pinned by the SIBLING CalcDirectionAtParameter @0x821F4DB8,
        // which loads the SAME four points and computes the SAME two intermediates with the
        // VMX128 `vmaddcfp128 vD, vA, vB` form (vD = vD*vA + vB) -- unambiguously
        // `span * 0.5 + p0`, i.e. the rung MIDPOINT. So:
        //     midpoint = span * 0.5 + p0                (vmaddfp: A=span, C=half,  B=p0)
        //     result   = (midB - midA) * frac + midA    (vmaddfp: A=delta, C=frac, B=midA)
        // Both are the standard midpoint + lerp the lane sampler must produce, and MultAdd's
        // committed contract is MultAdd(a,b,c) == a*b + c.
        const Vector3 lvA = rw::math::vpu::MultAdd(lvSpanA, lvHalf, lrRungA.maPoints[0]);
        const Vector3 lvB = rw::math::vpu::MultAdd(lvSpanB, lvHalf, lrRungB.maPoints[0]);

        // vsubfp v0 = B - A ; vmaddfp -> A + frac*(B - A) ; stvx128 -> lResult
        const Vector3 lvDelta = lvB - lvA;
        lrResult = rw::math::vpu::MultAdd(lvDelta, lvFrac, lvA);
    }

    // -------------------------------------------------------------------------
    // BrnTraffic::Section::CalcDirectionAtParameter  @ 0x821F4DB8  -> void
    //
    // The lane's FORWARD direction across local segment luSegment: the normalised vector from
    // the segment's first rung MIDPOINT to its second rung MIDPOINT. It does NOT interpolate by
    // the fractional parameter -- the asm computes no frac at all; lfParam only feeds the
    // "segment matches the truncated parameter" assert (the same one CalcPositionAtParameter
    // fires at line 0x271).
    //
    // ASM walk-through (r22 = this, r21 = lpaGlobalRungs, r29 = luSegment, r20 = &lrDirection):
    //   asserts: lpaGlobalRungs != NULL (:0x26F) / muNumRungs > 0 (:0x170) /
    //            luSegment < GetNumSegments() (:0x270) / "Mismatched segment & param" (:0x271)
    //   r11 = ((*(this+0) /*muRungOffset*/ + luSegment) << 5) + lpaGlobalRungs  -> &rungA
    //   r10 = r11 + 0x20                                                       -> &rungB
    //   v0  = rungA.maPoints[0]   v10 = rungA.maPoints[1]
    //   v13 = rungB.maPoints[0]   v9  = rungB.maPoints[1]
    //   v127 = vcsxwfp128(1,1) == 0.5
    //   v125 = v10 - v0                       ; spanA
    //   v124 = v9  - v13                      ; spanB
    //   vmaddcfp128 v125, v127, v125, v0      ; v125 = v125*v127 + v0  == A midpoint
    //   vmaddcfp128 v124, v127, v124, v13     ; v124 = v124*v127 + v13 == B midpoint
    //   |v125 - v124| vs flt_82001740 -> assert "Zero length rung " (:0x278)
    //   v13 = v124 - v125                     ; B midpoint - A midpoint
    //   vrsqrtefp + 2 Newton-Raphson, vmulfp  ; Normalize(v13)
    //   stvx128 -> lrDirection
    //
    // FLAG (VMX->portable): the rsqrt estimate + two Newton-Raphson refinement steps are the
    //   committed rw::math::vpu::Normalize (exact 1/sqrt) -- numerically tighter than the
    //   console estimate, the same substitution LaneRung::GetRightVector above documents.
    // FLAG (IsZero epsilon): flt_82001740 is an un-valued .rdata float; rw::math::vpu::IsZero's
    //   committed default supplies the small-epsilon test the vandc+vcmpgtfp pair computes.
    //
    // ⭐ THIS BODY IS WHAT PINNED A REAL BUG IN ITS SIBLING. CalcPositionAtParameter above used
    //   to read the same fused-multiply-add's operands as `span * p0 + 0.5` (position times
    //   position, plus a constant) and never used the fractional weight; the vmaddcfp128 form
    //   here is unambiguously `span * 0.5 + p0` == the rung midpoint. The sibling is corrected
    //   -- see its own note.
    // -------------------------------------------------------------------------
    void Section::CalcDirectionAtParameter(const LaneRung* lpaGlobalRungs, VecFloat lfParam,
                                           u32 luSegment, Vector3& lrDirection) const
    {
        CGS_ASSERT(lpaGlobalRungs != NULL, "lpaGlobalRungs != NULL");
        CGS_ASSERT(muNumRungs > 0, "muNumRungs > 0");
        CGS_ASSERT(luSegment < GetNumSegments(), "luSegment < GetNumSegments()");
        CGS_ASSERT(luSegment == static_cast<u32>(static_cast<s32>(lfParam.x)),
                   "Mismatched segment & param values: seg=");

        const Vector3 lvHalf{ 0.5f, 0.5f, 0.5f, 0.5f };

        const LaneRung& lrRungA = lpaGlobalRungs[muRungOffset + luSegment];
        const LaneRung& lrRungB = lpaGlobalRungs[muRungOffset + luSegment + 1];

        const Vector3 lvSpanA = lrRungA.maPoints[1] - lrRungA.maPoints[0];   // vsubfp128 v125
        const Vector3 lvSpanB = lrRungB.maPoints[1] - lrRungB.maPoints[0];   // vsubfp128 v124

        // vmaddcfp128: vD = vD*vA + vB  -> the two rung midpoints.
        const Vector3 lvMidA = rw::math::vpu::MultAdd(lvSpanA, lvHalf, lrRungA.maPoints[0]);
        const Vector3 lvMidB = rw::math::vpu::MultAdd(lvSpanB, lvHalf, lrRungB.maPoints[0]);

        const Vector3 lvDelta = lvMidB - lvMidA;                             // vsubfp128 v13

        CGS_ASSERT(!rw::math::vpu::IsZero(lvMidA - lvMidB), "Zero length rung ");

        lrDirection = rw::math::vpu::Normalize(lvDelta);
    }

    // -------------------------------------------------------------------------
    // BrnTraffic::Section::CalcTransformAtParameter  -> void
    //
    // Recovered from the console's TWO-function split:
    //   sub_82219030   -- the LaneRung* overload (this signature). Asserts lpaGlobalRungs,
    //                     resolves the global rung id with GetGlobalRungForSegment, COPIES the
    //                     two 32-byte rungs onto its own stack (`ld`/`std` x4 each) and calls:
    //   sub_82207998   -- the by-value overload that does the arithmetic.
    // Only the outer symbol has callers, so the pair is joined here (a de-inlining split, not
    // two semantics); the inner's own asserts are reproduced in place.
    //
    // ⭐ THE ARITHMETIC IS NOT A SPLINE. The inner body's assert literals say "DoSplineInterp
    // gave us zero vector for " / "DoSplineInterp gave us invalid vector: " -- the shipped
    // interpolation is straight linear, exactly the sampler CalcPositionAtParameter and
    // CalcDirectionAtParameter above already implement. Walked op for op (r21 = &rungA,
    // r20 = &rungB, v121 = the section-local fraction, v127 = 0.5, r18/r16/r17 = the outs):
    //
    //   0x82207BE4  v123 = rungA.maPoints[1] - rungA.maPoints[0]        spanA
    //   0x82207BE8  v122 = rungB.maPoints[1] - rungB.maPoints[0]        spanB
    //   0x82207BF4  vmaddcfp128 v123, 0.5, v123, A.p0                   midA
    //   0x82207BF8  vmaddcfp128 v122, 0.5, v122, B.p0                   midB
    //   0x82207BFC  v0  = LaneRung::GetRightVector(rungA)
    //   0x82207C08  v13 = LaneRung::GetRightVector(rungB)
    //   0x82207C3C  vmaddcfp128 v126, frac, (v13 - v0), v0              lerped right
    //   0x82207C90  v122 = midB - midA
    //   0x82207CA0  vmaddfp128 v123, v122, frac, v123                   POSITION  -> *out1 (r18)
    //   0x82207CDC  v0 = v126 * rsqrt(|v126|^2)  (2 NR)                 RIGHT     -> *out3 (r17)
    //   0x82207DA4  v0 = v122 * rsqrt(|v122|^2)  (2 NR)                 DIRECTION -> *out2 (r16)
    //   0x82207F38  the direction is re-normalised once more after its validity asserts
    //
    // FLAG (VMX->portable): every rsqrt-estimate + two-Newton-Raphson refinement is the
    // committed rw::math::vpu::Normalize (exact 1/sqrt) -- the same substitution the two
    // samplers above already document. The second (idempotent) re-normalisation of the
    // direction at 0x82207F38 collapses into the first.
    // -------------------------------------------------------------------------
    void Section::CalcTransformAtParameter(const LaneRung* lpaGlobalRungs, VecFloat lfParam,
                                           u32 luSegment, Vector3& lrPosition,
                                           Vector3& lrDirection, Vector3& lrUp) const
    {
        CGS_ASSERT(lpaGlobalRungs != NULL, "lpaGlobalRungs != NULL");

        const s32       liGlobalRung = GetGlobalRungForSegment(lfParam, luSegment);
        const LaneRung& lrRungA      = lpaGlobalRungs[liGlobalRung];
        const LaneRung& lrRungB      = lpaGlobalRungs[liGlobalRung + 1];

        const f32 lfParamScalar = lfParam.x;
        CGS_ASSERT(luSegment == static_cast<u32>(static_cast<s32>(lfParamScalar)),
                   "Mismatched segment & param values: seg=");

        const f32     lfFrac = lfParamScalar - std::floor(lfParamScalar);
        const Vector3 lvFrac{ lfFrac, lfFrac, lfFrac, lfFrac };
        const Vector3 lvHalf{ 0.5f, 0.5f, 0.5f, 0.5f };

        const Vector3 lvSpanA = lrRungA.maPoints[1] - lrRungA.maPoints[0];
        const Vector3 lvSpanB = lrRungB.maPoints[1] - lrRungB.maPoints[0];

        const Vector3 lvMidA = rw::math::vpu::MultAdd(lvSpanA, lvHalf, lrRungA.maPoints[0]);
        const Vector3 lvMidB = rw::math::vpu::MultAdd(lvSpanB, lvHalf, lrRungB.maPoints[0]);

        const Vector3 lvRightA = lrRungA.GetRightVector();
        const Vector3 lvRightB = lrRungB.GetRightVector();
        const Vector3 lvRight  = rw::math::vpu::MultAdd(lvRightB - lvRightA, lvFrac, lvRightA);

        CGS_ASSERT(!rw::math::vpu::IsZero(lvRight), "DoSplineInterp gave us zero vector for ");

        const Vector3 lvDelta = lvMidB - lvMidA;

        lrPosition = rw::math::vpu::MultAdd(lvDelta, lvFrac, lvMidA);
        lrUp       = rw::math::vpu::Normalize(lvRight);

        CGS_ASSERT(!rw::math::vpu::IsZero(lvDelta), "DoSplineInterp gave us zero vector for ");

        lrDirection = rw::math::vpu::Normalize(lvDelta);

        CGS_ASSERT(rw::math::vpu::IsValid(lrDirection),
                   "DoSplineInterp gave us invalid vector: ");
    }

    // BrnTraffic::Section::GetGlobalRungForSegment  @ 0x821F5068  -> int32_t
    // Map a section-local segment index to its whole-graph rung id: muRungOffset + luSegment.
    // The truncated parameter must agree with the segment, the segment must be in range, and
    // the resulting rung id must not run past this section's last rung.
    s32 Section::GetGlobalRungForSegment(VecFloat lfParam, u32 luSegment) const
    {
        const s32 liParam = static_cast<s32>(lfParam.x);   // fctidz: truncate toward zero
        CGS_ASSERT(luSegment == static_cast<u32>(liParam),
                   "Mismatched segment & param values: seg=");

        CGS_ASSERT(muNumRungs > 0, "muNumRungs > 0");
        CGS_ASSERT(luSegment < GetNumSegments(),
                   "Out-of-range segment index: luSegment=");

        const s32 liRungId = static_cast<s32>(muRungOffset + luSegment);

        CGS_ASSERT(muNumRungs > 0, "muNumRungs > 0");
        CGS_ASSERT(liRungId <= (static_cast<s32>(GetNumSegments() + muRungOffset) - 1),
                   "liRungId <= ( (int32_t)( GetNumSegments() + muRungOffset ) - 1 )");

        return liRungId;
    }

    // BrnTraffic::Section::CalcSignedDistanceAlongSection  @ 0x82705BC0  -> float32_t
    // Signed arc-length from (A) to (B) along this section: the distance of B from the section
    // start minus the distance of A from the section start, so B ahead of A is positive. Both
    // legs share the cumulative rung-length table lpafRungLengths.
    f32 Section::CalcSignedDistanceAlongSection(f32 lfParamA, u32 luSegmentA,
                                                f32 lfParamB, u32 luSegmentB,
                                                const f32* lpafRungLengths) const
    {
        const f32 lfDistB = CalcDistanceAlongSection(lfParamB, luSegmentB, lpafRungLengths);
        const f32 lfDistA = CalcDistanceAlongSection(lfParamA, luSegmentA, lpafRungLengths);
        return lfDistB - lfDistA;
    }
}
