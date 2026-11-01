#pragma once

#include <cstddef>
#include "vendor/renderware/collision/AABBox.hpp"

namespace rw { namespace collision {

// DecFIGS aalineclipper.h:54-59; ARTIST 828AED60 stores these four rows.
struct AALineClipper
{
    rw::math::vpu::Vector3 m_origin;
    rw::math::vpu::Vector3 m_delta;
    rw::math::vpu::Vector3 m_recip;
    rw::math::vpu::Vector3 m_padding;

    AALineClipper(rw::math::vpu::Vector3 start, rw::math::vpu::Vector3 end, rw::math::vpu::Vector3 padding, const AABBox& box)
    {
        const __vector4 all=__vspltisw(-1), mask=__vslw(all,all);
        const f32 epsilon=1.0e-6f; const __vector4 eps={epsilon,epsilon,epsilon,epsilon};
        const __vector4 seed=__vmaddfp(__vmaxfp(__vandc(start.mV,mask),__vandc(end.mV,mask)),eps,padding.mV);
        Init(start,end,rw::math::vpu::Vector3(seed),box);
    }
    AALineClipper(rw::math::vpu::Vector3 start, rw::math::vpu::Vector3 end, const AABBox& box);
    void Init(rw::math::vpu::Vector3 start, rw::math::vpu::Vector3 end, rw::math::vpu::Vector3 padding, const AABBox& box);
};

CGS_STATIC_ASSERT((sizeof(AALineClipper)==0x40), "AALineClipper size");
CGS_STATIC_ASSERT((offsetof(AALineClipper,m_origin)==0x00), "AALineClipper origin");
CGS_STATIC_ASSERT((offsetof(AALineClipper,m_delta)==0x10), "AALineClipper delta");
CGS_STATIC_ASSERT((offsetof(AALineClipper,m_recip)==0x20), "AALineClipper reciprocal");
CGS_STATIC_ASSERT((offsetof(AALineClipper,m_padding)==0x30), "AALineClipper padding");

} }
