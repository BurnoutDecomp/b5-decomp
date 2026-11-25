#pragma once

// rwmath 1.02.00 rw/math/vpu/vector_intrinsic_operation.h (the DecFIGS DWARF lists the
// header; LegacySrc carries its PS3 bodies in detail/vector_intrinsic_operation_inline.h).
// The register-level predicates shared by the typed operations. The Xenon bodies below
// are read from ARTIST, where every typed caller inlines them.

#include "SDKs/EATech/include/rw/math/vpu/vector_intrinsic.h"
#include "SDKs/EATech/include/rw/math/vpu/vector_axis.h"

namespace rw
{
namespace math
{
namespace vpu
{
    // DecFIGS vector_intrinsic_operation_inline.h:14. Gather XYZ and retain
    // X's first lane in W, as ARTIST SphereVolume::GetBBoxDiag does.
    template<VectorAxis XAXIS, VectorAxis YAXIS, VectorAxis ZAXIS>
    inline VectorIntrinsic _asmCreateVectorAxis(VectorIntrinsicInParam x,
                                               VectorIntrinsicInParam y,
                                               VectorIntrinsicInParam z)
    {
        const uint32_t byteStride = 0x04040404;
        static const CGS_ALIGN(16) uint32_t xy[] =
        {
            0x00010203 + byteStride * XAXIS,
            0x10111213 + byteStride * YAXIS,
            0x00010203,
            0x00010203,
        };
        const VectorIntrinsic packed = __vperm(x, y, __lvx(xy, 0));
        return __vrlimi(packed, z, 2, (ZAXIS + 2) & 3);
    }

    // All four lanes equal: the vcmpeqfp. record form, CR6's "all true" bit returned as a
    // bool. ARTIST 0x8270DFB8..0x8270DFC8 (IsValid(VecFloat) in Vehicle::SetWheelRot):
    // li/stw of the zeroed CR word, vcmpeqfp128., mfocrf, rlwinm 25,31,31.
    inline bool _asmIsEqualV4(VectorIntrinsicInParam v0, VectorIntrinsicInParam v1)
    {
        unsigned int cr = 0;
        __vcmpeqfpR(v0, v1, &cr);
        return (cr & 0x80) == 0x80;
    }

    // x and y only: each operand's x,y lanes are replicated into z,w (vrlimi mask 3,
    // rotate 2; PS3 permutes X0,Y0,X0,Y0) before the four-lane predicate.
    // ARTIST 0x827DD570..0x827DD594 (CrashNavMapIcon::SetPosition's Vector2 compare).
    inline bool _asmIsEqualV2(VectorIntrinsicInParam v0, VectorIntrinsicInParam v1)
    {
        unsigned int cr = 0;
        __vcmpeqfpR(__vrlimi(v0, v0, 3, 2), __vrlimi(v1, v1, 3, 2), &cr);
        return (cr & 0x80) == 0x80;
    }
}
}
}
