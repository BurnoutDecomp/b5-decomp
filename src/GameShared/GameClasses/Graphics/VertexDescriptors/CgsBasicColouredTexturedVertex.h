#pragma once

#include "types.hpp"

namespace renderengine
{
    class VertexDescriptor;
}

// CgsGraphics::BasicColouredTexturedVertex - the 3D world-space vertex used by the textured
// immediate-mode 3D renderer (the ImRenderer<BasicColouredTexturedVertex> instantiation behind
// CgsGraphics::Im3d, the smoke / spark / blobby-shadow / above-car / debug-3D paths). Layout from
// the DecFIGS DWARF (CgsBasicColouredTexturedVertex.h) -- a 12-byte FLOAT3 position, a packed
// RGBA8 vertex colour and one FLOAT2 UV set, packed to a 24-byte stride (the X360 ImRenderer<V>::
// Render copies `24 * count` bytes per batch).
namespace CgsGraphics
{
    struct Vector3F { f32 x, y, z; };
    struct RGBA8    { u8 r, g, b, a; };
    struct Vector2F { f32 x, y; };

    struct BasicColouredTexturedVertex
    {
        Vector3F mv3Pos;     // +0x00 (DWARF CgsBasicColouredTexturedVertex.h:46)
        RGBA8    mv4Colour;  // +0x0C (DWARF :47)
        Vector2F mv2Tex0UV;  // +0x10 (DWARF :48)

        // DWARF CgsBasicColouredTexturedVertex.h:68 -- fill the vertex-descriptor parameter block
        // for the position+colour+UV stream. The X360 ImRenderer<BasicColouredTexturedVertex>::
        // Construct inlines this (no out-of-line body is attested for THIS TU), so it is
        // declaration-only here; the three element words come straight from the Construct asm.
        void FillVertexDescriptorParameters(renderengine::VertexDescriptor::Parameters& lrParameters);
    };
}
