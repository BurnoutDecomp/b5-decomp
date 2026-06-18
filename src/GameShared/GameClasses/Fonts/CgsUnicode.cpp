#include "GameShared/GameClasses/Fonts/CgsUnicode.h"

// CgsUnicode -- UTF-8 helpers. Faithful ports of the X360 ARTIST bodies:
//   ConvertUtf8CharToUtf16Char  0x827E6B08
//   IncrementUtf8Pointer        0x827E6A28
// Both index the X360 table byte_820D1710 (trailing-byte count per lead byte, 255 = invalid).
// That table is a UTF-8-spec constant reproduced here as lUtf8TrailingBytes (the data table is
// not in the function export, but its values are fully determined by the spec). The
// dword_820D1810 offset table the converter subtracts is likewise the standard UTF-8 offsets.
// Debug-only validity asserts (CgsUnicode.h:914/955/958/959/990) are documented, not called.

namespace CgsUnicode
{
    namespace
    {
        // = byte_820D1710[lead]: number of trailing (continuation) bytes for a UTF-8 lead byte,
        // or 255 if the byte is not a valid lead (a continuation byte, or a >4-byte lead).
        u32 lUtf8TrailingBytes(u8 lu8Lead)
        {
            if (lu8Lead < 0x80u) return 0u;    // 0xxxxxxx  ASCII
            if (lu8Lead < 0xC0u) return 255u;  // 10xxxxxx  continuation byte (invalid lead)
            if (lu8Lead < 0xE0u) return 1u;    // 110xxxxx
            if (lu8Lead < 0xF0u) return 2u;    // 1110xxxx
            if (lu8Lead < 0xF8u) return 3u;    // 11110xxx
            return 255u;                        // invalid
        }
    }

    // Faithful port of X360 0x827E6A28: advance past one UTF-8 character (lead + trailing bytes).
    const u8* IncrementUtf8Pointer(const u8* lpUtf8Char)
    {
        // (X360 asserts byte_820D1710[*p] != 255, CgsUnicode.h:914.)
        const u32 luTrailing = lUtf8TrailingBytes(*lpUtf8Char);
        // X360 returns p + trailing + 1; on an invalid lead (255) that would over-run by 256, so
        // advance a single byte instead to stay in bounds.
        if (luTrailing > 3u)
            return lpUtf8Char + 1;
        return lpUtf8Char + luTrailing + 1u;
    }

    // Faithful port of X360 0x827E6B08: decode one UTF-8 character to a UTF-16 code unit.
    u16 ConvertUtf8CharToUtf16Char(const u8* lpUtf8Char)
    {
        // dword_820D1810: the offset removed after accumulating the bytes (folds out the lead/
        // continuation high-bit markers). Indexed by the trailing-byte count.
        static const u32 skauOffsetsFromUtf8[4] =
        {
            0x00000000u, 0x00003080u, 0x000E2080u, 0x03C82080u
        };

        const u8* lpByte = lpUtf8Char;
        const u32 luExtraBytes = lUtf8TrailingBytes(*lpByte);

        // (X360 asserts: IsValidUtf8Character; luExtraBytes != 255; luExtraBytes < 4.) Guard the
        // invalid lead rather than indexing the offset table out of range as the X360 release would.
        if (luExtraBytes > 3u)
            return 0u;

        // Accumulate (luExtraBytes + 1) bytes, shifting 6 bits per step (X360 LABEL_9..LABEL_11).
        u32 luUtf32 = 0u;
        switch (luExtraBytes)
        {
        case 3: luUtf32 += *lpByte++; luUtf32 <<= 6; // fallthrough
        case 2: luUtf32 += *lpByte++; luUtf32 <<= 6; // fallthrough
        case 1: luUtf32 += *lpByte++; luUtf32 <<= 6; // fallthrough
        case 0: luUtf32 += *lpByte;
        }
        luUtf32 -= skauOffsetsFromUtf8[luExtraBytes];

        // (X360 asserts luUtf32 <= KUTF32_MAX_UTF16 (0xFFFF).)
        return static_cast<u16>(luUtf32);
    }
}
