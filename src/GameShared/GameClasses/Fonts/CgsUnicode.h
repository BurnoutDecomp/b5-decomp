#ifndef CGS_UNICODE_H
#define CGS_UNICODE_H

#include "types.hpp"

namespace CgsUnicode
{
    // The project's UTF-8 code-unit type (DWARF CgsUnicode.h:85 `typedef uint8_t CgsUtf8`).
    // Text buffers and string pointers throughout the GUI are CgsUtf8*/CgsUtf8[].
    typedef u8 CgsUtf8;

    // The project's UTF-16 code-unit type (DWARF). Used by UnicodeBuffer::Convert(const CgsUtf16*).
    typedef u16 CgsUtf16;

    // Scratch buffer that renders a scalar/string/bool into UTF-8 text for the Print<...> string
    // formatters (Feb-2007 CgsUnicode.h + DWARF CgsUnicode.h:132). Layout is DWARF-authoritative:
    // maBuffer is the first instance member (offset 0), so a UnicodeBuffer* aliases its buffer.
    class UnicodeBuffer
    {
    public:
        void Reset();

        void Convert(s32 liInteger);                         // CgsUnicode.h:145 (X360 out-of-batch)
        void Convert(f32 lfFloat);                           // CgsUnicode.h:149 (X360 out-of-batch)
        void Convert(const CgsUtf8* lUtf8String);            // CgsUnicode.h:153  X360 0x82443380 (this batch)
        void Convert(const CgsUtf16* lUtf16String);          // CgsUnicode.h:157 (X360 out-of-batch)
        void Convert(bool lbBool);                           // CgsUnicode.h:161 (X360 out-of-batch)

        const CgsUtf8* GetBuffer();                          // CgsUnicode.h:165

        static void SetThousandsSeparator(const CgsUtf8* lUtf8ThousandsSeparator);  // :169
        static void SetDecimalPointCharacter(const CgsUtf8* lUtf8DecimalPointChar); // :173
        static void SetDecimalPlaces(u8 luDecimalPlaces);    // :177
        static void SetMinimumDigits(u8 luMinimumDigits);    // :181
        static void SetTrueString(CgsUtf8* lUtf8TrueString); // :185
        static void SetFalseString(CgsUtf8* lUtf8FalseString);// :189

    private:
        CgsUtf8 maBuffer[256];                    // CgsUnicode.h:195  KU_UNICODE_BUFFER_SIZE=256, offset 0
        CgsUtf8 maUtf8ThousandsSeparator[4];      // CgsUnicode.h:196
        CgsUtf8 maUtf8DecimalPointCharacter[4];   // CgsUnicode.h:197
        u8      muDecimalPlaces;                  // CgsUnicode.h:199
        u8      muMinimumDigits;                  // CgsUnicode.h:200

        static CgsUtf8 maUtf8TrueString[16];      // CgsUnicode.h:192
        static CgsUtf8 maUtf8FalseString[16];     // CgsUnicode.h:193
    };

    // Decode one UTF-8 character (1-4 bytes starting at lpUtf8Char) to a UTF-16 code unit.
    // X360 ARTIST 0x827E6B08. Used by the font glyph lookup + the text renderer.
    u16 ConvertUtf8CharToUtf16Char(const u8* lpUtf8Char);

    // Advance past one UTF-8 character (lead byte + its trailing bytes). X360 ARTIST 0x827E6A28.
    // Used to walk a UTF-8 string a character at a time (font measurement + rendering).
    const u8* IncrementUtf8Pointer(const u8* lpUtf8Char);

    // True iff the string is empty or contains only single-byte (ASCII, high-bit-clear) UTF-8
    // characters up to its NUL terminator. X360 ARTIST 0x824EA850 (DWARF CgsUnicode.h:828
    // `bool IsSingleByteUtf8(const CgsUtf8*)`; assigned symbol name IsSingleByteOnlyUtf8String).
    // Used by BrnGui::GuiModule::Update.
    bool IsSingleByteOnlyUtf8String(const CgsUtf8* lpUtf8String);

    // Byte length of a NUL-terminated UTF-8 string (X360 CgsUnicode.cpp:108; DWARF
    // uint32_t ByteLength(const UnicodeBuffer::CgsUtf8*)). Declared here in its canonical
    // home; the body is its own TU. Used by CgsResource::LanguageResourceType's serialised
    // descriptor (string-table size).
    u32 ByteLength(const u8* lpUtf8String);

    // Validate the UTF-8 character at lpUtf8Char: its lead byte must be a valid lead and be
    // followed by the right number of continuation (10xxxxxx) bytes. X360 ARTIST 0x82834D10.
    // Returns 1 (valid / NUL) or 0 (invalid). Used by IsValidUtf8String.
    bool IsValidUtf8Character(const u8* lpUtf8Char);

    // Validate a whole NUL-terminated UTF-8 string (every character valid). X360 ARTIST
    // 0x82834EA0. Returns 1 (empty / all valid) or 0. Used by CgsAptString::Prepare's
    // string-validity assert.
    bool IsValidUtf8String(const u8* lpUtf8String);

