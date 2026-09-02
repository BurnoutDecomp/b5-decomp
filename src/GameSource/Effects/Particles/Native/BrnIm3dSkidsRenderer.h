#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnIm3dSkidsRenderer.h
//
// BrnGraphics::Im3dSkidsRenderer -- the immediate-mode renderer behind the
// skid / tyre-mark decals: a CgsGraphics::ImRenderer<BrnGraphics::SkidVertex>
// (BrnSkidVertex.cpp owns those template bodies) carrying ONE vertex/pixel
// program pair and the three program-constant handles that pair exposes:
//
//   gWorldViewProj  (c0..c3)  gStartColour (c4)  gEndColour (c5)
//
// The vertex program lerps gStartColour -> gEndColour by the vertex's TIME
// lane (the mark's age, 0..1), scales the alpha by the STRENGTH lane and
// passes uv through; the pixel program is tex2D * colour. Recovered by
// disassembling the two executable-embedded Xenos programs the X360
// Construct hands to ImRenderer<SkidVertex>::Construct (unk_8200E9D0, 456
// bytes vertex; unk_8200EB98, 228 bytes pixel) with
// tools/assets/shaders/xenos.py -- see tools/assets/shaders/brn_skid.fx.
//
// X360 ARTIST: Construct @0x82295150 (the only out-of-line body; its
// primary_file attributes to CgsImRenderer.h because the base Construct is
// inlined into it).
//
// DWARF AUTHORITY (DecFIGS BrnIm3dSkidsRenderer.h:49-124):
//   struct Im3dSkidsRenderer : public CgsGraphics::ImRenderer<SkidVertex>
//     ProgramVariableHandle mWorldViewProjStateHandle (:72)
//     ProgramVariableHandle mStartColourStateHandle   (:73)
//     ProgramVariableHandle mEndColourStateHandle     (:74)
//     Construct(rw::IResourceAllocator*) (:55)  SetTransform(Matrix44) (:99)
//     SetBlendStartColour(Vector4) (:111)  SetBlendEndColour(Vector4) (:124)
// Console layout: the three handles sit at +0x58 / +0x5C / +0x60 straight
// after the 0x58-byte ImRenderer base (sizeof 0x64 == the 100-byte slot
// ParticleModule reserves at +0x9210). +0x58 is also the word ImRenderer<V>::
// SetTransform addresses for program slot 0, which is why the console's
// TrailRenderer::BeginRender writes the view-projection through it directly.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                             // Matrix44 / Vector4
#include "GameSource/Effects/Particles/Native/BrnSkidVertex.h"          // BrnGraphics::SkidVertex
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h" // CgsGraphics::ImRenderer<V>
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h" // renderengine::ProgramVariableHandle

namespace rw { class IResourceAllocator; }

namespace BrnGraphics
{
    struct Im3dSkidsRenderer : public CgsGraphics::ImRenderer<SkidVertex>
    {
        // @0x82295150. Build the base renderer with the single skid program pair, then
        // resolve the three named constants against the VERTEX program.
        void Construct(rw::IResourceAllocator* lpAllocator);

        // DWARF :99 / :111 / :124 -- each is one renderengine::Device::BeginShaderStates
        // on the matching handle followed by the raw copy (64 bytes for the matrix,
        // 16 for a colour). The X360 inlines them into TrailRenderer::BeginRender
        // @0x82284468 (transform) and TrailRenderer::Render @0x82295930 (the colours).
        void SetTransform(Matrix44::InParam lTransform);
        void SetBlendStartColour(Vector4 lColour);
        void SetBlendEndColour(Vector4 lColour);

    protected:
        renderengine::ProgramVariableHandle mWorldViewProjStateHandle;   // +0x58
        renderengine::ProgramVariableHandle mStartColourStateHandle;     // +0x5C
        renderengine::ProgramVariableHandle mEndColourStateHandle;       // +0x60
    };
}
