#pragma once

#include <vectorintrinsics.h>
#include "SDKs/EATech/include/rw/math/vpu/vector3.h"
#include "SDKs/EATech/include/rw/math/vpu/vector4.h"
#include "SDKs/EATech/include/rw/math/vpu/matrix33.h"
#include "SDKs/EATech/include/rw/math/vpu/vector3plus.h"
#include "SDKs/EATech/include/rw/math/vpu/maskscalar.h"
#include "SDKs/EATech/include/rw/math/vpu/mask4_operation.h"

// Shared rwmath type entry point. Vector3/4 use their genuine SDK class homes above.
// Vector2 remains the separately recovered VPU16 type; packed FPU/Basic2D pairs
// and the existing matrix/quaternion declarations retain their distinct contracts.

namespace rw
{
namespace math
{
namespace vpu
{
    // ARTIST821F0DB0 / DecFIGS vector2.h:45/49/130: genuine VPU register.
    // Copy construction and assignment stay implicit for the native value ABI.
    struct CGS_ALIGN(16) __declspec(passinreg) Vector2
    {
        __vector4 mV;
        typedef const Vector2 InParam;
        Vector2() {}
        // ARTIST821F0DB0 body; the SDK declares this constructor inline.
        Vector2(float lfX, float lfY)
        {
            __vector4 lValue;
            lValue.x = lfX;
            lValue.y = lfY;
            lValue.u[2] = 0;
            lValue.u[3] = 0;
            mV = lValue;
        }
        // rwmath vector2.h / vector2_type_inline.h: the raw-register constructor the
        // vector2 operations return through (e.g. operator+ = Vector2(vaddfp(a, b))).
        Vector2(VectorIntrinsicInParam v) : mV(v) {}
        void SetZero() { mV = __vspltisw(0); }

        // DecFIGS vector2_type_inline.h:175, 241 and 268. A scalar assignment
        // keeps the other lanes of the existing vector.
        void SetComponent(int liComponent, float lfValue)
        {
            detail::VectorIntrinsicUnion lValue;
            lValue.vector = mV;
            lValue.scalarFloat[liComponent] = lfValue;
            mV = lValue.vector;
        }

        void SetX(float lfX) { SetComponent(0, lfX); }
        void SetY(float lfY) { SetComponent(1, lfY); }
    };

    // ADDITIVE GROW (rw-physics solver group): the SDK quaternion -- one 16-byte lane
    // register, (x, y, z, w) with w the scalar part. The DWARF spells
    // JointFrames::mQuatA/mQuatB/mQuatL, DriveFrames::mQuatA/mQuatB and RigidBody::mQuat as
    // `Quaternion`, and the rwmath quaternion_operation family returns it.
    //
    // This type was ALREADY WRITTEN, in src/vendor/renderware/physics/JointFrames.hpp, whose
    // own banner says: "defined here because their canonical homes (the vendor
    // rw/math/vpu/types.h ...) sit outside this wave's writable tree; move them there on the
    // next grow of that header". This is that grow -- the definition moved here verbatim and
    // JointFrames.hpp now includes it rather than declaring a second copy.
    struct CGS_ALIGN(16) Quaternion
    {
        float x, y, z, w;
        void SetIdentity() { x = y = z = 0.0f; w = 1.0f; }
    };
    // Row-major; the last row of an affine is implicit (0,0,0,1) but still stored.
    struct CGS_ALIGN(16) Matrix44
    {
        typedef const Matrix44& InParam;
        typedef Matrix44& OutParam;
        typedef Matrix44& InOutParam;
        Vector4 xAxis, yAxis, zAxis, wAxis;
        // DecFIGS matrix44.h:28/31; the nontrivial copy is also required by
        // ARTIST's pointer ABI for Matrix44 values (for example 8227B8A0).
        Matrix44() {}
        Matrix44(const Matrix44& lrMatrix)
            : xAxis(lrMatrix.xAxis), yAxis(lrMatrix.yAxis),
              zAxis(lrMatrix.zAxis), wAxis(lrMatrix.wAxis) {}
        // DecFIGS matrix44.h:61 and matrix44_type_inline.h:22 declare the
        // original assignment. ARTIST 0x827B0060 copies all four SIMD rows.
        Matrix44& operator=(const Matrix44& lrMatrix)
        {
            xAxis.mV = lrMatrix.xAxis.mV;
            yAxis.mV = lrMatrix.yAxis.mV;
            zAxis.mV = lrMatrix.zAxis.mV;
            wAxis.mV = lrMatrix.wAxis.mV;
            return *this;
        }

