#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"
#include <ppcintrinsics.h>
#include <cstring>

namespace CgsGraphics
{
namespace
{
const char* const kBufferFile = "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBuffer.h";
void NativeAssert3d(const char* message, int line)
{
    CgsDev::Assert::BeginAssert(); CgsDev::Assert::FireAssert(message, kBufferFile, line); CgsDev::Assert::EndAssert();
}
}

// ARTIST823FEFE0 / 823FF0D8: dispatch through the derived virtual handler,
// then use the current dispatch buffer again after each assertion/command.
template<class V> void Im3dRenderBufferBase<V>::Dispatch(Im3dBase<V>* renderer) const
{
    if (!this->mpDispatchBuffer->muCommandBufferWritePos) return;
    const ImCommand* command = reinterpret_cast<const ImCommand*>(this->mpDispatchBuffer->mpu8CommandBuffer);
    while (command)
    {
        if (!this->HandleCommand(command, renderer))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("HandleCommand( lpCurrentCommand, lpImRenderer )",
                "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h", 204);
            CgsDev::Assert::EndAssert();
        }
        const u32 pos = static_cast<u32>(reinterpret_cast<uintptr_t>(command) -
            reinterpret_cast<uintptr_t>(this->mpDispatchBuffer->mpu8CommandBuffer));
        const u32 next = pos + command->muSize;
        if (pos >= this->mpDispatchBuffer->muCommandBufferWritePos)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "luCurrentCommandPos >= 0 && luCurrentCommandPos < mpDispatchBuffer->muCommandBufferWritePos",
                kBufferFile, 1301);
            CgsDev::Assert::EndAssert();
        }
        if (next >= this->mpDispatchBuffer->muCommandBufferWritePos) break;
        command = reinterpret_cast<const ImCommand*>(this->mpDispatchBuffer->mpu8CommandBuffer + next);
    }
}

template<class V> void Im3dRenderBufferBase<V>::PostCommand3d(u32 type, const ImCommand* source, u32 bytes)
{
    if (!this->mbInRenderBlock) NativeAssert3d("Command called outside of a BeginRendering/EndRendering block", 1157);
    __dcbz128(static_cast<int>(this->mpWriteBuffer->muCommandBufferWritePos + 128u), this->mpWriteBuffer->mpu8CommandBuffer);
    const u32 position = this->mpWriteBuffer->muCommandBufferWritePos;
    const u32 next = position + bytes;
    if (next > this->muCommandBufferSize)
    {
        this->SetBufferFullRewindToLastEndRender();
        if (!this->mbFailGracefully) NativeAssert3d("ImRenderBuffer command buffer is full", 1195);
        return;
    }
    this->mpWriteBuffer->muCommandBufferWritePos = next;
    std::memcpy(this->mpWriteBuffer->mpu8CommandBuffer + position, source, bytes);
}

template<class V> void Im3dRenderBufferBase<V>::SetTransform(Matrix44::InParam vp)
{
    ImCommandSetTransform3dVp command;
    command.muType = 16; command.muSize = 80; command.mViewProjectionMatrix = vp;
    PostCommand3d(16, &command, 80);
}

template<class V> void Im3dRenderBufferBase<V>::SetTransform(Matrix44::InParam model, Matrix44::InParam vp)
{
    ImCommandSetTransform3dMtwVp command;
    command.muType = 20; command.muSize = 144;
    command.mModelToWorldMatrix = model; command.mViewProjectionMatrix = vp;
    PostCommand3d(20, &command, 144);
}

