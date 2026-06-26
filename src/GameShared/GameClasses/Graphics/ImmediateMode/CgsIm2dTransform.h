#pragma once

#include "rw/math/vpu/types.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"   // rw::math::fpu::Matrix33Template<float> (aspect-correction matrix)

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

        // The class-wide aspect-correction matrix (X360 static @0x830110F0, nine floats),
        // populated at runtime by SetAspectCorrectionMatrix(const Matrix33&) and read by
        // TransformByAspectRatio. A static member, so it does not change the 64-byte
        // instance layout (4 x Vector4). Storage homed in CgsIm2dTransform.cpp.
        static rw::math::fpu::Matrix33Template<float> msAspectCorrectionMatrix;
    };
}
