#pragma once

#include "types.hpp"
// Vector2 / RGBA8 already live in the 2D coloured+textured vertex home; reuse them rather than fork.
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"  // CgsGraphics::Vector2, RGBA8

// CgsGraphics::Basic2dColouredVertex - the screen-space vertex used by the untextured
// immediate-mode 2D renderer (CgsGraphics::Im2dUntex). Position + packed RGBA8 colour, no UV.
//
// The X360 ImRenderer<Basic2dColouredVertex>::Render submits this vertex with a 12-byte stride
// (li r6, 0xC -> D3DDevice_BeginVertices VertexStreamZeroStride == 12), copying three dwords per
// vertex: the two position floats followed by the packed colour word. That fixes the layout as a
// 2-float position + one packed 4x8-bit colour:
//   Vector2 mv2Pos;     // +0x00 (x,y floats)
//   RGBA8   mv4Colour;  // +0x08 packed 8-bit-per-channel colour
// => 12-byte vertex (the textured sibling is 16 bytes: it carries one extra UV pair).
namespace CgsGraphics
{
    struct Basic2dColouredVertex
    {
        Vector2 mv2Pos;     // +0x00
        RGBA8   mv4Colour;  // +0x08
    };
}
