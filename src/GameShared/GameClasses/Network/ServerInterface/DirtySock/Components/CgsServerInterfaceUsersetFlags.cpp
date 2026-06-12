#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8287C7C0
//   (CgsNetwork::ConvertUsersetFlags)
//
// Maps a packed userset-flag word (lxFlags) into the engine's flag representation
// by consulting six static mapping tables. For each table, if every bit of that
// table's "mask" element is present in the input, the table's per-index value is
// OR'd into the result.
//
//   half = (cntlzw(liIndex) >> 3) & 4;   // selects mask element 0 or 1 (byte 0/4)
//   for each table T in {T0..T5}:
//       if ((T[half] & lxFlags) == T[half])  result |= T[liIndex];
//   return result;
//
// DATA CAVEAT: the six tables (dword_820E953C, _9544, _954C, _9554, _955C, _9564)
// live in .rdata and are *unvalued* in the function-only IDA export, so their
// contents cannot be recovered here. They are declared `extern` (data-stub) and
// must be populated by a later .rdata-recovery pass; the control/bit logic below
// is faithful to the pseudocode. The first table's branch is written as an
// assignment in the pseudocode (result starts at 0) — equivalent to the OR used
// for the rest.

namespace CgsNetwork
{
    // Six flag-mapping tables (.rdata, values not in the export). Each is indexed
    // both by a 0/1 "half" selector (mask element) and by the flag index.
    extern const u32 gaUsersetFlagTable0[];   // dword_820E953C
    extern const u32 gaUsersetFlagTable1[];   // dword_820E9544
    extern const u32 gaUsersetFlagTable2[];   // dword_820E954C
    extern const u32 gaUsersetFlagTable3[];   // dword_820E9554
    extern const u32 gaUsersetFlagTable4[];   // dword_820E955C
    extern const u32 gaUsersetFlagTable5[];   // dword_820E9564

    namespace
    {
        // count-leading-zeros of a 32-bit word (PPC cntlzw), portable.
        inline u32 CountLeadingZeros32(u32 luValue)
        {
            if (luValue == 0)
                return 32;
            u32 luCount = 0;
            while ((luValue & 0x80000000u) == 0)
            {
                ++luCount;
                luValue <<= 1;
            }
            return luCount;
        }
    }

    int ConvertUsersetFlags(int lxFlags, unsigned int luIndex)
    {
        // Mask element index, 0 or 4. The pseudocode reads the mask via
        // `*(table + v2)` where v2 = (cntlzw(index) >> 3) & 4 — element-indexed
        // (the symbol is a dword), so the index stays {0,4}, not collapsed to {0,1}.
        const u32 luMaskIndex = (CountLeadingZeros32(luIndex) >> 3) & 4u;

        const u32* const lapTables[6] =
        {
            gaUsersetFlagTable0, gaUsersetFlagTable1, gaUsersetFlagTable2,
            gaUsersetFlagTable3, gaUsersetFlagTable4, gaUsersetFlagTable5
        };

        int liResult = 0;
        for (int liTable = 0; liTable < 6; ++liTable)
        {
            const u32 luMask = lapTables[liTable][luMaskIndex];
            if ((luMask & static_cast<u32>(lxFlags)) == luMask)
                liResult |= static_cast<int>(lapTables[liTable][luIndex]);
        }
        return liResult;
    }
}
