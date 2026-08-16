// Tiny embed check: force ODR use of the reconstructed ColourCube::GetResourceDescriptor so the
// compile gate exercises its signature end to end. Compile-only (cl /c); the body lives in
// rwgpfxcolourcube.cpp.
//
// ⭐ IT INCLUDES THE REAL HEADER NOW (2026-08-16, step-10 fix round). This TU used to RE-DECLARE
// the class, which was harmless only while both definitions were empty; once rwgpfxcolourcube.h
// grew the real 16-byte layout the program held two different definitions of the same class -- the
// latent ODR violation AGENTS.md forbids outright ("Don't locally redefine, re-declare, or
// padding-fork a type that has a reconstructable home header").
#include "types.hpp"
#include "rw/rwcore_structs.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.h"

void rwgpfxcolourcube_embed_check()
{
    rw::BaseResourceDescriptors<5> lDescriptor{};
    rw::graphics::postfx::ColourCube::Parameters lParameters{};
    lParameters.size = 16;
    (void)rw::graphics::postfx::ColourCube::GetResourceDescriptor(&lDescriptor, &lParameters);
}
