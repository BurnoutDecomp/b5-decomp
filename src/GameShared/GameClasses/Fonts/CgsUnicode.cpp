#include "GameShared/GameClasses/Fonts/CgsUnicode.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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

    // Faithful port of X360 ARTIST 0x82834D10: validate one UTF-8 character.
    //   - a NUL lead byte is "valid" (the empty-string terminator);
    //   - byte_820DE3C8[lead] gives the trailing-byte count (0..3) or 255 (not a valid lead);
    //   - a valid lead byte must NOT itself be a continuation byte (10xxxxxx) and must be
    //     followed by exactly that many continuation bytes.
    // (The X360 build emits a debug log line on each failure when the message filter bit is
    // set; that diagnostic print is debug-only formatting and is not reproduced here -- the
    // observable result, 0/1, is.)
    bool IsValidUtf8Character(const u8* lpUtf8Char)
    {
        const u8 lu8Lead = *lpUtf8Char;
        if (lu8Lead == 0)
            return true;

        const u32 luTrailing = lUtf8TrailingBytes(lu8Lead);
        if (luTrailing == 255u)
            return false;                       // not a valid start byte (continuation / >4-byte)

        // luTrailing <= 3 and the lead is not a 10xxxxxx continuation byte: walk the trailing
        // bytes and require each to be a 10xxxxxx continuation. (X360: luTrailing > extra count
        // -> too many trailing bytes -> invalid; fewer -> the loop ends early and is valid.)
        // luSeen counts the CONTINUATION bytes seen (starts at 0): a char is invalid only when
        // MORE than luTrailing continuations follow. (The prior luSeen=1 seed counted the lead
        // itself and rejected EVERY valid multi-byte character -- e.g. the 2-byte (C2 A9)
        // copyright sign failed on its first continuation byte. Off-by-one, fixed 2026-07-04.)
        const u8* lpByte = lpUtf8Char + 1;
        u32 luSeen = 0u;
        while ((*lpByte & 0xC0) == 0x80)
        {
            ++lpByte;
            if (++luSeen > luTrailing)
                return false;                   // more continuation bytes than the lead allows
        }
        return true;
    }

    // Faithful port of X360 ARTIST 0x82834EA0: a string is valid when it is empty or every
    // character in it is a valid UTF-8 character.
    bool IsValidUtf8String(const u8* lpUtf8String)
    {
        const u8* lpByte = lpUtf8String;
        if (*lpByte == 0)
            return true;

        while (IsValidUtf8Character(lpByte))
        {
            lpByte = IncrementUtf8Pointer(lpByte);
            if (*lpByte == 0)
                return true;
        }
        return false;
    }

    // Faithful port of X360 ARTIST 0x82834478: copy at most lnMaxTargetStringLength bytes of
    // the source string into the target, always NUL-terminating, and NEVER cutting a multi-byte
    // UTF-8 character: when the byte cap is reached, back up over any trailing continuation
    // bytes (10xxxxxx) so the truncation lands on a character boundary.
    CgsUtf8* CopyN(CgsUtf8* lpUtf8TargetString, const CgsUtf8* lpUtf8SourceString,
                   s32 lnMaxTargetStringLength)
    {
        CGS_ASSERT(lpUtf8TargetString != 0, "lpUtf8TargetString!= NULL");
        CGS_ASSERT(lpUtf8SourceString != 0, "lpUtf8SourceString!= NULL");
        CGS_ASSERT(lnMaxTargetStringLength > 0, "lnMaxTargetStringLength > 0");

        const CgsUtf8* lpSource = lpUtf8SourceString;
        CgsUtf8*       lpDest    = lpUtf8TargetString;
        s32            liCopied  = 0;

        // Copy bytes until the source ends or the cap is reached (X360: the loop breaks at
        // v8 == a3 -- i.e. exactly lnMaxTargetStringLength bytes written -- or at the source NUL).
        bool lbHitCap = false;
        while (*lpSource != 0)
        {
            if (liCopied >= lnMaxTargetStringLength)
            {
                lbHitCap = true;
                break;
            }
            *lpDest++ = *lpSource++;
            ++liCopied;
        }
        if (liCopied == lnMaxTargetStringLength)
            lbHitCap = true;

        // If the cap was hit, the last byte written may be mid-character: rewind over any
        // trailing continuation bytes so the NUL lands on a leading byte. (X360 asserts it
        // found a leading byte before the start of the buffer; that under-run is impossible
        // for any well-formed UTF-8 input shorter than the cap, so the guard is documentary.)
        if (lbHitCap)
        {
            do
            {
                --lpDest;
            }
            while ((*lpDest & 0xC0) == 0x80);

            CGS_ASSERT(lpDest >= lpUtf8TargetString,
                       "Could not find a leading byte in string.");
        }

        *lpDest = 0;
        return lpUtf8TargetString;
    }

    // Byte length of a NUL-terminated UTF-8 string (X360 CgsUnicode.cpp:108). Counts the
    // bytes preceding the terminator -- the LanguageResourceType string-table descriptor size.
    u32 ByteLength(const u8* lpUtf8String)
    {
        const u8* lpByte = lpUtf8String;
        while (*lpByte != 0u)
            ++lpByte;
        return static_cast<u32>(lpByte - lpUtf8String);
    }

    // Faithful port of X360 ARTIST 0x824EA850 (DWARF CgsUnicode.h:828 `bool IsSingleByteUtf8(
    // const CgsUtf8*)`; assigned symbol name IsSingleByteOnlyUtf8String). True iff the string is
    // empty or every byte up to the NUL terminator is a single-byte (high-bit-clear / ASCII) UTF-8
    // character: scans forward, returns true on the terminator, false on the first byte with bit 7
    // set (a multi-byte lead or continuation). The scan is capped at KI_MAX_CHARACTERS_TO_SEARCH
    // (2048); running past the cap without finding a NUL fires an assert (X360 CgsUnicode.h:879)
    // and returns false. Only caller: BrnGui::GuiModule::Update.
    bool IsSingleByteOnlyUtf8String(const CgsUtf8* lpUtf8String)
    {
        const s32 KI_MAX_CHARACTERS_TO_SEARCH = 2048;

        s32 liIndex = 0;
        while (true)
        {
            const CgsUtf8 lu8Char = lpUtf8String[liIndex];
            if (lu8Char == 0)
                return true;                    // reached the terminator: all bytes were single-byte
            if ((lu8Char & 0x80) != 0)
                return false;                   // a multi-byte lead / continuation byte
            if (++liIndex >= KI_MAX_CHARACTERS_TO_SEARCH)
            {
                CGS_ASSERT(false,
                           "\n\nRan through 0x%X Utf8 characters without finding a null terminator.\n"
                           "If this is correct the change the KI_MAX_CHARACTERS_TO_SEARCH variable to a larger number\n\n\n");
                return false;
            }
        }
    }

    // Faithful port of X360 0x82443380 (CgsUnicode.h:499, the release out-of-line body of the
    // UnicodeBuffer::Convert(const CgsUtf8*) inline). Stage a parameter string into this buffer
    // (maBuffer, the first instance member -> &maBuffer == this) via CgsUnicode::Copy, first
    // asserting the source fits the 256-byte parameter buffer. The X360 build streamed the runtime
    // length and the 256 cap into the assert via a StrStream; that collapses to the single base
    // assert here (streamed length/" < "/256/"\n" dropped per project assert rule). Returns void.
    void UnicodeBuffer::Convert(const CgsUtf8* lUtf8String)
    {
        // Byte length of the source excluding its NUL terminator (X360 inlines this walk).
        const CgsUtf8* lpByte = lUtf8String;
        while (*lpByte != 0)
            ++lpByte;
        const s32 lnLength = static_cast<s32>(lpByte - lUtf8String);

        CGS_ASSERT(lnLength < 256,
                   "Parameter for a string localisation is too large to fit in the parameter buffer : ");

        Copy(maBuffer, lUtf8String);
    }
}
