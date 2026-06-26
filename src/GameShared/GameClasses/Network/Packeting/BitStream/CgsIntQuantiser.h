#pragma once

#include "types.hpp"

// CgsNetwork::IntQuantiser
//
// Canonical class home for the bounded-integer bit quantiser. A value in the
// inclusive range [liMinValue, liMaxValue] is packed as the unsigned offset
// (value - min) into the smallest number of bits that can hold the range; the
// inverse adds the offset back to the minimum and clamps. All four members are
// static (CgsIntQuantiser.h:35 in the DWARF).
//
//   Pack @ 0x82871568 -- clamp value to [min,max], emit (clamped-min) and the bit
//                       width GetNumBits(min,max) reports.
//   UnPack @ 0x82871710 -- *value = min + packed, then clamp to [min,max].
//                       (Reconstructed store-for-store from the X360 asm.)
//   GetNumBits       -- bit width for the range [min,max] = bit_width(max-min);
//                       0 when max == min. (Inlined @0x828800F0.)
//   PackAndUnPackTest-- round-trip self-test (not yet homed).

namespace CgsNetwork
{
    struct IntQuantiser
    {
        static void Pack(s32 liValue, s32 liMinValue, s32 liMaxValue,
                         u32* lpuPackedValue, s32* lpiNumBitsUsed);
        static void UnPack(s32* lpiValue, s32 liMinValue, s32 liMaxValue,
                           u32 luPackedValue);
        static s32  GetNumBits(s32 liMinValue, s32 liMaxValue);
        static s32  PackAndUnPackTest(s32 liValue, s32 liMinValue, s32 liMaxValue);
    };
}
