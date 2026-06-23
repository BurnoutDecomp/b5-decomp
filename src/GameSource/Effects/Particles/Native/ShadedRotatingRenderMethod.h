#ifndef BRN_SHADED_ROTATING_RENDER_METHOD_H
#define BRN_SHADED_ROTATING_RENDER_METHOD_H

// ============================================================================
// GameSource/Effects/Particles/Native/ShadedRotatingRenderMethod.h
//
// BrnParticle::Native::ShadedRotatingRenderMethod -- the per-particle quad builder used
// by the simple-particle renderer's CB4 bank (caller:
// BrnParticle::Native::BrnSimpleParticleArray::CB4ParticleBank::Render @0x8291E600).
// BuildQuad takes a particle's transform/colour/rotation state plus a vertex-output
// pointer and emits one shaded, rotated, screen-space quad (four packed vertices with
// dword-packed RGBA colour and UVs).
//
// HONEST STUB -- VMX KEYSTONE (NOT reconstructed in this pass):
//   BuildQuad @0x8291E150 is a multi-stage hand-vectorised VMX/AltiVec pipeline. The
//   body is almost entirely inline VMX (lvx128 strided gathers from rodata vector
//   constants @0x8307A3B0..0x8307A680 / 0x82CDA350, vmaddfp polynomial evaluation of a
//   fract()-based wave (vrfim/vsubfp/vminfp), vrlimi128/vperm lane shuffles building the
//   rotated quad corners, vmulfp/vmaxfp/vminfp colour clamp, and vctuxs->byte-packing of
//   the final RGBA dwords). This does NOT reliably lower to scalar C++: there is no
//   faithful per-lane scalar formula without inventing one. Per project policy a VMX
//   keystone is FLAGGED and given an honest non-fabricated stub rather than a paraphrase.
//   Reconstructing it requires decoding the rodata vector constant tables and the exact
//   permute-immediate lane maps (a dedicated VMX pass).
// ============================================================================

#include "types.hpp"

namespace BrnParticle
{
namespace Native
{
    class ShadedRotatingRenderMethod
    {
    public:
        // BrnParticle::Native::ShadedRotatingRenderMethod::BuildQuad @0x8291E150.
        // Build one shaded, rotated quad into the vertex output. Arguments mirror the
        // X360 register/stack contract (this, a particle-state pointer, the dword-packed
        // vertex output, and a state block); kept opaque pending the VMX-decode pass.
        // HONEST STUB: not implemented -- see file header (VMX keystone).
        void BuildQuad(void* lpParticleState, int* lpVertexOut, void* lpRenderState);
    };
}
}

#endif
