#pragma once

#include "SDKs/EATech/include/rw/math/vpu/vector_intrinsic.h"

namespace rw { namespace math { namespace vpu {
// DecFIGS maskscalar.h:24/33..53. ARTIST BoxOverlappingTest825BF088
// returns this complete mask in v1; self-copy and assignment remain implicit.
class __declspec(passinreg) MaskScalar
{
public:
    typedef const MaskScalar InParam;
    MaskScalar();
    explicit MaskScalar(VectorIntrinsicInParam value);
    explicit MaskScalar(bool value);
    explicit MaskScalar(unsigned int value);
    bool GetBool() const;
    unsigned int ToUInt32() const;
    void SetTrue();
    void SetFalse();
public:
    VectorIntrinsic mV;
};
}}}

#include "SDKs/EATech/include/rw/math/vpu/detail/maskscalar_type_inline.h"
