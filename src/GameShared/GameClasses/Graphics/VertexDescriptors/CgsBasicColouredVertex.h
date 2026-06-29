#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"  // Vector3 (rw::math::vpu::Vector3)
// RGBA8 already lives in the 2D coloured+textured vertex home; reuse it rather than fork.
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"  // CgsGraphics::RGBA8

// CgsGraphics::BasicColouredVertex - the position+colour vertex used by the untextured
// immediate-mode 3D renderer (CgsGraphics::Im3dUntex). Layout from the DecFIGS DWARF
// (CgsBasicColouredVertex.h):
//   Vector3 mv3Pos;      // +0x00 (12 bytes used; the 4th SIMD lane is unwritten padding)
//   RGBA8   mv4Colour;   // +0x0C packed 8-bit-per-channel colour
// => 16-byte vertex (the X360 RenderQuadUntex writes each quad corner as a 16-byte run:
//    a position vector whose trailing dword is overwritten by the packed colour word).
namespace CgsGraphics
{
    struct BasicColouredVertex
    {
        Vector3 mv3Pos;     // DWARF CgsBasicColouredVertex.h:46
        RGBA8   mv4Colour;  // DWARF CgsBasicColouredVertex.h:47
    };

    // DWARF CgsImRenderBuffer.h:336 -- the untextured immediate-mode vertex is the basic
    // coloured vertex.
    typedef BasicColouredVertex Im3dUntexVertex;
}
