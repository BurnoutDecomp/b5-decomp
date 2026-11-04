#include "GameSource/Resource/BrnGameDataModule.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm2dRenderBuffer.h"

namespace BrnResource
{
    // ARTIST 8266ACC8. The last flag is not read by this build.
    void GameDataModule::TextureRenderCallback(renderengine::Texture* lpTexture, Vector2 lvPosition,
                                               Vector2 lvSize, void* lpUserData, bool lbFirst, bool)
    {
        GameDataModule* lpModule = static_cast<GameDataModule*>(lpUserData);
        CgsGraphics::Im2dRenderBuffer* lpBuffer = lpModule->mpDebug2dRenderBuffer;
        if (lpBuffer)
        {
            static renderengine::Texture* volatile spLastTexture = 0;
            if (lpTexture != spLastTexture && lpTexture)
                spLastTexture = lpTexture;
            if (lbFirst)
            {
                lpBuffer->SetTransform(CgsGraphics::Im2dTransform::GetDefault());
                const CgsGraphics::ImRendererBase::StateLibrary& lrStates = CgsGraphics::ImRendererBase::GetStateLibrary();
                lpBuffer->SetState(lrStates.mpBlendState_Standard);
                lpBuffer->SetState(lrStates.mpRasterizerState_CullNone);
                lpBuffer->SetState(lrStates.mpDepthStencilState_ZBufferOff);
            }
            if (lpTexture)
            {
                const f32 x = lvPosition.mV.x;
                const f32 y = lvPosition.mV.y;
                const f32 x1 = x + lvSize.mV.x;
                const f32 y1 = y + lvSize.mV.y;
                const CgsGraphics::Basic2dColouredTexturedVertex laVertices[4] = {
                    {{x, y}, {255,255,255,255}, {0.0f,0.0f}},
                    {{x, y1}, {255,255,255,255}, {0.0f,1.0f}},
                    {{x1, y}, {255,255,255,255}, {1.0f,0.0f}},
                    {{x1, y1}, {255,255,255,255}, {1.0f,1.0f}}
                };
                lpBuffer->SetTexture(lpTexture);
                lpBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);
            }
        }
    }
}
