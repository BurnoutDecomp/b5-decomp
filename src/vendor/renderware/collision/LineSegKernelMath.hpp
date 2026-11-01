#pragma once

#include <cmath>     // std::sqrt
#include <cfloat>
#include <vectorintrinsics.h>

#include "types.hpp"
#include "vendor/renderware/collision/FeatureEdge.hpp"   // Vec4

// ===========================================================================
// The idioms the three volume line kernels are built from, written the way the console computes them:
//   rw::collision::SphereVolume::LineSegIntersect   @ 0x82BA82C8
//   rw::collision::BoxVolume::LineSegIntersect      @ 0x82BA9478
//   rw::collision::CapsuleVolume::LineSegIntersect  @ 0x82BAFCF8
//   rw::collision::rwcPlaneLineSegIntersect         @ 0x82BA8818
// (all four in LineSegIntersect.cpp, beside the rwc* kernels they call)
// (crash parity FX-FOLLOWUPS stage (b), 2026-09-25.)
// and, since 2026-09-27 (owner's list, lane L2), the two primitive tests those kernels call:
//   rw::collision::rwcSphereLineSegIntersect        @ 0x82BA81D8
//   rw::collision::rwcCylinderLineSegIntersect      @ 0x82BAF8A0
// and, since 2026-09-28 (lane L2, stage (c)), the clip the fat cylinder's torus arm builds (VmxMin / VmxMax / Fsel):
//   rw::collision::AALineClipper::Init              @ 0x828AED60   (AALineClipper.cpp)
//   rw::collision::AALineClipper::AALineClipper     @ 0x82BAE3C8
// and the cylinder's line kernel (LineSegIntersect.cpp):
//   rw::collision::CylinderVolume::LineSegIntersect      @ 0x82BAF688   (the dispatcher)
//   rw::collision::CylinderVolume::ThinLineSegIntersect  @ 0x82BADCE0
//   rw::collision::CylinderVolume::FatLineSegIntersect   @ 0x82BAEB10
//
// The rounding is chosen per instruction (scratch/CRASHPARITY_0922/ROUNDING_RULE.md):
//   rule 3  vmaddfp / vnmsubfp / vmaddfp128 / fmadds / fmsubs / fnmsubs round ONCE. Native VMX/FPU is used with the
//           operands in the console's order: the classic `vmaddfp D,A,B,C` is D = A*C + B,
//           `vnmsubfp D,A,B,C` is D = B - A*C, and `vmaddfp128 D,A,B` is D = A*B + D.
//   rule 4  vmulfp / vaddfp / vsubfp / fmuls / fadds / fsubs / fdivs each round separately, as written.
//   rule 1  vmsum3fp128 is ONE rounding of the f64 sum. FLAG (model): xenia DOT_PRODUCT_3.
//   rule 5  the vrefp / vrsqrtefp estimate is taken at its correctly rounded value. FLAG (model). The console's
//           two Newton-Raphson steps then run as written.
//   rule 6  VMX denormal flushing is not modelled.
// All four lanes are computed wherever the console computes four.
// ===========================================================================

namespace rw
{
namespace collision
{
namespace linemath
{
    // The kernels' .rdata words (tools/re/x360rd.py) and loop counts.
    const f32 KF_LINE_ZERO        = 0.0f;                // flt_82001CC0 == 0x00000000
    const f32 KF_LINE_ONE         = 1.0f;                // flt_82001C98 == 0x3F800000
    const f32 KF_LINE_FLT_MIN     = FLT_MIN;           // 0x00800000: unk_821800C0 (the sphere kernel's |n| floor)
                                                         //   and flt_8218017C (the box's edge-cap denominator floor)
    const f32 KF_LINE_NEG_FLT_MAX = -FLT_MAX;   // flt_82035570 == 0xFF7FFFFF (the box's separation seed)
    const u32 KU_BOX_LINE_STEPS     = 6u;                // li r19, 6 @0x82BA94E0 ; addic. -1 @0x82BA9C70
    const u32 KU_CAPSULE_LINE_STEPS = 3u;                // li r24, 3 @0x82BAFD78 ; addic. -1 @0x82BB0088

