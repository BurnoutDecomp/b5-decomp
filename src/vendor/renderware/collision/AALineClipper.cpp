#include "vendor/renderware/collision/AALineClipper.hpp"
#include "vendor/renderware/collision/LineSegKernelMath.hpp"

namespace rw { namespace collision {

void AALineClipper::Init(rw::math::vpu::Vector3 start, rw::math::vpu::Vector3 end, rw::math::vpu::Vector3 padding, const AABBox& box)
{
    const __vector4 all=__vspltisw(-1), signMask=__vslw(all,all);
    const __vector4 half=__vcfsx(__vspltisw(1),1);
    const f32 epsilon=1.0e-6f;
    const __vector4 eps={epsilon,epsilon,epsilon,epsilon};
    const __vector4 hdelta=__vmulfp(__vsubfp(end.mV,start.mV),half);
    const __vector4 extent=__vmaxfp(__vmaxfp(__vandc(box.mMin.mV,signMask),__vandc(box.mMax.mV,signMask)),__vandc(start.mV,signMask));
    const __vector4 absolute=__vandc(hdelta,signMask);
    const __vector4 extra=__vsubfp(__vmaxfp(absolute,__vmulfp(extent,eps)),absolute);
    const __vector4 signs={linemath::Fsel(hdelta.x,1.0f,-1.0f),linemath::Fsel(hdelta.y,1.0f,-1.0f),linemath::Fsel(hdelta.z,1.0f,-1.0f),0.0f};
    const __vector4 offset=__vmulfp(signs,extra);
    m_origin.mV=__vsubfp(start.mV,offset);
    m_delta.mV=__vsubfp(__vaddfp(end.mV,offset),m_origin.mV);
    const __vector4 one=__vcfsx(__vspltisw(1),0);
    __vector4 estimate=__vrefp(m_delta.mV);
    __vector4 correction=__vnmsubfp(estimate,m_delta.mV,one);
    estimate=__vmaddfp(estimate,correction,estimate);
    correction=__vnmsubfp(estimate,m_delta.mV,one);
    m_recip.mV=__vmaddfp(estimate,correction,estimate);
    m_padding.mV=__vaddfp(padding.mV,extra);
}

AALineClipper::AALineClipper(rw::math::vpu::Vector3 start, rw::math::vpu::Vector3 end, const AABBox& box)
{
    const __vector4 all=__vspltisw(-1), mask=__vslw(all,all);
    const f32 epsilon=1.0e-6f;
    const __vector4 eps={epsilon,epsilon,epsilon,epsilon};
    const __vector4 seed=__vmulfp(__vmaxfp(__vandc(start.mV,mask),__vandc(end.mV,mask)),eps);
    Init(start,end,rw::math::vpu::Vector3(seed),box);
}

} }
