#pragma once

#include "SDKs/EATech/include/rw/math/vpu/maskscalar.h"

namespace rw { namespace math { namespace vpu {
inline MaskScalar::MaskScalar() {}
inline MaskScalar::MaskScalar(VectorIntrinsicInParam value) : mV(value) {}
inline MaskScalar::MaskScalar(bool value)
{
    mV = value ? __vspltisw(-1) : __vspltisw(0);
}
inline MaskScalar::MaskScalar(unsigned int value)
{
    __declspec(align(16)) unsigned int words[4] = {value, value, value, value};
    mV = __lvx(words, 0);
}
inline bool MaskScalar::GetBool() const
{
    // ARTIST821F0D78..98: not(all four FP lanes compare equal to zero).
    unsigned int condition;
    __vcmpeqfpR(mV, __vspltisw(0), &condition);
    return (condition & 0x80u) == 0;
}
inline unsigned int MaskScalar::ToUInt32() const
{
    __declspec(align(16)) unsigned int words[4];
    __stvx(mV, words, 0);
    return words[0];
}
inline void MaskScalar::SetTrue() { mV = __vspltisw(-1); }
inline void MaskScalar::SetFalse() { mV = __vspltisw(0); }
}}}
