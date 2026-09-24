#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnIm3dSmokeRenderer.h
//
// BrnGraphics::Im3dSmokeRenderer -- the immediate-mode renderer the native SIMPLE particles (impact
// smoke, crash impact dust, the ten skid-smoke surfaces) draw through. A
// CgsGraphics::ImRenderer<BasicColouredTexturedVertex> carrying TWO program pairs:
//
//   program 0  Im3dSmokeRenderer           -- soft particles: the pixel program reads the resolved
//              depth buffer (DepthSampler, s1) and fades each fragment out as it nears the scene
//              behind it. Vertex constants gScale / gOffset (NDC -> depth-texture UV and a remapped
//              depth), pixel constants gDepthConversion / gDepthFadeConstants.
//   program 1  Im3dSmokeRendererSansZFade  -- tex2D * vertex colour, nothing else.
//
// BrnSimpleParticleRenderer::Dispatch picks between them with its z-fade flag.
// The programs are the re-authored BrnIm3dSmokeRendererProgramsPC.cpp (see its banner).
//
// DWARF (DecFIGS BrnIm3dSmokeRenderer.h:49-73):
//   struct Im3dSmokeRenderer : public CgsGraphics::ImRenderer<CgsGraphics::BasicColouredTexturedVertex> {
//     ProgramVariableHandle mOffsetHandle;              // :69
//     ProgramVariableHandle mScaleHandle;               // :70
//     ProgramVariableHandle mDepthConversionHandle;     // :72
//     ProgramVariableHandle mDepthFadeConstantsHandle;  // :73
//     void Construct(rw::IResourceAllocator*);          // :56  (X360 @0x82295260)
//     void SetConstants(Vector2, float32_t, float32_t, float32_t);   // :64 (X360 @0x822839C8)
//   }
// The console Construct stores the four handles at +0x58 / +0x5C / +0x60 / +0x64 (sizeof 0x68, inside
// the 0x6C slot ParticleModule reserves at +0x9274). Host pointers widen the base; nothing addresses
// this object by absolute offset.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"                                                        // Vector2
#include "pc/gcm/renderengine/VertexDescriptor.h"   // renderengine::VertexDescriptor::Parameters (the vertex header needs it complete)
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"               // CgsGraphics::ImRenderer<V>
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"            // renderengine::ProgramVariableHandle

namespace rw { class IResourceAllocator; }

namespace BrnGraphics
{
    struct Im3dSmokeRenderer : public CgsGraphics::ImRenderer<CgsGraphics::BasicColouredTexturedVertex>
    {
        // The two program slots Construct uploads, in the console's order (the four stack words
        // at 0x82295270..0x822952C4). BrnSimpleParticleRenderer::Dispatch binds 0 when its z-fade
        // flag is set and 1 when it is not (`li r4, 0` / `li r4, 1` at 0x8228CB64 / 0x8228CB58).
        static const s8 KI8_PROGRAM_ZFADE      = 0;
        static const s8 KI8_PROGRAM_SANS_ZFADE = 1;

        // @0x82295260: build the base over both program pairs, then resolve gOffset / gScale against
        // vertex program 0 and gDepthConversion / gDepthFadeConstants against pixel program 0.
        void Construct(rw::IResourceAllocator* lpAllocator);

        // @0x822839C8: write the four z-fade constants (the soft-particle path only).
        void SetConstants(rw::math::vpu::Vector2 lvDepthUvOffset,
                          f32 lfNearPlane,
                          f32 lfFarPlane,
                          f32 lfFadeDistance);

    protected:
        renderengine::ProgramVariableHandle mOffsetHandle;              // +0x58
        renderengine::ProgramVariableHandle mScaleHandle;               // +0x5C
        renderengine::ProgramVariableHandle mDepthConversionHandle;     // +0x60
        renderengine::ProgramVariableHandle mDepthFadeConstantsHandle;  // +0x64
    };
}
