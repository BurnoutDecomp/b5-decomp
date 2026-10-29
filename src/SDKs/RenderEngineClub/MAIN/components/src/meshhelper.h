#pragma once

#include "types.hpp"

namespace rw { struct Resource; struct ResourceDescriptor; }

namespace renderengine
{
class Device;
class IndexBuffer;
class VertexBuffer;

// ARTIST stores the pointer tail directly after these two counts. Allocation
// size is (2 + indexCount + vertexCount) * sizeof(u32), not sizeof a wrapper.
class MeshHelper
{
public:
    struct Parameters
    {
        IndexBuffer* indexBuffers[1];
        VertexBuffer* vertexBuffers[16];

        Parameters();
        void SetIndexBuffer(IndexBuffer* value, u32 index = 0) { indexBuffers[index] = value; }
        void SetVertexBuffer(VertexBuffer* value, u32 index = 0) { vertexBuffers[index] = value; }
        const IndexBuffer* GetIndexBuffer(u32 index) const { return indexBuffers[index]; }
        const VertexBuffer* GetVertexBuffer(u32 index) const { return vertexBuffers[index]; }
        u32 CountIndexBuffers() const;
        u32 CountVertexBuffers() const;
    };

    static rw::ResourceDescriptor GetResourceDescriptor(const Parameters&);
    static MeshHelper* Initialize(const rw::Resource&, const Parameters&);
    void Release();
    u32 GetNumIndexBuffers() const { return m_numIndexBuffers; }
    u32 GetNumVertexBuffers() const { return m_numVertexBuffers; }
    const VertexBuffer* GetVertexBuffer(u32 index) const;
    VertexBuffer* GetVertexBuffer(u32 index);
    const IndexBuffer* GetIndexBuffer(u32 index) const;
    IndexBuffer* GetIndexBuffer(u32 index);

    template<class TDevice> void Dispatch(TDevice*) const;

protected:
    u32 m_numIndexBuffers;
    u32 m_numVertexBuffers;
    void* m_buffers[1];
};

template<> void MeshHelper::Dispatch<Device>(Device*) const;

CGS_STATIC_ASSERT((sizeof(MeshHelper) == 12), "ARTIST mesh pointer tail");
CGS_STATIC_ASSERT((sizeof(MeshHelper::Parameters) == 68), "ARTIST mesh parameters");
}
