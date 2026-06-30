#pragma once

#include "types.hpp"

// BrnGraphics::Im3dSkyDomeVertex - the vertex used by the immediate-mode 3D sky-dome renderer
// (CgsGraphics::ImRenderer<BrnGraphics::Im3dSkyDomeVertex>, driven by BrnSkyDomeManager::Render /
// RenderToEnvironmentMap). No DWARF / Feb-2007 source was recovered for this vertex; the layout
// below is read straight from the two vertex-descriptor elements the X360 renderer Construct builds
// (BURNOUT_X360_ARTIST.XEX @ 0x82404CF8):
//
//   element[0]: stream 0, type-word 0x2A23B9, usageIndex 1  -> a 12-byte position attribute @ off 0
//   element[1]: stream 0, byte-offset 12, type-word 0x2C23A5, formatCode 5, usageIndex 6
//               -> a 4-byte packed attribute @ off 12
//
// The element[1] byte-offset of 12 (the asm immediate stored at the element's +2 word) pins the
// position attribute at 12 bytes (a Vector3), so the second attribute starts at +0x0C. Its exact
// semantic (colour / packed direction) is not attested, so it is modelled as a 4-byte packed word.
// The renderer template never dereferences the vertex type (Construct / AddProgram / SetProgram /
// SetTransform / Begin/EndRendering are all vertex-agnostic); the struct exists so the instantiation
// names a concrete vertex format whose size matches the descriptor (12 + 4 = 16 bytes).
namespace BrnGraphics
{
    struct Im3dSkyDomeVertex
    {
        f32 mfPosX;        // +0x00  position attribute (descriptor element[0], 12 bytes @ off 0)
        f32 mfPosY;        // +0x04
        f32 mfPosZ;        // +0x08
        u32 muPacked;      // +0x0C  packed attribute (descriptor element[1], byte-offset 12)
    };
}
