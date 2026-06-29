#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00) rw/math/vpu/vec_float.h.
//
// Declares the single-scalar lane wrappers over a VectorIntrinsic register:
//
//   VecFloat          -- owns its register; a broadcast scalar (all four lanes equal,
//                        the X lane being the canonical value).
//   VecFloatRef<I>    -- a *reference* into lane I of someone else's register
//                        (I is a compile-time constant).
//   VecFloatRefIndex  -- a *reference* into a runtime-chosen lane of someone else's
//                        register (mIndex is a runtime VectorAxis).
//
// These exist so that `someVector4.x += 1.0f;` etc. read/write a single lane of a SIMD
// register without round-tripping the whole register through memory. The method bodies
// (which on the console are AltiVec swizzle/permute/fp intrinsics) live in the
// detail/ inline header, lowered to portable scalar lane math.

#include "SDKs/EATech/include/rw/math/vpu/vector_axis.h"
#include "SDKs/EATech/include/rw/math/vpu/vector_intrinsic.h"

namespace rw
{
namespace math
{
namespace vpu
{
    class VecFloat;
    class VecFloatRefIndex;
    template<VectorAxis INDEX> class VecFloatRef;

    typedef VecFloatRef<VectorAxisX> VecFloatRefX;
    typedef VecFloatRef<VectorAxisY> VecFloatRefY;
    typedef VecFloatRef<VectorAxisZ> VecFloatRefZ;
    typedef VecFloatRef<VectorAxisW> VecFloatRefW;

    class VecFloat
    {
    public:
        typedef const VecFloat InParam;
        typedef VecFloat&      OutParam;
        typedef VecFloat&      InOutParam;

        template<VectorAxis INDEX>
        VecFloat(VecFloatRef<INDEX> v);
        VecFloat(VecFloatRefIndex v);
        VecFloat();
        VecFloat(const VecFloat& v);
        explicit VecFloat(VectorIntrinsicInParam v);
        VecFloat(VectorIntrinsicInParam v, int index);
        VecFloat(const float& f);

        float                   GetFloat() const;
        void                    SetFloat(float f);
        const VectorIntrinsic&  GetVector() const;
        operator float() const;

        template<VectorAxis INDEX>
        VecFloat& operator =(const VecFloatRef<INDEX>& v);
        VecFloat& operator =(const VecFloat& f);
        VecFloat& operator =(const float& f);
        VecFloat& operator =(const VecFloatRefIndex& v);

    public:
        VectorIntrinsic mV;
    };

    template<VectorAxis INDEX>
    class VecFloatRef
    {
    public:
        typedef const VecFloatRef InParam;
        typedef VecFloatRef&      OutParam;
        typedef VecFloatRef&      InOutParam;

        VecFloatRef(VectorIntrinsic& v);
        VecFloatRef(const VecFloatRef<INDEX>& v);

        float                   GetFloat() const;
        void                    SetFloat(float f);
        const VectorIntrinsic&  GetVector() const;
        int                     GetIndex() const;

        VecFloatRef<INDEX>& operator =(VecFloatRefX rhs);
        VecFloatRef<INDEX>& operator =(VecFloatRefY rhs);
        VecFloatRef<INDEX>& operator =(VecFloatRefZ rhs);
        VecFloatRef<INDEX>& operator =(VecFloatRefW rhs);
        VecFloatRef<INDEX>& operator =(VecFloatRefIndex rhs);
        VecFloatRef<INDEX>& operator =(VecFloat::InParam rhs);
        VecFloatRef<INDEX>& operator =(float f);

        operator float() const;

        template<VectorAxis INDEX_OTHER>
        VecFloatRef<INDEX>& operator += (VecFloatRef<INDEX_OTHER> b);
        VecFloatRef<INDEX>& operator += (VecFloat::InParam b);
        VecFloatRef<INDEX>& operator += (const VecFloatRefIndex& b);
        VecFloatRef<INDEX>& operator += (const float& b);

