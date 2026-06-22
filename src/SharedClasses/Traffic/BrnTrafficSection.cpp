#include "SharedClasses/Traffic/BrnTrafficSection.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"               // operator-, IsZero, Normalize

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
}
