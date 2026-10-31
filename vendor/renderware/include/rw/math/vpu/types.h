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
        Vector2(float lfX, float lfY);
        void SetZero() { mV = __vspltisw(0); }
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
        Vector4 xAxis, yAxis, zAxis, wAxis;
        // DecFIGS matrix44.h:28/31; the nontrivial copy is also required by
        // ARTIST's pointer ABI for Matrix44 values (for example 8227B8A0).
        Matrix44() {}
        Matrix44(const Matrix44& lrMatrix)
            : xAxis(lrMatrix.xAxis), yAxis(lrMatrix.yAxis),
              zAxis(lrMatrix.zAxis), wAxis(lrMatrix.wAxis) {}
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); wAxis.SetZero(); }
        void SetIdentity()
        {
            const Vector4 lxAxis(1.0f, 0.0f, 0.0f, 0.0f); xAxis = lxAxis; const Vector4 lyAxis(0.0f, 1.0f, 0.0f, 0.0f); yAxis = lyAxis;
            const Vector4 lzAxis(0.0f, 0.0f, 1.0f, 0.0f); zAxis = lzAxis; const Vector4 lwAxis(0.0f, 0.0f, 0.0f, 1.0f); wAxis = lwAxis;
        }
    };
    struct CGS_ALIGN(16) Matrix44Affine
    {
        typedef const Matrix44Affine& InParam;
        Vector3 xAxis, yAxis, zAxis, wAxis;
        // DecFIGS matrix44affine.h:61/63/64. ARTIST 822B33B8 receives
        // this value through r5; the explicit copy preserves all four raw rows.
        Matrix44Affine() {}
        Matrix44Affine(const Matrix44Affine& lrMatrix)
            : xAxis(lrMatrix.xAxis), yAxis(lrMatrix.yAxis),
              zAxis(lrMatrix.zAxis), wAxis(lrMatrix.wAxis) {}
        Matrix44Affine(Vector3 lxAxis, Vector3 lyAxis, Vector3 lzAxis, Vector3 lwAxis)
            : xAxis(lxAxis), yAxis(lyAxis), zAxis(lzAxis), wAxis(lwAxis) {}
        void SetZero() { xAxis.SetZero(); yAxis.SetZero(); zAxis.SetZero(); wAxis.SetZero(); }
        void SetIdentity()
        {
            const Vector3 lxAxis(1.0f, 0.0f, 0.0f); xAxis = lxAxis; const Vector3 lyAxis(0.0f, 1.0f, 0.0f); yAxis = lyAxis;
            const Vector3 lzAxis(0.0f, 0.0f, 1.0f); zAxis = lzAxis; const Vector3 lwAxis(0.0f, 0.0f, 0.0f); wAxis = lwAxis;
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