        template<VectorAxis INDEX_OTHER>
        VecFloatRef<INDEX>& operator /= (VecFloatRef<INDEX_OTHER> b);
        VecFloatRef<INDEX>& operator /= (VecFloat::InParam b);
        VecFloatRef<INDEX>& operator /= (const VecFloatRefIndex& b);
        VecFloatRef<INDEX>& operator /= (const float& b);

        template<VectorAxis INDEX_OTHER>
        VecFloatRef<INDEX>& operator -= (VecFloatRef<INDEX_OTHER> b);
        VecFloatRef<INDEX>& operator -= (VecFloat::InParam b);
        VecFloatRef<INDEX>& operator -= (const VecFloatRefIndex& b);
        VecFloatRef<INDEX>& operator -= (const float& b);

        template<VectorAxis INDEX_OTHER>
        VecFloatRef<INDEX>& operator *= (VecFloatRef<INDEX_OTHER> b);
        VecFloatRef<INDEX>& operator *= (VecFloat::InParam b);
        VecFloatRef<INDEX>& operator *= (const VecFloatRefIndex& b);
        VecFloatRef<INDEX>& operator *= (const float& b);

    public:
        VectorIntrinsic& mV;

        enum
        {
            mIndex = INDEX,
        };
    };

    class VecFloatRefIndex
    {
    public:
        typedef const VecFloatRefIndex& InParam;
        typedef VecFloatRefIndex&       OutParam;
        typedef VecFloatRefIndex&       InOutParam;

        VecFloatRefIndex(VectorIntrinsic& v, int index);
        VecFloatRefIndex(const VecFloatRefIndex& v);

        float                   GetFloat() const;
        void                    SetFloat(float f);
        const VectorIntrinsic&  GetVector() const;
        int                     GetIndex() const;

        template<VectorAxis INDEX>
        VecFloatRefIndex& operator =(VecFloatRef<INDEX> rhs);
        VecFloatRefIndex& operator =(VecFloatRefIndex rhs);
        VecFloatRefIndex& operator =(VecFloat::InParam rhs);
        VecFloatRefIndex& operator =(float f);

        operator float() const;

        template<VectorAxis INDEX1>
        VecFloatRefIndex& operator += (VecFloatRef<INDEX1> b);
        VecFloatRefIndex& operator += (VecFloat::InParam b);
        VecFloatRefIndex& operator += (VecFloatRefIndex::InParam b);
        VecFloatRefIndex& operator += (const float& b);

        template<VectorAxis INDEX1>
        VecFloatRefIndex& operator -= (VecFloatRef<INDEX1> b);
        VecFloatRefIndex& operator -= (VecFloat::InParam b);
        VecFloatRefIndex& operator -= (VecFloatRefIndex::InParam b);
        VecFloatRefIndex& operator -= (const float& b);

        template<VectorAxis INDEX1>
        VecFloatRefIndex& operator *= (VecFloatRef<INDEX1> b);
        VecFloatRefIndex& operator *= (VecFloat::InParam b);
        VecFloatRefIndex& operator *= (VecFloatRefIndex::InParam b);
        VecFloatRefIndex& operator *= (const float& b);

        template<VectorAxis INDEX1>
        VecFloatRefIndex& operator /= (VecFloatRef<INDEX1> b);
        VecFloatRefIndex& operator /= (VecFloat::InParam b);
        VecFloatRefIndex& operator /= (VecFloatRefIndex::InParam b);
        VecFloatRefIndex& operator /= (const float& b);

    public:
        VectorIntrinsic& mV;
        VectorAxis       mIndex;
    };

    // Free GetFloat() overloads (the SDK's uniform "read the scalar out of anything"
    // helper). Kept inline here, as in the Feb-2007 source.
    template<VectorAxis AXIS>
    inline float GetFloat(const VecFloatRef<AXIS>& v) { return v.GetFloat(); }
    inline float GetFloat(const VecFloatRefIndex& v)  { return v.GetFloat(); }
    inline float GetFloat(const VecFloat& v)          { return v.GetFloat(); }
    inline float GetFloat(float f)                    { return f; }
}
}
}

#include "SDKs/EATech/include/rw/math/vpu/detail/vec_float_type_inline.h"
#include "SDKs/EATech/include/rw/math/vpu/detail/vec_float_type_platform_inline.h"
