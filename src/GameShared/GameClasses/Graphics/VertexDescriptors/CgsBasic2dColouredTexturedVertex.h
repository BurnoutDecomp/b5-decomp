#pragma once

#include "types.hpp"

// CgsGraphics::Basic2dColouredTexturedVertex - the screen-space vertex used by the
// immediate-mode 2D renderer: a position, a packed RGBA8 colour, and one UV set.
// Layout from the DecFIGS DWARF (CgsBasic2dColouredTexturedVertex.h).
namespace CgsGraphics
{
    struct Vector2 { f32 x, y; };
    struct RGBA8   { u8 r, g, b, a; };

    struct Basic2dColouredTexturedVertex
    {
        Vector2 mv2Pos;
        RGBA8   mv4Colour;
        Vector2 mv2Tex0UV;
    };
}
