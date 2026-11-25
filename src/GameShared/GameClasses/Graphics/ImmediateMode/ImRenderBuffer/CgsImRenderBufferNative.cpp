#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstring>
#include <ppcintrinsics.h>

namespace CgsGraphics
{
namespace
{
const char* const kFile = "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBuffer.h";
void NativeAssert(const char* expression, int line)
{
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(expression, CGS_ASSERT_LOCATION(kFile, line));
    CgsDev::Assert::EndAssert();
}
}

// ARTIST82404A50/82404308/824045B0 allocate both streams for both buffers,
// then clear and swap twice, leaving buffer zero as the writer.
template<class V>
bool ImRenderBuffer<V>::Prepare(u32 luCommandBufferSizeBytes, u32 luVertexBufferSizeBytes,
                                rw::IResourceAllocator* lpAllocator, bool lbFailGracefully)
{
    muCommandBufferSize = luCommandBufferSizeBytes;
    muVertexBufferSize = luVertexBufferSizeBytes;
    mbFailGracefully = lbFailGracefully;

    // ARTIST allocates each stream independently with an extra cache line.
    maBuffers[0].mpu8CommandBuffer = static_cast<u8*>(
        lpAllocator->AllocateMemoryResource(luCommandBufferSizeBytes + 128, 128, NULL));
    maBuffers[0].mpu8VertexBuffer = static_cast<u8*>(
        lpAllocator->AllocateMemoryResource(luVertexBufferSizeBytes + 128, 128, NULL));
    maBuffers[1].mpu8CommandBuffer = static_cast<u8*>(
        lpAllocator->AllocateMemoryResource(luCommandBufferSizeBytes + 128, 128, NULL));
    maBuffers[1].mpu8VertexBuffer = static_cast<u8*>(
        lpAllocator->AllocateMemoryResource(luVertexBufferSizeBytes + 128, 128, NULL));

    CGS_ASSERT(maBuffers[0].mpu8CommandBuffer,
        CGS_ASSERT_MESSAGE_AT("maBuffers[0].mpu8CommandBuffer", kFile, 511));
    CGS_ASSERT(maBuffers[0].mpu8VertexBuffer,
        CGS_ASSERT_MESSAGE_AT("maBuffers[0].mpu8VertexBuffer", kFile, 512));
    CGS_ASSERT(maBuffers[1].mpu8CommandBuffer,
        CGS_ASSERT_MESSAGE_AT("maBuffers[1].mpu8CommandBuffer", kFile, 513));
    CGS_ASSERT(maBuffers[1].mpu8VertexBuffer,
        CGS_ASSERT_MESSAGE_AT("maBuffers[1].mpu8VertexBuffer", kFile, 514));

    miWriteBufferIndex = 0;
    mpWriteBuffer = &maBuffers[miWriteBufferIndex];
    mpDispatchBuffer = &maBuffers[1 - miWriteBufferIndex];
    Clear();
    Swap();
    Clear();
    Swap();
    return true;
}

template<class V>
void ImRenderBuffer<V>::SetBufferFullRewindToLastEndRender()
{
    mbBufferIsFull = true;
    mpWriteBuffer->muCommandBufferWritePos = muLastEndRenderPos;
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "Buffer is full, rewinding command to " << muLastEndRenderPos << "\n";
}

template<class V> bool ImRenderBuffer<V>::IsInARenderingBlock() { return mbInRenderBlock; }

// DecFIGS CgsImRenderBuffer.h:1213 and ARTIST's inlined command writers.
template<class V>
void* ImRenderBuffer<V>::AllocateCommand(s32 liCommandType, u32 luCommandSize)
{
    if (liCommandType != IM_CMD_BEGIN_RENDERING && liCommandType != IM_CMD_END_RENDERING)
    {
        CGS_ASSERT(mbInRenderBlock,
            CGS_ASSERT_MESSAGE_AT("Command called outside of a BeginRendering/EndRendering block", kFile, 1216));
    }

    __dcbz128(static_cast<s32>(mpWriteBuffer->muCommandBufferWritePos + 128),
        mpWriteBuffer->mpu8CommandBuffer);
    const u32 luWritePosition = mpWriteBuffer->muCommandBufferWritePos;
    u8* const lpCommandBuffer = mpWriteBuffer->mpu8CommandBuffer;
    const u32 luNextPosition = luWritePosition + luCommandSize;
    if (luNextPosition <= muCommandBufferSize)
    {
        ImCommand* lpCommand = reinterpret_cast<ImCommand*>(&lpCommandBuffer[luWritePosition]);
        lpCommand->muSize = luCommandSize;
        lpCommand->muType = liCommandType;
        mpWriteBuffer->muCommandBufferWritePos = luNextPosition;
        if (liCommandType == IM_CMD_END_RENDERING)
        {
            muLastEndRenderPos = luWritePosition;
        }
        return lpCommand;
    }

    SetBufferFullRewindToLastEndRender();
    CGS_ASSERT(mbFailGracefully,
        CGS_ASSERT_MESSAGE_AT("ImRenderBuffer command buffer is full", kFile, 1254));
    return NULL;
}

template<class V> void ImRenderBuffer<V>::BeginRendering()
{
    CGS_ASSERT(mbInRenderBlock == false,
        CGS_ASSERT_MESSAGE_AT("BeginRendering called inside a BeginRendering/EndRendering block", kFile, 619));
    mbInRenderBlock = true;
    AllocateCommand(IM_CMD_BEGIN_RENDERING, 16);
}

template<class V> void ImRenderBuffer<V>::EndRendering()
{
    CGS_ASSERT(mbInRenderBlock == true,
        CGS_ASSERT_MESSAGE_AT("EndRendering called without a matching call to BeginRendering", kFile, 658));
    mbInRenderBlock = false;
    AllocateCommand(IM_CMD_END_RENDERING, 16);
}

template<class V> V* ImRenderBuffer<V>::AllocVertices(u32 count)
{
    const u32 position = mpWriteBuffer->muVertexBufferWritePos;
    const u32 next = position + sizeof(V) * count;
    if (next > muVertexBufferSize) return 0;
    __dcbz128(128, mpWriteBuffer->mpu8VertexBuffer + position);
    V* vertices = reinterpret_cast<V*>(mpWriteBuffer->mpu8VertexBuffer + position);
    mpWriteBuffer->muVertexBufferWritePos = next;
    return vertices;
}

template<class V> void ImRenderBuffer<V>::Render(renderengine::PrimitiveType primitive, const V* source, u32 count)
{
    if (mbBufferIsFull) return;
    V* vertices = AllocVertices(count);
    if (!vertices)
    {
        if (!mbFailGracefully) NativeAssert("ImRenderBuffer vertex buffer is full", 708);
        SetBufferFullRewindToLastEndRender();
        return;
    }
    ImCommandRenderPrimitives<V>* command = static_cast<ImCommandRenderPrimitives<V>*>(
        AllocateCommand(IM_CMD_RENDER_PRIMITIVES, 32));
    if (command)
    {
        command->mePrimitiveType = primitive;
        command->muNumVertices = count;
        command->mpVertices = vertices;
        std::memcpy(vertices, source, sizeof(V) * count);
    }
}

template<class V> void ImRenderBuffer<V>::RenderFromStaticVertexBuffer(renderengine::PrimitiveType primitive, const V* vertices, u32 count)
{
    if (!vertices) NativeAssert("lpVertices", 731);
    ImCommandRenderPrimitives<V>* command = static_cast<ImCommandRenderPrimitives<V>*>(
        AllocateCommand(IM_CMD_RENDER_PRIMITIVES, 32));
    if (command)
    {
        command->mePrimitiveType = primitive;
        command->muNumVertices = count;
        command->mpVertices = vertices;
    }
}

template<class V> V* ImRenderBuffer<V>::RenderStart(u32 count)
{
    if (miNumRendersStarted < 0) NativeAssert("Mismatched RenderStart/RenderEnd on ImRenderBuffer", 755);
    ++miNumRendersStarted;
    V* vertices = AllocVertices(count);
    if (!vertices)
    {
        if (!mbFailGracefully)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream message(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            message << "Failed to allocate " << count << " in ImRenderBuffer";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, CGS_ASSERT_LOCATION(kFile, 765));
            CgsDev::Assert::EndAssert();
        }
        --miNumRendersStarted;
    }
    return vertices;
}

