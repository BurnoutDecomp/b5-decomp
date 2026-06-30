#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00) rw/math/vpu/vector3.h.
//
// Declares rw::math::vpu::Vector3 -- a 3-component vector stored in a single
// VectorIntrinsic register (lanes [X,Y,Z], the W lane unused/garbage). The class is a
// thin owner of a VectorIntrinsic `mV`; every ctor/getter/setter is a swizzle/permute on
// that register. On the X360 those are AltiVec/VMX intrinsics; here they are lowered to
// the equivalent portable lane math (see detail/vector3_type_inline.h), in the same style
// as the committed VecFloat lane wrappers.
//
// Only the construction / component access surface declared in the Feb-2007 source's
// vector3_type_inline.h lives here. The arithmetic operators (Add/Dot/Cross/Length/...)
// are declared in separate operation headers, not this TU.

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"

namespace rw
{
namespace math
{
namespace vpu
{
    namespace fpu { template<class T> class Vector3Template; }

    class Vector3
    {
    public:
        typedef const Vector3& InParam;
        typedef Vector3&       OutParam;
        typedef Vector3&       InOutParam;

        Vector3();
        Vector3(const Vector3& rhs);
        Vector3(float x, float y, float z);
        Vector3(VecFloat::InParam x, VecFloat::InParam y, VecFloat::InParam z);
        Vector3(VecFloatRefIndex::InParam x, VecFloatRefIndex::InParam y, VecFloatRefIndex::InParam z);
        template<VectorAxis XSRC_INDEX, VectorAxis YSRC_INDEX, VectorAxis ZSRC_INDEX>
        Vector3(VecFloatRef<XSRC_INDEX> x, VecFloatRef<YSRC_INDEX> y, VecFloatRef<ZSRC_INDEX> z);
        template<class T>
        Vector3(const fpu::Vector3Template<T>& v);
        explicit Vector3(const float xyz[3]);
        Vector3(VectorIntrinsicInParam v);

        Vector3& operator =(InParam v);
        Vector3& operator =(VectorIntrinsicInParam v);
        operator VectorIntrinsic&();
        operator const VectorIntrinsic&() const;

        void Set(float x, float y, float z);
        void Set(VecFloat::InParam x, VecFloat::InParam y, VecFloat::InParam z);
        void Set(VecFloatRefIndex::InParam x, VecFloatRefIndex::InParam y, VecFloatRefIndex::InParam z);
        template<VectorAxis XSRC_INDEX, VectorAxis YSRC_INDEX, VectorAxis ZSRC_INDEX>
        void Set(VecFloatRef<XSRC_INDEX> x, VecFloatRef<YSRC_INDEX> y, VecFloatRef<ZSRC_INDEX> z);

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

        VecFloatRefX       GetX();
        const VecFloatRefX GetX() const;
        VecFloatRefY       GetY();
        const VecFloatRefY GetY() const;
        VecFloatRefZ       GetZ();
        const VecFloatRefZ GetZ() const;

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

#include "SDKs/EATech/include/rw/math/vpu/detail/vector3_type_inline.h"
