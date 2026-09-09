#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
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

    // ARTIST 0x82857748: publish default states before each custom component.
    void CustomRenderComponentInterface::Render(ImRendererSet* lpRendererSet)
    {
        auto& lrBuffer = lpRendererSet->mpIm2dRenderBuffer->mCommandBuffer;
        lrBuffer.BeginRendering();
        lrBuffer.SetState(gpGuiBlendStateStandard);
        lrBuffer.SetState(gpGuiRasterizerStateCullNone);
        lrBuffer.SetState(gpBillboardDepthStencilState);
        lrBuffer.EndRendering();
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
