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
}

#endif
