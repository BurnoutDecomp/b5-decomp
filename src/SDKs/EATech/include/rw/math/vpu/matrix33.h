#pragma once

#include "SDKs/EATech/include/rw/math/vpu/vector3.h"

namespace rw
{
namespace math
{
namespace vpu
{
    // DecFIGS matrix33.h:26/48..71/131..133. Three genuine vector rows;
    // explicit copy/assignment are independent of the vector value convention.
    class Matrix33
    {
    public:
        typedef const Matrix33 InParam;
        typedef Matrix33& OutParam;
        typedef Matrix33& InOutParam;

        Matrix33() {}
        Matrix33(const Matrix33& m)
            : xAxis(m.xAxis), yAxis(m.yAxis), zAxis(m.zAxis) {}
        Matrix33(Vector3::InParam row0, Vector3::InParam row1, Vector3::InParam row2)
            : xAxis(row0), yAxis(row1), zAxis(row2) {}
        Matrix33(VectorIntrinsicInParam row0, VectorIntrinsicInParam row1,
                 VectorIntrinsicInParam row2)
            : xAxis(row0), yAxis(row1), zAxis(row2) {}

        Matrix33& operator=(const Matrix33& m)
        {
            xAxis.mV = m.xAxis.mV;
            yAxis.mV = m.yAxis.mV;
            zAxis.mV = m.zAxis.mV;
            return *this;
        }
        void SetZero()
        {
            xAxis.mV = __vspltisw(0);
            yAxis.mV = xAxis.mV;
            zAxis.mV = xAxis.mV;
        }
        void SetIdentity()
        {
            const VectorIntrinsic i = {1.0f, 0.0f, 0.0f, 0.0f};
            const VectorIntrinsic j = {0.0f, 1.0f, 0.0f, 0.0f};
            const VectorIntrinsic k = {0.0f, 0.0f, 1.0f, 0.0f};
            xAxis.mV = i;
            yAxis.mV = j;
            zAxis.mV = k;
        }
        void Set(Vector3::InParam row0, Vector3::InParam row1, Vector3::InParam row2)
        {
            xAxis.mV = row0.mV;
            yAxis.mV = row1.mV;
            zAxis.mV = row2.mV;
        }

        Vector3& GetX() { return xAxis; }
        Vector3& GetY() { return yAxis; }
        Vector3& GetZ() { return zAxis; }
        const Vector3& GetX() const { return xAxis; }
        const Vector3& GetY() const { return yAxis; }
        const Vector3& GetZ() const { return zAxis; }
        Vector3& XAxis() { return xAxis; }
        Vector3& YAxis() { return yAxis; }
        Vector3& ZAxis() { return zAxis; }
        const Vector3& XAxis() const { return xAxis; }
        const Vector3& YAxis() const { return yAxis; }
        const Vector3& ZAxis() const { return zAxis; }
        Vector3& Right() { return xAxis; }
        Vector3& Up() { return yAxis; }
        Vector3& At() { return zAxis; }
        const Vector3& Right() const { return xAxis; }
        const Vector3& Up() const { return yAxis; }
        const Vector3& At() const { return zAxis; }

        Vector3 xAxis, yAxis, zAxis;
    };
}
}
}
