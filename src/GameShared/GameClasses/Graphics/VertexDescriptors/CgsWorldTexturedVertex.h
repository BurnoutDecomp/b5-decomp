#pragma once

// New concrete vertex header authored for the ImRenderer<BrnGraphics::WorldTexturedVertex>
// instantiation TU (CgsIm3dTexPlusLighting.cpp).
//
// The renderer template never dereferences the vertex type (Construct/AddProgram/SetProgram/
// BeginRendering are all vertex-agnostic); the struct exists only so the explicit instantiation
// names a concrete vertex format whose size matches the descriptor. DWARF (BrnWorldTexturedVertex.h)
// names the members mv3PosPlusTransformIndex (Vector4U_32, 16B @ off 0), mv3Normal (Vector3U_32,
// 12B @ off 16 -- descriptor Pad0=16) and mv2UVs (Vector2U_32, 8B @ off 28 -- descriptor Pad0=28),
// for a 36-byte vertex. The cross-namespace Vector*U_32 helper types are not broadly available in
// src, so the layout is modelled with plain scalars sized/offset to the descriptor (mirrors the
// CgsPositionOnlyVertex.h approach). The three descriptor element type-words (0x1A23A6 / 0x2A23B9 /
// 0x2C23A5) come from the X360 Construct @ 0x8228DE30.

#include "types.hpp"

namespace BrnGraphics
{
    struct WorldTexturedVertex
    {
        // element[0]: position + transform index (descriptor type-word 0x1A23A6, in-stream off 0)
        f32 mfPosX;              // +0x00
        f32 mfPosY;              // +0x04
        f32 mfPosZ;              // +0x08
        u32 muTransformIndex;    // +0x0C

        // element[1]: normal (descriptor type-word 0x2A23B9, in-stream off 16)
        f32 mfNormalX;           // +0x10
        f32 mfNormalY;           // +0x14
        f32 mfNormalZ;           // +0x18

        // element[2]: texture UVs (descriptor type-word 0x2C23A5, in-stream off 28)
        f32 mfU;                 // +0x1C
        f32 mfV;                 // +0x20
    };                           // sizeof == 36
}
