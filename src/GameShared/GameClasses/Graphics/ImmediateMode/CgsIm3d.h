#pragma once

#include "BrnCommonTypes.h"  // Matrix44 (rw::math::vpu::Matrix44)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"            // CgsGraphics::ImRenderer<V>
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredVertex.h"  // CgsGraphics::BasicColouredVertex
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"  // CgsGraphics::BasicColouredTexturedVertex (Im3d)
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // renderengine::ProgramVariableHandle
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"

// CgsGraphics::Im3d* - the immediate-mode 3D render hierarchy. Mirrors the 2D
// fold in CgsIm2d.h: Im3dBase<V> adds the world transform on top of ImRenderer<V>, and
// Im3dUntex specialises it for the position+colour vertex (BasicColouredVertex). Hierarchy
// from the DecFIGS DWARF (CgsIm3d.h:56/199):
//   Im3dUntex : Im3dBase<BasicColouredVertex> : ImRenderer<BasicColouredVertex> : ImRendererBase
//   Im3d : Im3dBase<BasicColouredTexturedVertex> : ImRenderer<BasicColouredTexturedVertex>
//
// Textured3D publishes full one/two-matrix transforms; the legacy untextured
// ProgressBar slice remains limited to its previously recovered members.
namespace CgsGraphics
{
    template <typename V>
    struct Im3dBase : public ImRenderer<V>
    {
        using ImRenderer<V>::SetTransform;
        // DWARF CgsIm3d.h:74 -- install the world->view->proj transform used by the next
        // Render submissions. (The X360 takes the Matrix44 by value; the RenderQuadUntex
        // call passes the identity matrix.)
        void SetTransform(Matrix44 lTransform);
        void SetTransform(Matrix44 lModelToWorld, Matrix44 lViewProjection);

        // DWARF CgsIm3d.h:85 -- the active transform the immediate batches are drawn with.
        Matrix44 mCurrentTransform;
    };

    // DWARF CgsIm3d.h:199.
    struct Im3dUntex : public Im3dBase<BasicColouredVertex>
    {
    };

    // CgsGraphics::Im3d - the concrete TEXTURED immediate-mode 3D renderer (the smoke / spark /
    // blobby-shadow / above-car / debug-3D textured paths). It IS the
    // ImRenderer<BasicColouredTexturedVertex> instantiation bodied in CgsIm3d.cpp, grown with the
    // stencil-mask state the Apt/GUI mask path drives (PushMask / PopMask / SaveMaskShaderConstants
    // / SetMaskPixelShaderState). Mirrors the 2D fold CgsGraphics::Im2d (CgsIm2d.h), which likewise
    // derives from its ImRenderer<V> and adds the mask API.
    //
    // Only PushMask (X360 @0x827DCF78) is X360-ARTIST-attested for THIS ledger key and bodied here.
    // Its three collaborators are separate (still-unhomed) ledger keys, declared here as members so
    // the class interface is complete and PushMask can reach them BY NAME; their bodies land with
    // their own TUs. FLAG: SaveMaskShaderConstants / SetMaskPixelShaderState are declaration-only
    // (no X360 body attested for this key), and their parameter TYPES are width-only (4-byte,
    // no DecFIGS DWARF for this TU) -- modelled as opaque handles, not fabricated.
    struct Im3d : public Im3dBase<BasicColouredTexturedVertex>
    {
        // V_IM3D_MAX_MASK_COUNT -- the mask-stack ceiling PushMask asserts against (X360 immediate
        // `cmplwi 2`). CgsIm3d.h:374 in the X360 source.
        static const u32 KU_MAX_MASK_COUNT = 2;

        // Construct @0x827FC748 (289 instr). Stamp the identity into mCurrentTransform, build the
        // ImRenderer<BasicColouredTexturedVertex> base over the title's TWO {vertex, pixel} program
        // pairs, then resolve "worldViewProj" against each program's VERTEX buffer into that
        // program's shader-state handle slot (the X360 writes them at this+0x58 + i*4 == the base's
        // maShaderStateBlocks[i], which is exactly the handle ImRenderer<V>::SetTransform pushes
        // through), and "gvMaskUseFlags" against program 1's vertex AND pixel buffers into the mask
        // handle at this+0x160.
        void Construct(rw::IResourceAllocator* lpAllocator);
        // FLAG PC-platform leaf: program allocation/adoption can fail on the host.
        bool HasProgramsPC() const
        { return mapVertexProgramBuffer[0] != nullptr && mapPixelProgramBuffer[0] != nullptr; }

        // The "gvMaskUseFlags" handle Construct resolves (X360 this+0x160), consumed by the mask
        // pixel-shader state. Declared so Construct can reach it by name.
        renderengine::ProgramVariableHandle mMaskUseFlagsHandle;

        // PushMask @0x827DCF78. Open one stencil-mask region: on the FIRST mask of the stack, bind
        // the next program slot (mi8CurrentProgram + 1) and install the current world transform;
        // then save this mask's shader constants, bump the live mask count, and push the mask pixel-
        // shader state. Returns the renderengine shader-state result (X360 r3 tail-passthrough).
        void* PushMask(const void* lpMaskParam0, const void* lpMaskParam1, const void* lpMaskParam2);

        // ---- mask collaborators: own ledger keys, declaration-only here (see FLAG above) ----------
        // X360 call site stores this mask's constants; PushMask forwards (lpMaskParam2, lpMaskParam0,
        // lpMaskParam1) -- i.e. the X360 arg order SaveMaskShaderConstants(this, a4, a2, a3).
        void  SaveMaskShaderConstants(const void* lpParam0, const void* lpParam1, const void* lpParam2);
        // Bind the mask pixel-shader state for the current mask count. Returns the X360 r3 result.
        void* SetMaskPixelShaderState();

        // The live number of pushed masks (X360 mu32NumMasks @ this+0x180). FLAG: the preceding
        // per-mask shader-constant storage members (written by SaveMaskShaderConstants) are unhomed
        // -- they arrive with that collaborator's TU; PushMask only ever reaches mu32NumMasks, so it
        // is the only Im3d-specific member modelled here (absolute X360 +0x180 offset is not
        // LLP64-invariant and is deliberately not pinned with padding).
        u32 mu32NumMasks;
    };

}
