#pragma once

#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "SDKs/EATech/include/rw/math/vpu/mask4.h"

namespace CgsGeometric
{
    // DecFIGS CgsAxisAlignedBox4.h:96–106. Seven aligned VMX rows in the
    // polygon-soup bounds block. ARTIST 8283AA68 reads the six bound rows;
    // the containing array advances by 0x70 at 82841F04.
    struct AxisAlignedBox4
    {
        AxisAlignedBox GetAxisAlignedBox(s32 liBoxNum) const;

    private:
        Vector4 mAabbMinX;
        Vector4 mAabbMinY;
        Vector4 mAabbMinZ;
        Vector4 mAabbMaxX;
        Vector4 mAabbMaxY;
        Vector4 mAabbMaxZ;
        rw::math::vpu::Mask4 mValidMasks;
    };

    CGS_STATIC_ASSERT(sizeof(AxisAlignedBox4) == 0x70, "ARTIST bounds block stride");
    CGS_STATIC_ASSERT(__alignof(AxisAlignedBox4) == 16, "ARTIST vector row alignment");
}