    // Copy lpUtf8SourceString into lpUtf8TargetString, NUL-terminating, never writing past
    // lnMaxTargetStringLength bytes and never truncating in the middle of a multi-byte UTF-8
    // character (backs up to the last leading byte when the cap is hit). Returns the target.
    // X360 ARTIST 0x82834478. Used by CgsAptString::Prepare to copy the resolved text into the
    // caller's buffer.
    CgsUtf8* CopyN(CgsUtf8* lpUtf8TargetString, const CgsUtf8* lpUtf8SourceString,
                   s32 lnMaxTargetStringLength);

    // ADDITIVE GROW (BrnNetworkLoginManagerBase TU): unbounded copy of a NUL-terminated UTF-8
    // string into lpUtf8TargetString, returning a pointer to the written NUL terminator (stpcpy
    // semantics -- the X360 body advances and returns the target register, so the result points at
    // the end, NOT the original target). X360 reaches it from BrnNetwork::LoginManagerBase::
    // UpdateDownloadingTOS (CgsUnicode::Copy(mpTOS, downloadBuffer)) to copy the freshly-downloaded
    // terms-of-service text into the manager's buffer. Body in CgsUnicode.cpp (X360 0x82834448).
    CgsUtf8* Copy(CgsUtf8* lpUtf8TargetString, const CgsUtf8* lpUtf8SourceString);

    // NUL-terminate lpUtf8String within lnMaxTargetLength bytes without splitting a multi-byte
    // UTF-8 character (backs up to the last lead byte). Returns the target. Body lives in
    // CgsUnicode.cpp's own TU. Used by GuiHudMessage::GetParam after SnPrintf.
    CgsUtf8* SafelyTerminate(CgsUtf8* lpUtf8String, s32 lnMaxTargetLength);

    // The variadic-string formatter engine (X360 ARTIST 0x82834AF0). Copies lpUtf8SourceString
    // into lpUtf8TargetString (never writing past lnTargetStringSize bytes), substituting each
    // "%N" token (N in 1..9) with the (N-1)th converted parameter string from lppArgs. Returns
    // the advanced target pointer. Bodied in its own file TU (GameShared/.../CgsUnicode.cpp);
    // declared here in its canonical home so the Print<...> formatters below resolve it under
    // the per-TU compile gate.
    CgsUtf8* _Print(CgsUtf8*       lpUtf8TargetString,
                    const CgsUtf8* lpUtf8SourceString,
                    s32            lnTargetStringSize,
                    const CgsUtf8** lppArgs,
                    u8             luArgCount);

    // Print<...> -- the per-arity string formatters the GUI in-game message renderer uses
    // (X360 ARTIST 0x824490E8 = 2-arg, 0x82449150 = 3-arg). Each stages its N string arguments
    // through a scratch UnicodeBuffer (UnicodeBuffer::Convert stages the text into maBuffer, the
    // buffer's first member, so &buffer aliases its text), gathers the N converted buffers into a
    // pointer array, and forwards to _Print with the argument count. The parameters are taken by
    // const-reference exactly as the X360 threads them (r6/r7/... = &stringPtr), and _Print
    // receives (target, source, size, args, count) -- note the X360 swaps source/target into
    // _Print's (target, source) slots. Bodies are out-of-line here; the two instantiations the
    // build uses are pinned by explicit instantiation in CgsUnicode.cpp.
    template< typename T0, typename T1 >
    CgsUtf8* Print(const CgsUtf8* lpUtf8SourceString,
                   CgsUtf8*       lpUtf8TargetString,
                   s32            lnTargetStringSize,
                   const T0&      lrArg0,
                   const T1&      lrArg1)
    {
        UnicodeBuffer laBuffer0;
        UnicodeBuffer laBuffer1;
        laBuffer0.Convert(lrArg0);
        laBuffer1.Convert(lrArg1);

        const CgsUtf8* lapArgs[2] =
        {
            laBuffer0.GetBuffer(),
            laBuffer1.GetBuffer(),
        };
        return _Print(lpUtf8TargetString, lpUtf8SourceString, lnTargetStringSize, lapArgs, 2);
    }

    template< typename T0, typename T1, typename T2 >
    CgsUtf8* Print(const CgsUtf8* lpUtf8SourceString,
                   CgsUtf8*       lpUtf8TargetString,
                   s32            lnTargetStringSize,
                   const T0&      lrArg0,
                   const T1&      lrArg1,
                   const T2&      lrArg2)
    {
        UnicodeBuffer laBuffer0;
        UnicodeBuffer laBuffer1;
        UnicodeBuffer laBuffer2;
        laBuffer0.Convert(lrArg0);
        laBuffer1.Convert(lrArg1);
        laBuffer2.Convert(lrArg2);

        const CgsUtf8* lapArgs[3] =
        {
            laBuffer0.GetBuffer(),
            laBuffer1.GetBuffer(),
            laBuffer2.GetBuffer(),
        };
        return _Print(lpUtf8TargetString, lpUtf8SourceString, lnTargetStringSize, lapArgs, 3);
    }
}

#endif
