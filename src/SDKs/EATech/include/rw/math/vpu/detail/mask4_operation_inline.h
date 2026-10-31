#pragma once

#include "SDKs/EATech/include/rw/math/vpu/mask4.h"

namespace rw { namespace math { namespace vpu {
// DecFIGS mask4_operation_inline.h:22/29/36. Preserve every mask bit;
// numeric floating-point operations do not define this interface.
inline Mask4 Not(Mask4::InParam value)
{ return Mask4(__vnor(value.mV, value.mV)); }
inline Mask4 And(Mask4::InParam a, Mask4::InParam b)
{ return Mask4(__vand(a.mV, b.mV)); }
inline Mask4 Or(Mask4::InParam a, Mask4::InParam b)
{ return Mask4(__vor(a.mV, b.mV)); }
}}}