        Matrix44(Vector4::InParam x, Vector4::InParam y, Vector4::InParam z, Vector4::InParam w)
            : xAxis(x), yAxis(y), zAxis(z), wAxis(w) {}
        Matrix44(VectorIntrinsicInParam x, VectorIntrinsicInParam y, VectorIntrinsicInParam z, VectorIntrinsicInParam w)
            : xAxis(x), yAxis(y), zAxis(z), wAxis(w) {}
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); wAxis.SetZero(); }
        // rwmath matrix44_type_inline.h:78 (DecFIGS the same line; always_inline in the SDK): the four
        // rows from the unit axis registers, as ARTIST loads them in Debug3DImmediateRender::
        // SetProjectionMode 0x8281B9F0..0x8281BA6C (lvx from 0x82181500/10/20/30).
        __forceinline void SetIdentity()
        {
            { xAxis.mV = ::rw::math::vpu::detail::gIVector; };
            { yAxis.mV = ::rw::math::vpu::detail::gJVector; };
            { zAxis.mV = ::rw::math::vpu::detail::gKVector; };
            { wAxis.mV = ::rw::math::vpu::detail::gLVector; };
        }
    };
    struct CGS_ALIGN(16) Matrix44Affine
    {
        typedef const Matrix44Affine& InParam;
        typedef Matrix44Affine& OutParam;
        typedef Matrix44Affine& InOutParam;
        Vector3 xAxis, yAxis, zAxis, wAxis;
        // DecFIGS matrix44affine.h:61/63/64. ARTIST 822B33B8 receives
        // this value through r5; the explicit copy preserves all four raw rows.
        Matrix44Affine() {}
        Matrix44Affine(const Matrix44Affine& lrMatrix)
            : xAxis(lrMatrix.xAxis), yAxis(lrMatrix.yAxis),
              zAxis(lrMatrix.zAxis), wAxis(lrMatrix.wAxis) {}
        // Original SDK matrix44affine_type_inline.h:26 copies all four raw SIMD rows.
        Matrix44Affine& operator=(const Matrix44Affine& lrMatrix)
        {
            xAxis.mV = lrMatrix.xAxis.mV;
            yAxis.mV = lrMatrix.yAxis.mV;
            zAxis.mV = lrMatrix.zAxis.mV;
            wAxis.mV = lrMatrix.wAxis.mV;
            return *this;
        }

        Matrix44Affine(Vector3 lxAxis, Vector3 lyAxis, Vector3 lzAxis, Vector3 lwAxis)
            : xAxis(lxAxis), yAxis(lyAxis), zAxis(lzAxis), wAxis(lwAxis) {}
        // DecFIGS matrix44affine_type_inline.h:424 (rwmath 1.02.00 same line): the four raw
        // rows, each stored into its axis's register.
        Matrix44Affine(VectorIntrinsicInParam row0, VectorIntrinsicInParam row1,
                       VectorIntrinsicInParam row2, VectorIntrinsicInParam row3)
        {
            xAxis.mV = row0;
            yAxis.mV = row1;
            zAxis.mV = row2;
            wAxis.mV = row3;
        }
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); wAxis.SetZero(); }
        // rwmath matrix44affine_type_inline.h:47 / :72 (DecFIGS matrix44affine.h:64; always_inline in the SDK): the
        // identity is the twelve-float Set, each row a Vector3::Set (ARTIST inlines it, four three-lane fills with
        // their zeroed w words, in ExternallySimulatedBody::Construct 0x8259CF30..).
        __forceinline void Set(float m0, float m1, float m2, float m3, float m4, float m5,
                               float m6, float m7, float m8, float m9, float m10, float m11)
        {
            xAxis.Set(m0, m1, m2);
            yAxis.Set(m3, m4, m5);
            zAxis.Set(m6, m7, m8);
            wAxis.Set(m9, m10, m11);
        }
        __forceinline void SetIdentity()
        {
            Set(1.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 0.0f);
        }

        Vector3& Right() { return xAxis; }
        Vector3& Up() { return yAxis; }
        Vector3& At() { return zAxis; }
        Vector3& Pos() { return wAxis; }

        const Vector3& Right() const { return xAxis; }
        const Vector3& Up() const { return yAxis; }
        const Vector3& At() const { return zAxis; }
        const Vector3& Pos() const { return wAxis; }
    };

}
}
}
