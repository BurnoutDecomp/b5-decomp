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

        // vmaddfp v13 = spanA * A.p0 + 0.5 ; vmaddfp v0 = spanB * B.p0 + 0.5
        const Vector3 lvA = rw::math::vpu::MultAdd(lvSpanA, lrRungA.maPoints[0], lvHalf);
        const Vector3 lvB = rw::math::vpu::MultAdd(lvSpanB, lrRungB.maPoints[0], lvHalf);

        // vsubfp v0 = B - A ; vmaddfp v0 = (B - A) * A + frac ; stvx128 -> lResult
        const Vector3 lvDelta = lvB - lvA;
        lrResult = rw::math::vpu::MultAdd(lvDelta, lvA, lvFrac);
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
