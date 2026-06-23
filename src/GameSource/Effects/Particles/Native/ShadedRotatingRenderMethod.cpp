#include "GameSource/Effects/Particles/Native/ShadedRotatingRenderMethod.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed-home (HONEST STUB) for BURNOUT_X360_ARTIST.XEX
//   BrnParticle::Native::ShadedRotatingRenderMethod::BuildQuad  @ 0x8291E150
//
// VMX KEYSTONE -- DELIBERATELY NOT BODIED. See ShadedRotatingRenderMethod.h for the
// full rationale: BuildQuad is a multi-stage hand-vectorised VMX/AltiVec pipeline
// (lvx128 strided gathers from rodata vector-constant tables, vmaddfp polynomial wave
// evaluation, vrlimi128/vperm lane shuffles, vctuxs RGBA byte-packing) with no faithful
// scalar lowering. Per project policy a VMX pipeline is flagged, not paraphrased: a
// fabricated per-lane formula would be guessed, not X360 fact. The honest floor is an
// unimplemented body that fires the project assert if ever reached, so no incorrect
// vertex data is silently produced.

namespace BrnParticle
{
namespace Native
{
    void ShadedRotatingRenderMethod::BuildQuad(void* /*lpParticleState*/, int* /*lpVertexOut*/, void* /*lpRenderState*/)
    {
        // HONEST FLOOR: the VMX quad-build pipeline (@0x8291E150) is not reconstructed in
        // this pass. Do not fabricate output vertices.
        CGS_ASSERT(false, "BrnParticle::Native::ShadedRotatingRenderMethod::BuildQuad: VMX quad-build pipeline not yet reconstructed");
    }
}
}
