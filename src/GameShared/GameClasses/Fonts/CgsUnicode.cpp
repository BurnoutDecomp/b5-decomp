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

    // Faithful port of X360 ARTIST 0x82834740 (CgsUnicode.cpp:549 is its one assert line):
    // render liValue as UTF-8 decimal digits with optional thousands separators.
    //
    // The X360 body accumulates the digits LEAST-significant-first into a 176-byte stack
    // scratch and copies them back out in reverse at the end, so the separator run is written
    // reversed into the scratch too (the `for (i = trailing; i >= 0; ...) scratch[++n] = sep[i]`
    // loop). The digit counter stops at 32 (KU_MAX_DIGITS below) -- the console's overflow
    // guard on the scratch. `lu8MinimumDigits` is a MINIMUM: zeroes are emitted ahead of the
    // digits when the value is shorter.
    CgsUtf8* IntToString(CgsUtf8* lpUtf8TargetString, s32 liValue, u8 lu8MinimumDigits,
                         const CgsUtf8* lpUtf8ThousandsSeparator)
    {
        // The console's digit cap (the `if (v9 >= 0x20) goto LABEL_15` early-out).
        const u32 KU_MAX_DIGITS = 32u;

        CgsUtf8* lpOut  = lpUtf8TargetString;
        s32      liRest = liValue;

        if (liValue < 0)
        {
            liRest    = -liValue;
            *lpOut++  = static_cast<CgsUtf8>('-');
        }

        CgsUtf8 lacScratch[176];
        u32     luScratchLen  = 0u;   // bytes written into the scratch (digits + separators)
        u32     luDigitCount  = 0u;   // DIGITS only (what the minimum-digits pad compares to)
        u32     luGroupDigits = 0u;   // digits since the last separator

        for (;;)
        {
            const s32 liNext = liRest / 10;
            lacScratch[luScratchLen] = static_cast<CgsUtf8>((liRest % 10) + '0');

            if (liNext == 0)
            {
                ++luDigitCount;
                ++luScratchLen;
                break;
            }

            if (lpUtf8ThousandsSeparator != 0 && *lpUtf8ThousandsSeparator != 0)
            {
                ++luGroupDigits;
                if (luGroupDigits == 3u)
                {
                    luGroupDigits = 0u;

                    const u32 luTrailing = lUtf8TrailingBytes(*lpUtf8ThousandsSeparator);
                    // (X360 streams "Character supplied is not a valid start UTF8 character
                    // (<byte>)" at CgsUnicode.cpp:549 when the table returns 255.)
                    CGS_ASSERT(luTrailing != 255u,
                               "Character supplied is not a valid start UTF8 character");
                    if (luTrailing <= 3u)
                    {
                        // Written back-to-front so the final reverse copy restores the order.
                        for (s32 liByte = static_cast<s32>(luTrailing); liByte >= 0; --liByte)
                        {
                            ++luScratchLen;
                            lacScratch[luScratchLen] = lpUtf8ThousandsSeparator[liByte];
                        }
                    }
                }
            }

            ++luDigitCount;
            ++luScratchLen;
            liRest = liNext;

            if (luDigitCount >= KU_MAX_DIGITS)
                break;
        }

        for (u32 luPad = lu8MinimumDigits; luPad > luDigitCount; --luPad)
            *lpOut++ = static_cast<CgsUtf8>('0');

        for (u32 luLeft = luScratchLen; luLeft != 0u; )
        {
            --luLeft;
            *lpOut++ = lacScratch[luLeft];
        }

        *lpOut = 0;
        return lpOut;
    }

    // [H1 wave 2026-08-25] Faithful port of X360 ARTIST 0x82834930: render lfValue as UTF-8
    // decimal text -- the integer part through IntToString (sign handled there; the console
    // truncates toward zero), then, when lu8DecimalPlaces > 0, the decimal-point character
    // (lead byte validated through the trailing-bytes table, its continuation bytes copied)
    // followed by the fraction |value - (s32)value| scaled by 10^lu8DecimalPlaces, rendered
    // through IntToString with a NULL separator and NO minimum digits -- so a fraction with
    // leading zeroes prints unpadded, exactly as the console does. The console's power table
    // (flt_820DE4E8[10]) is the plain powers-of-ten run reproduced below. Asserts
    // CgsUnicode.cpp:606 (luDecimalPlaces < 10) and :620 (the lead-byte tripwire, streamed
    // on the console, folded static per convention). lu8MinimumDigits is never forwarded
    // (the console passes literal 0 to both IntToString calls); see the header note.
    CgsUtf8* FloatToString(CgsUtf8* lpUtf8TargetString, f32 lfValue, u8 lu8MinimumDigits,
                           u8 lu8DecimalPlaces, const CgsUtf8* lpUtf8ThousandsSeparator,
                           const CgsUtf8* lpUtf8DecimalPointCharacter)
    {
        (void)lu8MinimumDigits;

        // flt_820DE4E8 -- 10^n for the fraction scale, indexed by lu8DecimalPlaces.
        static const f32 KAF_DECIMAL_SCALE[10] =
        {
            1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f,
            100000.0f, 1000000.0f, 10000000.0f, 100000000.0f, 1000000000.0f,
        };

        f32       lfAbsValue    = lfValue;
        s32       liIntegerPart = static_cast<s32>(lfValue);
        CgsUtf8*  lpOut         = lpUtf8TargetString;

        IntToString(lpUtf8TargetString, liIntegerPart, 0, lpUtf8ThousandsSeparator);

        if (lfAbsValue < 0.0f)
        {
            liIntegerPart = -liIntegerPart;
            lfAbsValue    = -lfAbsValue;
        }

        CGS_ASSERT(lu8DecimalPlaces < 10u, "luDecimalPlaces < 10");   // cpp:606

        const s32 liFraction = static_cast<s32>(
            (lfAbsValue - static_cast<f32>(liIntegerPart)) * KAF_DECIMAL_SCALE[lu8DecimalPlaces]);

        if (lu8DecimalPlaces != 0u)
        {
            while (*lpOut != 0)
                ++lpOut;

            const u32 luTrailing = lUtf8TrailingBytes(*lpUtf8DecimalPointCharacter);
            // (The console streams "Character supplied is not a valid start UTF8 character
            // (<byte>)" at CgsUnicode.cpp:620 when the table answers 255.)
            CGS_ASSERT(luTrailing != 255u,
                       "Character supplied is not a valid start UTF8 character");

            for (u32 luByte = 0; luByte <= luTrailing; ++luByte)
                *lpOut++ = lpUtf8DecimalPointCharacter[luByte];

            lpOut = IntToString(lpOut, liFraction, 0, 0);
        }

        return lpOut;
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

    // [gateui r5] Faithful port of X360 ARTIST 0x828357A0: the CHARACTER length of a UTF-8
    // string. The console validates the string first (its one assert, CgsUnicode.cpp:77), then
    // walks the bytes counting every byte that is NOT a 10xxxxxx continuation byte -- i.e. one
    // count per UTF-8 character however many bytes it occupies.
    s32 StringLength(const CgsUtf8* lpUtf8String)
    {
        CGS_ASSERT(IsValidUtf8String(lpUtf8String), "IsValidUtf8String(lpUtf8String)");   // cpp:77

        s32 liLength = 0;
        for (const CgsUtf8* lpByte = lpUtf8String; *lpByte != 0; ++lpByte)
        {
            if ((*lpByte & 0xC0) != 0x80)
                ++liLength;
        }
        return liLength;
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

    // Faithful port of X360 ARTIST 0x82834448: copy a whole NUL-terminated UTF-8 string into the
    // target (unbounded -- no cap, no null/length asserts; the asm is a bare byte loop). The X360
    // body advances the target register (r3) as it writes each byte and returns it, so the result
    // points at the written NUL terminator (stpcpy semantics), NOT the original target. Reached
    // from UnicodeBuffer::Convert (below), CgsLanguage::LanguageManager::Format*String and
    // BrnNetwork::LoginManagerBase::UpdateDownloadingTOS.
    CgsUtf8* Copy(CgsUtf8* lpUtf8TargetString, const CgsUtf8* lpUtf8SourceString)
    {
        CgsUtf8*       lpDest   = lpUtf8TargetString;
        const CgsUtf8* lpSource = lpUtf8SourceString;

        // X360: load *source; while non-zero, ++source, store the byte, ++dest, reload *source.
        CgsUtf8 lu8Char = *lpSource;
        while (lu8Char != 0)
        {
            ++lpSource;
            *lpDest++ = lu8Char;
            lu8Char = *lpSource;
        }
        *lpDest = 0;
        return lpDest;   // X360 returns the advanced target: a pointer to the written NUL terminator
    }

    // Faithful port of X360 ARTIST 0x828345F0: NUL-terminate lpUtf8String so it occupies at most
    // lnMaxTargetLength bytes without splitting a multi-byte UTF-8 character. Start at the last byte
    // inside the cap (str + lnMaxTargetLength - 1); if that byte is a continuation byte (10xxxxxx),
    // step back until a leading/single byte (or the start of the string), then write the terminator
    // there. A cap of <= 0, or an already-NUL byte at the cap, is a no-op. Returns the target.
    // Reached from BrnGui::GuiHudMessage::GetParam / AddParam.
    CgsUtf8* SafelyTerminate(CgsUtf8* lpUtf8String, s32 lnMaxTargetLength)
    {
        CgsUtf8* lpByte = lpUtf8String + lnMaxTargetLength - 1;   // last byte within the cap
        if (lpByte >= lpUtf8String)
        {
            while (*lpByte != 0)
            {
                if ((*lpByte & 0xC0) != 0x80)   // a leading / single byte: safe to cut here
                {
                    *lpByte = 0;
                    return lpUtf8String;
                }
                --lpByte;                        // a continuation byte: back up one
                if (lpByte < lpUtf8String)
                    return lpUtf8String;
            }
        }
        return lpUtf8String;
}

    // @ 0x82834638 -- uppercasing bounded copy (LanguageManager::FormatText's
    // E_FORMAT_ID_LOOKUP_UPPER branch). Byte loop, capped at lnMaxTargetStringLength-1:
    // plain-ASCII lowercase bytes ('a'..'z', high bit clear) are uppercased in flight;
    // every other byte (including UTF-8 continuation/lead bytes) copies verbatim; the
    // target is always NUL-terminated. Returns the write cursor (the target's NUL) --
    // the X360 r3 at exit.
    CgsUtf8* ToUpperN(CgsUtf8* lpUtf8TargetString, const CgsUtf8* lpUtf8SourceString,
                      s32 lnMaxTargetStringLength)
    {
        CGS_ASSERT(lpUtf8TargetString != 0, "lpUtf8TargetString!= NULL");
        CGS_ASSERT(lpUtf8SourceString != 0, "lpUtf8SourceString!= NULL");
        CGS_ASSERT(lnMaxTargetStringLength > 0, "lnMaxTargetStringLength > 0");

        CgsUtf8* lpWrite = lpUtf8TargetString;
        s32 li = 0;
        for (CgsUtf8 luByte = *lpUtf8SourceString; luByte != 0; luByte = *lpUtf8SourceString)
        {
            if (li >= lnMaxTargetStringLength - 1)
                break;
            *lpWrite = luByte;
            if ((luByte & 0x80u) == 0 && luByte >= 0x61u && luByte <= 0x7Au)
                *lpWrite = static_cast<CgsUtf8>(luByte - 32);
            ++lpUtf8SourceString;
            ++lpWrite;
            ++li;
        }
        *lpWrite = 0;
        return lpWrite;
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

    // Faithful port of X360 ARTIST 0x82834AF0 (CgsUnicode::_Print): copy the source string
    // into the target, substituting each "%<digit>" positional marker with the matching
    // argument string (argument 1 == lppUtf8Arguments[0]); a '%' not followed by a plain
    // ASCII digit -- or one that would not fit -- is copied literally. The output is capped
    // at lnTargetStringSize bytes; when the cap is hit exactly, back up over trailing UTF-8
    // continuation bytes so the terminating NUL lands on a character boundary. Returns the
    // terminator position. Used by the LanguageManager positional formatters (FormatTextV /
    // Obsolete_FormatTextByArray).
    CgsUtf8* _Print(CgsUtf8* lpUtf8TargetString, const CgsUtf8* lpUtf8SourceString,
                    s32 lnTargetStringSize, const CgsUtf8* const* lppUtf8Arguments,
                    u8 luNumArguments)
    {
        CGS_ASSERT(lpUtf8SourceString != 0, "lpUtf8SourceString != NULL");   // cpp:654
        CGS_ASSERT(lpUtf8TargetString != 0, "lpUtf8TargetString != NULL");   // cpp:655
        // The console checks zero (cpp:656) then non-positive (cpp:657) separately; a
        // zero size fires both, a negative one only the second -- reproduced exactly.
        CGS_ASSERT(lnTargetStringSize != 0, "lnTargetStringSize != NULL");
        CGS_ASSERT(lnTargetStringSize > 0, "lnTargetStringSize > 0");

        const CgsUtf8* lpChar    = lpUtf8SourceString;
        CgsUtf8*       lpOut     = lpUtf8TargetString;
        s32            lnWritten = 0;
        bool           lbAtCap   = (lnWritten == lnTargetStringSize);

        if (*lpChar != 0)
        {
            for (;;)
            {
                lbAtCap = (lnWritten == lnTargetStringSize);
                if (lnWritten >= lnTargetStringSize)
                    break;

                const CgsUtf8 lu8Char = *lpChar;
                const CgsUtf8 lu8Next = lpChar[1];
                if (lu8Char != '%' || lnWritten + 1 >= lnTargetStringSize ||
                    (lu8Next & 0x80) != 0 || lu8Next < '0' || lu8Next > '9')
                {
                    // Literal byte (including a '%' with no usable digit after it).
                    *lpOut++ = lu8Char;
                    ++lnWritten;
                }
                else
                {
                    ++lpChar;   // now at the argument-selecting digit
                    // cpp:677 -- the X360 streams "Only <count> argument(s) supplied for
                    // string <source>"; folded static per convention.
                    CGS_ASSERT(static_cast<u8>(*lpChar - '0') <= luNumArguments,
                               "Only <n> argument(s) supplied for string");

                    // %1 selects lppUtf8Arguments[0] (the console indexes digit-1).
                    const CgsUtf8* lpArg = lppUtf8Arguments[(*lpChar - '0') - 1];
                    while (*lpArg != 0)
                    {
                        if (lnWritten >= lnTargetStringSize)
                            break;
                        *lpOut++ = *lpArg++;
                        ++lnWritten;
                    }
                }

                ++lpChar;
                if (*lpChar == 0)
                {
                    lbAtCap = (lnWritten == lnTargetStringSize);
                    break;
                }
            }
        }

        // Exactly full: rewind to the last character-leading byte so the NUL fits and
        // never splits a multi-byte character.
        if (lbAtCap)
        {
            do
            {
                --lpOut;
            }
            while ((*lpOut & 0xC0) == 0x80);
        }

        *lpOut = 0;
        return lpOut;
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

    // Explicit instantiations of the Print<...> string formatters the build actually emits.
    // The template body lives in CgsUnicode.h (it stages each argument through a scratch
    // UnicodeBuffer then forwards to _Print); these pin the out-of-line copies the X360 keeps
    // at 0x824490E8 (2 args) and 0x82449150 (3 args), each over three UTF-8 string parameters
    // (const CgsUtf8* == unsigned char const *). Only caller: BrnGui::InGameMessageRenderer::
    // InGameMessage::SetupMessage.
    template CgsUtf8* Print<const CgsUtf8*, const CgsUtf8*>(
        const CgsUtf8*, CgsUtf8*, s32, const CgsUtf8* const&, const CgsUtf8* const&);

    template CgsUtf8* Print<const CgsUtf8*, const CgsUtf8*, const CgsUtf8*>(
        const CgsUtf8*, CgsUtf8*, s32,
        const CgsUtf8* const&, const CgsUtf8* const&, const CgsUtf8* const&);
}
