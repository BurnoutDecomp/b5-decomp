#include "GameShared/GameClasses/Fonts/CgsUnicode.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "types.hpp"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#define CGS_ARTIST_UNICODE_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\fonts\\CgsUnicode.cpp"
#define CGS_ARTIST_UNICODE_H_FILE "..\\..\\..\\GameShared\\GameClasses\\Fonts/CgsUnicode.h"

namespace CgsUnicode
{
    namespace
    {
        inline bool IsSingleByteUtf8(const CgsUtf8* lpUtf8Char)
        {
            return (*lpUtf8Char & 0x80) == 0;
        }

        // The original lead-byte table returns the continuation count or 255.
        const u8 KAU8_UTF8_TRAILING_BYTES[256] =
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
            3, 3, 3, 3, 3, 3, 3, 3, 255, 255, 255, 255, 255, 255, 255, 255,
        };

        inline u8 lUtf8TrailingBytes(u8 lu8Lead)
        {
            return KAU8_UTF8_TRAILING_BYTES[lu8Lead];
        }
    }

    // ARTIST82835828..5AE8; DecFIGS declares (const CgsUtf8*,CgsUtf16*) -> CgsUtf16*.
    // The original converter accepts one BMP code unit per UTF8 character; it asserts
    // above0xffff, then stores the low16 bits if the assertion continues. No surrogate
    // conversion, replacement, bound or return-on-error is added. The return is the NUL.
    CgsUtf16* ConvertUtf8ToUtf16(const CgsUtf8* lpUtf8String, CgsUtf16* lpUtf16String)
    {
        if (!IsValidUtf8String(lpUtf8String))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer,
                                       CgsDev::Assert::giMessageBufferSize);
            lMessage << "Invalid UTF8 Character " << static_cast<u32>(*lpUtf8String) << " at "
                     << CgsDev::E_PRINTMODE_HEX << static_cast<u64>(reinterpret_cast<uintptr_t>(lpUtf8String));
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(), CGS_ASSERT_LOCATION(CGS_ARTIST_UNICODE_CPP_FILE, 802));
            CgsDev::Assert::EndAssert();
        }
        // ARTIST dword_820DE4C8; trailing-byte classification uses the same
        // byte values as the existing helper (attested at byte_820DE3C8).
        const u32 lauUtf8Offsets[4] = { 0, 0x00003080u, 0x000e2080u, 0x03c82080u };
        while (*lpUtf8String != 0)
        {
            u32 luChar = 0;
            const u32 luExtraBytes = lUtf8TrailingBytes(*lpUtf8String);
            if (luExtraBytes == 255)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer,
                                           CgsDev::Assert::giMessageBufferSize);
                lMessage << "Character supplied is not a valid start UTF8 character ("
                         << static_cast<u32>(*lpUtf8String) << ")";
                CgsDev::Assert::FireAssert(lMessage.GetBuffer(), CGS_ASSERT_LOCATION(CGS_ARTIST_UNICODE_CPP_FILE, 808));
                CgsDev::Assert::EndAssert();
            }
            if (luExtraBytes >= 4)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("luExtraBytes < 4", CGS_ASSERT_LOCATION(CGS_ARTIST_UNICODE_CPP_FILE, 810));
                CgsDev::Assert::EndAssert();
            }
            switch (luExtraBytes)
            {
            case 3: luChar = static_cast<u32>(*lpUtf8String++) << 6; // fall through
            case 2: luChar = (luChar + *lpUtf8String++) << 6;        // fall through
            case 1: luChar = (luChar + *lpUtf8String++) << 6;        // fall through
            case 0: luChar += *lpUtf8String++; break;
            default: break;
            }
            luChar -= lauUtf8Offsets[luExtraBytes];
            if (luChar > 0xffffu)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lUtf32Char <= KUTF32_MAX_UTF16", CGS_ASSERT_LOCATION(CGS_ARTIST_UNICODE_CPP_FILE, 841));
                CgsDev::Assert::EndAssert();
            }
            *lpUtf16String++ = static_cast<CgsUtf16>(luChar);
        }
        *lpUtf16String = 0;
        return lpUtf16String;
    }

    // Faithful port of X360 0x827E6A28: advance past one UTF-8 character (lead + trailing bytes).
    const u8* IncrementUtf8Pointer(const u8* lpUtf8Char)
    {
        CGS_ASSERT_STREAM_AT(lUtf8TrailingBytes(*lpUtf8Char) != 255,
            "Character supplied is not a valid start UTF8 character (" << static_cast<s32>(*lpUtf8Char) << ")",
            CGS_ARTIST_UNICODE_H_FILE, 914);
        return lpUtf8Char + lUtf8TrailingBytes(*lpUtf8Char) + 1;
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
        if (luTrailing == 255)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "Character supplied is not a valid start UTF8 character\n("
                    << static_cast<s32>(lu8Lead) << ") : ("
                    << reinterpret_cast<const char*>(lpUtf8Char) << ")\n";
            return false;
        }

        if (luTrailing > 3 || (lu8Lead & 0xC0) == 0x80)
            return false;

        const u8* lpTrailing = lpUtf8Char + 1;
        for (u8 lu8Index = 1; lu8Index <= luTrailing; ++lu8Index, ++lpTrailing)
        {
            if ((*lpTrailing & 0xC0) != 0x80)
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "Invalid UTF8 Char " << static_cast<s32>(*lpTrailing)
                        << " . Incorrect number of trailing bytes. Most likely cause of this is the string has been copied into a buffer and truncated in the middle of a multi-byte character.";
                return false;
            }
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
    // on the console, folded static per convention).
    CgsUtf8* FloatToString(CgsUtf8* lpUtf8TargetString, f32 lfValue, u8 lu8DecimalPlaces, const CgsUtf8* lpUtf8ThousandsSeparator,
                           const CgsUtf8* lpUtf8DecimalPointCharacter)
    {
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
        while (*lpUtf8String != 0)
        {
            if (!IsValidUtf8Character(lpUtf8String))
                return false;
            lpUtf8String = IncrementUtf8Pointer(lpUtf8String);
        }
        return true;
    }

    // [gateui r5] Faithful port of X360 ARTIST 0x828357A0: the CHARACTER length of a UTF-8
    // string. The console validates the string first (its one assert, CgsUnicode.cpp:77), then
    // walks the bytes counting every byte that is NOT a 10xxxxxx continuation byte -- i.e. one
    // count per UTF-8 character however many bytes it occupies.
    s32 StringLength(const CgsUtf8* lpUtf8String)
    {
        CGS_ASSERT_AT(IsValidUtf8String(lpUtf8String), "IsValidUtf8String(lpUtf8String)",
                      CGS_ARTIST_UNICODE_CPP_FILE, 77);
        s32 liLength = 0;
        while (*lpUtf8String != 0)
        {
            if ((*lpUtf8String & 0xC0) != 0x80)
                ++liLength;
            ++lpUtf8String;
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
        CGS_ASSERT_AT(lpUtf8TargetString != 0, "lpUtf8TargetString!= NULL", CGS_ARTIST_UNICODE_CPP_FILE, 190);
        CGS_ASSERT_AT(lpUtf8SourceString != 0, "lpUtf8SourceString!= NULL", CGS_ARTIST_UNICODE_CPP_FILE, 191);
        CGS_ASSERT_AT(lnMaxTargetStringLength > 0, "lnMaxTargetStringLength > 0", CGS_ARTIST_UNICODE_CPP_FILE, 192);

        CgsUtf8* lpDest = lpUtf8TargetString;
        s32 liCopied = 0;
        while (*lpUtf8SourceString != 0 && liCopied < lnMaxTargetStringLength)
        {
            *lpDest++ = *lpUtf8SourceString++;
            ++liCopied;
        }
        if (liCopied == lnMaxTargetStringLength)
        {
            do
            {
                --lpDest;
            }
            while ((*lpDest & 0xC0) == 0x80);

            CGS_ASSERT_STREAM_AT(lpDest >= lpUtf8TargetString,
                "Could not find a leading byte in string " << reinterpret_cast<const char*>(lpUtf8SourceString) << ".",
                CGS_ARTIST_UNICODE_CPP_FILE, 215);
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
        while (*lpUtf8SourceString != 0)
            *lpUtf8TargetString++ = *lpUtf8SourceString++;
        *lpUtf8TargetString = 0;
        return lpUtf8TargetString;
    }

    // Faithful port of X360 ARTIST 0x828345F0: NUL-terminate lpUtf8String so it occupies at most
    // lnMaxTargetLength bytes without splitting a multi-byte UTF-8 character. Start at the last byte
    // inside the cap (str + lnMaxTargetLength - 1); if that byte is a continuation byte (10xxxxxx),
    // step back until a leading/single byte (or the start of the string), then write the terminator
    // there. A cap of <= 0, or an already-NUL byte at the cap, is a no-op. Returns the target.
    // Reached from BrnGui::GuiHudMessage::GetParam / AddParam.
    CgsUtf8* SafelyTerminate(CgsUtf8* lpUtf8String, s32 lnMaxTargetLength)
    {
        // Compare native byte addresses before forming a pointer to dereference.
        const uintptr_t luStart = reinterpret_cast<uintptr_t>(lpUtf8String);
        uintptr_t luByte = luStart + lnMaxTargetLength - 1;
        while (luByte >= luStart)
        {
            CgsUtf8* lpByte = reinterpret_cast<CgsUtf8*>(luByte);
            if (*lpByte == 0)
                return lpUtf8String;
            if ((*lpByte & 0xC0) != 0x80)
            {
                *lpByte = 0;
                return lpUtf8String;
            }
            --luByte;
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
        CGS_ASSERT_AT(lpUtf8TargetString != 0, "lpUtf8TargetString!= NULL", CGS_ARTIST_UNICODE_CPP_FILE, 278);
        CGS_ASSERT_AT(lpUtf8SourceString != 0, "lpUtf8SourceString!= NULL", CGS_ARTIST_UNICODE_CPP_FILE, 279);
        CGS_ASSERT_AT(lnMaxTargetStringLength > 0, "lnMaxTargetStringLength > 0", CGS_ARTIST_UNICODE_CPP_FILE, 280);

        // ARTIST adds the unsigned byte delta before storing the uppercase value.
        const u8 lu8UppercaseDelta = static_cast<u8>('A' - 'a');
        s32 liCopied = 0;
        CgsUtf8 lu8Byte = *lpUtf8SourceString;
        while (lu8Byte != 0 && liCopied < lnMaxTargetStringLength - 1)
        {
            *lpUtf8TargetString = lu8Byte;
            if (IsSingleByteUtf8(lpUtf8TargetString) && lu8Byte >= 'a' && lu8Byte <= 'z')
                *lpUtf8TargetString = lu8Byte + lu8UppercaseDelta;
            ++lpUtf8SourceString;
            ++lpUtf8TargetString;
            ++liCopied;
            lu8Byte = *lpUtf8SourceString;
        }
        *lpUtf8TargetString = 0;
        return lpUtf8TargetString;
    }

    // Byte length of a NUL-terminated UTF-8 string (X360 CgsUnicode.cpp:108). Counts the
    // bytes preceding the terminator -- the LanguageResourceType string-table descriptor size.
    u32 ByteLength(const u8* lpUtf8String)
    {
        u32 luLength = 0;
        while (*lpUtf8String != 0)
        {
            ++lpUtf8String;
            ++luLength;
        }
        return luLength;
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

#undef CGS_ARTIST_UNICODE_H_FILE
#undef CGS_ARTIST_UNICODE_CPP_FILE
