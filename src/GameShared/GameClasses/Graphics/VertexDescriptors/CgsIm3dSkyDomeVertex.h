#pragma once

#include "types.hpp"

// BrnGraphics::Im3dSkyDomeVertex - the vertex used by the immediate-mode 3D sky-dome renderer
// (CgsGraphics::ImRenderer<BrnGraphics::Im3dSkyDomeVertex>, driven by BrnSkyDomeManager::Render /
// RenderToEnvironmentMap).
//
// Descriptor elements the X360 renderer Construct builds (BURNOUT_X360_ARTIST.XEX @ 0x82404CF8):
//   element[0]: stream 0, byte-offset 0,  type-word 0x2A23B9  -> the ray direction   @ off 0
//   element[1]: stream 0, byte-offset 12, type-word 0x2C23A5  -> the two distances   @ off 12
// Both type-words are GENERIC -- BrnSunCorona::Construct stores the identical pair for a totally
// different vertex format -- so they do not by themselves pin the second element's width.
//
// The second element is a FLOAT2, which makes the vertex 20 bytes. Four independent sources agree,
// and together they corrected the earlier 16-byte "packed u32" model of this struct (which would
// have mis-strided every sky-dome vertex fetch):
//   1. BrnSkyDomeManager::CreateGeometry @0x824076D8 sizes the vertex buffer at 20 * vertexCount
//      and writes FIVE floats per vertex (see BrnSkyDomeManager.cpp).
//   2. The DecFIGS DWARF (GameSource/Graphics/ImmediateMode/BrnSkyDomeVertex.h:41-44) names the
//      two members `Vector3 mDirection` + `Vector2U_32 mDistances` -- 12 + 8 bytes.
//   3. The Feb-2007 FillVertexDescriptorParameters sets element[1]'s format to VERTEXFORMAT_FLOAT2
//      and its type to ELEMENTTYPE_TEX0, at offset sizeof(float3).
//   4. The sky-dome vertex shader takes its input as
//         float3 direction : POSITION;   float2 distanceAndLength : TEXCOORD0;
//
// The two distances are exactly what CreateGeometry computes per vertex: the ray-sphere distance
// along the dome normal, and that normal's XZ-plane length -- hence the shader's "distanceAndLength".
namespace BrnGraphics
{
    struct Im3dSkyDomeVertex
    {
        // element[0] @ +0x00 -- the outward ray direction for this dome vertex (a unit Vector3).
        f32 mfDirectionX;          // +0x00
        f32 mfDirectionY;          // +0x04
        f32 mfDirectionZ;          // +0x08

        // element[1] @ +0x0C -- {ray-sphere distance along the direction, the direction's XZ length}.
        f32 mfRaySphereDistance;   // +0x0C
        f32 mfXZLength;            // +0x10
    };

    // The stride BrnSkyDomeManager::CreateGeometry allocates (20 * vertexCount).
    static_assert(sizeof(Im3dSkyDomeVertex) == 20,
                  "Im3dSkyDomeVertex is the 20-byte float3 + float2 sky-dome vertex");
}
