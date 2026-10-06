#include "GameSource/Gui/CustomRenderer/Renderers/BrnBlackBarRenderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h"
#include "GameSource/Gui/Events/BrnGuiEventSetBlackBars.h"

namespace BrnGui
{
    // Construct @0x82446E68; base owns mbRenderEnabled (guest +4).
    void BlackBarRenderer::Construct()
    {
        CgsGui::CustomRenderComponentInterface::Construct();
        mfBarHeight = 0.0f;
        mbStartedGame = false;
    }

    // RecvEvent @0x82446EA8. The queue holds payload bytes, without the GuiEvent header.
    void BlackBarRenderer::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
    {
        if (liEventType == 221)
        {
            const f32 lfBarHeight = *reinterpret_cast<const f32*>(lpEvent);
            mfBarHeight = lfBarHeight > 0.0f
                ? (lfBarHeight < 0.5f ? lfBarHeight : 0.5f) : 0.0f;
            mbRenderEnabled = mfBarHeight > 0.0f;
        }
        else if (liEventType == 145)
        {
            mbStartedGame = true;
        }
    }

    // GetID @0x82446F18 builds the complete CgsID in r3, including the high word.
    CgsID BlackBarRenderer::GetID() const
    {
        return 0x55082A6E21681C00ull;
    }

    // RenderComponent @0x8245CA80. Bars are drawn after the game-start event,
    // as two opaque black strips whose height is the event-supplied amount.
    void BlackBarRenderer::RenderComponent(CgsGui::ImRendererSet* lpRendererSet)
    {
        if (!mbStartedGame)
            return;

        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>*
            lpIm2dRenderBuffer = lpRendererSet->mpIm2dRenderBuffer;
        CGS_ASSERT(lpIm2dRenderBuffer != 0, "lpIm2dRenderBuffer");

        CgsGraphics::Basic2dColouredTexturedVertex laVertices[4] = {};
        const f32 lafUv[4][2] = { { 0.0f, 0.0f }, { 0.0f, 1.0f },
                                 { 1.0f, 0.0f }, { 1.0f, 1.0f } };
        for (u32 luVertex = 0; luVertex < 4; ++luVertex)
        {
            laVertices[luVertex].mv2Pos.x = lafUv[luVertex][0];
            laVertices[luVertex].mv2Pos.y = lafUv[luVertex][1] * mfBarHeight;
            laVertices[luVertex].mv4Colour = CgsGraphics::RGBA8{ 0, 0, 0, 255 };
            laVertices[luVertex].mv2Tex0UV.x = lafUv[luVertex][0];
            laVertices[luVertex].mv2Tex0UV.y = lafUv[luVertex][1];
        }

        lpIm2dRenderBuffer->BeginRendering();
        // FLAG PC-platform leaf: this command buffer consumes logical 1280x720
        // positions and byte colour scales; the shared unit-to-screen transform
        // is the host form of ARTIST's full-screen Im2d transform @0x830112D0.
        lpIm2dRenderBuffer->SetTransform(CgsGui::gBillboardScreenTransform);
        lpIm2dRenderBuffer->SetState(CgsGui::gpGuiBlendStateStandard);
        lpIm2dRenderBuffer->SetState(CgsGui::gpGuiRasterizerStateCullNone);
        lpIm2dRenderBuffer->SetState(CgsGui::gpBillboardDepthStencilState);
        lpIm2dRenderBuffer->SetTexture(0);
        lpIm2dRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);

        for (u32 luVertex = 0; luVertex < 4; ++luVertex)
            laVertices[luVertex].mv2Pos.y = lafUv[luVertex][1] > 0.0f
                ? 1.0f : 1.0f - mfBarHeight;
        lpIm2dRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);
        lpIm2dRenderBuffer->EndRendering();
    }
}
