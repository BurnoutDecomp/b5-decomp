#include "types.hpp"
// The four clip rows come in as rw::math::vpu::Matrix44. VOCABULARY NOTE: the
// project carries TWO declarations of that type -- the EATech SDK header
// (SDKs/EATech/include/rw/math/vpu/matrix44.h, `class`, SIMD-register lanes
// behind GetComponent()/GetW() proxies) and the portable PC reconstruction
// vendor/renderware/include/rw/math/vpu/types.h (`struct`, named float lanes)
// that BrnCommonTypes.h aliases. MSVC bakes the class-key into the mangling, so
// this TU MUST use the same declaration as its only caller, the dispatch walker
// in CgsDispatcherCommands.cpp -- which builds its clip rows through
// BrnCommonTypes.h. Using the EATech header here forked
// `?FrustumTest@CgsGraphics@@YAIAEBVMatrix44@...` (V) from the caller's
// `...AEBUMatrix44@...` (U) and left the call unresolvable at link.
#include "BrnCommonTypes.h"   // rw::math::vpu::Matrix44 / Vector4 (the four clip rows)

#include <cmath>  // std::fabs

// ===========================================================================
// CgsGraphics::FrustumTest @ 0x827EE478 -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// A branchless VMX AABB-vs-frustum cull test. The argument points at four 16-byte lanes
// (lvx128 from off 0/16/32/48): the four rows of a clip-space transform. Rows 0..2 are
// the x/y/z axis rows, row 3 is the w/translation row. Each SIMD LANE is one clip plane;
// the row's w component (vspltw lane 3) is broadcast across all lanes.
//
// Per lane L (one clip plane), forming the box's projected half-extent along that axis:
//   pExtent = |row0[L]+w0| + |row1[L]+w1| + |row2[L]+w2|       (the +w half-space radius)
//   mExtent = |row0[L]-w0| + |row1[L]-w1| + |row2[L]-w2|       (the -w half-space radius)
//   insidePlus   = (row3[L] + w3) >= -pExtent    [vcmpgefp(row3+w3, -pExtent)]
//   outsideMinus = (row3[L] - w3) >  mExtent     [vcmpgtfp(row3-w3,  mExtent)]
//   laneOutside  = outsideMinus | !insidePlus    [vor(B, vnot(A))]
// vsel writes 1.0 into the outside lanes and 0.0 elsewhere; the trailing vcmpeqfp.(result,
// 0.0) sets CR6, and the return extracts the NEGATED CR6 'all-equal' (LT) bit:
//     mfocrf r11,2 ; not r11 ; extrwi r3,r11,1,24  ==  ~(all lanes == 0.0)
// i.e. the function returns 1 ('cull') when AT LEAST ONE lane is outside (any-plane reject),
// and 0 ('keep') otherwise -- the standard AABB-vs-frustum any-plane-rejects cull.
//
// FLAG (confidence=medium): the VMX is reconstructed SEMANTICALLY (this project's
// CgsTriangle4 / CgsPackedOobb VMX-parity precedent) -- portable named float maths that
// preserve the per-lane arithmetic, the two slab compares, and the ANY-lane-outside CR6
// reduction in the float domain. The Vector4 component reads go through the vendor
// GetComponent(i)/GetW() proxies (rw::math::vpu::Vector4 stores a SIMD register, not plain
// floats). The returned boolean is consumed by DrawRenderable::Interpret as a cull gate.
// ===========================================================================

namespace CgsGraphics
{
    unsigned int FrustumTest(const rw::math::vpu::Matrix44& lrClipRows)
    {
        const rw::math::vpu::Vector4* lapRow[4] =
        {
            &lrClipRows.xAxis, &lrClipRows.yAxis, &lrClipRows.zAxis, &lrClipRows.wAxis,
        };

        // Per-row w splat (lane 3 of each row -- the `vspltw v,vX,3`), broadcast across lanes.
        const f32 lafW[4] =
        {
            lapRow[0]->w, lapRow[1]->w, lapRow[2]->w, lapRow[3]->w,
        };

        // rowK component in lane L (0=x,1=y,2=z,3=w). The PC vector type stores the
        // four register lanes as named floats in lane order, so the SDK's
        // GetComponent(L).GetFloat() proxy is the indexed named read here.
        struct LaneComp
        {
            static f32 Get(const rw::math::vpu::Vector4* const* lapRow, int liRow, int liLane)
            {
                const rw::math::vpu::Vector4& lrRow = *lapRow[liRow];
                switch (liLane)
                {
                case 0:  return lrRow.x;
                case 1:  return lrRow.y;
                case 2:  return lrRow.z;
                default: return lrRow.w;
                }
            }
        };

        // The box is culled (return 1) if ANY lane/plane comes out 'outside' -- the vsel wrote
        // 1.0 into that lane and the vcmpeqfp. all-equal-to-zero reduction went false, whose
        // negation the return extracts. This is an OR reduction, NOT an AND.
        bool lbAnyLaneOutside = false;
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            // pExtent = |r0+w0| + |r1+w1| + |r2+w2| (this lane's +w projected radius).
            const f32 lfPExtent =
                std::fabs(LaneComp::Get(lapRow, 0, liLane) + lafW[0]) +
                std::fabs(LaneComp::Get(lapRow, 1, liLane) + lafW[1]) +
                std::fabs(LaneComp::Get(lapRow, 2, liLane) + lafW[2]);

            // mExtent = |r0-w0| + |r1-w1| + |r2-w2| (this lane's -w projected radius).
            const f32 lfMExtent =
                std::fabs(LaneComp::Get(lapRow, 0, liLane) - lafW[0]) +
                std::fabs(LaneComp::Get(lapRow, 1, liLane) - lafW[1]) +
                std::fabs(LaneComp::Get(lapRow, 2, liLane) - lafW[2]);

            const f32 lfPlusTerm  = LaneComp::Get(lapRow, 3, liLane) + lafW[3];   // v6  = row3 + w3
            const f32 lfMinusTerm = LaneComp::Get(lapRow, 3, liLane) - lafW[3];   // v10 = row3 - w3

            const bool lbInsidePlus   = (lfPlusTerm  >= -lfPExtent);  // vcmpgefp(v6, -pExtent)
            const bool lbOutsideMinus = (lfMinusTerm >   lfMExtent);  // vcmpgtfp(v10, mExtent)
            const bool lbLaneOutside  = lbOutsideMinus || !lbInsidePlus;  // v0 = B | ~A

            if (lbLaneOutside)
            {
                lbAnyLaneOutside = true;   // ANY lane outside -> box culled
            }
        }

        return lbAnyLaneOutside ? 1u : 0u;
    }

} // namespace CgsGraphics
