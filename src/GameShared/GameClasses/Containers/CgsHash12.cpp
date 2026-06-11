#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82815570
//   (CgsContainers::CgsHash12::CalculateHash)
//
// Behaviour-faithful to the X360 pseudocode/asm.
// This is a 12-bit hash (CRC-12 variant) processing 4 bits (a nibble) at a time.
// It uses a 16-entry table at VA 0x82F314C0.
//
// If the input length count <= 0, the function early-outs and returns the
// initial hash value 4095 (0xFFF).

namespace CgsContainers
{
    namespace CgsHash12
    {
        namespace
        {
            // Extracted from BURNOUT_X360_ARTIST.XEX @ 0x82F314C0
            constexpr u32 kHash12Table[16] = {
                0,      // 0x000
                641,    // 0x281
                1282,   // 0x502
                1923,   // 0x783
                2564,   // 0xA04
                2181,   // 0x885
                3846,   // 0xF06
                3463,   // 0xD87
                3079,   // 0xC07
                3718,   // 0xE86
                2309,   // 0x905
                2948,   // 0xB84
                1539,   // 0x603
                1154,   // 0x482
                769,    // 0x301
                384     // 0x180
            };
        }

        unsigned int CalculateHash(char* a1, int a2)
        {
            u32 result = 4095; // 0xFFF
            if (a2 <= 0)
            {
                return result;
            }

            for (int i = 0; i < a2; ++i)
            {
                const s8 val = static_cast<s8>(a1[i]); // extsb: char is signed
                
                // First step: process low nibble
                const u32 idx1 = (static_cast<u32>(val) ^ result) & 0xFu;
                const u32 t1 = kHash12Table[idx1];
                
                // Second step: process high nibble
                const s8 high_nibble = static_cast<s8>(val >> 4); // arithmetic shift
                const u32 t1_xor = t1 ^ (result >> 4);
                const u32 idx2 = (static_cast<u32>(high_nibble) ^ t1_xor) & 0xFu;
                const u32 t2 = kHash12Table[idx2];
                
                result = t2 ^ (t1_xor >> 4);
            }

            return result;
        }
    }
}