template<class V> void ImRenderBuffer<V>::RenderEnd(renderengine::PrimitiveType primitive, const V* vertices, u32 count)
{
    if (miNumRendersStarted <= 0) NativeAssert("Mismatched RenderEnd/RenderStart on ImRenderBuffer", 792);
    --miNumRendersStarted;
    if (!vertices) NativeAssert("lpVerticesFromRenderStart", 796);
    ImCommandRenderPrimitives<V>* command = static_cast<ImCommandRenderPrimitives<V>*>(
        AllocateCommand(IM_CMD_RENDER_PRIMITIVES, 32));
    if (command)
    {
        command->mePrimitiveType = primitive;
        command->muNumVertices = count;
        command->mpVertices = vertices;
    }
}

template<class V> void ImRenderBuffer<V>::SetTexture(renderengine::Texture* texture)
{
    if (!texture) NativeAssert("lpTexture!=NULL", 825);
    ImCommandSetTexture* command = static_cast<ImCommandSetTexture*>(AllocateCommand(IM_CMD_SET_TEXTURE, 16));
    if (command)
    {
        command->mpTexture = texture;
    }
}
template<class V> void ImRenderBuffer<V>::SetProgram(s8 program)
{
    ImCommandSetShaderProgram* command = static_cast<ImCommandSetShaderProgram*>(
        AllocateCommand(IM_CMD_SET_SHADER_PROGRAM, 16));
    if (command)
    {
        command->mi8ShaderProgram = program;
    }
}
#define DEFINE_STATE(TYPE, RECORD, MEMBER, OPCODE, LINE, MESSAGE) \
template<class V> void ImRenderBuffer<V>::SetState(const renderengine::TYPE* value) \
{ if (!value) NativeAssert(MESSAGE, LINE); \
  RECORD* command = static_cast<RECORD*>(AllocateCommand(OPCODE, 16)); \
  if (command) command->MEMBER = value; }
