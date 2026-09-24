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
		const Vector3& lrRow0   = arFrame.xAxis;
		const Vector3& lrRow1   = arFrame.yAxis;
		const Vector3& lrRow2   = arFrame.zAxis;
		const Vector3& lrOrigin = arFrame.wAxis;
		const Vector3  lPoint   = mVertex;

		// vsubfp v12 = 0 - T (0x825E6DE8): spelled 0 - t, not -t, so a zero lane keeps the
		// console's sign.
		const f32 lfNegOriginX = 0.0f - lrOrigin.x;
		const f32 lfNegOriginY = 0.0f - lrOrigin.y;
		const f32 lfNegOriginZ = 0.0f - lrOrigin.z;

		// L_i = dot3(R_i, P - T), accumulated in the console's order.
		f32 lfLocalX = ((((lfNegOriginZ * lrRow0.z) + lfNegOriginY * lrRow0.y) + lfNegOriginX * lrRow0.x)
		                + lrRow0.x * lPoint.x) + lrRow0.y * lPoint.y + lrRow0.z * lPoint.z;
		const f32 lfLocalY = ((((lfNegOriginZ * lrRow1.z) + lfNegOriginY * lrRow1.y) + lfNegOriginX * lrRow1.x)
		                      + lrRow1.x * lPoint.x) + lrRow1.y * lPoint.y + lrRow1.z * lPoint.z;
		const f32 lfLocalZ = ((((lfNegOriginZ * lrRow2.z) + lfNegOriginY * lrRow2.y) + lfNegOriginX * lrRow2.x)
		                      + lrRow2.x * lPoint.x) + lrRow2.y * lPoint.y + lrRow2.z * lPoint.z;

		lfLocalX *= -1.0f;   // flt_820037C8 (x360rd 0xBF800000), the x lane only (vrlimi128 mask 8)

		mVertex.x = ((lrOrigin.x + lrRow0.x * lfLocalX) + lrRow1.x * lfLocalY) + lrRow2.x * lfLocalZ;
		mVertex.y = ((lrOrigin.y + lrRow0.y * lfLocalX) + lrRow1.y * lfLocalY) + lrRow2.y * lfLocalZ;
		mVertex.z = ((lrOrigin.z + lrRow0.z * lfLocalX) + lrRow1.z * lfLocalY) + lrRow2.z * lfLocalZ;
		mVertex.w = ((lrOrigin.w + lrRow0.w * lfLocalX) + lrRow1.w * lfLocalY) + lrRow2.w * lfLocalZ;
	}
}
}
