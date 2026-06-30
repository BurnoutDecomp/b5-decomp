#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00) rw/math/vpu/vector4.h.
//
// Declares rw::math::vpu::Vector4 -- a 4-component vector stored in a single
// VectorIntrinsic register (lanes [X,Y,Z,W]). Same thin-register-owner design as Vector3;
// every ctor/getter/setter is a swizzle/permute on `mV`, lowered to portable lane math in
// detail/vector4_type_inline.h. Only the construction / component access surface declared
// in the Feb-2007 source's vector4_type_inline.h lives here.

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/vector3.h"

namespace rw
{
namespace math
{
namespace vpu
{
    namespace fpu { template<class T> class Vector4Template; }

    class Vector4
    {
    public:
        typedef const Vector4& InParam;
        typedef Vector4&       OutParam;
        typedef Vector4&       InOutParam;

        Vector4();
        Vector4(const Vector4& rhs);
        Vector4(float x, float y, float z, float w);
        Vector4(VecFloat::InParam x, VecFloat::InParam y, VecFloat::InParam z, VecFloat::InParam w);
        Vector4(VecFloatRefIndex::InParam x, VecFloatRefIndex::InParam y, VecFloatRefIndex::InParam z, VecFloatRefIndex::InParam w);
        template<VectorAxis XSRC_INDEX, VectorAxis YSRC_INDEX, VectorAxis ZSRC_INDEX, VectorAxis WSRC_INDEX>
        Vector4(VecFloatRef<XSRC_INDEX> x, VecFloatRef<YSRC_INDEX> y, VecFloatRef<ZSRC_INDEX> z, VecFloatRef<WSRC_INDEX> w);
        Vector4(Vector3::InParam v3, VecFloatRefW::InParam w);
        Vector4(Vector3::InParam v3, VecFloatRefIndex::InParam w);
        Vector4(Vector3::InParam v3, VecFloat::InParam w);
        Vector4(Vector3::InParam v3, const float w);
        template<class T>
        Vector4(const fpu::Vector4Template<T>& v);
        explicit Vector4(const float xyzw[4]);
        Vector4(VectorIntrinsicInParam v);

        Vector4& operator =(InParam v);
        Vector4& operator =(VectorIntrinsicInParam v);
        operator VectorIntrinsic&();
        operator const VectorIntrinsic&() const;

        void Set(float x, float y, float z, float w);
        void Set(VecFloat::InParam x, VecFloat::InParam y, VecFloat::InParam z, VecFloat::InParam w);
        void Set(VecFloatRefIndex::InParam x, VecFloatRefIndex::InParam y, VecFloatRefIndex::InParam z, VecFloatRefIndex::InParam w);
        template<VectorAxis XSRC_INDEX, VectorAxis YSRC_INDEX, VectorAxis ZSRC_INDEX, VectorAxis WSRC_INDEX>
        void Set(VecFloatRef<XSRC_INDEX> x, VecFloatRef<YSRC_INDEX> y, VecFloatRef<ZSRC_INDEX> z, VecFloatRef<WSRC_INDEX> w);

        void SetComponent(int i, float value);
        void SetComponent(int i, VecFloat::InParam value);
        void SetComponent(int i, VecFloatRefIndex::InParam value);
        template<VectorAxis INDEX>
        void SetComponent(int i, VecFloatRef<INDEX> value);

        void SetX(float x);
        void SetX(VecFloat::InParam x);
        void SetX(VecFloatRefIndex::InParam x);
        template<VectorAxis INDEX> void SetX(VecFloatRef<INDEX> x);
        void SetY(float y);
        void SetY(VecFloat::InParam y);
        void SetY(VecFloatRefIndex::InParam y);
        template<VectorAxis INDEX> void SetY(VecFloatRef<INDEX> y);
        void SetZ(float z);
        void SetZ(VecFloat::InParam z);
        void SetZ(VecFloatRefIndex::InParam z);
        template<VectorAxis INDEX> void SetZ(VecFloatRef<INDEX> z);
        void SetW(float w);
        void SetW(VecFloat::InParam w);
        void SetW(VecFloatRefIndex::InParam w);
        template<VectorAxis INDEX> void SetW(VecFloatRef<INDEX> w);

        void SetZero();

        void                   SetVector(VectorIntrinsicInParam v);
        VectorIntrinsic&       GetVector();
        const VectorIntrinsic& GetVector() const;

        VecFloatRefX       X();
        const VecFloatRefX X() const;
        VecFloatRefY       Y();
        const VecFloatRefY Y() const;
        VecFloatRefZ       Z();
        const VecFloatRefZ Z() const;
        VecFloatRefW       W();
        const VecFloatRefW W() const;

        VecFloatRefX       GetX();
        const VecFloatRefX GetX() const;
        VecFloatRefY       GetY();
        const VecFloatRefY GetY() const;
        VecFloatRefZ       GetZ();
        const VecFloatRefZ GetZ() const;
        VecFloatRefW       GetW();
        const VecFloatRefW GetW() const;

        VecFloatRefIndex       GetComponent(int i);
        const VecFloatRefIndex GetComponent(int i) const;
        VecFloatRefIndex       operator [](int i);
        const VecFloatRefIndex operator [](int i) const;

    public:
        VectorIntrinsic mV;
    };
}
}
}

#include "SDKs/EATech/include/rw/math/vpu/detail/vector4_type_inline.h"
