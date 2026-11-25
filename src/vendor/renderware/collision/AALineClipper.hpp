#pragma once

#include <cstddef>
#include <ppcintrinsics.h>
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
        using math::vpu::Abs;
        using math::vpu::Max;
        using math::vpu::MultAdd;
        using math::vpu::VecFloat;
        using math::vpu::Vector3;
        const Vector3 maximum = Max(Abs(start), Abs(end));
        const VecFloat epsilon(1.0e-6f);
        Init(start, end, MultAdd(maximum, Vector3(epsilon.mV), padding), box);
    }
    // DecFIGS aalineclipper.h:177 and ARTIST 0x82BAE3C8.
    AALineClipper(rw::math::vpu::Vector3 start, rw::math::vpu::Vector3 end, const AABBox& box)
    {
        using math::vpu::Abs;
        using math::vpu::Max;
        using math::vpu::VecFloat;
        using math::vpu::Vector3;
        const Vector3 maximum = Max(Abs(start), Abs(end));
        const VecFloat epsilon(1.0e-6f);
        Init(start, end, Vector3(__vmulfp(maximum.mV, epsilon.mV)), box);
    }
    void Init(rw::math::vpu::Vector3 start, rw::math::vpu::Vector3 end, rw::math::vpu::Vector3 padding, const AABBox& box);
    // DecFIGS aalineclipper.h:200. ARTIST 0x828AEF30-0x828AEFD4 inlines the slab clip.
    __forceinline s32 ClipToAABBox(f32& lrfStart, f32& lrfEnd, const AABBox& lrBox)
    {
        using namespace math::vpu;
        const Vector3 lLower = Mult(m_recip, Subtract(Subtract(lrBox.mMin, m_padding), m_origin));
        const Vector3 lUpper = Mult(m_recip, Subtract(Add(lrBox.mMax, m_padding), m_origin));
        const Vector3 lEnter = Min(lLower, lUpper);
        const Vector3 lExit = Max(lLower, lUpper);
        const f32 lfEnterX = lEnter.mV.x;
        const f32 lfEnterY = lEnter.mV.y;
        const f32 lfEnterZ = lEnter.mV.z;
        const f32 lfExitX = lExit.mV.x;
        const f32 lfExitY = lExit.mV.y;
        const f32 lfExitZ = lExit.mV.z;
        const f32 lfEnterXY = static_cast<f32>(fpmax(lfEnterX, lfEnterY));
        const f32 lfExitXY = static_cast<f32>(fpmin(lfExitX, lfExitY));
        const f32 lfEnter = static_cast<f32>(fpmax(lfEnterXY, lfEnterZ));
        const f32 lfExit = static_cast<f32>(fpmin(lfExitXY, lfExitZ));
        lrfStart = static_cast<f32>(fpmax(lrfStart, lfEnter));
        lrfEnd = static_cast<f32>(fpmin(lrfEnd, lfExit));
        return lrfStart < lrfEnd;
    }

};

CGS_STATIC_ASSERT((sizeof(AALineClipper)==0x40), "AALineClipper size");
CGS_STATIC_ASSERT((offsetof(AALineClipper,m_origin)==0x00), "AALineClipper origin");
CGS_STATIC_ASSERT((offsetof(AALineClipper,m_delta)==0x10), "AALineClipper delta");
CGS_STATIC_ASSERT((offsetof(AALineClipper,m_recip)==0x20), "AALineClipper reciprocal");
CGS_STATIC_ASSERT((offsetof(AALineClipper,m_padding)==0x30), "AALineClipper padding");

} }
