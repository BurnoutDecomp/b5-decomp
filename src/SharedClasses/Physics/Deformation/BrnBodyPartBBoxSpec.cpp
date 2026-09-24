#include "SharedClasses/Physics/Deformation/BrnBodyPartBBoxSpec.h"

// ============================================================================
// BrnPhysics::Deformation::BodyPartBBoxSpec::HackCheckHandedness @ 0x825E6EA0
//
// RECONSTRUCTED 2026-09-24 (crash parity G16-D1; decoded by FX-DEFORM-LAT, re-verified against
// the ARTIST words and landed by FX-XLANE). The old "KEYSTONE -- not reconstructed" banner is
// retired: every VMX128 field decodes (tools/re/vmx128.py + the classic VA field order
// D = A*C + B, so vnmsubfp D,A,B,C == D = B - A*C):
//   0x825E6EC0/C4/D4  lvx128 v0/v12/v11 <- mOrientation rows 0/1/2 ; v13 = vspltisw 0
//   0x825E6EC8/D0     vpermwi128 0x63 (lanes 1,2,0,3) of row0 -> v10, of row1 -> v12 ; v9 = row1
//   0x825E6EDC        v0 = row0 * perm(row1)
//   0x825E6EE0        vnmsubfp v0,v10,v0,v9 : v0 = v0 - perm(row0)*row1 -> (c.z, c.x, c.y)
//   0x825E6EE4        vpermwi128 0x63 -> cross(row0, row1)
//   0x825E6EE8        vmsum3fp128 with row2 -> T = dot3(cross(row0,row1), row2)
//   0x825E6EEC/F0     vcmpgefp T>=0 ; vcmpeqfp. against 0 ; mfocrf CR6, all-true bit ; beq skip
//                     => the mirror runs iff !(T >= 0): a LEFT-handed (or NaN) basis.
//   0x825E6F04..F3C   HackSwapHandedness(this+0x40+0x20*i, this) for the 8 corners, then the
//                     centre (+0x140) and the joint (+0x160) -- all with the ORIGINAL basis
//   0x825E6F40..F50   vspltisw -1 ; vslw (-> 0x80000000 lanes) ; vxor ; stvx128 -> row0 = -row0
//                     (all four lanes, sign-bit flip), AFTER the swaps.
// Measured cost on shipped content: 0 left-handed records among ~10.8k real BodyPartBBoxSpecs in
// the 429 retail VEH_*_AT.BIN (the 429 all-zero placeholders give T = +0 -> no mirror, as on the
// console). So this is 1:1 completeness; any non-retail left-handed or NaN basis now mirrors.
// ⚠️ The same scan pins mOrientation's row 3 (the row this header used to call SIMD padding):
// it is (0, 0, 0, 1) in 10,829 of 10,829 real records -- a genuine affine translation row whose
// value happens to be zero. PhysicalBodyPart::CalculateBoundingBoxExtents loads and uses it, and
// HackSwapHandedness uses it as the mirror's origin T.
// ============================================================================

namespace BrnPhysics
{
namespace Deformation
{
    void BodyPartBBoxSpec::HackCheckHandedness()
    {
        const Vector3& lrRow0 = mOrientation.xAxis;
        const Vector3& lrRow1 = mOrientation.yAxis;
        const Vector3& lrRow2 = mOrientation.zAxis;
        const f32 lfCrossX = lrRow0.y * lrRow1.z - lrRow0.z * lrRow1.y;
        const f32 lfCrossY = lrRow0.z * lrRow1.x - lrRow0.x * lrRow1.z;
        const f32 lfCrossZ = lrRow0.x * lrRow1.y - lrRow0.y * lrRow1.x;
        const f32 lfTripleProduct = lfCrossX * lrRow2.x + lfCrossY * lrRow2.y + lfCrossZ * lrRow2.z;   // vmsum3fp128

        if ( !(lfTripleProduct >= 0.0f) )   // vcmpgefp / vcmpeqfp. / CR6 all-true : T < 0 or NaN
        {
            for ( s32 li = 0; li < KI_NUM_BBOX_CORNER_POINTS; ++li )
            {
                maCornerSkinData[li].HackSwapHandedness( mOrientation );
            }
            mCentreSkinData.HackSwapHandedness( mOrientation );
            mJointSkinData.HackSwapHandedness( mOrientation );

            mOrientation.xAxis.x = -mOrientation.xAxis.x;   // vxor 0x80000000, all four lanes
            mOrientation.xAxis.y = -mOrientation.xAxis.y;
            mOrientation.xAxis.z = -mOrientation.xAxis.z;
            mOrientation.xAxis.w = -mOrientation.xAxis.w;
        }
    }
}
}
