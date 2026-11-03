#include "SharedClasses/Physics/Deformation/BrnBBoxPointSkinData.h"

// BrnPhysics::Deformation::BBoxPointSkinData::HackSwapHandedness @ 0x825E6DB8.
//
// RECONSTRUCTED 2026-09-24 (crash parity G16-D1; decoded by FX-DEFORM-LAT, re-verified against
// the ARTIST words and landed by FX-XLANE). The old "KEYSTONE -- not reconstructed" banner is
// retired; every field decodes (tools/re/vmx128.py for the VMX128 forms; the classic vmaddfp
// prints raw field order D,A,B,C == D = A*C + B):
//   0x825E6DC4..0x825E6DF0  R0/R1/R2/T = arFrame rows +0x00/+0x10/+0x20/+0x30 ; P = mVertex ;
//                           vspltw P.x/P.y/P.z ; vsubfp v12 = 0 - T (0x825E6DE8)
//   0x825E6DF4..0x825E6E34  vmrghw/vmrglw transpose -> columns (R0.a, R1.a, R2.a, 0), a = x,y,z
//   0x825E6E38..0x825E6E54  L = ((((-T.z*colZ + -T.y*colY) + -T.x*colX) + colX*P.x) + colY*P.y)
//                           + colZ*P.z, i.e. L_i = dot3(R_i, P - T) (lane 3 = 0)
//   0x825E6E60/64           vmulfp128 by splat(flt_820037C8 = x360rd 0xBF800000 = -1.0) ;
//                           vrlimi128 v12,v0,8,0 -> only L.x is negated (vrlimi mask bits
//                           8/4/2/1 = x/y/z/w, pinned by two PS3 vperm twins in G17-D1)
//   0x825E6E8C..0x825E6E98  mVertex = ((T + R0*L.x) + R1*L.y) + R2*L.z, all four lanes
// (the stores at 0x825E6E58 / 0x825E6E6C are dead: 0x825E6E98 overwrites them). The skin
// weights and bone indices are not touched. Reached only through
// BodyPartBBoxSpec::HackCheckHandedness's left-handed arm -- see its banner for the measured
// (zero) cost on shipped content.

namespace BrnPhysics
{
namespace Deformation
{
	void BBoxPointSkinData::HackSwapHandedness( const Matrix44Affine& arFrame )
	{
        const __vector4 r0 = arFrame.xAxis.mV, r1 = arFrame.yAxis.mV;
        const __vector4 r2 = arFrame.zAxis.mV, origin = arFrame.wAxis.mV;
        const __vector4 point = mVertex.mV;
        const __vector4 zero = __vspltisw(0);
        const __vector4 negOrigin = __vsubfp(zero, origin);
        const __vector4 hi = __vmrghw(r0, r2), lo = __vmrglw(r0, r2);
        const __vector4 r1hi = __vmrghw(r1, zero), r1lo = __vmrglw(r1, zero);
        const __vector4 cx = __vmrghw(hi, r1hi), cy = __vmrglw(hi, r1hi);
        const __vector4 cz = __vmrghw(lo, r1lo);
        __vector4 local = __vmulfp(__vspltw(negOrigin, 2), cz);
        local = __vmaddfp(__vspltw(negOrigin, 1), cy, local);
        local = __vmaddfp(__vspltw(negOrigin, 0), cx, local);
        local = __vmaddfp(cx, __vspltw(point, 0), local);
        local = __vmaddfp(cy, __vspltw(point, 1), local);
        local = __vmaddfp(cz, __vspltw(point, 2), local);
        const __vector4 minusOne = __vcfsx(__vspltisw(-1), 0);
        local = __vrlimi(local, __vmulfp(local, minusOne), 8, 0);
        __vector4 result = __vmaddfp(r0, __vspltw(local, 0), origin);
        result = __vmaddfp(r1, __vspltw(local, 1), result);
        mVertex.mV = __vmaddfp(r2, __vspltw(local, 2), result);
	}
}
}
