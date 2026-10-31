#ifndef RW_GPFX_COLOUR_CUBE_H
#define RW_GPFX_COLOUR_CUBE_H

#include "types.hpp"
#include "rw/rwcore_structs.h"
#include <cstddef>

namespace rw { namespace graphics { namespace postfx {
    // ARTIST 823F83F0..83FC reads the fixed-up pixel pointer at +0x04.
    // DecFIGS rwgpfxcolourcube.h supplies the two-member declaration.
    struct ColourCube
    {
        struct Parameters
        {
            u32 size;
        };

        // ARTIST 82402C48 reserves a 16-byte resource header before the RGB
        // cells. That allocation extent is distinct from sizeof this type.
        static const u32 KU_HEADER_BYTES = 16u;
        static const u32 KU_BYTES_PER_CELL = 3u;

        static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(
            rw::BaseResourceDescriptors<5>* lpResult, const Parameters* lpParameters);

        u32 GetSize() const { return m_size; }
        u8* GetPixels() const { return m_pixels; }

        u32 m_size;
        u8* m_pixels;
    };

    CGS_STATIC_ASSERT(sizeof(ColourCube) == 8, "ARTIST colour cube declaration");
    CGS_STATIC_ASSERT(offsetof(ColourCube, m_pixels) == 4, "ARTIST fixed-up cube pixels");
} } }

#endif // RW_GPFX_COLOUR_CUBE_H
