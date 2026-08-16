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
// VA 0x82F310C0 (`dword_82F310C0`). It is regenerated here from the standard
// reflected CRC-32 polynomial 0xEDB88320.
//
// ⭐ NO LONGER AN ASSUMPTION (2026-08-16, tutorial-ticker leg). The old note said the
// data section "is not present in the available IDA exports" and asked for the diff to be
// done if the table were ever extracted. It has been: read straight out of the unpacked
// X360 image and compared word-for-word against the poly-0xEDB88320 table generated below
// -- 256 of 256 entries identical, 0 mismatches. The polynomial is confirmed, and with it
// every language-string lookup that hashes through here (LanguageManager::FindString ->
// CalculateHash -> FindStringByHash) is resolving on the console's own hash.
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

        // Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828154F0
        //   (CgsContainers::CgsHash::CalculatePathHash)
        //
        // Case- and separator-normalising path hash. NUL-terminated input (no
        // length arg); the same reflected CRC-32 inner loop as CalculateHash over
        // the SAME .rdata table (dword_82F310C0), but each character is normalised
        // first so file paths hash identically regardless of case or slash style:
        //   * lowercase a-z (0x61..0x7A) are upper-cased  (byte -= 0x20)   -- the
        //     asm compares the SIGN-EXTENDED byte, so bytes >= 0x80 skip this.
        //   * backslash '\\' (0x5C) is folded to forward slash '/' (0x2F).
        // Then result = T[(result & 0xFF) ^ ch] ^ (result >> 8), init 0xFFFFFFFF,
        // NO final inversion. An empty string returns 0xFFFFFFFF (the `beqlr`
        // early-out on the first byte being NUL).
        unsigned int CalculatePathHash(unsigned char* a1)
        {
            u32 result = 0xFFFFFFFFu;
            for (const unsigned char* p = a1; *p != 0; ++p)
            {
                s32 ch = static_cast<s8>(*p);                    // extsb: signed byte
                if (ch >= 0x61 && ch <= 0x7A)                    // 'a'..'z' -> upper
                    ch -= 0x20;
                if (ch == 0x5C)                                  // '\\' -> '/'
                    ch = 0x2F;
                const u32 idx = (result & 0xFFu) ^ static_cast<u32>(ch);
                result = kCrc32.entry[idx & 0xFFu] ^ (result >> 8);
            }
            return result;
        }
    }
}
