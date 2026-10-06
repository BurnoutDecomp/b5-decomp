#include <Windows.h>
#include <d3d9.h>
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "pc/gcm/renderengine/device.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/matrix44_operation.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "pc/gcm/renderengine/ImWhiteTexturePCLeaf.h"
#include <cstring>

void ImDeviceSetBlendState(void*);
void ImDeviceSetDepthStencilState(void*);
void ImDeviceSetRasterizerState(void*);
extern "C" void D3DDevice_SetRenderState_StencilRef(IDirect3DDevice9*, u32);

namespace CgsGui {
extern const renderengine::BlendState* gpGuiBlendStateStandard;
extern const renderengine::BlendState* gpGuiBlendStateAdditive;
extern const renderengine::DepthStencilState* gpBillboardDepthStencilState;
const renderengine::DepthStencilState* gpGuiDepthStencilStateZOn = nullptr;
renderengine::Texture* gpGuiWhiteTexture = nullptr;
extern const renderengine::RasterizerState* gpGuiRasterizerStateCullNone;
}

namespace CgsGraphics {
namespace {
// Original ConstructOnceOnly827F1C20 publishes F20/F24/F3C/F48/F54.
// Native storage replaces the16-byte GUI identity sentinels with full state
// objects. Build through the real marshaling Initializers, preserving every
// original parameter from ConstructBlend/Rasteriser/DepthStencilState.
struct Im3dStateLibraryPC
{
    renderengine::BlendMaterialState mStandard, mAdditive;
    renderengine::RasterizerState mCullNone;
    renderengine::DepthStencilState mZOn, mZOff;
    Im3dStateLibraryPC()
    {
        renderengine::BlendStateParameters blend = {};
        for (u32& factor : blend.maBlendFactor) factor = 0x07060706u;
        blend.muState15 = 7u;
        blend.muState4 = blend.muState5 = blend.muState6 = blend.muState7 = 15u;
        blend.muState8 = 135u;
        blend.muState9 = 0xFFFFFFFFu;
        blend.mbHasCustomBlendFactors = 1;
        auto* standard = &mStandard;
        renderengine::BlendState::Initialize(&standard, &blend);
        blend.maBlendFactor[0] = 0x07060106u;
        auto* additive = &mAdditive;
        renderengine::BlendState::Initialize(&additive, &blend);

        renderengine::RasterizerState::Parameters raster = {};
        raster.muMultisampleEnable = 0xFFFFFFFFu;
        raster.muAntialiasedLineEnable = 0xFFFFu;
        raster.mu8DepthClipEnable = 1;
        raster.mu8FrontCounterClockwise = 1;
        raster.mu8PaddingMode = 1;
        auto* cullNone = &mCullNone;
        renderengine::RasterizerState::Initialize(&cullNone, &raster);

        renderengine::DepthStencilState::Parameters depth = {};
        depth.muFunction = 3u;
        depth.muState4 = depth.muState8 = 7u;
        depth.muStencilReadMask = depth.muStencilWriteMask = 0xFFFFFFFFu;
        depth.muState14 = depth.muState15 = 0xFFFFFFFFu;
        depth.mbDepthTestEnable = depth.mbDepthWriteEnable = 1;
        auto* zOn = &mZOn;
        renderengine::DepthStencilState::Initialize(&zOn, &depth);
        depth.mbDepthTestEnable = depth.mbDepthWriteEnable = 0;
        auto* zOff = &mZOff;
        renderengine::DepthStencilState::Initialize(&zOff, &depth);
    }
};
Im3dStateLibraryPC& Im3dStatesPC()
{
    static Im3dStateLibraryPC states;
    return states;
}
}
void PrepareIm3dStateLibraryPC()
{
    // FLAG PC-platform leaf: share one full native state home across the
    // original2D identity checks and the original3D typed state dispatch.
    auto& states = Im3dStatesPC();
    CgsGui::gpGuiBlendStateStandard = reinterpret_cast<const renderengine::BlendState*>(&states.mStandard);
    CgsGui::gpGuiBlendStateAdditive = reinterpret_cast<const renderengine::BlendState*>(&states.mAdditive);
    CgsGui::gpGuiRasterizerStateCullNone = &states.mCullNone;
    CgsGui::gpBillboardDepthStencilState = &states.mZOff;
    CgsGui::gpGuiDepthStencilStateZOn = &states.mZOn;
    GetImWhiteTexturePC();
}
renderengine::Texture* GetImWhiteTexturePC()
{
    CgsGui::gpGuiWhiteTexture = renderengine::GetImmediateWhiteTexturePC(renderengine::gDevice);
    return CgsGui::gpGuiWhiteTexture;
}
const renderengine::DepthStencilState* GetIm3dDepthStencilZBufferOnPC()
{
    return &Im3dStatesPC().mZOn;
}
// The original Dispatch823FEFE0 walks the frozen command bank through the
// subclass's virtual handler; it never treats world vertices as screen pixels.
template<class V>
void Im3dRenderBufferBase<V>::Dispatch(Im3dBase<V>* lpRenderer) const
{
    for (const ImCommand* lpCommand = this->GetFirstCommand(); lpCommand != nullptr;
         lpCommand = this->GetNextCommand(lpCommand))
        CGS_ASSERT(HandleCommand(lpCommand, lpRenderer), "HandleCommand( lpCurrentCommand, lpImRenderer )");
}

// Original inlined PostCommand,8244FC48: assert the block, reserve the exact
// record, or rewind the unfinished block and report an ungraceful overflow.
template<class V>
void Im3dRenderBufferBase<V>::PostCommand3d(u32 luType, const ImCommand* lpSource, u32 luBytes)
{
    CGS_ASSERT(this->mbInRenderBlock, "Command called outside of a BeginRendering/EndRendering block");
    const u32 luPos = this->mpWriteBuffer->muCommandBufferWritePos;
    if (luPos + luBytes > this->muCommandBufferSize)
    {
        this->SetBufferFullRewindToLastEndRender();
        CGS_ASSERT(this->mbFailGracefully, "ImRenderBuffer command buffer is full");
        return;
    }
    ImCommand* lpDest = reinterpret_cast<ImCommand*>(this->mpWriteBuffer->mpu8CommandBuffer + luPos);
    std::memcpy(lpDest, lpSource, luBytes);
    lpDest->muType = luType;
    lpDest->muSize = luBytes;
    this->mpWriteBuffer->muCommandBufferWritePos = luPos + luBytes;
}
template<class V>
void Im3dRenderBufferBase<V>::SetTransform(Matrix44::InParam lViewProjection)
{
    ImCommandSetTransform3dVp command = {};
    command.mViewProjectionMatrix = lViewProjection;
    PostCommand3d(16, &command, sizeof(command));
}
template<class V>
void Im3dRenderBufferBase<V>::SetTransform(Matrix44::InParam lModelToWorld, Matrix44::InParam lViewProjection)
{
    ImCommandSetTransform3dMtwVp command = {};
    command.mModelToWorldMatrix = lModelToWorld;
    command.mViewProjectionMatrix = lViewProjection;
    PostCommand3d(20, &command, sizeof(command));
}

// ARTIST827E1878: the ordinary textured3D command handler. The state appliers
// below are the native D3D9 counterparts of ImRendererBase's original states.
template<class V>
bool Im3dRenderBufferBase<V>::HandleCommand(const ImCommand* lpCommand, Im3dBase<V>* lpRenderer) const
{
    switch (lpCommand->muType)
    {
    case 0: lpRenderer->BeginRendering(); break;
    case 1: lpRenderer->EndRendering(); break;
    case 2: {
        const auto& c = *static_cast<const ImCommandRenderPrimitives<V>*>(lpCommand);
        lpRenderer->Render(c.mePrimitiveType, c.mpVertices, c.muNumVertices); break;
    }
    case 3: shadow::Device::SetState(reinterpret_cast<const renderengine::BlendMaterialState*>(
                static_cast<const ImCommandSetStateBlend*>(lpCommand)->mpBlendState)); break;
    case 4: shadow::Device::SetState(
                static_cast<const ImCommandSetStateDepthStencil*>(lpCommand)->mpDepthStencilState); break;
    case 5: {
        const auto& c = *static_cast<const ImCommandSetStateDepthStencilStencilRef*>(lpCommand);
        shadow::Device::SetState(c.mpDepthStencilState);
        D3DDevice_SetRenderState_StencilRef(renderengine::gDevice, c.muStencilRef); break;
    }
    case 6: shadow::Device::SetState(
                static_cast<const ImCommandSetStateRasterizer*>(lpCommand)->mpRasterizerState); break;
    case 7: renderengine::Device::SetState(static_cast<const ImCommandSetStateRenderTarget*>(lpCommand)->mpRenderTargetState); break;
    case 8: shadow::Device::SetState(const_cast<renderengine::SamplerState*>(
                static_cast<const ImCommandSetStateSampler*>(lpCommand)->mpSamplerState), 0); break;
    case 9: shadow::Device::SetState(reinterpret_cast<const renderengine::TextureState*>(
                static_cast<const ImCommandSetStateTexture*>(lpCommand)->mpTextureState), 0); break;
    case 10: shadow::Device::SetResource(static_cast<const ImCommandSetTexture*>(lpCommand)->mpTexture, 0); break;
    case 12: {
        const auto& c = *static_cast<const ImCommandSetScissor*>(lpCommand);
        RECT rect = {LONG(c.mu32StartPosX), LONG(c.mu32StartPosY),
                     LONG(c.mu32StartPosX + c.mu32Width), LONG(c.mu32StartPosY + c.mu32Height)};
        // FLAG PC-platform leaf: native surface scissor equivalent to the original ring call.
        renderengine::gDevice->SetScissorRect(&rect); break;
    }
    case 16: lpRenderer->SetTransform(static_cast<const ImCommandSetTransform3dVp*>(lpCommand)->mViewProjectionMatrix); break;
    case 20: {
        const auto& c = *static_cast<const ImCommandSetTransform3dMtwVp*>(lpCommand);
        lpRenderer->SetTransform(c.mModelToWorldMatrix, c.mViewProjectionMatrix); break;
    }
    default: return false;
    }
    return true;
}
bool Im3dRenderBuffer::HandleCommand(const ImCommand* lpCommand, Im3dBase<BasicColouredTexturedVertex>* lpRenderer) const
{
    CGS_ASSERT(lpRenderer != nullptr, "lpImRenderer");
    // Mask commands17..19 need the still-unreconstructed program1 mask
    // collaborator, and intentionally reach the dispatch assert while absent.
    // They are not emitted by the original AboveCar text/draw path.
    return Im3dRenderBufferBase<BasicColouredTexturedVertex>::HandleCommand(lpCommand, lpRenderer);
}
template class Im3dRenderBufferBase<BasicColouredTexturedVertex>;
}
