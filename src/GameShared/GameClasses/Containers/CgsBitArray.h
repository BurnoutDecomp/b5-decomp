#pragma once

#include "types.hpp"

namespace CgsContainers
{
// Reconstructed from the DecFIGS DWARF (CgsBitArray.h). A fixed-capacity bit set
// whose storage is an array of 64-bit fields (one field per 64 bits, rounded up).
// The storage member fixes the type's size so the container can be embedded by
// value. The bit-manipulation methods are header-inline because the X360 build
// inlines every instantiation at its call site (e.g. IsBitSet/UnSetBit folded
// into StreamedVaultAllocator::GetSlotMemory/ReleaseSlot); the semantics here
// match that inlined bit math (field = index / 64, bit = index % 64).
//
// Bounds asserts that the DWARF shows guarding these methods (CgsBitArray.h:203,
// :241) are emitted at the call sites by callers that own the CgsDev::Assert API,
// so this header stays free of the assert-system dependency.
template <u32 tuNumBits>
class BitArray
{
public:
    static const u32 kuNumberOfBitsInBitField = 64;
    static const u32 kuNumberOfBitFields =
        (tuNumBits + kuNumberOfBitsInBitField - 1) / kuNumberOfBitsInBitField;
    static const u32 kuBitsInBitFieldMask = kuNumberOfBitsInBitField - 1;
    static const s32 KI_INVALID_BITINDEX = -1;

    bool IsBitSet(u32 luIndex) const
    {
        const u32 luField = luIndex / kuNumberOfBitsInBitField;
        const u64 lu64Mask = (u64)1 << (luIndex & kuBitsInBitFieldMask);
        return (maxBits[luField] & lu64Mask) != 0;
    }

    void SetBit(u32 luIndex)
    {
        const u32 luField = luIndex / kuNumberOfBitsInBitField;
        const u64 lu64Mask = (u64)1 << (luIndex & kuBitsInBitFieldMask);
        maxBits[luField] |= lu64Mask;
    }

    void UnSetBit(u32 luIndex)
    {
        const u32 luField = luIndex / kuNumberOfBitsInBitField;
        const u64 lu64Mask = (u64)1 << (luIndex & kuBitsInBitFieldMask);
        maxBits[luField] &= ~lu64Mask;
    }

    u32 GetCapacity() const
    {
        return tuNumBits;
    }

private:
    u64 maxBits[kuNumberOfBitFields];
};
}
