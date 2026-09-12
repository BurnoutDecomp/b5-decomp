#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnIm3dTexPlusLighting.h
//
// BrnGraphics::Im3dTexPlusLighting -- the immediate-mode renderer the debris
// arrays draw through: a CgsGraphics::ImRenderer<BrnGraphics::WorldTexturedVertex>
// (CgsIm3dTexPlusLighting.cpp owns those template bodies) carrying ONE
// vertex/pixel program pair and the six program-constant handles that pair
// exposes:
//
//   vertex program:  gaWorldTransforms (32 x float4x4, c0..c127)
//                    gViewProjection   (c128..c131)
//                    gEyeLocation      (c132)
//   pixel program:   gLightDirection   (c0)
//                    gLightColour      (c1)
//                    gShinyParams      (c2)
//
// The vertex program indexes gaWorldTransforms by the transform-index lane of
// the vertex position, transforms position and normal into world space, exports
// the eye vector and passes the per-instance RGBA that rides the transform's
// unused fourth column; the pixel program is a Blinn-Phong lit, alpha-
// premultiplied tex2D. See pc/gcm/renderengine/WorldTexturedProgramsPC.cpp.
//
// LAYOUT AUTHORITY: DecFIGS plus the console Construct. The six handles sit straight after the ImRenderer base at +0x58 /
// +0x5C / +0x60 / +0x64 / +0x68 / +0x6C, for sizeof 0x70 == the 112-byte slot
// ParticleModule reserves at +0x91A0. (Host pointers widen the base; nothing
// addresses this object by absolute offset.)
//
// Console: Construct is the only out-of-line body. The six setters
// (SetTransformArray / SetViewProjection / SetEye / SetLightDirection /
// SetLightColour / SetShinyParams) are inline there, so their bodies are
// recovered from the stores their ONLY two call sites make --
// BrnDebrisRenderer::BeginRender and ::RenderDebrisArray, both reconstructed
// now. Every setter is the same shape: open the named constant's shader-state
// row and write the value into the returned cursor. GROW additively.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                              // Matrix44 / Vector3
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsWorldTexturedVertex.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"  // CgsGraphics::ImRenderer<V>
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h" // renderengine::ProgramVariableHandle

namespace BrnGraphics
{
    struct Im3dTexPlusLighting : public CgsGraphics::ImRenderer<WorldTexturedVertex>
    {
        // The transform array the vertex program indexes.
        static const u32 KU_NUM_TRANSFORMS = 32;

        // Build the base renderer with the single world-textured program pair, then resolve
        // the three vertex-program constants and the three pixel-program constants by name.
        void Construct(rw::IResourceAllocator* lpAllocator);

        // Write the KU_NUM_TRANSFORMS per-instance world matrices the vertex program indexes as
        // gaWorldTransforms. The per-instance RGBA rides each matrix's fourth column.
        void SetTransformArray(const Matrix44* lpTransforms);
        void SetViewProjection(Matrix44::InParam lViewProjection);
        void SetEye(Vector3 lEye);
        void SetLightDirection(Vector3 lDirection);
        void SetLightColour(Vector3 lColour);
        void SetShinyParams(f32 lfX, f32 lfY, f32 lfZ, f32 lfW);

    protected:
        renderengine::ProgramVariableHandle mTransformArrayStateHandle;      // +0x58
        renderengine::ProgramVariableHandle mViewProjectionMatrixStateHandle; // +0x5C
        renderengine::ProgramVariableHandle mEyeStateHandle;                  // +0x60
        renderengine::ProgramVariableHandle mWorldLightDirectionStateHandle;  // +0x64
        renderengine::ProgramVariableHandle mLightColourStateHandle;          // +0x68
        renderengine::ProgramVariableHandle mShinyParamsStateHandle;          // +0x6C
    };
}
