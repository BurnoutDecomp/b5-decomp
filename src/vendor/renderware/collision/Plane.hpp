#pragma once

#include "types.hpp"
#include "SDKs/EATech/include/rw/math/vpu/vector3plus.h"
#include "SDKs/EATech/include/rw/math/vpu/vector4.h"

namespace rw
{
namespace collision
{
    // DecFIGS plane.h:33..49 and ARTIST 0x82839550. A plane occupies
    // one aligned Vector3Plus containing its normal and signed offset.
    struct Plane
    {
        Plane() {}

        Plane(math::vpu::Vector3 lNormal, f32 lfDistance)
            : m_data(lNormal.X().GetFloat(), lNormal.Y().GetFloat(), lNormal.Z().GetFloat(), lfDistance)
        {
        }

        explicit Plane(const math::vpu::Vector4& lrPacked)
            : m_data(lrPacked.mV)
        {
        }

        math::vpu::Vector3 GetNormal() const { return m_data.GetVector3(); }
        f32 GetDistance() const { return m_data.GetPlus().GetFloat(); }

    private:
        math::vpu::Vector3Plus m_data;
    };

    CGS_STATIC_ASSERT((sizeof(Plane) == 16), "A RenderWare plane occupies one VMX vector");
    CGS_STATIC_ASSERT((__alignof(Plane) == 16), "RenderWare planes use aligned VMX access");
}
}