    // Native scalar FPU multiply-add; the XDK contracts this expression to fmadds.
    inline f32 ScalarMadd(f32 afA, f32 afB, f32 afC)
    {
        return afA * afB + afC;
    }

    // Scalar FPU sites use fnmsubs rather than the VMX negative multiply-subtract.
    inline f32 ScalarNmsub(f32 afA, f32 afC, f32 afB)
    {
        return -(afA * afC - afB);
    }

    // Native fused VMX operation: one rounding, as the ARTIST vmaddfp.
    inline f32 Madd(f32 afA, f32 afB, f32 afC)
    {
        const __vector4 lvA = { afA, afA, afA, afA };
        const __vector4 lvB = { afB, afB, afB, afB };
        const __vector4 lvC = { afC, afC, afC, afC };
        return __vmaddfp(lvA, lvB, lvC).x;
    }

    // ARTIST cylinder cap tests 82BAE184..190 / 82BAEF80..94: splat y,
    // round y*y with vmulfp, then fuse x*x + that rounded product with vmaddfp.
    // Keep the operations in VMX so their denormal handling is not replaced by FPU.
    inline f32 CapRadialSquared(const rw::math::vpu::Vector3& p)
    { const __vector4 y=__vspltw(p.mV,1), x=__vspltw(p.mV,0); return __vmaddfp(x,x,__vmulfp(y,y)).x; }
    inline f32 Dot3(const rw::math::vpu::Vector3& a,const rw::math::vpu::Vector3& b)
    { return __vmsum3fp(a.mV,b.mV).x; }
    inline f32 Nmsub(f32 a,f32 c,f32 b)
    { const __vector4 av={a,a,a,a},cv={c,c,c,c},bv={b,b,b,b};return __vnmsubfp(av,cv,bv).x; }
    inline f32 RefinedRecip(f32 value)
    {
        const __vector4 x={value,value,value,value}, one=__vcfsx(__vspltisw(1),0);
        __vector4 e=__vrefp(x), c=__vnmsubfp(e,x,one);
        e=__vmaddfp(e,c,e);c=__vnmsubfp(e,x,one);return __vmaddfp(e,c,e).x;
    }
    inline f32 RefinedRsqrt(f32 value)
    {
        const __vector4 x={value,value,value,value},one=__vcfsx(__vspltisw(1),0),half=__vcfsx(__vspltisw(1),1);
        __vector4 e=__vrsqrtefp(x), c=__vnmsubfp(x,__vmulfp(e,e),one);
        e=__vmaddfp(__vmulfp(e,half),c,e);c=__vnmsubfp(x,__vmulfp(e,e),one);
        return __vmaddfp(__vmulfp(e,half),c,e).x;
    }
    inline rw::math::vpu::Vector3 MakeVec4(f32 x,f32 y,f32 z,f32 w)
    { const __vector4 v={x,y,z,w};return rw::math::vpu::Vector3(v); }
    inline rw::math::vpu::Vector3 Cross(const rw::math::vpu::Vector3& a,const rw::math::vpu::Vector3& b)
    { return rw::math::vpu::Vector3(__vpermwi(__vnmsubfp(__vpermwi(a.mV,0x63),b.mV,__vmulfp(a.mV,__vpermwi(b.mV,0x63))),0x63)); }
    inline f32& Lane(rw::math::vpu::Vector3& v,u32 lane) { return (&v.mV.x)[lane]; }
    inline f32 Lane(const rw::math::vpu::Vector3& v,u32 lane) { return (&v.mV.x)[lane]; }
    inline rw::math::vpu::Vector3 Sub(const rw::math::vpu::Vector3& a,const rw::math::vpu::Vector3& b)
    { return rw::math::vpu::Vector3(__vsubfp(a.mV,b.mV)); }
    inline rw::math::vpu::Vector3 MulSplat(const rw::math::vpu::Vector3& a,f32 s)
    { const __vector4 v={s,s,s,s};return rw::math::vpu::Vector3(__vmulfp(a.mV,v)); }
    inline rw::math::vpu::Vector3 MaddSplat(const rw::math::vpu::Vector3& a,f32 s,const rw::math::vpu::Vector3& b)
    { const __vector4 v={s,s,s,s};return rw::math::vpu::Vector3(__vmaddfp(a.mV,v,b.mV)); }
    inline void ComposeFrame(const rw::math::vpu::Vector3* frame,const rw::math::vpu::Vector3* transform,rw::math::vpu::Vector3 (&out)[4])
    {
        for(u32 i=0;i<3;++i) out[i]=MaddSplat(transform[2],frame[i].mV.z,MaddSplat(transform[1],frame[i].mV.y,MulSplat(transform[0],frame[i].mV.x)));
        out[3]=MaddSplat(transform[2],frame[3].mV.z,MaddSplat(transform[1],frame[3].mV.y,MaddSplat(transform[0],frame[3].mV.x,transform[3])));
    }
    struct LocalFrame { rw::math::vpu::Vector3 mColumn0,mColumn1,mColumn2,mTranslation; };
    inline LocalFrame InvertFrame(const rw::math::vpu::Vector3 (&frame)[4])
    {
        LocalFrame f;f.mColumn0=MakeVec4(frame[0].mV.x,frame[1].mV.x,frame[2].mV.x,0);
        f.mColumn1=MakeVec4(frame[0].mV.y,frame[1].mV.y,frame[2].mV.y,0);
        f.mColumn2=MakeVec4(frame[0].mV.z,frame[1].mV.z,frame[2].mV.z,0);
        const rw::math::vpu::Vector3 n=Sub(MakeVec4(0,0,0,0),frame[3]);
        f.mTranslation=MaddSplat(f.mColumn0,n.mV.x,MaddSplat(f.mColumn1,n.mV.y,MulSplat(f.mColumn2,n.mV.z)));return f;
    }
    inline rw::math::vpu::Vector3 ToLocal(const LocalFrame& f,const rw::math::vpu::Vector3& p)
    { return MaddSplat(f.mColumn2,p.mV.z,MaddSplat(f.mColumn1,p.mV.y,MaddSplat(f.mColumn0,p.mV.x,f.mTranslation))); }
    inline rw::math::vpu::Vector3 FramePoint(const rw::math::vpu::Vector3* f,const rw::math::vpu::Vector3& p)
    { return MaddSplat(f[2],p.mV.z,MaddSplat(f[1],p.mV.y,MaddSplat(f[0],p.mV.x,f[3]))); }
    inline rw::math::vpu::Vector3 FrameDirection(const rw::math::vpu::Vector3* f,const rw::math::vpu::Vector3& p)
    { return MaddSplat(f[2],p.mV.z,MaddSplat(f[1],p.mV.y,MulSplat(f[0],p.mV.x))); }

