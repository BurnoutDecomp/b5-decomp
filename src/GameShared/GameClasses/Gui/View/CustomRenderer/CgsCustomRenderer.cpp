#include "types.hpp"

#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsCustomRenderer.cpp -- the custom-render component base bodies, plus the one
// derived-renderer leaf DecFIGS attributes to this file.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsGui::CustomRenderComponentInterface::GetRenderOutput @ 0x828476C0  (cpp:112)
//   CgsGui::CustomRenderComponentInterface::Render          @ 0x82857748  (cpp:135)
//   BrnGui::MainMapRenderer::SetRenderEnabled               @ 0x82C290D8
//
// (Construct @0x828476B0 is `mbRenderEnabled = false` and stays inline in the header,
// as do the remaining trivial DWARF defaults.)

namespace CgsGui
{
    // ---- GetRenderOutput @ 0x828476C0 -----------------------------------------------
    // The base refuses: a component that does not render to a texture must never be asked
    // for one. Both console asserts are reproduced verbatim (the null out-pointer check,
    // then the unconditional refusal), and the out-pointer is zeroed BETWEEN them exactly
    // as the guest does.
    renderengine::Texture* CustomRenderComponentInterface::GetRenderOutput(
        s32 /*liTextureIndex*/, s32* lpiShaderProgram, ImRendererSet* /*lpRendererSet*/)
    {
        CGS_ASSERT(lpiShaderProgram != 0, "lpiShaderProgram != NULL");

        *lpiShaderProgram = 0;

        CGS_ASSERT(false,
                   "attempting to get a texture for a component which does not render to texture");
        return 0;
    }

    // ---- Render @ 0x82857748 --------------------------------------------------------
    // Non-virtual (DWARF). The guest installs the shared 2D immediate-mode render state on
    // the set's Im2d render buffer, then tail-calls the virtual RenderComponent:
    //
    //   v4 = *a2 + 4;                       ; &set->mpIm2dRenderBuffer->mCommandBuffer
    //   sub_824587B0(v4);                   ; open/begin the render block
    //   sub_82458EC0(v4, dword_83010F20);   ; install program state
    //   sub_82458CD0(v4, dword_83010F3C);   ; install blend/raster state
    //   sub_82458DC8(v4, dword_83010F54);   ; install sampler state
    //   sub_82458898(v4);                   ; commit
    //   return (*(*a1 + 52))(a1, a2);       ; RenderComponent(lpRendererSet)
    //
    // FLAG (out of scope, NOT invented): the five Im2dRenderBuffer state entry points and
    // the three state-library globals they take are uncommitted -- CgsGui::ImRendererSet
    // itself is only forward-declared in this slice, so `*a2 + 4` cannot even be spelled
    // here without inventing a layout. The state block is therefore DOCUMENTED, not
    // stubbed with a plausible-looking substitute, and the dispatch the manager actually
    // depends on (the RenderComponent tail-call) is reproduced. Every custom render
    // component in this build currently draws through its own path or not at all, so the
    // missing state install is a no-draw, never a wrong-draw.
    void CustomRenderComponentInterface::Render(ImRendererSet* lpRendererSet)
    {
        RenderComponent(lpRendererSet);
    }
}

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82C290D8
//   (BrnGui::MainMapRenderer::SetRenderEnabled)
//
// NOTE: this function is keyed to CgsCustomRenderer.h by DecFIGS file attribution, but its
// identity is BrnGui::MainMapRenderer::SetRenderEnabled (a derived custom renderer).
// Behaviour-faithful to the X360 pseudocode:
//     *(this + 4) = a2;             // store the enabled flag at offset 4
//     return this;
//
// ⚠️ ODR FORK CLOSED (2026-08-16). `BrnGui::MainMapRenderer` was declared THREE times with
// three different layouts: as a CustomRenderComponentInterface subclass in
// CgsCustomRenderer.h, as a bare `struct { u32; bool; }` at namespace scope in THIS file,
// and as the real { void* mpVtable; u32 maZeroGroups[6][5]; ParticleSystem2d[4] } in
// GameSource/Gui/CustomRenderer/Renderers/BrnMainMapRenderer.h. Three namespace-scope
// definitions of one class link SILENTLY, and whichever the linker keeps decides what
// every call site actually touches. There is now exactly ONE: the real home header, which
// this TU includes -- so the ledger function below is a genuine member of the genuine
// class, and mbRenderEnabled is the member the console's `stb r4, 4(r3)` writes.
#include "GameSource/Gui/CustomRenderer/Renderers/BrnMainMapRenderer.h"

namespace BrnGui
{
    MainMapRenderer* MainMapRenderer::SetRenderEnabled(bool lbRenderEnabled)
    {
        mbRenderEnabled = lbRenderEnabled;
        return this;
    }
}
