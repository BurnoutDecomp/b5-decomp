#ifndef BRN_SHADED_ROTATING_RENDER_METHOD_H
#define BRN_SHADED_ROTATING_RENDER_METHOD_H

// ============================================================================
// GameSource/Effects/Particles/Native/ShadedRotatingRenderMethod.h
//
// BrnParticle::Native::ShadedRotatingRenderMethod -- the per-particle quad builder of the native
// simple-particle renderer. Its only caller is BrnSimpleParticleArray::CB4ParticleBank::Render
// @0x8291E600, which builds ONE of these on its stack (var_470) before its loops and hands `this`
// to every BuildQuad call in r3.
//
// ⭐ 2026-09-24 (FX-CRASHVFX): BODIED. The old header called BuildQuad "a VMX keystone with no
// faithful scalar lowering" and gave it an opaque (void*, int*, void*) signature and an asserting
// stub. Neither holds: every lane of the pipeline is independent, the rodata tables it gathers
// from are CRT-initialised splats whose values the init thunks state outright (see the .cpp), and
// the result was checked against the console's own instruction words executed in an emulator
// (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu).
//
// LAYOUT (the three stores Render makes into var_470 before its loops, 0x8291E6B0..0x8291E6F0):
//   +0x00  mvCornerScale     splat(sqrt 2)  -- flt_821012CC, the half-diagonal of a unit square
//   +0x10  mvLightingBase    (mid, mid, mid, 1.0)
//   +0x20  mvLightingRange   (half, half, half, 0.0)
// where mid/half are the middle and half-width of [min(1, lighting.min), min(1, lighting.max)]
// (CB4ParticleArrayStandardParams::mLightingMinMax). Xenon-only code -- the PS3 DWARF carries no
// shape for it, so the member names are ours, named for what the console computes into them.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"

namespace BrnParticle
{
namespace Native
{
    struct CB4Particle;

    class ShadedRotatingRenderMethod
    {
    public:
        // BrnParticle::Native::ShadedRotatingRenderMethod::BuildQuad @0x8291E150 (202 instructions).
        // Register contract (the four Render call sites, 0x8291F33C / F498 / F5E8 / F734):
        //   r3 this, r4 laPositions[4] (out), r5 lauColours[4] (out), r6 the spawn record,
        //   r7 the camera's view matrix (never read -- `li r7, 0x10` overwrites it first),
        //   v1 the particle's projected position, v2 (size x, size y, 0, size x),
        //   v3 its colour, v4 splat(age), v5 splat(alpha).
        // Writes the four corner positions and the four RGBA8 colour words (memory order R,G,B,A).
        void BuildQuad(rw::math::vpu::Vector4* lpaPositions,
                       u32* lpauColours,
                       const CB4Particle* lpParticle,
                       const rw::math::vpu::Matrix44* lpViewMatrix,
                       const rw::math::vpu::Vector4& lvPosition,
                       const rw::math::vpu::Vector4& lvSize,
                       const rw::math::vpu::Vector4& lvColour,
                       const rw::math::vpu::Vector4& lvAge,
                       const rw::math::vpu::Vector4& lvAlpha) const;

        rw::math::vpu::Vector4 mvCornerScale;     // +0x00
        rw::math::vpu::Vector4 mvLightingBase;    // +0x10
        rw::math::vpu::Vector4 mvLightingRange;   // +0x20
    };
}
}

#endif
