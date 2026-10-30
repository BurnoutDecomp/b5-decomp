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
        static void SetAspectCorrectionMatrix(const rw::math::fpu::Matrix33Template<float>&);

        // ARTIST830112D0 and83011090; native callers use these shared transforms.
        static const Im2dTransform& GetDefault() { return mgDefault; }
        static const Im2dTransform& GetAspectCorrected() { return mgAspectCorrected; }
        static const rw::math::fpu::Matrix33Template<float>& GetAspectCorrectionMatrix()
        { return mgAspectRatioCorrection; }

    protected:
        static Im2dTransform mgDefault;
        static Im2dTransform mgAspectCorrected;
        static rw::math::fpu::Matrix33Template<float> mgAspectRatioCorrection;
    };
}
