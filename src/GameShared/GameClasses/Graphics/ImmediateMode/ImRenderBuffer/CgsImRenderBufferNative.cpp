#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h"
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
    CgsDev::Assert::FireAssert(expression, kFile, line);
    CgsDev::Assert::EndAssert();
}
}

// ARTIST82404A50/82404308/824045B0 allocate both streams for both buffers,
// then clear and swap twice, leaving buffer zero as the writer.
template<class V>
bool ImRenderBuffer<V>::Prepare(u32 commandSize, u32 vertexSize,
                                rw::IResourceAllocator* allocator, bool failGracefully)
{
    muCommandBufferSize = commandSize;
    muVertexBufferSize = vertexSize;
    mbFailGracefully = failGracefully;
    rw::ResourceDescriptor commandDescriptor;
    commandDescriptor.m_baseResourceDescriptors[0].m_size = commandSize + 128;
    commandDescriptor.m_baseResourceDescriptors[0].m_alignment = 128;
    rw::ResourceDescriptor vertexDescriptor;
    vertexDescriptor.m_baseResourceDescriptors[0].m_size = vertexSize + 128;
    vertexDescriptor.m_baseResourceDescriptors[0].m_alignment = 128;
    maBuffers[0].mpu8CommandBuffer = static_cast<u8*>(allocator->DoAllocate(commandDescriptor, 0).m_baseResources[0]);
    maBuffers[0].mpu8VertexBuffer = static_cast<u8*>(allocator->DoAllocate(vertexDescriptor, 0).m_baseResources[0]);
    maBuffers[1].mpu8CommandBuffer = static_cast<u8*>(allocator->DoAllocate(commandDescriptor, 0).m_baseResources[0]);
    maBuffers[1].mpu8VertexBuffer = static_cast<u8*>(allocator->DoAllocate(vertexDescriptor, 0).m_baseResources[0]);
    if (!maBuffers[0].mpu8CommandBuffer) NativeAssert("maBuffers[0].mpu8CommandBuffer", 511);
    if (!maBuffers[0].mpu8VertexBuffer) NativeAssert("maBuffers[0].mpu8VertexBuffer", 512);
    if (!maBuffers[1].mpu8CommandBuffer) NativeAssert("maBuffers[1].mpu8CommandBuffer", 513);
    if (!maBuffers[1].mpu8VertexBuffer) NativeAssert("maBuffers[1].mpu8VertexBuffer", 514);
    mpWriteBuffer = &maBuffers[0]; miWriteBufferIndex = 0; mpDispatchBuffer = &maBuffers[1];
    Clear(); Swap(); Clear(); Swap();
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

// The original appender warms the next 128-byte line before testing capacity.
#define RESERVE_COMMAND(TYPE, BYTES, OPCODE) \
    __dcbz128(static_cast<int>(mpWriteBuffer->muCommandBufferWritePos + 128u), mpWriteBuffer->mpu8CommandBuffer); \
    const u32 position = mpWriteBuffer->muCommandBufferWritePos; \
    const u32 next = position + (BYTES); \
    if (next > muCommandBufferSize) { SetBufferFullRewindToLastEndRender(); \
        if (!mbFailGracefully) NativeAssert("ImRenderBuffer command buffer is full", 1254); return; } \
    TYPE* command = reinterpret_cast<TYPE*>(mpWriteBuffer->mpu8CommandBuffer + position); \
    command->muSize = (BYTES); command->muType = (OPCODE); mpWriteBuffer->muCommandBufferWritePos = next
#define ASSERT_IN_BLOCK() if (!mbInRenderBlock) NativeAssert("Command called outside of a BeginRendering/EndRendering block", 1216)

template<class V> void ImRenderBuffer<V>::BeginRendering()
{
    if (mbInRenderBlock) NativeAssert("BeginRendering called inside a BeginRendering/EndRendering block", 619);
    mbInRenderBlock = true;
    RESERVE_COMMAND(ImCommand, 16u, 0u);
}

template<class V> void ImRenderBuffer<V>::EndRendering()
{
    if (!mbInRenderBlock) NativeAssert("EndRendering called without a matching call to BeginRendering", 658);
    mbInRenderBlock = false;
    RESERVE_COMMAND(ImCommand, 16u, 1u);
    muLastEndRenderPos = position;
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
    ASSERT_IN_BLOCK();
    RESERVE_COMMAND(ImCommandRenderPrimitives<V>, 32u, 2u);
    command->mePrimitiveType = primitive; command->muNumVertices = count; command->mpVertices = vertices;
    std::memcpy(vertices, source, sizeof(V) * count);
}

template<class V> void ImRenderBuffer<V>::RenderFromStaticVertexBuffer(renderengine::PrimitiveType primitive, const V* vertices, u32 count)
{
    if (!vertices) NativeAssert("lpVertices", 731);
    ASSERT_IN_BLOCK();
    RESERVE_COMMAND(ImCommandRenderPrimitives<V>, 32u, 2u);
    command->mePrimitiveType = primitive; command->muNumVertices = count; command->mpVertices = vertices;
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
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, kFile, 765);
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
    ASSERT_IN_BLOCK();
    RESERVE_COMMAND(ImCommandRenderPrimitives<V>, 32u, 2u);
    command->mePrimitiveType = primitive; command->muNumVertices = count; command->mpVertices = vertices;
}

template<class V> void ImRenderBuffer<V>::SetTexture(renderengine::Texture* texture)
{
    if (!texture) NativeAssert("lpTexture!=NULL", 825);
    ASSERT_IN_BLOCK(); RESERVE_COMMAND(ImCommandSetTexture, 16u, 10u); command->mpTexture = texture;
}
template<class V> void ImRenderBuffer<V>::SetProgram(s8 program)
{
    ASSERT_IN_BLOCK(); RESERVE_COMMAND(ImCommandSetShaderProgram, 16u, 15u); command->mi8ShaderProgram = program;
}
#define DEFINE_STATE(TYPE, RECORD, MEMBER, OPCODE, LINE, MESSAGE) \
template<class V> void ImRenderBuffer<V>::SetState(const renderengine::TYPE* value) \
{ if (!value) NativeAssert(MESSAGE, LINE); ASSERT_IN_BLOCK(); \
  RESERVE_COMMAND(RECORD, 16u, OPCODE); command->MEMBER = value; }
DEFINE_STATE(BlendState, ImCommandSetStateBlend, mpBlendState, 3u, 849, "lpBlendState!=NULL")
DEFINE_STATE(DepthStencilState, ImCommandSetStateDepthStencil, mpDepthStencilState, 4u, 874, "lpDepthStencilState!=NULL")
DEFINE_STATE(RasterizerState, ImCommandSetStateRasterizer, mpRasterizerState, 6u, 924, "lpRasterizerState!=NULL")
DEFINE_STATE(TextureState, ImCommandSetStateTexture, mpTextureState, 9u, 997, "lpTextureState!=NULL")
#undef DEFINE_STATE

template<class V> void ImRenderBuffer<V>::SetTransform(const Im2dTransform& transform)
{
    ASSERT_IN_BLOCK(); RESERVE_COMMAND(ImCommandSetTransform, 80u, 16u); command->mTransform = transform;
}
#undef ASSERT_IN_BLOCK
#undef RESERVE_COMMAND

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
