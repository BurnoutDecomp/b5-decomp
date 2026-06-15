#pragma once

#include "rw/math/vpu/types.h"

// CgsGraphics::Im2dTransform - the screen-space transform an Im2d batch is rendered
// through: an origin + right/up basis (packed as Vector4s) plus a colour shift/scale.
// Layout/method set from the DecFIGS DWARF (CgsIm2dTransform.h). TransformByAspectRatio
// folds the current display aspect ratio into the transform in place.
namespace CgsGraphics
{
    struct Im2dTransform
    {
        rw::math::vpu::Vector4 mOriginXYZ;
        rw::math::vpu::Vector4 mRightUp;
        rw::math::vpu::Vector4 mColourShift;
        rw::math::vpu::Vector4 mColourScale;

        void TransformByAspectRatio();
    };
}
