#pragma once

#include "types.hpp"

namespace renderengine
{
    // DWARF cmn/renderengine/types.h:76. ARTIST82282414..8228245C packs the
    // four channels into this word before passing its address to the writer.
    struct RGBA8
    {
        RGBA8();

        RGBA8(u8 r, u8 g, u8 b, u8 a)
            : m_rgba((static_cast<u32>(r) << 24) |
                     (static_cast<u32>(g) << 16) |
                     (static_cast<u32>(b) << 8) | static_cast<u32>(a))
        {
        }

    private:
        u32 m_rgba;
    };

    CGS_STATIC_ASSERT((sizeof(RGBA8) == 4), "ARTIST packed color word");
    CGS_STATIC_ASSERT((__alignof(RGBA8) == 4), "ARTIST packed color alignment");
}