template<class V> bool Im3dRenderBufferBase<V>::HandleCommand(const ImCommand* c, Im3dBase<V>* r) const
{
    switch (c->muType)
    {
    case 0: r->BeginRendering(); break;
    case 1: r->EndRendering(); break;
    case 2:
    {
        const ImCommandRenderPrimitives<V>* p = static_cast<const ImCommandRenderPrimitives<V>*>(c);
        r->Render(p->mePrimitiveType, p->mpVertices, p->muNumVertices); break;
    }
    case 3: r->SetState(static_cast<const ImCommandSetStateBlend*>(c)->mpBlendState); break;
    case 4: r->SetState(static_cast<const ImCommandSetStateDepthStencil*>(c)->mpDepthStencilState); break;
    case 5:
    {
        const ImCommandSetStateDepthStencilStencilRef* p = static_cast<const ImCommandSetStateDepthStencilStencilRef*>(c);
        r->SetState(p->mpDepthStencilState, p->muStencilRef); break;
    }
    case 6: r->SetState(static_cast<const ImCommandSetStateRasterizer*>(c)->mpRasterizerState); break;
    case 7: r->SetState(static_cast<const ImCommandSetStateRenderTarget*>(c)->mpRenderTargetState); break;
    case 8: r->SetState(static_cast<const ImCommandSetStateSampler*>(c)->mpSamplerState); break;
    case 9: r->SetState(static_cast<const ImCommandSetStateTexture*>(c)->mpTextureState); break;
    case 10: r->SetTexture(static_cast<const ImCommandSetTexture*>(c)->mpTexture); break;
    case 12:
    {
        const ImCommandSetScissor* p = static_cast<const ImCommandSetScissor*>(c);
        RECT rect; rect.left = p->mu32StartPosX; rect.top = p->mu32StartPosY;
        rect.right = p->mu32StartPosX + p->mu32Width; rect.bottom = p->mu32StartPosY + p->mu32Height;
        D3DDevice_SetScissorRect(gpD3DDevice, &rect); break;
    }
    case 16: r->SetTransform(static_cast<const ImCommandSetTransform3dVp*>(c)->mViewProjectionMatrix); break;
    case 20:
    {
        const ImCommandSetTransform3dMtwVp* p = static_cast<const ImCommandSetTransform3dMtwVp*>(c);
        r->SetTransform(p->mModelToWorldMatrix, p->mViewProjectionMatrix); break;
    }
    default: return false;
    }
    return true;
}

// ARTIST827E1D78: the textured renderer handles its mask records then
// falls back to the common 3D command handler.
bool Im3dRenderBuffer::HandleCommand(const ImCommand* c, Im3dBase<BasicColouredTexturedVertex>* renderer) const
{
    if (!renderer)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpImRenderer", "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h", 316);
        CgsDev::Assert::EndAssert();
    }
    Im3d* r = static_cast<Im3d*>(renderer);
    if (c->muType == 17)
    {
        const ImCommandPushMaskTextureState<BasicColouredTexturedVertex>* p = static_cast<const ImCommandPushMaskTextureState<BasicColouredTexturedVertex>*>(c);
        r->PushMask(const_cast<renderengine::TextureState*>(p->mpTextureState), 0, p->mpVertices); return true;
    }
    if (c->muType == 18)
    {
        const ImCommandPushMaskTexture<BasicColouredTexturedVertex>* p = static_cast<const ImCommandPushMaskTexture<BasicColouredTexturedVertex>*>(c);
        r->PushMask(0, p->mpTexture, p->mpVertices); return true;
    }
    if (c->muType == 19) { r->PopMask(); return true; }
    return Im3dRenderBufferBase<BasicColouredTexturedVertex>::HandleCommand(c, renderer);
}

#define INSTANTIATE(V) \
template void Im3dRenderBufferBase<V>::Dispatch(Im3dBase<V>*) const; \
template void Im3dRenderBufferBase<V>::PostCommand3d(u32,const ImCommand*,u32); \
template void Im3dRenderBufferBase<V>::SetTransform(Matrix44::InParam); \
template void Im3dRenderBufferBase<V>::SetTransform(Matrix44::InParam,Matrix44::InParam); \
template bool Im3dRenderBufferBase<V>::HandleCommand(const ImCommand*, Im3dBase<V>*) const;
INSTANTIATE(BasicColouredTexturedVertex)
INSTANTIATE(BasicColouredVertex)
#undef INSTANTIATE
}
