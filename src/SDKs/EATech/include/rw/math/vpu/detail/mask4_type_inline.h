#pragma once

#include "SDKs/EATech/include/rw/math/vpu/mask4.h"

namespace rw { namespace math { namespace vpu {
inline Mask4::Mask4() {}
inline Mask4::Mask4(VectorIntrinsicInParam value) : mV(value) {}
inline void Mask4::SetFalse() { mV = __vspltisw(0); }
}}}