    // vminfp / vmaxfp: a NaN operand gives a NaN (the first NaN operand); -0 is below +0.
    // (AALineClipper::Init @0x828AED60 / the constructor @0x82BAE3C8; the fat cylinder's torus clip 0x82BAF30C /
    // 0x82BAF310.)
    inline f32 VmxMin(f32 afA, f32 afB)
    {
        const __vector4 lvA = { afA, afA, afA, afA };
        const __vector4 lvB = { afB, afB, afB, afB };
        return __vminfp(lvA, lvB).x;
    }
    inline f32 VmxMax(f32 afA, f32 afB)
    {
        const __vector4 lvA = { afA, afA, afA, afA };
        const __vector4 lvB = { afB, afB, afB, afB };
        return __vmaxfp(lvA, lvB).x;
    }
    // fsel fD, fA, fC, fB (IDA prints that order): fA >= 0 (a -0 included) ? fC : fB -- a NaN fA takes fB.
    inline f32 Fsel(f32 afA, f32 afC, f32 afB)
    {
        return (afA >= 0.0f) ? afC : afB;
    }

    // The walk's axis successor, `slw r11, 1, axis ; clrlwi r10, r11, 30`: 0 -> 1 -> 2 -> 0.
    inline u32 NextAxis(u32 luAxis)
    {
        return (1u << luAxis) & 3u;
    }
}
}
}
