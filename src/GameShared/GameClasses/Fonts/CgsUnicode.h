#ifndef CGS_UNICODE_H
#define CGS_UNICODE_H

#include "types.hpp"

namespace CgsUnicode
{
    // The project's UTF-8 code-unit type (DWARF CgsUnicode.h:85 `typedef uint8_t CgsUtf8`).
    // Text buffers and string pointers throughout the GUI are CgsUtf8*/CgsUtf8[].
    typedef u8 CgsUtf8;

    // Decode one UTF-8 character (1-4 bytes starting at lpUtf8Char) to a UTF-16 code unit.
    // X360 ARTIST 0x827E6B08. Used by the font glyph lookup + the text renderer.
    u16 ConvertUtf8CharToUtf16Char(const u8* lpUtf8Char);

    // Advance past one UTF-8 character (lead byte + its trailing bytes). X360 ARTIST 0x827E6A28.
    // Used to walk a UTF-8 string a character at a time (font measurement + rendering).
    const u8* IncrementUtf8Pointer(const u8* lpUtf8Char);

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
}

#endif
