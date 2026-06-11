#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828154A0
//   (CgsContainers::CgsHash::CalculateHash)
//
// Behaviour-faithful to the X360 pseudocode/asm. This is the textbook
// table-driven, reflected CRC-32 byte loop:
//   crc = 0xFFFFFFFF;  for each byte: crc = T[(crc ^ byte) & 0xFF] ^ (crc >> 8);
// It returns the raw register WITHOUT the conventional final ~crc inversion
// (the asm ends `xor r3, r8, r7; blr` — no trailing xori), and an empty/empty
// (count <= 0) input returns 0xFFFFFFFF unchanged (the `blelr` early-out).
//
// Table note: the original indexes a precomputed 256-word table in .rdata at
// VA 0x82F310C0 (`dword_82F310C0`). That data section is not present in the
// available IDA exports, so the table is regenerated here from the standard
// reflected CRC-32 polynomial 0xEDB88320 — the only polynomial that produces
// this canonical algorithm shape. If the binary's table is ever extracted it
// should be diffed against this to confirm the polynomial.
//
// Signedness note: the asm sign-extends each byte (`extsb`) before the xor, and
// does NOT mask the index down to 8 bits, so a byte >= 0x80 would index outside
// the 256-entry table — a latent out-of-bounds the engine never hits because
// callers hash ASCII identifiers (GUI labels, movie-clip / text-field names).
// We mirror the signed extension and keep the index in range.

namespace CgsContainers
{
    namespace CgsHash
    {
        namespace
        {
            // Standard reflected CRC-32 table (poly 0xEDB88320), built at compile time.
            struct Crc32Table
            {
                u32 entry[256];

                constexpr Crc32Table() : entry{}
                {
                    for (u32 n = 0; n < 256; ++n)
                    {
                        u32 c = n;
                        for (int k = 0; k < 8; ++k)
                            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                        entry[n] = c;
                    }
                }
            };

            constexpr Crc32Table kCrc32{};
        }

        unsigned int CalculateHash(char* a1, int a2)
        {
            u32 result = 0xFFFFFFFFu;
            for (int i = 0; i < a2; ++i)
            {
                const s32 b = static_cast<s8>(a1[i]);            // extsb: char is signed
                const u32 idx = (result & 0xFFu) ^ static_cast<u32>(b);
                result = kCrc32.entry[idx & 0xFFu] ^ (result >> 8);
            }
            return result;
        }
    }
}
