#pragma once

#include "types.hpp"

namespace CgsContainers
{
// Reconstructed from the DecFIGS DWARF (CgsBitArray.h). A fixed-capacity bit set
// whose storage is an array of 64-bit fields (one field per 64 bits, rounded up).
// Only the storage member is modelled here — it fixes the type's size so the
// container can be embedded by value; the bit-manipulation methods live in the
// BitArray TUs and are added when those are reconstructed.
template <u32 tuNumBits>
class BitArray
{
public:
    static const u32 kuNumberOfBitsInBitField = 64;
    static const u32 kuNumberOfBitFields =
        (tuNumBits + kuNumberOfBitsInBitField - 1) / kuNumberOfBitsInBitField;

private:
    u64 maxBits[kuNumberOfBitFields];
};
}
