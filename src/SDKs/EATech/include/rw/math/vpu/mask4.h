#pragma once

#include "SDKs/EATech/include/rw/math/vpu/vector_intrinsic.h"

namespace rw { namespace math { namespace vpu {
// DecFIGS mask4.h:25/34..62: a distinct four-lane bit mask with one real
// register. The required raw value interface retains implicit self-copy.
class __declspec(passinreg) Mask4
{
public:
    typedef const Mask4 InParam;
    Mask4();
    explicit Mask4(VectorIntrinsicInParam value);
    void SetFalse();
public:
    VectorIntrinsic mV;
};
}}}

#include "SDKs/EATech/include/rw/math/vpu/detail/mask4_type_inline.h"
