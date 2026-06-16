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

    // ===== Added for the CarCheckpointData TU (BitArray<16>) =====
    // The four generic methods below are the EXACT methods the Feb-2007 reference template
    // declares (DWARF BitArray<16u> at CgsBitArray.h:4482/:4485/:4496/:4502 attests
    // UnSetAll/IsZero/GetFirstNonZeroBit/GetNextNonZeroBit) and that CarCheckpointData's four
    // X360 bodies inline. Header-inline, matching the X360 inlined bit math; zero-risk additive
    // (no layout change, no behaviour change to existing IsBitSet/SetBit/UnSetBit users).

    // Clear every bit (the X360 `*this = 0` of the single-field BitArray<16>).
    void UnSetAll()
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            maxBits[luField] = 0;
        }
    }

    // True iff no bit is set (the popcount==0 guard).
    bool IsZero() const
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            if (maxBits[luField] != 0)
            {
                return false;
            }
        }
        return true;
    }

    // Index of the lowest set bit, or KI_INVALID_BITINDEX(-1) if none.
    s32 GetFirstNonZeroBit() const
    {
        for (u32 luField = 0; luField < kuNumberOfBitFields; ++luField)
        {
            if (maxBits[luField] != 0)
            {
                const s32 liBit = static_cast<s32>(luField) * static_cast<s32>(kuNumberOfBitsInBitField)
                                + GetZeroBitInInt(maxBits[luField]);
                if (static_cast<u32>(liBit) >= tuNumBits)
                {
                    return KI_INVALID_BITINDEX;
                }
                return liBit;
            }
        }
        return KI_INVALID_BITINDEX;
    }

    // Index of the lowest set bit with index strictly greater than liAfter, or -1 if none.
    s32 GetNextNonZeroBit(s32 liAfter) const
    {
        for (s32 liBit = liAfter + 1; static_cast<u32>(liBit) < tuNumBits; ++liBit)
        {
            if (IsBitSet(static_cast<u32>(liBit)))
            {
                return liBit;
            }
        }
        return KI_INVALID_BITINDEX;
    }

private:
    // Index of the lowest set bit within a single 64-bit field (the X360 (field*64 -
    // clz64(field & -field) + 63) lowest-set-bit idiom, value-identical to a count-trailing-zeros).
    static s32 GetZeroBitInInt(u64 lu64Field)
    {
        s32 liBit = 0;
        while ((lu64Field & 1) == 0)
        {
            lu64Field >>= 1;
            ++liBit;
        }
        return liBit;
    }

    u64 maxBits[kuNumberOfBitFields];
};
}
