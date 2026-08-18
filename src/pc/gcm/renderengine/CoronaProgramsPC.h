// b5-decomp/src/pc/gcm/renderengine/CoronaProgramsPC.h
#pragma once

#include "types.hpp"

// =============================================================================
// CoronaProgramsPC.h  (pc/gcm/renderengine)
//
// [PC platform leaf] The four symbols CoronaProgramsPC.cpp -- a GENERATED file
// -- defines: the two converted corona (head/tail-light FLARE) program images
// and their sizes.
//
// WHY A HEADER AT ALL, when the sibling generated leaves have none. SkyDome /
// PostFx / PostFxBloom / PostFxB4Blur / PostFxHelper are consumed from
// GameSource/... TUs that re-spell the `extern const u8 gau...[]` block locally
// (BrnIm3d.cpp:51-57, BrnPostFxShader.cpp:142-..., BrnPostFxBloom.cpp:138-...).
// That works, but it puts the declaration and the definition in two files that
// no compiler ever sees together, so a size-type or a spelling can drift
// silently. The corona consumer lives in a THIRD tree
// (SDKs/RenderEngineClub/.../coronas/rwgcoronarenderer.cpp), so this header is
// the one place the four names are written down -- and CoronaProgramsPC.cpp
// includes it, which is what makes the declarations and the definitions be
// checked against each other at compile time.
//
// Including it is optional: a consumer that prefers the sibling leaves' local
// `extern` block gets identical declarations either way.
//
// ⚠ THE SIZES ARE THE *PC IMAGE* SIZES, NOT THE X360 MICROCODE SIZES (788 / 228).
// renderengine::ProgramBufferPC_Adopt bounds-checks the image against the size
// it is handed, so pass gu*ProgramPCSize (or sizeof() of the array).
//
// Usage (the sky dome's exact shape -- the adopt call is the CONSUMER's, not a
// wrapper in the leaf; ProgramBufferPC_Adopt is programbuffer.h:123, body
// ImmediateModePCLeaf.cpp:833, and its third argument is 0 = vertex, 1 = pixel):
//
//     renderengine::ProgramBufferData* lpVs = renderengine::ProgramBufferPC_Adopt(
//         renderengine::gauCoronaVertexProgramPC,
//         renderengine::guCoronaVertexProgramPCSize, 0u);
//     renderengine::ProgramBufferData* lpPs = renderengine::ProgramBufferPC_Adopt(
//         renderengine::gauCoronaPixelProgramPC,
//         renderengine::guCoronaPixelProgramPCSize, 1u);
//
// The three vertex constants to resolve against lpVs with
// renderengine::ProgramBuffer::GetVariableHandleByName -- the console's own
// strings, CoronaRenderer::Initialize @0x82285250 / 0x82285268 / 0x82285280:
//     "viewProjectionMatrix"          c0, 4 registers, ROW-major
//     "cameraPositionPlusBrightness"  c4  (.xyz camera position, .w white level)
//     "viewXyScale"                   c5  (.xy only)
// The pixel program's only variable is the sampler "coronaTexture" at s0 -- so
// the corona atlas TextureState binds to sampler UNIT 0, which is what
// CoronaRenderer::Begin's shadow::Device::SetState(state, 0) already does.
// =============================================================================

namespace renderengine
{
    extern const u8  gauCoronaVertexProgramPC[];
    extern const u32 guCoronaVertexProgramPCSize;
    extern const u8  gauCoronaPixelProgramPC[];
    extern const u32 guCoronaPixelProgramPCSize;
}
