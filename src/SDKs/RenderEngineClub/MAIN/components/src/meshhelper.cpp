#include "SDKs/RenderEngineClub/MAIN/components/src/meshhelper.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "rw/rwcore_structs.h"

namespace renderengine
{
MeshHelper::Parameters::Parameters()
{
    indexBuffers[0] = 0;
    for (u32 i = 0; i != 16; ++i) vertexBuffers[i] = 0;
}

u32 MeshHelper::Parameters::CountIndexBuffers() const
{
    u32 count = 0;
    // ARTIST 82B63EB8 / 82B64070 evaluates the pointer before the upper bound.
    while (indexBuffers[count] && count < 1) ++count;
    return count;
}

u32 MeshHelper::Parameters::CountVertexBuffers() const
{
    u32 count = 0;
    while (vertexBuffers[count] && count < 16) ++count;
    return count;
}

// ARTIST 82B64070..82B64104, recovered from raw bytes (export set has a hole).
rw::ResourceDescriptor MeshHelper::GetResourceDescriptor(const Parameters& parameters)
{
    const u32 countVertex = parameters.CountVertexBuffers();
    const u32 countIndex = parameters.CountIndexBuffers();
    rw::ResourceDescriptor descriptor;
    for (u32 i = 0; i != 5; ++i)
    {
        descriptor.m_baseResourceDescriptors[i].m_size = 0;
        descriptor.m_baseResourceDescriptors[i].m_alignment = 1;
    }
    descriptor.m_baseResourceDescriptors[0].m_size = (2 + countIndex + countVertex) * 4;
    descriptor.m_baseResourceDescriptors[0].m_alignment = 4;
    return descriptor;
}

// ARTIST 82B63EB8.
MeshHelper* MeshHelper::Initialize(const rw::Resource& resource, const Parameters& parameters)
{
    MeshHelper* const mesh = static_cast<MeshHelper*>(resource.m_baseResources[0]);
    mesh->m_numIndexBuffers = parameters.CountIndexBuffers();
    mesh->m_numVertexBuffers = parameters.CountVertexBuffers();
    u32 buffer = 0;
    for (u32 i = 0; i < mesh->m_numIndexBuffers; ++i)
        mesh->m_buffers[buffer++] = parameters.indexBuffers[i];
    for (u32 i = 0; i < mesh->m_numVertexBuffers; ++i)
        mesh->m_buffers[buffer++] = parameters.vertexBuffers[i];
    return mesh;
}

// ARTIST 82402F20 calls this ICF-folded body at 82AD5078: exactly 4E800020.
void MeshHelper::Release()
{
}

const VertexBuffer* MeshHelper::GetVertexBuffer(u32 index) const
{
    return static_cast<const VertexBuffer*>(m_buffers[m_numIndexBuffers + index]);
}
VertexBuffer* MeshHelper::GetVertexBuffer(u32 index)
{
    return static_cast<VertexBuffer*>(m_buffers[m_numIndexBuffers + index]);
}
const IndexBuffer* MeshHelper::GetIndexBuffer(u32 index) const
{
    return static_cast<const IndexBuffer*>(m_buffers[index]);
}
IndexBuffer* MeshHelper::GetIndexBuffer(u32 index)
{
    return static_cast<IndexBuffer*>(m_buffers[index]);
}

// ARTIST's mangled QBAXPAVDevice@1@ signature is a const member, with a device
// argument. Its body binds the native singleton, SDK stride=0 and offset=0.
template<> void MeshHelper::Dispatch<Device>(Device*) const
{
    u32 index = 0;
    for (; index < m_numIndexBuffers; ++index)
        D3DDevice_SetIndices(::gpD3DDevice, reinterpret_cast<D3DIndexBuffer*>(m_buffers[index]));
    for (u32 stream = 0; stream < m_numVertexBuffers; ++stream)
        D3DDevice_SetStreamSource_Inline(::gpD3DDevice, stream,
            static_cast<VertexBuffer*>(m_buffers[index + stream]), 0, 0);
}
}
