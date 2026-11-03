#pragma once

#include "SDKs/EATech/include/Apt/AptCharacter.h"

// ARTIST Unresolve 0x82AF77B0: name, glyph count and character table
// follow the 16-byte AptCharacter header. Names describe observed roles.
struct AptCharacterFont : public AptCharacter
{
    const char* mpName;
    int32_t mnGlyphCount;
    AptCharacter** mpGlyphs;
};
