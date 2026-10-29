#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbufferhelper.h"
#include "rw/rwcore_structs.h"

namespace renderengine
{
// ARTIST 82B63E08.
u32 VertexBufferHelper::CalculateStride(const VertexFormat* formats, u32 count)
{
    u32 stride = 0;
    while (count--)
        stride += VertexFormatGetStride(*formats++);
    return stride;
}

// ARTIST 823F8BF0. The native array is terminated by VERTEXFORMAT_NA.
void VertexBufferHelper::Parameters::CalculateBufferSize()
{
    numElements = 0;
    while (elementFormats[numElements] != VERTEXFORMAT_NA) ++numElements;
    stride = VertexBufferHelper::CalculateStride(elementFormats, numElements);
    bufferSize = numVertices * stride;
}

// Native PfxHelper constructor82408348 recalculates before each operation.
rw::ResourceDescriptor VertexBufferHelper::GetResourceDescriptor(Parameters& parameters)
{
    parameters.CalculateBufferSize();
    return VertexBuffer::GetResourceDescriptor(parameters);
}

VertexBufferHelper* VertexBufferHelper::Initialize(const rw::Resource& resource, Parameters& parameters)
{
    parameters.CalculateBufferSize();
    return static_cast<VertexBufferHelper*>(VertexBuffer::Initialize(resource, parameters));
}

// ARTIST 82B63FA8: member receiver in r3; iterator r4; parameters r5; flags r6.
bool VertexBufferHelper::Lock(BatchIterator& iterator, const Parameters& parameters,
    u32 flags, u32 offset, u32 size)
{
    return VertexBuffer::Lock(flags, iterator, parameters.stride * offset, parameters.stride * size);
}

// ARTIST 82B63FF0 is a direct tail branch to the genuine SDK unlock.
void VertexBufferHelper::Unlock(const BatchIterator&, const Parameters&)
{
    D3DVertexBuffer_Unlock(this);
}
}
