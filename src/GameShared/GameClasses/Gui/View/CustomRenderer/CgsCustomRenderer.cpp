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

// ⭐ 2026-08-29 (map-world wave) -- BrnGui::MainMapRenderer::SetRenderEnabled @0x82C290D8
// NO LONGER LIVES HERE. DecFIGS keys the leaf to CgsCustomRenderer.h only because it is an
// ICF-folded `stb r4, 4(r3); blr`; its identity is MainMapRenderer's, and the DWARF
// declares it `virtual void SetRenderEnabled(bool)` at BrnMainMapRenderer.cpp:377.
//
// The body that used to sit here had the signature `MainMapRenderer* SetRenderEnabled(bool)`
// on a class that had no base -- a NON-virtual, wrong-return-type member that would have
// SHADOWED the base vtable slot instead of overriding it the moment the class became a real
// CgsGui::CustomRenderComponentInterface (the H3b shadowing-redeclaration defect class).
// MainMapRenderer is now that real component, so the override lives with the rest of the
// class in GameSource/Gui/CustomRenderer/Renderers/BrnMainMapRenderer.cpp. Nothing about
// this file's own two bodies changed.