DEFINE_STATE(BlendState, ImCommandSetStateBlend, mpBlendState, 3u, 849, "lpBlendState!=NULL")
DEFINE_STATE(DepthStencilState, ImCommandSetStateDepthStencil, mpDepthStencilState, 4u, 874, "lpDepthStencilState!=NULL")
DEFINE_STATE(RasterizerState, ImCommandSetStateRasterizer, mpRasterizerState, 6u, 924, "lpRasterizerState!=NULL")
DEFINE_STATE(TextureState, ImCommandSetStateTexture, mpTextureState, 9u, 997, "lpTextureState!=NULL")
#undef DEFINE_STATE

template<class V> void ImRenderBuffer<V>::SetTransform(const Im2dTransform& transform)
{
    ImCommandSetTransform* command = static_cast<ImCommandSetTransform*>(AllocateCommand(IM_CMD_SET_TRANSFORM, 80));
    if (command)
    {
        command->mTransform = transform;
    }
}

#define INSTANTIATE(TYPE) \
template bool ImRenderBuffer<TYPE>::Prepare(u32,u32,rw::IResourceAllocator*,bool); \
template void ImRenderBuffer<TYPE>::SetBufferFullRewindToLastEndRender(); \
template bool ImRenderBuffer<TYPE>::IsInARenderingBlock(); \
template void ImRenderBuffer<TYPE>::BeginRendering(); \
template void ImRenderBuffer<TYPE>::EndRendering(); \
template TYPE* ImRenderBuffer<TYPE>::AllocVertices(u32); \
template void ImRenderBuffer<TYPE>::Render(renderengine::PrimitiveType,const TYPE*,u32); \
template void ImRenderBuffer<TYPE>::RenderFromStaticVertexBuffer(renderengine::PrimitiveType,const TYPE*,u32); \
template TYPE* ImRenderBuffer<TYPE>::RenderStart(u32); \
template void ImRenderBuffer<TYPE>::RenderEnd(renderengine::PrimitiveType,const TYPE*,u32); \
template void ImRenderBuffer<TYPE>::SetTexture(renderengine::Texture*); \
template void ImRenderBuffer<TYPE>::SetProgram(s8); \
template void ImRenderBuffer<TYPE>::SetState(const renderengine::BlendState*); \
template void ImRenderBuffer<TYPE>::SetState(const renderengine::DepthStencilState*); \
template void ImRenderBuffer<TYPE>::SetState(const renderengine::RasterizerState*); \
template void ImRenderBuffer<TYPE>::SetState(const renderengine::TextureState*);
INSTANTIATE(Basic2dColouredTexturedVertex)
INSTANTIATE(BasicColouredTexturedVertex)
INSTANTIATE(BasicColouredVertex)
template void ImRenderBuffer<Basic2dColouredTexturedVertex>::SetTransform(const Im2dTransform&);
#undef INSTANTIATE
}
