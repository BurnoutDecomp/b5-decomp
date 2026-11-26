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

    // rwmath vpu/detail/<platform>/vector_intrinsic_operation_macros.h (DecFIGS ps3/ppu :11, LegacySrc
    // ps3/ppu/vector_intrinsic_operation_macros.h): the scalar broadcast the Vector3 operators call. The PPU
    // body is a union fill; the Xenon one stores { f } with three zero words, loads it and splats lane 0
    // (BrnTraffic::ParamTransform::Initialise 0x82712DBC..0x82712DDC; the same expansion as the VecFloat
    // operator set's const float& operand, detail/vec_float_operation_inline.h).
    __forceinline VectorIntrinsic RW_MATH_VPU_CreateVectorFromScalar_R(float f0)
    {
        VectorIntrinsic ret;
        { VectorIntrinsic u = { f0 }; ret = __vspltw(u, 0); }
        return ret;
    }

    // rwmath vector_intrinsic_operation.h:61..66 (bodies LegacySrc detail/vector_intrinsic_operation_inline.h
    // :453..:490, DecFIGS lists the <X,X> <Y,Y> <Z,Z> <W,W> instances and the runtime form :284): copy lane
    // INDEX_SRC of src into lane INDEX_DEST of dest. The PS3 builds a VectorPermuteConstant for the
    // constant-lane form; the Xenon body is the vrlimi128 lane insert, mask 8 >> DEST, rotate (SRC - DEST) & 3
    // (VecFloatRef<Z>::operator*= in BehaviourGameplayExternal::ModifyTargetAngles 0x82225624: vrlimi 2, 0;
    // VecFloatRef<Y> = float in Serialise<1> 0x82213CA0: 4, 3). A runtime lane indexes gSwizzleStoreConstants
    // and permutes, as on the PS3 (VecFloatRefIndex::operator*= in MainMapComponent::SetZoom 0x82469BC8).
    template<VectorAxis INDEX_DEST, VectorAxis INDEX_SRC>
    __forceinline VectorIntrinsic _asmSwizzleStore(VectorIntrinsicInParam dest, VectorIntrinsicInParam src)
    {
        VectorIntrinsic ret;
        { ret = __vrlimi(dest, src, (0x8 >> (INDEX_DEST)), (((int)(INDEX_SRC)) - (INDEX_DEST)) & 3); };
        return ret;
    }

    template<VectorAxis INDEX_DEST>
    __forceinline VectorIntrinsic _asmSwizzleStore(VectorIntrinsicInParam dest, VectorIntrinsicInParam src, VectorAxis srcIndex)
    {
        VectorIntrinsic ret;
        { ret = __vperm(dest, src, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((INDEX_DEST) * 4 ) + (srcIndex))]); };
        return ret;
    }

    template<VectorAxis INDEX_SRC>
    __forceinline VectorIntrinsic _asmSwizzleStore(VectorIntrinsicInParam dest, VectorAxis destIndex, VectorIntrinsicInParam src)
    {
        VectorIntrinsic ret;
        { ret = __vperm(dest, src, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((destIndex) * 4 ) + (INDEX_SRC))]); };
        return ret;
    }

    __forceinline VectorIntrinsic _asmSwizzleStore(VectorIntrinsicInParam dest, VectorAxis destIndex, VectorIntrinsicInParam src, VectorAxis srcIndex)
    {
        VectorIntrinsic ret;
        { ret = __vperm(dest, src, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((destIndex) * 4 ) + (srcIndex))]); };
        return ret;
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
