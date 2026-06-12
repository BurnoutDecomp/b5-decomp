#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828155E0
//   (CgsContainers::CgsHash16::CalculateHash)
//
// Behaviour-faithful to the X360 pseudocode/asm.
// This is a 16-bit hash (CRC-16 variant) using a 256-entry table.
//
// If the input length count <= 0, the function early-outs and returns the
// initial hash value 0xFFFFFFFF (as in the X360 register r3 initialization).
//
// Table note: the table is regenerated here using the polynomial 0x8810,
// which matches the table observed in the X360 binary at 0x82F31500.
//
// Signedness note: the asm sign-extends each byte (extsb) before the xor, and
// does NOT mask the index down to 8 bits, so a byte >= 0x80 would index outside
// the 256-entry table. We keep the index in range.

namespace CgsContainers
{
    namespace CgsHash16
    {
        namespace
        {
            struct Crc16Table
            {
                u32 entry[256];

                constexpr Crc16Table() : entry{}
                {
                    for (u32 n = 0; n < 256; ++n)
                    {
                        u32 c = n;
                        for (int k = 0; k < 8; ++k)
                            c = (c & 1u) ? (0x8810u ^ (c >> 1)) : (c >> 1);
                        entry[n] = c & 0xFFFFu;
                    }
                }
            };

            constexpr Crc16Table kCrc16{};
        }

        int CalculateHash(char* a1, int a2)
        {
            u32 result = 0xFFFFFFFFu;
            for (int i = 0; i < a2; ++i)
            {
                const s32 b = static_cast<s8>(a1[i]);            // extsb: char is signed
                const u32 idx = (result & 0xFFu) ^ static_cast<u32>(b);
                result = kCrc16.entry[idx & 0xFFu] ^ ((result >> 8) & 0xFFu);
            }
            return static_cast<int>(result);
        }
    }
}
