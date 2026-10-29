#pragma once

#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/vertexformat.h"

namespace renderengine
{
class VertexBufferHelper : public VertexBuffer
{
public:
    struct Parameters : public VertexBuffer::Parameters
    {
        u32 numVertices;
        VertexFormat elementFormats[16];
        u32 numElements;
        u32 stride;

        void SetNumVertices(u32 value) { numVertices = value; }
        void SetElementFormat(u32 index, VertexFormat value) { elementFormats[index] = value; }
        u32 GetNumVertices() const { return numVertices; }
        VertexFormat GetElementFormat(u32 index) const { return elementFormats[index]; }
        void CalculateBufferSize();
    };

    static rw::ResourceDescriptor GetResourceDescriptor(Parameters&);
    static VertexBufferHelper* Initialize(const rw::Resource&, Parameters&);
    static u32 CalculateStride(const VertexFormat*, u32 count);
    bool Lock(BatchIterator&, const Parameters&, u32 flags, u32 offset = 0, u32 size = 0);
    void Unlock(const BatchIterator&, const Parameters&);
};

CGS_STATIC_ASSERT((sizeof(VertexBufferHelper) == 0x28), "ARTIST helper adds no vertex fields");
CGS_STATIC_ASSERT((sizeof(VertexBufferHelper::Parameters) == 0x54), "ARTIST helper parameters");
CGS_STATIC_ASSERT((offsetof(VertexBufferHelper::Parameters, elementFormats) == 0xC), "ARTIST helper formats");
CGS_STATIC_ASSERT((offsetof(VertexBufferHelper::Parameters, numElements) == 0x4C), "ARTIST helper count");
CGS_STATIC_ASSERT((offsetof(VertexBufferHelper::Parameters, stride) == 0x50), "ARTIST helper stride");
}
