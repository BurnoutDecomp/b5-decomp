// b5-decomp/src/pc/gcm/renderengine/SunCoronaProgramsPC.h
#pragma once

#include "types.hpp"

// =============================================================================
// SunCoronaProgramsPC.h  (pc/gcm/renderengine)
//
// [PC platform leaf] The eight symbols SunCoronaProgramsPC.cpp -- a GENERATED
// file -- defines: the four converted BrnSunCorona program images and their
// sizes.
//
// Same shape and same rationale as the sibling CoronaProgramsPC.h: the consumer
// (GameSource/Graphics/BrnSunCorona.cpp) lives in a different tree from the
// definitions, and a header is the only way one compiler ever sees both, which
// turns "the sizes are u32, the arrays are const u8[]" into a checked fact
// rather than a convention. SunCoronaProgramsPC.cpp includes it. Including it is
// optional: a consumer that prefers the older leaves' local `extern` block gets
// identical declarations either way.
//
// ⚠ THE SIZES ARE THE *PC IMAGE* SIZES, NOT THE X360 MICROCODE SIZES
// (204 / 524 / 240 / 464). renderengine::ProgramBufferPC_Adopt bounds-checks the
// image against the size it is handed, so pass gu*ProgramPCSize (or sizeof() of
// the array).
//
// Usage (the sky dome's / corona's exact shape -- the adopt call is the
// CONSUMER's, not a wrapper in the leaf; ProgramBufferPC_Adopt is
// programbuffer.h:123, body ImmediateModePCLeaf.cpp:833, and its third argument
// is 0 = vertex, 1 = pixel):
//
//     renderengine::ProgramBufferData* lpVs = renderengine::ProgramBufferPC_Adopt(
//         renderengine::gauSunCoronaOcclusionVertexProgramPC,
//         renderengine::guSunCoronaOcclusionVertexProgramPCSize, 0u);
//
// The constants to resolve with
// renderengine::ProgramBuffer::GetVariableHandleByName -- the console's own
// strings, BrnSunCorona::Construct @0x82400BD4 / @0x82400CEC:
//     occlusion PIXEL : "kUvStartAndOffset"  c0  (.xy sun screen position,
//                                                 .zw one depth-tap step)
//                       sampler "SamplerSource"   s0 -- the scene DEPTH texture
//     flare     PIXEL : "kColourAndPower"    c0  (.rgb sun colour * white level
//                                                 * brightness, .w mfSunFlarePow)
//                       sampler "OcclusionSource" s0 -- the 1x1 sun-corona buffer
// NEITHER VERTEX PROGRAM DECLARES A CONSTANT (both are pure pass-throughs; the
// CPU feeds NDC positions directly), so there is nothing to look up on them.
// Both are drawn through ONE shared two-element vertex declaration:
// POSITION0 FLOAT3 at +0, TEXCOORD0 FLOAT2 at +12, STRIDE 20.
// =============================================================================

namespace renderengine
{
    extern const u8  gauSunCoronaOcclusionVertexProgramPC[];
    extern const u32 guSunCoronaOcclusionVertexProgramPCSize;
    extern const u8  gauSunCoronaOcclusionPixelProgramPC[];
    extern const u32 guSunCoronaOcclusionPixelProgramPCSize;
    extern const u8  gauSunCoronaFlareVertexProgramPC[];
    extern const u32 guSunCoronaFlareVertexProgramPCSize;
    extern const u8  gauSunCoronaFlarePixelProgramPC[];
    extern const u32 guSunCoronaFlarePixelProgramPCSize;
}
