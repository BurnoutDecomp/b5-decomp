#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
// RGBA8 already lives in the 2D coloured+textured vertex home; reuse it rather than fork (so this
// header can coexist with CgsBasicColouredVertex.h, which reuses the same RGBA8 -- e.g. when both
// the untextured and textured 3D vertices are visible via CgsIm3d.h).
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"  // CgsGraphics::RGBA8

namespace renderengine
{
    class VertexDescriptor;
}

// CgsGraphics::BasicColouredTexturedVertex - the 3D world-space vertex used by the textured
// immediate-mode 3D renderer (the ImRenderer<BasicColouredTexturedVertex> instantiation behind
// CgsGraphics::Im3d, the smoke / spark / blobby-shadow / above-car / debug-3D paths). Layout from
// the DecFIGS DWARF and ARTIST Render824041B8: the CPU record has a 16-byte
// Vector3, colour at16 and UV at20/24, and advances32bytes. Its iterator packs
// position.xyz, colour and UV into the separate24-byte GPU stream.
namespace CgsGraphics
{
    struct Vector3F { f32 x, y, z; };
    struct Vector2F { f32 x, y; };

    struct BasicColouredTexturedVertex
    {
        Vector3  mv3Pos;     // +0x00 (DWARF CgsBasicColouredTexturedVertex.h:46)
        RGBA8    mv4Colour;  // +0x10 (DWARF :47)
        Vector2F mv2Tex0UV;  // +0x14 (DWARF :48)

        // DWARF CgsBasicColouredTexturedVertex.h:68 -- fill the vertex-descriptor parameter block
        // for the position+colour+UV stream. The X360 ImRenderer<BasicColouredTexturedVertex>::
        // Construct inlines this (no out-of-line body is attested for THIS TU), so it is
        // declaration-only here; the three element words come straight from the Construct asm.
        void FillVertexDescriptorParameters(renderengine::VertexDescriptor::Parameters& lrParameters);
    };
    static_assert(sizeof(BasicColouredTexturedVertex) == 32, "ARTIST CPU vertex stride");
}
