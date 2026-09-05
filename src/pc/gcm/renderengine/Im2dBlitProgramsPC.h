// b5-decomp/src/pc/gcm/renderengine/Im2dBlitProgramsPC.h
#pragma once

#include "types.hpp"

// =============================================================================
// Im2dBlitProgramsPC.h  (pc/gcm/renderengine)
//
// [PC platform leaf] The eight symbols Im2dBlitProgramsPC.cpp -- a GENERATED
// file -- defines: the four converted BrnRendererMemory blit program images and
// their sizes.
//
// Same shape and same rationale as the sibling SunCoronaProgramsPC.h: the
// consumer (GameSource/Graphics/BrnRendererMemory.cpp) lives in a different tree
// from the definitions, and a header is the only way one compiler ever sees
// both, which turns "the sizes are u32, the arrays are const u8[]" into a
// checked fact rather than a convention.
//
// ⚠ THE SIZES ARE THE *PC IMAGE* SIZES, NOT THE X360 MICROCODE SIZES
// (340 / 236 / 400 / 508). renderengine::ProgramBufferPC_Adopt bounds-checks the
// image against the size it is handed, so pass gu*ProgramPCSize (or sizeof() of
// the array).
//
// The constants to resolve with renderengine::ProgramBuffer::GetVariableHandleByName
// -- the console's own strings, BrnRendererMemory::BlitComposite @0x82406A68:
//     composite VERTEX : "gUVOffsets" c2   (.xy base half-texel, .zw overlay)
//                        "gUVScales"  c3   (.xy base scale,      .zw overlay)
//     composite PIXEL  : sampler "OverlaySampler" s1 -- the quarter-res particle
//                        buffer
//     depth     PIXEL  : sampler "DiffuseSampler" s0 -- the source depth texture
// The console's composite PIXEL also declares "gQuincunxOffsets" c0 and
// "BaseSampler" s0; NEITHER is in the PC recompile, because the scene term is
// re-associated onto the D3D9 blend unit -- brn_im2dblit.fx's banner is the
// authority on why, and BrnRendererMemory::BlitComposite repeats it at the call
// site. Neither vertex program declares the console's gOffsetXYZ/gRightUp (the
// Im2d transform): the quad arrives in NDC, as the sun corona's already does.
//
// Both pairs are drawn through ONE shared two-element vertex declaration --
// POSITION0 FLOAT3 at +0, TEXCOORD0 FLOAT2 at +12, STRIDE 20 -- the sun
// corona's, value for value.
// =============================================================================

namespace renderengine
{
    extern const u8  gauIm2dDepthBlitVertexProgramPC[];
    extern const u32 guIm2dDepthBlitVertexProgramPCSize;
    extern const u8  gauIm2dDepthBlitPixelProgramPC[];
    extern const u32 guIm2dDepthBlitPixelProgramPCSize;
    extern const u8  gauIm2dCompositeBlitVertexProgramPC[];
    extern const u32 guIm2dCompositeBlitVertexProgramPCSize;
    extern const u8  gauIm2dCompositeBlitPixelProgramPC[];
    extern const u32 guIm2dCompositeBlitPixelProgramPCSize;
}
