#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00) rw/math/vpu/matrix44.h.
//
// Declares rw::math::vpu::Matrix44 (a 4x4 matrix as four Vector4 row axes) and the
// row-major affine variant Matrix44Affine. The free-function matrix operations homed in
// detail/matrix44_operation_platform_inline.h (Mult / Transpose / Inverse /
// Matrix44FromAxisRotationAngle) operate on these.
//
// Layout: four contiguous Vector4 rows (xAxis,yAxis,zAxis,wAxis), each a 16-byte/16-aligned
// VectorIntrinsic -- 64 bytes, matching the console register-row layout.

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/vector3.h"
#include "SDKs/EATech/include/rw/math/vpu/vector4.h"

namespace rw
{
namespace math
{
namespace vpu
{
    class Matrix44
    {
    public:
        typedef const Matrix44& InParam;
        typedef Matrix44&       OutParam;
        typedef Matrix44&       InOutParam;

        Matrix44() {}
        Matrix44(Vector4::InParam x, Vector4::InParam y, Vector4::InParam z, Vector4::InParam w)
            : xAxis(x), yAxis(y), zAxis(z), wAxis(w) {}
        Matrix44(VectorIntrinsicInParam x, VectorIntrinsicInParam y, VectorIntrinsicInParam z, VectorIntrinsicInParam w)
            : xAxis(x), yAxis(y), zAxis(z), wAxis(w) {}

    public:
        Vector4 xAxis;
        Vector4 yAxis;
        Vector4 zAxis;
        Vector4 wAxis;
    };

    // Affine 4x4 -- same four-row storage; the affine-specialised Mult treats the wAxis as a
    // translation (its xAxis/yAxis/zAxis lanes used as scale-rotation broadcasts).
    class Matrix44Affine
    {
    public:
        typedef const Matrix44Affine& InParam;
        typedef Matrix44Affine&       OutParam;
        typedef Matrix44Affine&       InOutParam;

        Matrix44Affine() {}
        Matrix44Affine(Vector4::InParam x, Vector4::InParam y, Vector4::InParam z, Vector4::InParam w)
            : xAxis(x), yAxis(y), zAxis(z), wAxis(w) {}

    public:
        Vector4 xAxis;
        Vector4 yAxis;
        Vector4 zAxis;
        Vector4 wAxis;
    };
}
}
}

#include "SDKs/EATech/include/rw/math/vpu/detail/matrix44_operation_platform_inline.h"
